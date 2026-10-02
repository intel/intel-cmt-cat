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

#include "cap.h"
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

/** the thread the case is running on, for the callback below */
static pthread_t case_thread;
/** the nested teardown has reached the drain */
static int wait_entered;
/** the held emitter had been let go by the time the drain returned */
static int released_before_return;
/** how long the releaser leaves the drain waiting, in microseconds. Not a
 *  threshold anything is asserted against - what the case asserts is the order
 *  of two events - only long enough that a drain which does not wait is seen to
 *  return before the release rather than racing it
 */
#define HELD_FOR_US 50000

/**
 * @brief Finalizes on the case's thread and holds on any other
 *
 * One destination has one callback, and the two-emitter case needs the two
 * threads to do different things in it: the case's thread takes the log down
 * from inside its own message, and the other thread is the message that
 * teardown has to wait for.
 */
static void
finalizing_or_holding_callback(void *context __attribute__((unused)),
                               const size_t size __attribute__((unused)),
                               const char *message __attribute__((unused)))
{
        if (pthread_equal(pthread_self(), case_thread)) {
                log_fini();

                /* the releaser waits for this, so that what it is timing is the
                 * drain and not the creation of a thread
                 */
                pthread_mutex_lock(&hold_mutex);
                wait_entered = 1;
                pthread_cond_broadcast(&hold_cond);
                pthread_mutex_unlock(&hold_mutex);

                log_wait_quiescent();

                /* and the question the case asks: had the other emitter been
                 * let go by the time this returned? A drain that waited for it
                 * cannot return before it is released; one that skipped the
                 * wait returns while it is still held
                 */
                pthread_mutex_lock(&hold_mutex);
                released_before_return = callback_may_leave;
                pthread_mutex_unlock(&hold_mutex);

                nested_teardown = 1;

                return;
        }

        pthread_mutex_lock(&hold_mutex);
        callback_entered = 1;
        pthread_cond_broadcast(&hold_cond);
        while (!callback_may_leave)
                pthread_cond_wait(&hold_cond, &hold_mutex);
        pthread_mutex_unlock(&hold_mutex);
}

/**
 * @brief Lets the held message go after a while
 *
 * From a thread of its own because the case's thread is inside the teardown
 * that is waiting for it, and cannot release it itself.
 */
