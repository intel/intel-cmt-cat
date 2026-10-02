/*
 * BSD LICENSE
 *
 * Copyright(c) 2022-2026 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "log.h"
#include "test.h"

#include <fcntl.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* What a message longer than the payload buffer has to arrive as: all of what
 * fitted, so the buffer's capacity, and not merely something shorter than the
 * message started out as.
 */
#define LOG_PAYLOAD_MAX (AP_BUFFER_SIZE - 2)

/* what log_printf() hands its callback, per call */
static size_t callback_size;
static char callback_text[8 * 1024];
static unsigned callback_calls;

static void
log_callback(void *context __attribute__((unused)),
             const size_t size,
             const char *message)
{
        callback_calls++;
        callback_size = size;

        assert_true(size < sizeof(callback_text));
        memcpy(callback_text, message, size);
        callback_text[size] = '\0';
}

/* a message of a chosen length, built from one repeated character */
static char *
message_of(const size_t length)
{
        char *text = malloc(length + 1);

        assert_non_null(text);
        memset(text, 'x', length);
        text[length] = '\0';

        return text;
}

/* The lifetime cases below hold a message inside the application's callback and
 * ask what a teardown does about it. Everything the two threads tell each other
 * goes through these, because an assertion belongs on the thread cmocka is on.
 */
static pthread_mutex_t hold_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t hold_cond = PTHREAD_COND_INITIALIZER;
static int callback_entered;   /**< a message has reached the callback */
static int callback_may_leave; /**< and may return from it */
static int wait_returned;      /**< log_wait_quiescent() has come back */
static int nested_teardown;    /**< the callback took the log down itself */

/**
 * @brief A callback that stops until it is let go
 */
static void
holding_callback(void *context __attribute__((unused)),
                 const size_t size __attribute__((unused)),
                 const char *message __attribute__((unused)))
{
        pthread_mutex_lock(&hold_mutex);
        callback_entered = 1;
        pthread_cond_broadcast(&hold_cond);
        while (!callback_may_leave)
                pthread_cond_wait(&hold_cond, &hold_mutex);
        pthread_mutex_unlock(&hold_mutex);
}

/**
 * @brief A callback that takes the log down the way pqos_fini() does
 *
 * Retire, then wait - from inside an emission, which is the case that must not
 * wait for itself.
 */
static void
finalizing_callback(void *context __attribute__((unused)),
                    const size_t size __attribute__((unused)),
                    const char *message __attribute__((unused)))
{
        log_fini();
        log_wait_quiescent();
        nested_teardown = 1;
}

/** whether the streaming thread below should stop */
static int stream_stop;

/** how many messages the streaming case's callback has been handed */
static int stream_calls;

/**
 * @brief The streaming case's callback: it counts, and nothing more
 *
 * Deliberately no cmocka assertion and no shared buffer. A cmocka assertion
 * leaves by longjmp() to the jmp_buf of the thread cmocka is running the case
 * on, so one that fired here - on the streaming thread - would jump into
 * another thread's stack. The only thing the case needs from this side is that
 * messages arrived, which one atomic counter says.
 */
static void
stream_callback(void *context __attribute__((unused)),
                const size_t size __attribute__((unused)),
                const char *message __attribute__((unused)))
{
        __atomic_add_fetch(&stream_calls, 1, __ATOMIC_ACQ_REL);
}

/**
 * @brief Logs without pause until it is told to stop
 */
static void *
stream_thread(void *arg __attribute__((unused)))
{
        while (!__atomic_load_n(&stream_stop, __ATOMIC_ACQUIRE))
                log_printf(LOG_OPT_INFO, "one of many messages\n");

        return NULL;
}

static void *
emit_thread(void *arg __attribute__((unused)))
{
        log_printf(LOG_OPT_INFO, "a message that stops in the callback\n");

        return NULL;
}

static void *
wait_thread(void *arg __attribute__((unused)))
{
        log_wait_quiescent();
        __atomic_store_n(&wait_returned, 1, __ATOMIC_RELEASE);

        return NULL;
}

/**
 * @brief Starts a message and waits until it is inside the callback
 *
 * @param [out] thread the thread the message is on
 */
static void
start_a_held_message(pthread_t *thread)
{
        callback_entered = 0;
        callback_may_leave = 0;
        wait_returned = 0;

        assert_int_equal(log_init(-1, holding_callback, NULL, LOG_VER_VERBOSE),
                         LOG_RETVAL_OK);
        assert_int_equal(pthread_create(thread, NULL, emit_thread, NULL), 0);

        pthread_mutex_lock(&hold_mutex);
        while (!callback_entered)
                pthread_cond_wait(&hold_cond, &hold_mutex);
        pthread_mutex_unlock(&hold_mutex);
}

