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
#include "test_lock.h"

#include <dlfcn.h>
#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
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

/** the error the next pthread_key_create() / pthread_setspecific() call
 *  returns, zero to pass it through. Positive, as pthread functions return
 *  them
 */
static int m_key_create_error;
static int m_setspecific_error;
/** how many times pthread_setspecific() was called */
static int m_setspecific_calls;

int
__wrap_pthread_key_create(pthread_key_t *key, void (*destructor)(void *))
{
        if (m_key_create_error != 0)
                return m_key_create_error;
        return __real_pthread_key_create(key, destructor);
}

int
__wrap_pthread_setspecific(pthread_key_t key, const void *value)
{
        const int err =
            __atomic_exchange_n(&m_setspecific_error, 0, __ATOMIC_ACQ_REL);

        __atomic_fetch_add(&m_setspecific_calls, 1, __ATOMIC_ACQ_REL);
        if (err != 0)
                return err;
        return __real_pthread_setspecific(key, value);
}

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

/** a probe still waiting for the lock, joined once the case lets it go */
static pthread_t m_probe;
static int m_probe_waiting;

/**
 * @brief Whether another thread takes the lock within \a ms
 *
 * The probe is joined where it got the lock. One that did not is left waiting
 * and joinable: the_waiting_probe_gets_the_lock() joins it once the case gives
 * the lock up, so no probe is still inside lock_release() when the teardown
 * destroys the mutex.
 */
static int
another_thread_takes_the_lock(const unsigned ms)
{
        int got;

        __atomic_store_n(&m_probe_got, 0, __ATOMIC_RELEASE);
        assert_int_equal(pthread_create(&m_probe, NULL, probe, NULL), 0);
        got = wait_for(&m_probe_got, ms);
        if (got)
                assert_int_equal(pthread_join(m_probe, NULL), 0);
        else
                m_probe_waiting = 1;
        return got;
}

/**
 * @brief The probe left waiting takes the lock, and is joined
 */