static void *
releaser_thread(void *arg __attribute__((unused)))
{
        /* not a sleep of its own length: it waits for the drain to be entered,
         * so the release cannot happen before the thing it is meant to outlast
         * has started
         */
        pthread_mutex_lock(&hold_mutex);
        while (!wait_entered)
                pthread_cond_wait(&hold_cond, &hold_mutex);
        pthread_mutex_unlock(&hold_mutex);

        usleep(HELD_FOR_US);

        pthread_mutex_lock(&hold_mutex);
        callback_may_leave = 1;
        pthread_cond_broadcast(&hold_cond);
        pthread_mutex_unlock(&hold_mutex);

        return NULL;
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
test_a_teardown_from_inside_a_callback_is_quick(void **state
                                                __attribute__((unused)))
{
        struct timespec start, end;
        double seconds;

        nested_teardown = 0;

        assert_int_equal(
            log_init(-1, finalizing_callback, NULL, LOG_VER_VERBOSE),
            LOG_RETVAL_OK);

        /* the callback does what pqos_fini() does - retire the destination and
         * wait for the messages using it - from inside the one message there
         * is. Waiting for that message would be waiting for itself, so it does
         * not.
         */
        assert_int_equal(clock_gettime(CLOCK_MONOTONIC, &start), 0);
        log_printf(LOG_OPT_INFO, "a message whose callback finalizes\n");
        assert_int_equal(clock_gettime(CLOCK_MONOTONIC, &end), 0);

        assert_int_equal(nested_teardown, 1);
        assert_int_equal(log_is_initialized(), 0);

        /* and it did not wait: without the guard this is the whole drain, a
         * second of sleeping for a message that cannot finish until it returns.
         * Half of that is the threshold rather than a tenth, because what
         * distinguishes the two is a second and a case descheduled on a loaded
         * machine should not be read as the guard failing
         */
        seconds = (double)(end.tv_sec - start.tv_sec) +
                  (double)(end.tv_nsec - start.tv_nsec) / 1e9;
        assert_true(seconds < 0.5);
}

static void
test_a_teardown_from_a_callback_waits_for_the_other_emitter(
    void **state __attribute__((unused)))
{
        pthread_t emitter, releaser;

        /* Two messages on one destination, and a teardown reached from one of
         * them. Its own message is the one it must not wait for - that message
         * cannot finish until the teardown returns - and the other thread's is
         * one it must. Returning as soon as this thread was inside an emission
         * did both, so the second emitter kept using a destination the caller
         * had been told was finished with.
         *
         * The case's own thread is the one that finalizes; the second emitter
         * is held in the callback and let go by a thread of its own, because
         * this one is inside the wait.
         *
         * What is asserted is the order of two events and not how long anything
         * took: the releaser waits for the drain to be entered before it
         * starts, and the drain records whether the other emitter had been let
         * go by the time it returned. A wall-clock threshold would have this
         * case fail on a loaded machine that descheduled either thread.
         */
        nested_teardown = 0;
        callback_entered = 0;
        callback_may_leave = 0;
        wait_entered = 0;
        released_before_return = 0;
        case_thread = pthread_self();

        assert_int_equal(
            log_init(-1, finalizing_or_holding_callback, NULL, LOG_VER_VERBOSE),
            LOG_RETVAL_OK);

        assert_int_equal(pthread_create(&emitter, NULL, emit_thread, NULL), 0);
        pthread_mutex_lock(&hold_mutex);
        while (!callback_entered)
                pthread_cond_wait(&hold_cond, &hold_mutex);
        pthread_mutex_unlock(&hold_mutex);

        assert_int_equal(pthread_create(&releaser, NULL, releaser_thread, NULL),
                         0);

        log_printf(LOG_OPT_INFO, "a message whose callback finalizes\n");

        assert_int_equal(nested_teardown, 1);
        assert_int_equal(log_is_initialized(), 0);

        /* it waited for the other emitter: the drain cannot have returned
         * before that emitter was released, and one that skipped the wait
         * returns while it is still held - which is what returning on the
         * strength of "this thread is inside an emission" did
         */
        assert_int_equal(released_before_return, 1);

        assert_int_equal(pthread_join(emitter, NULL), 0);
        assert_int_equal(pthread_join(releaser, NULL), 0);
}

static void
test_the_uninitialized_diagnostic_needs_no_destination(void **state
                                                       __attribute__((unused)))
{
        /* What an API says when there is no library is said by a library that
         * may have no log either: a callback that calls one while pqos_fini()
         * is finalizing arrives after the log has been taken down, and so does
         * any caller after it. LOG_ERROR asserts on a destination that is not
         * there, so in a DEBUG build the diagnostic aborted the application
         * whose only mistake was the one being diagnosed - LOG_ERROR_IF_INIT
         * says nothing and returns.
         *
         * A regression shows as this binary dying on
         * "log_message: Assertion `!required || dest != NULL' failed" in a
         * DEBUG build, which is the thing being prevented and cannot be caught
         * from inside.
         */
        assert_int_equal(log_fini(), LOG_RETVAL_OK);
        assert_int_equal(log_is_initialized(), 0);

        assert_int_equal(_pqos_check_init(1), PQOS_RETVAL_INIT);

        /* and the library is not initialized here, so the other arm is the one
         * that answers OK
         */
        assert_int_equal(_pqos_check_init(0), PQOS_RETVAL_OK);

        /* the silent log back, because test.h installs one for every case and
         * the next of them expects it
         */
        test_log_init_silent();
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

static void
test_a_caller_that_may_run_before_the_log_says_nothing(void **state
                                                       __attribute__((unused)))
{
        /* LOG_ERROR_IF_INIT() is what pqos_open() and pqos_fopen() use, and
         * they run before pqos_init(). It used to test log_is_initialized() and
         * then call log_printf(), which asserts - so a teardown between the two
         * left the second asserting on a destination that was there when the
         * first looked, and a DEBUG build aborted. Both of those callers log
         * without the API lock, so the window is reachable.
         *
         * One call now takes the destination or finds there is none. With no
         * log installed at all - the strongest form of "there is none" - this
         * must return quietly, in a DEBUG build as much as in a release one.
         */
        log_fini();
        assert_int_equal(log_is_initialized(), 0);

        log_printf_if_init(LOG_OPT_ERROR, "a message with nowhere to go\n");

        /* and with one installed it writes, like any other message */
        callback_calls = 0;
        assert_int_equal(log_init(-1, log_callback, NULL, LOG_VER_VERBOSE),
                         LOG_RETVAL_OK);
        log_printf_if_init(LOG_OPT_ERROR, "a message with somewhere to go\n");
        assert_int_equal(callback_calls, 1);
        log_fini();
}

/** a pipe whose write end is full, so a message to it blocks in write() */
static int blocked_pipe[2] = {-1, -1};

/**
 * @brief Logs until it is cancelled, which happens inside write()
 */
static void *
blocked_emitter(void *arg __attribute__((unused)))
{
        /* and no return after it: the loop does not end, and the thread leaves
         * by being cancelled inside the write(), which is what the case is
         * about
         */
        for (;;)
                log_printf(LOG_OPT_INFO, "a message that cannot be written\n");
}

static void
test_a_cancelled_emission_gives_back_what_it_held(void **state
                                                  __attribute__((unused)))
{
        pthread_t emitter;
        struct timespec start, end;
        double seconds;
        char full[4096];

        /* write() is a POSIX cancellation point, so a thread cancelled while
         * blocked in it leaves the emission without running the code after it.
         * The reference that emission holds is given back by a pthread cleanup
         * handler - without one the destination would be held for the life of
         * the process, and every later wait would spend the whole bound looking
         * for it.
         */
        assert_int_equal(pipe(blocked_pipe), 0);
        memset(full, 'x', sizeof(full));

        /* non-blocking to fill it, blocking afterwards: a blocking write to a
         * full pipe is what the emitter has to park in, and a blocking write is
         * also what would park this loop for ever
         */
        assert_int_equal(fcntl(blocked_pipe[1], F_SETFL,
                               fcntl(blocked_pipe[1], F_GETFL) | O_NONBLOCK),
                         0);
        while (write(blocked_pipe[1], full, sizeof(full)) > 0)
                ;
        assert_int_equal(fcntl(blocked_pipe[1], F_SETFL,
                               fcntl(blocked_pipe[1], F_GETFL) & ~O_NONBLOCK),
                         0);

        assert_int_equal(log_init(blocked_pipe[1], NULL, NULL, LOG_VER_VERBOSE),
                         LOG_RETVAL_OK);
        assert_int_equal(pthread_create(&emitter, NULL, blocked_emitter, NULL),
                         0);
        sleep_a_moment();

        assert_int_equal(pthread_cancel(emitter), 0);
        assert_int_equal(pthread_join(emitter, NULL), 0);

        log_fini();

        assert_int_equal(clock_gettime(CLOCK_MONOTONIC, &start), 0);
        log_wait_quiescent();
        assert_int_equal(clock_gettime(CLOCK_MONOTONIC, &end), 0);

        seconds = (double)(end.tv_sec - start.tv_sec) +
                  (double)(end.tv_nsec - start.tv_nsec) / 1e9;

        /* prompt, because nothing is holding the retired destination any more.
         * Without the cleanup handler this is the whole bound
         */
        assert_true(seconds < 0.5);

        close(blocked_pipe[0]);
        close(blocked_pipe[1]);
        blocked_pipe[0] = -1;
        blocked_pipe[1] = -1;
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
            cmocka_unit_test(test_a_teardown_from_inside_a_callback_is_quick),
            cmocka_unit_test(
                test_a_teardown_from_a_callback_waits_for_the_other_emitter),
            cmocka_unit_test(
                test_the_uninitialized_diagnostic_needs_no_destination),
            cmocka_unit_test(
                test_an_install_and_a_teardown_beside_a_stream_of_messages),
            cmocka_unit_test(
                test_a_caller_that_may_run_before_the_log_says_nothing),
            cmocka_unit_test(
                test_a_cancelled_emission_gives_back_what_it_held)};

        result += cmocka_run_group_tests(tests_log, NULL, NULL);

        return result;
}