/**
 * @brief Lets the held message finish, and waits for its thread
 */
static void
release_the_held_message(pthread_t thread)
{
        pthread_mutex_lock(&hold_mutex);
        callback_may_leave = 1;
        pthread_cond_broadcast(&hold_cond);
        pthread_mutex_unlock(&hold_mutex);

        assert_int_equal(pthread_join(thread, NULL), 0);
}

/** a tenth of a second, which every case here uses to let the other thread run
 */
static void
sleep_a_moment(void)
{
        struct timespec ts;

        ts.tv_sec = 0;
        ts.tv_nsec = 100 * 1000 * 1000;
        (void)nanosleep(&ts, NULL);
}

static void
test_a_teardown_waits_for_a_message_already_on_its_way(void **state
                                                       __attribute__((unused)))
{
        pthread_t emitter, waiter;

        start_a_held_message(&emitter);

        /* the log is taken down while that message is inside the callback -
         * which is exactly the message pqos_fini() must not leave behind,
         * because the descriptor and the context it is using are the
         * application's to reclaim as soon as pqos_fini() returns
         */
        log_fini();
        assert_int_equal(log_is_initialized(), 0);

        assert_int_equal(pthread_create(&waiter, NULL, wait_thread, NULL), 0);
        sleep_a_moment();

        /* still inside the callback, so the wait has not finished */
        assert_int_equal(__atomic_load_n(&wait_returned, __ATOMIC_ACQUIRE), 0);

        release_the_held_message(emitter);
        assert_int_equal(pthread_join(waiter, NULL), 0);
        assert_int_equal(__atomic_load_n(&wait_returned, __ATOMIC_ACQUIRE), 1);
}

static void
test_the_wait_ends_even_for_a_callback_that_does_not(void **state
                                                     __attribute__((unused)))
{
        pthread_t emitter, waiter;
        struct timespec start, end;
        double seconds;

        start_a_held_message(&emitter);
        log_fini();

        /* the callback never returns on its own, and the wait still does: an
         * application must not be able to hang its own pqos_fini() with a
         * callback of its own
         */
        assert_int_equal(clock_gettime(CLOCK_MONOTONIC, &start), 0);
        assert_int_equal(pthread_create(&waiter, NULL, wait_thread, NULL), 0);
        assert_int_equal(pthread_join(waiter, NULL), 0);
        assert_int_equal(clock_gettime(CLOCK_MONOTONIC, &end), 0);

        seconds = (double)(end.tv_sec - start.tv_sec) +
                  (double)(end.tv_nsec - start.tv_nsec) / 1e9;

        /* it waited rather than passing straight through, and it came back
         * rather than waiting for ever
         */
        assert_true(seconds > 0.5);
        /* the bound is a second, and this is what the wait may overrun it by
         * before the case calls it unbounded: a sleep that the kernel returns
         * late, on a machine running every other case beside this one
         */
        assert_true(seconds < 3.0);

        release_the_held_message(emitter);
}

static void
test_an_install_and_a_teardown_beside_a_stream_of_messages(
    void **state __attribute__((unused)))
{
        pthread_t emitter;
        unsigned i;

        __atomic_store_n(&stream_calls, 0, __ATOMIC_RELEASE);
        stream_stop = 0;
        assert_int_equal(log_init(-1, stream_callback, NULL, LOG_VER_VERBOSE),
                         LOG_RETVAL_OK);

        /* pqos_open() and pqos_fopen() log through LOG_ERROR_IF_INIT() without
         * the API lock, so they can be emitting while pqos_fini() runs - which
         * is the reachable half of this race. Here the message is the same
         * shape with nothing else in the way: one thread emits, this one
         * installs and retires underneath it.
         */
        assert_int_equal(pthread_create(&emitter, NULL, stream_thread, NULL),
                         0);

        /* Installed over, rather than taken down and put back: a log_fini()
         * here would leave the destination empty for as long as it takes to
         * reach the log_init() after it, and log_printf() asserts that it has
         * one - a DEBUG build aborts on that, which is the library behaving as
         * documented rather than the race this case is about. An install over a
         * live destination retires it exactly as a teardown does, so the
         * lifetime being tested is the same one, and the destination is never
         * absent while the other thread emits.
         */
        for (i = 0; i < 200; i++)
                assert_int_equal(
                    log_init(-1, stream_callback, NULL, LOG_VER_VERBOSE),
                    LOG_RETVAL_OK);

        __atomic_store_n(&stream_stop, 1, __ATOMIC_RELEASE);
        assert_int_equal(pthread_join(emitter, NULL), 0);

        /* and the teardown once the stream has stopped, which is the other half
         * of what the loop exercised
         */
        log_fini();
        assert_int_equal(log_is_initialized(), 0);
        assert_int_equal(log_init(-1, stream_callback, NULL, LOG_VER_VERBOSE),
                         LOG_RETVAL_OK);

        /* what this asserts is that nothing crashed and the log is usable
         * afterwards; that no message wrote to a destination already freed is
         * what AddressSanitizer is for, and this case is the one it is run
         * against
         */
        log_printf(LOG_OPT_INFO, "after the storm\n");
        assert_true(__atomic_load_n(&stream_calls, __ATOMIC_ACQUIRE) > 0);
        log_fini();
}