static void
the_waiting_probe_gets_the_lock(void)
{
        assert_true(m_probe_waiting);
        assert_true(wait_for(&m_probe_got, FREE_WAIT_MS));
        assert_int_equal(pthread_join(m_probe, NULL), 0);
        m_probe_waiting = 0;
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
holder_holds_then_waits(void *arg)
{
        (void)arg;
        lock_get();
        __atomic_store_n(&m_holder_has, 1, __ATOMIC_RELEASE);
        while (!__atomic_load_n(&m_holder_go, __ATOMIC_ACQUIRE))
                sleep_ms(10);
        lock_release();
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
        the_waiting_probe_gets_the_lock();
}

static void
test_a_holder_that_is_not_recorded_is_reported(void **state)
{
        char buf[256] = "";
        FILE *captured;
        int saved;
        pthread_t t;

        /* The record could not be made: the call still holds the lock and
         * gives it back on return, and what it cannot do - give it back on a
         * thread exit - is said rather than left silent. On a thread of its
         * own, since a record is made by a thread's first lock and this
         * thread has locked before.
         */
        (void)state;
        __atomic_store_n(&m_holder_has, 0, __ATOMIC_RELEASE);
        __atomic_store_n(&m_holder_go, 0, __ATOMIC_RELEASE);
        captured = tmpfile();
        assert_non_null(captured);
        fflush(stderr);
        saved = dup(STDERR_FILENO);
        assert_true(saved >= 0);
        assert_true(dup2(fileno(captured), STDERR_FILENO) >= 0);

        __atomic_store_n(&m_setspecific_error, ENOMEM, __ATOMIC_RELEASE);
        assert_int_equal(
            pthread_create(&t, NULL, holder_holds_then_waits, NULL), 0);
        assert_true(wait_for(&m_holder_has, FREE_WAIT_MS));

        fflush(stderr);
        assert_true(dup2(saved, STDERR_FILENO) >= 0);
        close(saved);
        rewind(captured);
        (void)fread(buf, 1, sizeof(buf) - 1, captured);
        fclose(captured);
        assert_non_null(strstr(buf, "API lock holder not recorded"));

        assert_false(another_thread_takes_the_lock(300));
        __atomic_store_n(&m_holder_go, 1, __ATOMIC_RELEASE);
        assert_int_equal(pthread_join(t, NULL), 0);
        the_waiting_probe_gets_the_lock();
}

static void *
holder_locks_twice_then_waits(void *arg)
{
        (void)arg;
        lock_get();
        lock_release();
        lock_get();
        lock_release();
        __atomic_store_n(&m_holder_has, 1, __ATOMIC_RELEASE);
        while (!__atomic_load_n(&m_holder_go, __ATOMIC_ACQUIRE))
                sleep_ms(10);
        return NULL;
}

static void
test_a_release_clears_the_record_in_memory(void **state)
{
        pthread_t t;
        int calls;

        /* A thread's record is made once, by its first lock, and held and
         * released by writing to it, so a release cannot fail to clear it:
         * two locks and two releases are one call to the key. The thread
         * then exits with the lock released while this thread holds it, and
         * the destructor, reading a record that says not held, unlocks
         * nothing - if it did, a third thread would take a lock the case is
         * still holding.
         */
        (void)state;
        __atomic_store_n(&m_holder_has, 0, __ATOMIC_RELEASE);
        __atomic_store_n(&m_holder_go, 0, __ATOMIC_RELEASE);
        calls = __atomic_load_n(&m_setspecific_calls, __ATOMIC_ACQUIRE);
        assert_int_equal(
            pthread_create(&t, NULL, holder_locks_twice_then_waits, NULL), 0);
        assert_true(wait_for(&m_holder_has, FREE_WAIT_MS));
        assert_int_equal(
            __atomic_load_n(&m_setspecific_calls, __ATOMIC_ACQUIRE) - calls, 1);

        lock_get();
        __atomic_store_n(&m_holder_go, 1, __ATOMIC_RELEASE);
        assert_int_equal(pthread_join(t, NULL), 0);

        assert_false(another_thread_takes_the_lock(300));

        lock_release();
        the_waiting_probe_gets_the_lock();
}

/** the lock module as a shared object of its own, built beside this binary;
 *  its absolute path comes from the Makefile, so the case needs no way of
 *  finding its own binary - /proc/self/exe is Linux's alone
 */
#ifndef LOCK_PROBE_PATH
#define LOCK_PROBE_PATH "bin/liblock_probe.so"
#endif

/** the probe library's entry points, as dlsym() gives them */
struct lock_probe {
        int (*init)(void);
        int (*fini)(void);
        void (*get)(void);
        void (*release)(void);
};

static pthread_barrier_t m_unload_barrier;

/**
 * @brief Takes and releases the lock from the probe library, then outlives it
 *
 * @param [in] arg the probe library's entry points
 */
static void *
user_of_the_library(void *arg)
{
        const struct lock_probe *probe = arg;

        if (probe->init() != 0)
                _exit(3);
        probe->get();
        probe->release();
        probe->fini();
        /* the record is armed: the library may go now */
        pthread_barrier_wait(&m_unload_barrier);
        /* and it is gone: this thread's exit runs the key's destructors */
        pthread_barrier_wait(&m_unload_barrier);
        return NULL;
}

static void
test_a_thread_outlives_the_unloaded_library(void **state)
{
        pid_t pid;
        int status = -1;

        /* In a child, since the failure this guards against is a crash: a
         * thread takes the lock from the module loaded as a shared object,
         * the object is unloaded while the thread lives, and the thread then
         * exits. A key left behind by the unloaded module would have the
         * exit call its destructor at an address that is no longer mapped.
         */
        (void)state;
        fflush(stdout);
        fflush(stderr);
        pid = fork();
        assert_true(pid >= 0);
        if (pid == 0) {
                struct lock_probe probe;
                pthread_t t;
                void *handle = dlopen(LOCK_PROBE_PATH, RTLD_NOW | RTLD_LOCAL);

                if (handle == NULL)
                        _exit(2);
                probe.init = dlsym(handle, "lock_init");
                probe.fini = dlsym(handle, "lock_fini");
                probe.get = dlsym(handle, "lock_get");
                probe.release = dlsym(handle, "lock_release");
                if (probe.init == NULL || probe.fini == NULL ||
                    probe.get == NULL || probe.release == NULL)
                        _exit(2);
                pthread_barrier_init(&m_unload_barrier, NULL, 2);
                if (pthread_create(&t, NULL, user_of_the_library, &probe) != 0)
                        _exit(2);
                pthread_barrier_wait(&m_unload_barrier);
                if (dlclose(handle) != 0)
                        _exit(2);
                pthread_barrier_wait(&m_unload_barrier);
                _exit(pthread_join(t, NULL) == 0 ? 0 : 1);
        }

        assert_int_equal(waitpid(pid, &status, 0), pid);
        assert_true(WIFEXITED(status));
        assert_int_equal(WEXITSTATUS(status), 0);
}

static void
test_a_key_that_cannot_be_created_fails_init(void **state)
{
        pid_t pid;
        int status = -1;

        /* In a child: the key is made once per process, so a failure here is
         * permanent for whoever sees it, and the other cases need a key.
         */
        (void)state;
        fflush(stdout);
        fflush(stderr);
        pid = fork();
        assert_true(pid >= 0);
        if (pid == 0) {
                m_key_create_error = EAGAIN;
                /* refused, and before the lock file was made */
                _exit(lock_init() == -1 && access(LOCKFILE, F_OK) != 0 ? 0 : 1);
        }

        assert_int_equal(waitpid(pid, &status, 0), pid);
        assert_true(WIFEXITED(status));
        assert_int_equal(WEXITSTATUS(status), 0);
}

/* The lock module this binary links names a relative LOCKFILE (see the recipe
 * in unit-test/lib/Makefile), and each group runs in a directory of its own
 * that holds it: a lock file another user's run left in /tmp cannot be removed
 * by this one, so a shared path made the cases pass or fail on the host.
 */
static char m_dir[PATH_MAX];
static char m_home[PATH_MAX];

static int
dir_setup(void **state)
{
        (void)state;
        if (getcwd(m_home, sizeof(m_home)) == NULL)
                return -1;
        snprintf(m_dir, sizeof(m_dir), "/tmp/pqos_ut_lock_threads_XXXXXX");
        if (mkdtemp(m_dir) == NULL || chdir(m_dir) != 0)
                return -1;
        return mkdir("lock", 0755);
}

static int
dir_teardown(void **state)
{
        (void)state;
        (void)unlink(LOCKFILE);
        if (rmdir("lock") != 0 || chdir(m_home) != 0)
                return -1;
        return rmdir(m_dir);
}

/** The lock as the library sets it up, with its own lock file */
static int
lock_setup(void **state)
{
        if (dir_setup(state) != 0)
                return -1;
        return lock_init();
}

static int
lock_teardown(void **state)
{
        const int ret = lock_fini();

        return dir_teardown(state) != 0 ? -1 : ret;
}

int
main(void)
{
        int result = 0;

        const struct CMUnitTest tests_init[] = {
            cmocka_unit_test(test_a_key_that_cannot_be_created_fails_init),
            cmocka_unit_test(test_a_thread_outlives_the_unloaded_library)};

        const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_a_released_lock_is_not_given_back_twice),
            cmocka_unit_test(
                test_a_thread_that_exits_holding_the_lock_gives_it_back),
            cmocka_unit_test(test_a_cancelled_thread_gives_the_lock_back),
            cmocka_unit_test(test_a_release_clears_the_record_in_memory),
            cmocka_unit_test(test_a_holder_that_is_not_recorded_is_reported)};

        result += cmocka_run_group_tests(tests_init, dir_setup, dir_teardown);
        result += cmocka_run_group_tests(tests, lock_setup, lock_teardown);

        return result;
}
