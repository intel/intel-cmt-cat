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

/*
 * The API lock, held by a thread that leaves without releasing it.
 *
 * An API call holds the lock while it logs, and a log callback can leave by
 * pthread_exit() or be cancelled. These cases do what such a callback does to
 * the lock - take it, then leave - with the real mutex and real threads, and
 * ask whether any other thread can take the lock afterwards.
 */

#include "lock.h"
#include "test.h"

#include <pthread.h>
#include <time.h>
#include <unistd.h>

/** how long a lock that should be free is waited for, in milliseconds */
#define FREE_WAIT_MS 2000

/* The flags below are shared between threads, so they are read and written
 * with the __atomic builtins, as test_log.c's are, rather than as volatile.
 */

/** set by a probe thread once it holds the lock */
static int m_probe_got;
/** set by a holder thread once it holds the lock */
static int m_holder_has;
/** set by the case when the holder may leave */
static int m_holder_go;

static void
sleep_ms(const unsigned ms)
{
        struct timespec ts = {ms / 1000, (long)(ms % 1000) * 1000000L};

        (void)nanosleep(&ts, NULL);
}

/**
 * @brief Waits until a flag is set, or the time is up
 *
 * @return whether the flag was set in time
 */
static int
wait_for(int *flag, const unsigned ms)
{
        unsigned waited;

        for (waited = 0; waited < ms; waited += 10) {
                if (__atomic_load_n(flag, __ATOMIC_ACQUIRE))
                        return 1;
                sleep_ms(10);
        }
        return __atomic_load_n(flag, __ATOMIC_ACQUIRE);
}

static void *
probe(void *arg)
{
        (void)arg;
        lock_get();
        __atomic_store_n(&m_probe_got, 1, __ATOMIC_RELEASE);
        lock_release();
        return NULL;
}

/**
 * @brief Whether another thread takes the lock within \a ms
 *
 * The probe is joined only where it got the lock; one that did not is left
 * blocked, since a lock nobody gives back is the failure being asserted.
 */
static int
another_thread_takes_the_lock(const unsigned ms)
{
        pthread_t t;
        int got;

        __atomic_store_n(&m_probe_got, 0, __ATOMIC_RELEASE);
        assert_int_equal(pthread_create(&t, NULL, probe, NULL), 0);
        got = wait_for(&m_probe_got, ms);
        if (got)
                assert_int_equal(pthread_join(t, NULL), 0);
        else
                (void)pthread_detach(t);
        return got;
}

static void *
holder_exits(void *arg)
{
        (void)arg;
        lock_get();
        pthread_exit(NULL);
}

static void *
holder_waits_to_be_cancelled(void *arg)
{
        (void)arg;
        lock_get();
        __atomic_store_n(&m_holder_has, 1, __ATOMIC_RELEASE);
        for (;;)
                sleep_ms(10); /* nanosleep() is a cancellation point */
        return NULL;
}

static void *
holder_releases_then_waits(void *arg)
{
        (void)arg;
        lock_get();
        lock_release();
        __atomic_store_n(&m_holder_has, 1, __ATOMIC_RELEASE);
        while (!__atomic_load_n(&m_holder_go, __ATOMIC_ACQUIRE))
                sleep_ms(10);
        return NULL;
}

static void
test_a_thread_that_exits_holding_the_lock_gives_it_back(void **state)
{
        pthread_t t;

        (void)state;
        assert_int_equal(pthread_create(&t, NULL, holder_exits, NULL), 0);
        assert_int_equal(pthread_join(t, NULL), 0);

        assert_true(another_thread_takes_the_lock(FREE_WAIT_MS));
}

static void
test_a_cancelled_thread_gives_the_lock_back(void **state)
{
        void *ret = NULL;
        pthread_t t;

        (void)state;
        __atomic_store_n(&m_holder_has, 0, __ATOMIC_RELEASE);
        assert_int_equal(
            pthread_create(&t, NULL, holder_waits_to_be_cancelled, NULL), 0);
        assert_true(wait_for(&m_holder_has, FREE_WAIT_MS));

        assert_int_equal(pthread_cancel(t), 0);
        assert_int_equal(pthread_join(t, &ret), 0);
        assert_ptr_equal(ret, PTHREAD_CANCELED);

        assert_true(another_thread_takes_the_lock(FREE_WAIT_MS));
}

static void
test_a_released_lock_is_not_given_back_twice(void **state)
{
        pthread_t t;

        /* A thread that took and released the lock, and leaves while another
         * thread holds it, must not unlock that other thread's lock on the
         * way out: its record says it holds nothing. If it did, a third
         * thread would take a lock the case is still holding.
         */
        (void)state;
        __atomic_store_n(&m_holder_has, 0, __ATOMIC_RELEASE);
        __atomic_store_n(&m_holder_go, 0, __ATOMIC_RELEASE);
        assert_int_equal(
            pthread_create(&t, NULL, holder_releases_then_waits, NULL), 0);
        assert_true(wait_for(&m_holder_has, FREE_WAIT_MS));

        lock_get();
        __atomic_store_n(&m_holder_go, 1, __ATOMIC_RELEASE);
        assert_int_equal(pthread_join(t, NULL), 0);

        assert_false(another_thread_takes_the_lock(300));

        /* and once the case lets it go, the waiting probe gets it */
        lock_release();
        assert_true(wait_for(&m_probe_got, FREE_WAIT_MS));
}

int
main(void)
{
        const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_a_released_lock_is_not_given_back_twice),
            cmocka_unit_test(
                test_a_thread_that_exits_holding_the_lock_gives_it_back),
            cmocka_unit_test(test_a_cancelled_thread_gives_the_lock_back)};

        return cmocka_run_group_tests(tests, NULL, NULL);
}