static int
log_setup(void **state __attribute__((unused)))
{
        callback_calls = 0;
        callback_size = 0;
        memset(callback_text, 0, sizeof(callback_text));

        assert_int_equal(
            log_init(-1, log_callback, NULL, LOG_VER_SUPER_VERBOSE),
            LOG_RETVAL_OK);

        return 0;
}

static int
log_teardown(void **state __attribute__((unused)))
{
        log_fini();

        return 0;
}

static void
test_log_printf_reports_what_it_wrote(void **state __attribute__((unused)))
{
        const char *text = "a short line\n";

        log_printf(LOG_OPT_INFO, "%s", text);

        assert_int_equal(callback_calls, 1);
        assert_int_equal(callback_size, strlen(text));
        assert_string_equal(callback_text, text);
}

static void
test_log_printf_clamps_a_long_message(void **state __attribute__((unused)))
{
        /* longer than the payload buffer, so vsnprintf() truncates and returns
         * the length the message needed rather than the length it wrote
         */
        char *text = message_of(4 * 1024);

        log_printf(LOG_OPT_INFO, "%s", text);
        free(text);

        assert_int_equal(callback_calls, 1);
        /* the length has to describe the text that was written - all of it, so
         * the buffer's capacity, and not some other value below what the
         * message started as
         */
        assert_int_equal(callback_size, LOG_PAYLOAD_MAX);
        assert_int_equal(callback_size, strlen(callback_text));
        /* and the text is the message, as far as it fitted */
        assert_int_equal(strspn(callback_text, "x"), LOG_PAYLOAD_MAX);
}

static void
test_log_printf_writes_the_same_length_to_a_file(void **state
                                                 __attribute__((unused)))
{
        char path[] = "/tmp/pqos-test-log-XXXXXX";
        char *text = message_of(4 * 1024);
        char written[8 * 1024];
        ssize_t length;
        int fd = mkstemp(path);

        assert_true(fd >= 0);
        log_fini();
        assert_int_equal(
            log_init(fd, log_callback, NULL, LOG_VER_SUPER_VERBOSE),
            LOG_RETVAL_OK);

        log_printf(LOG_OPT_INFO, "%s", text);
        free(text);

        assert_int_equal(lseek(fd, 0, SEEK_SET), 0);
        length = read(fd, written, sizeof(written));
        close(fd);
        unlink(path);

        /* the file gets exactly the truncated text, not the length the message
         * would have needed - which is what read past the end of the buffer
         */
        assert_true(length >= 0);
        assert_int_equal((size_t)length, LOG_PAYLOAD_MAX);
        assert_int_equal((size_t)length, callback_size);
        written[length] = '\0';
        assert_int_equal(strspn(written, "x"), LOG_PAYLOAD_MAX);
}

int
main(void)
{
        int result = 0;

        const struct CMUnitTest tests_log[] = {
            cmocka_unit_test_setup_teardown(
                test_log_printf_reports_what_it_wrote, log_setup, log_teardown),
            cmocka_unit_test_setup_teardown(
                test_log_printf_clamps_a_long_message, log_setup, log_teardown),
            cmocka_unit_test_setup_teardown(
                test_log_printf_writes_the_same_length_to_a_file, log_setup,
                log_teardown),
            /* the lifetime cases install and retire the log themselves, so they
             * take neither the setup nor the teardown
             */
            cmocka_unit_test(
                test_a_teardown_waits_for_a_message_already_on_its_way),
            cmocka_unit_test(
                test_the_wait_ends_even_for_a_callback_that_does_not),
            cmocka_unit_test(
                test_an_install_and_a_teardown_beside_a_stream_of_messages)};

        result += cmocka_run_group_tests(tests_log, NULL, NULL);

        return result;
}
