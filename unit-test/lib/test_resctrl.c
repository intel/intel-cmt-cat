/*
 * BSD LICENSE
 *
 * Copyright(c) 2023-2026 Intel Corporation. All rights reserved.
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

#include "test_resctrl.h"

#include "resctrl.h"
#include "test.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <sys/file.h>
#include <time.h>
#include <unistd.h>

/* ======== mock ======== */

/* the directory the resctrl lock opens in place of RESCTRL_PATH, which the
 * lock's cases make; NULL outside them, and the opens of it, counted. A case
 * can have that open fail with lock_open_errno instead, or have the opening
 * thread ask for its own cancellation there, as one asked for once the lock
 * has started would be
 */
static char *lock_dir;
static unsigned lock_opens;
static int lock_open_errno;
static int lock_cancel_in_open;

int
__wrap_open(const char *path, int flags, ...)
{
        int mode = 0;

        if ((flags & O_CREAT) || (flags & O_TMPFILE) == O_TMPFILE) {
                va_list ap;

                va_start(ap, flags);
                mode = va_arg(ap, int);
                va_end(ap);
        }
        if (lock_dir != NULL && strcmp(path, RESCTRL_PATH) == 0) {
                __atomic_add_fetch(&lock_opens, 1, __ATOMIC_SEQ_CST);
                if (lock_open_errno != 0) {
                        errno = lock_open_errno;
                        return -1;
                }
                if (__atomic_exchange_n(&lock_cancel_in_open, 0,
                                        __ATOMIC_SEQ_CST))
                        (void)pthread_cancel(pthread_self());
                path = lock_dir;
        }

        return __real_open(path, flags, mode);
}

/* the sleeps of the lock's waits, and how many of them found the thread's
 * cancellation on, counted with the atomic builtins: a case's other thread
 * reads them. A case can have the first sleep:
 * - give a holder's lock up, and sleep as asked;
 * - give a holder's lock up, and last longer than the lock's timeout, as for a
 *   thread that is not run for a while;
 * - ask for the sleeping thread's own cancellation, which the wait has off.
 */
static unsigned lock_sleeps;
static unsigned lock_sleeps_cancellable;
static int lock_released_holder = -1;
static int lock_overslept_holder = -1;
static int lock_cancel_in_sleep;

int
__wrap_nanosleep(const struct timespec *req, struct timespec *rem)
{
        int found;
        int set;

        (void)pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &found);
        (void)pthread_setcancelstate(found, &set);
        __atomic_add_fetch(&lock_sleeps, 1, __ATOMIC_SEQ_CST);
        if (found == PTHREAD_CANCEL_ENABLE)
                __atomic_add_fetch(&lock_sleeps_cancellable, 1,
                                   __ATOMIC_SEQ_CST);
        if (__atomic_exchange_n(&lock_cancel_in_sleep, 0, __ATOMIC_SEQ_CST))
                (void)pthread_cancel(pthread_self());
        if (lock_released_holder != -1) {
                (void)flock(lock_released_holder, LOCK_UN);
                lock_released_holder = -1;
        }
        if (lock_overslept_holder != -1) {
                const struct timespec past = {0, 150000000L};

                (void)flock(lock_overslept_holder, LOCK_UN);
                lock_overslept_holder = -1;
                return __real_nanosleep(&past, rem);
        }

        return __real_nanosleep(req, rem);
}

/* the lock's looks at the clock: a case can have the next one, or the one
 * after it, fail with EINVAL, which no clock the library reads ever does; or
 * freeze the clock, so that the wait cannot time out however late its thread
 * runs. CLOCK_MONOTONIC's alone are counted, and the watchdog reads the
 * clock past the wrap, through __real_clock_gettime()
 */
static unsigned lock_clock_fails_in;
static int lock_clock_frozen;

int
__wrap_clock_gettime(clockid_t clk, struct timespec *tp)
{
        if (clk == CLOCK_MONOTONIC &&
            __atomic_load_n(&lock_clock_fails_in, __ATOMIC_SEQ_CST) != 0 &&
            __atomic_sub_fetch(&lock_clock_fails_in, 1, __ATOMIC_SEQ_CST) ==
                0) {
                errno = EINVAL;
                return -1;
        }
        if (clk == CLOCK_MONOTONIC &&
            __atomic_load_n(&lock_clock_frozen, __ATOMIC_SEQ_CST)) {
                tp->tv_sec = 1000;
                tp->tv_nsec = 0;
                return 0;
        }

        return __real_clock_gettime(clk, tp);
}

int
__wrap_mount(const char *source,
             const char *target,
             const char *filesystemtype,
             unsigned long mountflags,
             const void *data)
{
        check_expected(source);
        check_expected(target);
        check_expected(filesystemtype);
        check_expected(mountflags);
        if (data != NULL)
                check_expected(data);
        else
                check_expected_ptr(data);

        return mock_type(int);
}

int
__wrap_umount2(const char *target, int flags)
{
        check_expected(target);
        assert_int_equal(flags, 0);

        return mock_type(int);
}

/* ======== resctrl_mount ======== */

static void
test_resctrl_mount_default(void **state __attribute__((unused)))
{
        int ret;

        expect_string(__wrap_mount, source, "resctrl");
        expect_string(__wrap_mount, target, "/sys/fs/resctrl");
        expect_string(__wrap_mount, filesystemtype, "resctrl");
        expect_value(__wrap_mount, mountflags, 0);
        expect_value(__wrap_mount, data, NULL);
        will_return(__wrap_mount, 0);

        ret = resctrl_mount(PQOS_REQUIRE_CDP_OFF, PQOS_REQUIRE_CDP_OFF,
                            PQOS_MBA_DEFAULT);
        assert_int_equal(ret, PQOS_RETVAL_OK);

        expect_string(__wrap_mount, source, "resctrl");
        expect_string(__wrap_mount, target, "/sys/fs/resctrl");
        expect_string(__wrap_mount, filesystemtype, "resctrl");
        expect_value(__wrap_mount, mountflags, 0);
        expect_value(__wrap_mount, data, NULL);
        will_return(__wrap_mount, -1);

        ret = resctrl_mount(PQOS_REQUIRE_CDP_OFF, PQOS_REQUIRE_CDP_OFF,
                            PQOS_MBA_DEFAULT);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
}

static void
test_resctrl_mount_l3cdp(void **state __attribute__((unused)))
{
        int ret;

        expect_string(__wrap_mount, source, "resctrl");
        expect_string(__wrap_mount, target, "/sys/fs/resctrl");
        expect_string(__wrap_mount, filesystemtype, "resctrl");
        expect_value(__wrap_mount, mountflags, 0);
        expect_string(__wrap_mount, data, "cdp");
        will_return(__wrap_mount, 0);

        ret = resctrl_mount(PQOS_REQUIRE_CDP_ON, PQOS_REQUIRE_CDP_OFF,
                            PQOS_MBA_DEFAULT);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_resctrl_mount_l2cdp(void **state __attribute__((unused)))
{
        int ret;

        expect_string(__wrap_mount, source, "resctrl");
        expect_string(__wrap_mount, target, "/sys/fs/resctrl");
        expect_string(__wrap_mount, filesystemtype, "resctrl");
        expect_value(__wrap_mount, mountflags, 0);
        expect_string(__wrap_mount, data, "cdpl2");
        will_return(__wrap_mount, 0);

        ret = resctrl_mount(PQOS_REQUIRE_CDP_OFF, PQOS_REQUIRE_CDP_ON,
                            PQOS_MBA_DEFAULT);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_resctrl_mount_mba_ctrl(void **state __attribute__((unused)))
{
        int ret;

        expect_string(__wrap_mount, source, "resctrl");
        expect_string(__wrap_mount, target, "/sys/fs/resctrl");
        expect_string(__wrap_mount, filesystemtype, "resctrl");
        expect_value(__wrap_mount, mountflags, 0);
        expect_string(__wrap_mount, data, "mba_MBps");
        will_return(__wrap_mount, 0);

        ret = resctrl_mount(PQOS_REQUIRE_CDP_OFF, PQOS_REQUIRE_CDP_OFF,
                            PQOS_MBA_CTRL);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

/* ======== resctrl_umount ======== */

static void
test_resctrl_umount(void **state __attribute__((unused)))
{
        int ret;

        expect_string(__wrap_umount2, target, "/sys/fs/resctrl");
        will_return(__wrap_umount2, 0);

        ret = resctrl_umount();
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_resctrl_umount_error(void **state __attribute__((unused)))
{
        int ret;

        expect_string(__wrap_umount2, target, "/sys/fs/resctrl");
        will_return(__wrap_umount2, -1);

        ret = resctrl_umount();
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
}

/* ======== resctrl_cpumask_set ======== */

static void
test_resctrl_cpumask_set(void **state __attribute__((unused)))
{
        unsigned lcore = 5;
        struct resctrl_cpumask mask;
        unsigned i;

        memset(&mask, 0, sizeof(mask));

        resctrl_cpumask_set(lcore, &mask);
        for (i = 0; i < RESCTRL_MAX_CPUS; ++i)
                if (i == lcore)
                        assert_int_equal(resctrl_cpumask_get(i, &mask), 1);
                else
                        assert_int_equal(resctrl_cpumask_get(i, &mask), 0);
}

/* ======== resctrl_cpumask_unset ======== */

static void
test_resctrl_cpumask_unset(void **state __attribute__((unused)))
{
        unsigned lcore = 5;
        struct resctrl_cpumask mask;
        unsigned i;

        memset(&mask, 0xff, sizeof(mask));

        resctrl_cpumask_unset(lcore, &mask);
        for (i = 0; i < RESCTRL_MAX_CPUS; ++i)
                if (i == lcore)
                        assert_int_equal(resctrl_cpumask_get(i, &mask), 0);
                else
                        assert_int_equal(resctrl_cpumask_get(i, &mask), 1);
}

/* ======== resctrl_cpumask_read ======== */

static void
test_resctrl_cpumask_read(void **state __attribute__((unused)))
{
        int ret;
        struct resctrl_cpumask mask;
        FILE *fd = tmpfile();
        unsigned i;

        assert_non_null(fd);
        fprintf(fd, "0000,00000000,00001000,00000aA1\n");
        fseek(fd, 0, SEEK_SET);

        ret = resctrl_cpumask_read(fd, &mask);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        for (i = 0; i < RESCTRL_MAX_CPUS; ++i)
                if (i == 0 || i == 5 || i == 7 || i == 9 || i == 11 || i == 44)
                        assert_int_equal(resctrl_cpumask_get(i, &mask), 1);
                else
                        assert_int_equal(resctrl_cpumask_get(i, &mask), 0);
        fclose(fd);
}

/* a mask of RESCTRL_MAX_CPUS, with its commas, is read whole: the first CPU
 * and the last
 */
static void
test_resctrl_cpumask_read_every_cpu(void **state __attribute__((unused)))
{
        struct resctrl_cpumask mask;
        FILE *fd = tmpfile();
        unsigned i;

        assert_non_null(fd);
        fprintf(fd, "80000000");
        for (i = 1; i < RESCTRL_MAX_CPUS / 32 - 1; i++)
                fprintf(fd, ",00000000");
        fprintf(fd, ",00000001\n");
        fseek(fd, 0, SEEK_SET);

        assert_int_equal(resctrl_cpumask_read(fd, &mask), PQOS_RETVAL_OK);
        for (i = 0; i < RESCTRL_MAX_CPUS; ++i)
                assert_int_equal(resctrl_cpumask_get(i, &mask),
                                 i == 0 || i == RESCTRL_MAX_CPUS - 1);
        fclose(fd);
}

/* a mask longer than one of RESCTRL_MAX_CPUS is refused, with its commas or
 * without them, rather than read in part
 */
static void
test_resctrl_cpumask_read_longer_than_the_mask(void **state
                                               __attribute__((unused)))
{
        struct resctrl_cpumask mask;
        FILE *fd = tmpfile();
        unsigned i;

        assert_non_null(fd);
        fprintf(fd, "1");
        for (i = 0; i < RESCTRL_MAX_CPUS / 32; i++)
                fprintf(fd, ",00000000");
        fprintf(fd, "\n");
        fseek(fd, 0, SEEK_SET);
        assert_int_equal(resctrl_cpumask_read(fd, &mask), PQOS_RETVAL_ERROR);
        fclose(fd);

        fd = tmpfile();
        assert_non_null(fd);
        fprintf(fd, "1");
        for (i = 0; i < RESCTRL_MAX_CPUS / 4; i++)
                fprintf(fd, "0");
        fprintf(fd, "\n");
        fseek(fd, 0, SEEK_SET);
        assert_int_equal(resctrl_cpumask_read(fd, &mask), PQOS_RETVAL_ERROR);
        fclose(fd);

        /* a file one character longer than a whole mask's, whose extra
         * character, a comma at its end, is not a digit
         */
        fd = tmpfile();
        assert_non_null(fd);
        fprintf(fd, "80000000");
        for (i = 1; i < RESCTRL_MAX_CPUS / 32; i++)
                fprintf(fd, ",00000000");
        fprintf(fd, ",\n");
        fseek(fd, 0, SEEK_SET);
        assert_int_equal(resctrl_cpumask_read(fd, &mask), PQOS_RETVAL_ERROR);
        fclose(fd);

        /* and a file longer than one of such a mask, which a read of that
         * length would cut, whatever digits its start has
         */
        fd = tmpfile();
        assert_non_null(fd);
        for (i = 0; i < RESCTRL_MAX_CPUS / 32; i++)
                fprintf(fd, "%s00000001", i == 0 ? "" : ",  ");
        fprintf(fd, "\n");
        fseek(fd, 0, SEEK_SET);
        assert_int_equal(resctrl_cpumask_read(fd, &mask), PQOS_RETVAL_ERROR);
        fclose(fd);
}

/* a core past the mask has no bit in it: it is not set, and setting it or
 * unsetting it leaves the mask as it was
 */
static void
test_resctrl_cpumask_core_past_the_mask(void **state __attribute__((unused)))
{
        struct resctrl_cpumask mask;
        struct resctrl_cpumask copy;

        memset(&mask, 0xff, sizeof(mask));
        memcpy(&copy, &mask, sizeof(copy));

        assert_int_equal(resctrl_cpumask_get(RESCTRL_MAX_CPUS, &mask), 0);
        resctrl_cpumask_unset(RESCTRL_MAX_CPUS, &mask);
        assert_memory_equal(&mask, &copy, sizeof(mask));

        memset(&mask, 0, sizeof(mask));
        memcpy(&copy, &mask, sizeof(copy));
        resctrl_cpumask_set(RESCTRL_MAX_CPUS, &mask);
        assert_memory_equal(&mask, &copy, sizeof(mask));
}

/* ======== resctrl_cpumask_write ======== */

static void
test_resctrl_cpumask_write(void **state __attribute__((unused)))
{
        int ret;
        struct resctrl_cpumask mask_write;
        struct resctrl_cpumask mask_read;
        FILE *fd = tmpfile();

        assert_non_null(fd);

        memset(&mask_write, 0, sizeof(mask_write));

        resctrl_cpumask_set(2, &mask_write);
        resctrl_cpumask_set(5, &mask_write);
        resctrl_cpumask_set(30, &mask_write);

        ret = resctrl_cpumask_write(fd, &mask_write);
        assert_int_equal(ret, PQOS_RETVAL_OK);

        fseek(fd, 0, SEEK_SET);
        ret = resctrl_cpumask_read(fd, &mask_read);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_true(memcmp(&mask_read, &mask_write,
                           sizeof(struct resctrl_cpumask)) == 0);

        fclose(fd);
}

static void
test_resctrl_cpumask_write_zero(void **state __attribute__((unused)))
{
        int ret;
        struct resctrl_cpumask mask_write;
        struct resctrl_cpumask mask_read;
        FILE *fd = tmpfile();

        assert_non_null(fd);

        memset(&mask_write, 0, sizeof(mask_write));

        ret = resctrl_cpumask_write(fd, &mask_write);
        assert_int_equal(ret, PQOS_RETVAL_OK);

        fseek(fd, 0, SEEK_SET);
        ret = resctrl_cpumask_read(fd, &mask_read);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_true(memcmp(&mask_read, &mask_write,
                           sizeof(struct resctrl_cpumask)) == 0);

        fclose(fd);
}

/* ======== resctrl_lock ======== */

static int
lock_setup(void **state __attribute__((unused)))
{
        static char dir[] = "/tmp/test_resctrl_lock.XXXXXX";

        memcpy(dir + sizeof(dir) - 7, "XXXXXX", 6);
        lock_dir = mkdtemp(dir);
        lock_sleeps = 0;
        lock_sleeps_cancellable = 0;
        lock_released_holder = -1;
        lock_overslept_holder = -1;
        lock_cancel_in_sleep = 0;
        lock_clock_fails_in = 0;
        lock_clock_frozen = 0;
        lock_opens = 0;
        lock_open_errno = 0;
        lock_cancel_in_open = 0;

        return lock_dir == NULL ? -1 : 0;
}

static int
lock_teardown(void **state __attribute__((unused)))
{
        int ret = rmdir(lock_dir);

        lock_dir = NULL;

        return ret;
}

/* another holder of the lock, as another process would be one */
static int
lock_hold(const int type)
{
        int fd = open(lock_dir, O_RDONLY | O_DIRECTORY | O_CLOEXEC);

        assert_true(fd >= 0);
        assert_int_equal(flock(fd, type), 0);

        return fd;
}

static long
lock_ms_since(const struct timespec *start)
{
        struct timespec now;

        (void)clock_gettime(CLOCK_MONOTONIC, &now);

        return (now.tv_sec - start->tv_sec) * 1000L +
               (now.tv_nsec - start->tv_nsec) / 1000000L;
}

/* A watchdog over a case that expects the lock to time out on a holder: it
 * gives the holder's lock up after 2 s, so that a lock that waits without end,
 * as a blocking flock() would, fails the case rather than hanging the binary.
 * The 2 s are on CLOCK_MONOTONIC, read past the case's wrap of the clock: a
 * step of the wall clock neither fires it early nor holds it up, and a clock
 * a case freezes or fails is not the watchdog's
 */
struct lock_watchdog {
        pthread_t thread;
        pthread_mutex_t mutex;
        pthread_cond_t cond;
        int holder;
        int stop;
        int fired;
};

static void *
lock_watchdog_run(void *arg)
{
        struct lock_watchdog *watchdog = arg;
        struct timespec until;

        (void)__real_clock_gettime(CLOCK_MONOTONIC, &until);
        until.tv_sec += 2;
        (void)pthread_mutex_lock(&watchdog->mutex);
        while (!watchdog->stop)
                if (pthread_cond_timedwait(&watchdog->cond, &watchdog->mutex,
                                           &until) == ETIMEDOUT) {
                        watchdog->fired = 1;
                        (void)flock(watchdog->holder, LOCK_UN);
                        break;
                }
        (void)pthread_mutex_unlock(&watchdog->mutex);

        return NULL;
}

static void
lock_watchdog_start(struct lock_watchdog *watchdog, const int holder)
{
        pthread_condattr_t attr;

        memset(watchdog, 0, sizeof(*watchdog));
        watchdog->holder = holder;
        assert_int_equal(pthread_mutex_init(&watchdog->mutex, NULL), 0);
        assert_int_equal(pthread_condattr_init(&attr), 0);
        assert_int_equal(pthread_condattr_setclock(&attr, CLOCK_MONOTONIC), 0);
        assert_int_equal(pthread_cond_init(&watchdog->cond, &attr), 0);
        (void)pthread_condattr_destroy(&attr);
        assert_int_equal(pthread_create(&watchdog->thread, NULL,
                                        lock_watchdog_run, watchdog),
                         0);
}

/* @return 1 where the watchdog gave the holder's lock up */
static int
lock_watchdog_stop(struct lock_watchdog *watchdog)
{
        (void)pthread_mutex_lock(&watchdog->mutex);
        watchdog->stop = 1;
        (void)pthread_cond_signal(&watchdog->cond);
        (void)pthread_mutex_unlock(&watchdog->mutex);
        (void)pthread_join(watchdog->thread, NULL);
        (void)pthread_cond_destroy(&watchdog->cond);
        (void)pthread_mutex_destroy(&watchdog->mutex);

        return watchdog->fired;
}

/* runs \a worker in a thread of its own and waits for it to end, giving what it
 * returned in \a result
 *
 * @return pthread_create()'s or pthread_join()'s status, for the case to assert
 *         on once its watchdog is stopped: a failed assertion leaves the case
 *         at once, and a watchdog left running would go on using the case's
 *         stack after it
 */
static int
lock_run_worker(void *(*worker)(void *), void **result)
{
        pthread_t thread;
        int err = pthread_create(&thread, NULL, worker, NULL);

        if (err == 0)
                err = pthread_join(thread, result);

        return err;
}

/* the descriptors the process has open */
static unsigned
lock_open_fds(void)
{
        DIR *dir = opendir("/proc/self/fd");
        struct dirent *entry;
        unsigned count = 0;

        assert_non_null(dir);
        while ((entry = readdir(dir)) != NULL)
                if (entry->d_name[0] != '.')
                        count++;
        closedir(dir);

        return count;
}

static void
lock_alarm_handler(int signo __attribute__((unused)))
{
}

/* a lock and its release take nothing of the application's: its SIGALRM
 * handler, with its flags, and the alarm it has pending are as they were
 */
static void
test_resctrl_lock_keeps_the_callers_alarm(void **state __attribute__((unused)))
{
        struct sigaction sa;
        struct sigaction found;
        struct sigaction after;
        unsigned left;

        memset(&sa, 0, sizeof(sa));
        sa.sa_handler = lock_alarm_handler;
        sa.sa_flags = SA_RESTART;
        sigemptyset(&sa.sa_mask);
        assert_int_equal(sigaction(SIGALRM, &sa, &found), 0);
        (void)alarm(30);

        assert_int_equal(resctrl_lock_exclusive(), PQOS_RETVAL_OK);
        assert_int_equal(resctrl_lock_release(), PQOS_RETVAL_OK);
        assert_int_equal(resctrl_lock_shared(), PQOS_RETVAL_OK);
        assert_int_equal(resctrl_lock_release(), PQOS_RETVAL_OK);

        /* the alarm ended before the handler goes: a SIGALRM after it would
         * end the process
         */
        left = alarm(0);
        assert_int_equal(sigaction(SIGALRM, &found, &after), 0);
        assert_ptr_equal(after.sa_handler, lock_alarm_handler);
        assert_true(after.sa_flags & SA_RESTART);
        assert_in_range(left, 29, 30);
}

/* a lock another holder keeps is given up on after 100 ms, saying so, with
 * the thread's cancellation off in each sleep of the wait and on after it;
 * and the descriptor is closed: the lock is taken once the holder has gone
 */
static void
test_resctrl_lock_times_out(void **state __attribute__((unused)))
{
        const int holder = lock_hold(LOCK_EX);
        const unsigned fds = lock_open_fds();
        struct lock_watchdog watchdog;
        struct timespec start;
        unsigned fds_after;
        long took;
        int fired;
        int after;
        int set;
        int ret;

        lock_watchdog_start(&watchdog, holder);
        test_log_keep_start();
        (void)clock_gettime(CLOCK_MONOTONIC, &start);
        ret = resctrl_lock_exclusive();
        took = lock_ms_since(&start);
        (void)test_log_count_stop();
        fired = lock_watchdog_stop(&watchdog);
        fds_after = lock_open_fds();
        (void)pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &after);
        (void)pthread_setcancelstate(after, &set);
        close(holder);
        if (ret == PQOS_RETVAL_OK)
                (void)resctrl_lock_release();

        assert_false(fired);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
        assert_in_range(took, 100, 2000);
        assert_non_null(strstr(test_log_said(),
                               "Failed to acquire lock on resctrl filesystem "
                               "- timeout occurred"));
        assert_true(lock_sleeps > 0);
        assert_int_equal(lock_sleeps_cancellable, 0);
        assert_int_equal(after, PTHREAD_CANCEL_ENABLE);
        assert_int_equal(fds_after, fds);

        assert_int_equal(resctrl_lock_exclusive(), PQOS_RETVAL_OK);
        assert_int_equal(resctrl_lock_release(), PQOS_RETVAL_OK);
}

/* a lock whose holder gives it up during the wait, in the lock's first sleep,
 * is taken at the try after the sleep, however late its thread runs: the
 * clock is frozen for the case
 */
static void
test_resctrl_lock_taken_once_given_up(void **state __attribute__((unused)))
{
        const int holder = lock_hold(LOCK_EX);
        struct lock_watchdog watchdog;
        int fired;
        int ret;

        lock_released_holder = holder;
        __atomic_store_n(&lock_clock_frozen, 1, __ATOMIC_SEQ_CST);
        lock_watchdog_start(&watchdog, holder);
        ret = resctrl_lock_exclusive();
        fired = lock_watchdog_stop(&watchdog);
        __atomic_store_n(&lock_clock_frozen, 0, __ATOMIC_SEQ_CST);
        lock_released_holder = -1;
        close(holder);

        assert_false(fired);
        assert_int_equal(lock_sleeps, 1);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(resctrl_lock_release(), PQOS_RETVAL_OK);
}

/* a thread that sleeps past the timeout in its wait, as one that is not run
 * for a while does, does not take the lock after it, though the holder gave it
 * up in the meantime
 */
static void
test_resctrl_lock_not_taken_past_the_timeout(void **state
                                             __attribute__((unused)))
{
        const int holder = lock_hold(LOCK_EX);
        struct lock_watchdog watchdog;
        int fired;
        int ret;

        lock_overslept_holder = holder;
        lock_watchdog_start(&watchdog, holder);
        test_log_count_start();
        ret = resctrl_lock_exclusive();
        (void)test_log_count_stop();
        fired = lock_watchdog_stop(&watchdog);
        lock_overslept_holder = -1;
        close(holder);
        if (ret == PQOS_RETVAL_OK)
                (void)resctrl_lock_release();

        assert_false(fired);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
        assert_int_equal(lock_sleeps, 1);
}

/* a caller whose cancellation is off finds it off after the lock, whether the
 * lock was taken or timed out
 */
static void
test_resctrl_lock_keeps_cancellation_off(void **state __attribute__((unused)))
{
        struct lock_watchdog watchdog;
        int after_taken;
        int after_timeout;
        int holder;
        int found;
        int fired;
        int taken;
        int timed_out;

        assert_int_equal(pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &found),
                         0);
        taken = resctrl_lock_exclusive();
        (void)pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &after_taken);
        if (taken == PQOS_RETVAL_OK)
                (void)resctrl_lock_release();

        holder = lock_hold(LOCK_EX);
        lock_watchdog_start(&watchdog, holder);
        test_log_count_start();
        timed_out = resctrl_lock_exclusive();
        (void)test_log_count_stop();
        fired = lock_watchdog_stop(&watchdog);
        (void)pthread_setcancelstate(found, &after_timeout);
        close(holder);
        if (timed_out == PQOS_RETVAL_OK)
                (void)resctrl_lock_release();

        assert_int_equal(taken, PQOS_RETVAL_OK);
        assert_int_equal(after_taken, PTHREAD_CANCEL_DISABLE);
        assert_false(fired);
        assert_int_equal(timed_out, PQOS_RETVAL_ERROR);
        assert_int_equal(after_timeout, PTHREAD_CANCEL_DISABLE);
}

/* a look at the clock that fails, the first or the one after a sleep, ends
 * the wait, saying why, with the descriptor closed and the thread's
 * cancellation on as it was: the lock is taken once the holder has gone
 */
static void
test_resctrl_lock_clock_failure_ends_the_wait(void **state
                                              __attribute__((unused)))
{
        unsigned which;

        for (which = 1; which <= 2; which++) {
                const int holder = lock_hold(LOCK_EX);
                const unsigned fds = lock_open_fds();
                struct lock_watchdog watchdog;
                unsigned fds_after;
                int fired;
                int after;
                int set;
                int ret;

                lock_sleeps = 0;
                __atomic_store_n(&lock_clock_fails_in, which, __ATOMIC_SEQ_CST);
                lock_watchdog_start(&watchdog, holder);
                test_log_keep_start();
                ret = resctrl_lock_exclusive();
                (void)test_log_count_stop();
                fired = lock_watchdog_stop(&watchdog);
                __atomic_store_n(&lock_clock_fails_in, 0, __ATOMIC_SEQ_CST);
                fds_after = lock_open_fds();
                (void)pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &after);
                (void)pthread_setcancelstate(after, &set);
                close(holder);
                if (ret == PQOS_RETVAL_OK)
                        (void)resctrl_lock_release();

                assert_false(fired);
                assert_int_equal(ret, PQOS_RETVAL_ERROR);
                assert_non_null(strstr(test_log_said(),
                                       "Failed to acquire lock on resctrl "
                                       "filesystem - Invalid argument"));
                assert_int_equal(lock_sleeps, which - 1);
                assert_int_equal(fds_after, fds);
                assert_int_equal(after, PTHREAD_CANCEL_ENABLE);
        }

        assert_int_equal(resctrl_lock_exclusive(), PQOS_RETVAL_OK);
        assert_int_equal(resctrl_lock_release(), PQOS_RETVAL_OK);
}

/* the status the lock gave the worker of the cancellation case, written
 * before the cancellation point after it
 */
static int lock_cancelled_ret;

static void *
lock_cancelled_worker(void *arg __attribute__((unused)))
{
        lock_cancelled_ret = resctrl_lock_exclusive();
        pthread_testcancel();

        return NULL;
}

/* a cancellation asked for in the wait, here by the waiting thread itself in
 * its first sleep, is acted on at the caller's next cancellation point, after
 * the lock has timed out and closed its descriptor: no descriptor is left
 * open, and the lock is taken once the holder has gone
 */
static void
test_resctrl_lock_cancelled_in_the_wait(void **state __attribute__((unused)))
{
        const int holder = lock_hold(LOCK_EX);
        const unsigned fds = lock_open_fds();
        struct lock_watchdog watchdog;
        void *result = NULL;
        unsigned fds_after;
        int fired;
        int ran;

        lock_cancelled_ret = -1;
        __atomic_store_n(&lock_cancel_in_sleep, 1, __ATOMIC_SEQ_CST);
        lock_watchdog_start(&watchdog, holder);
        test_log_count_start();
        ran = lock_run_worker(lock_cancelled_worker, &result);
        (void)test_log_count_stop();
        fired = lock_watchdog_stop(&watchdog);
        fds_after = lock_open_fds();
        close(holder);

        assert_int_equal(ran, 0);
        assert_false(fired);
        assert_ptr_equal(result, PTHREAD_CANCELED);
        assert_int_equal(lock_cancelled_ret, PQOS_RETVAL_ERROR);
        assert_true(lock_sleeps > 0);
        assert_int_equal(fds_after, fds);

        assert_int_equal(resctrl_lock_exclusive(), PQOS_RETVAL_OK);
        assert_int_equal(resctrl_lock_release(), PQOS_RETVAL_OK);
}

/* a log callback with a cancellation point in it, as a write() to a full pipe
 * is one: for the lock's failure, it records whether the thread's
 * cancellation was on, and acts on a cancellation asked for
 */
static int lock_log_found_cancellable;

static void
lock_log_cancellation_point(void *context __attribute__((unused)),
                            const size_t size __attribute__((unused)),
                            const char *message)
{
        int found;
        int set;

        if (strstr(message, "Failed to acquire lock") == NULL)
                return;
        (void)pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &found);
        (void)pthread_setcancelstate(found, &set);
        lock_log_found_cancellable = found == PTHREAD_CANCEL_ENABLE;
        pthread_testcancel();
}

/* a wait that fails is logged with the thread's cancellation on, so that a log
 * that blocks can be cancelled: a cancellation asked for in the wait is acted
 * on in that log, with the descriptor closed already
 */
static void
test_resctrl_lock_failure_logged_cancellable(void **state
                                             __attribute__((unused)))
{
        const int holder = lock_hold(LOCK_EX);
        const unsigned fds = lock_open_fds();
        struct lock_watchdog watchdog;
        void *result = NULL;
        unsigned fds_after;
        int fired;
        int ran;

        lock_cancelled_ret = -1;
        lock_log_found_cancellable = -1;
        __atomic_store_n(&lock_cancel_in_sleep, 1, __ATOMIC_SEQ_CST);
        lock_watchdog_start(&watchdog, holder);
        (void)log_init(-1, lock_log_cancellation_point, NULL, LOG_VER_DEFAULT);
        ran = lock_run_worker(lock_cancelled_worker, &result);
        test_log_init_silent();
        fired = lock_watchdog_stop(&watchdog);
        fds_after = lock_open_fds();
        close(holder);

        assert_int_equal(ran, 0);
        assert_false(fired);
        assert_ptr_equal(result, PTHREAD_CANCELED);
        assert_int_equal(lock_log_found_cancellable, 1);
        assert_int_equal(lock_cancelled_ret, -1);
        assert_int_equal(fds_after, fds);

        assert_int_equal(resctrl_lock_exclusive(), PQOS_RETVAL_OK);
        assert_int_equal(resctrl_lock_release(), PQOS_RETVAL_OK);
}

static void *
lock_cancelled_before_worker(void *arg __attribute__((unused)))
{
        (void)pthread_cancel(pthread_self());
        lock_cancelled_ret = resctrl_lock_exclusive();

        return NULL;
}

/* 1 where another descriptor can take the lock exclusively at once */
static int
lock_free_now(void)
{
        const int fd = open(lock_dir, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
        int free_now;

        assert_true(fd >= 0);
        free_now = flock(fd, LOCK_EX | LOCK_NB) == 0;
        close(fd);

        return free_now;
}

/* a cancellation asked for before the lock is acted on at its start, before
 * anything is opened: the cancelled thread leaves no descriptor open and no
 * lock held
 */
static void
test_resctrl_lock_cancelled_before_the_call(void **state
                                            __attribute__((unused)))
{
        const unsigned fds = lock_open_fds();
        void *result = NULL;

        lock_cancelled_ret = -1;
        assert_int_equal(lock_run_worker(lock_cancelled_before_worker, &result),
                         0);

        assert_ptr_equal(result, PTHREAD_CANCELED);
        assert_int_equal(lock_cancelled_ret, -1);
        assert_int_equal(lock_opens, 0);
        assert_int_equal(lock_open_fds(), fds);
        assert_true(lock_free_now());

        assert_int_equal(resctrl_lock_exclusive(), PQOS_RETVAL_OK);
        assert_int_equal(resctrl_lock_release(), PQOS_RETVAL_OK);
}

/* a cancellation asked for once the lock has started, here as its descriptor
 * is opened, with the lock free, is acted on before the lock is handed over,
 * with the lock given up: acted on at the caller's next cancellation point, it
 * would leave the lock held and the descriptor open, with nothing to give
 * them up, and the next lock waiting out its timeout on them
 */
static void
test_resctrl_lock_cancelled_as_it_is_taken(void **state __attribute__((unused)))
{
        const unsigned fds = lock_open_fds();
        void *result = NULL;
        int ran;

        lock_cancelled_ret = -1;
        __atomic_store_n(&lock_cancel_in_open, 1, __ATOMIC_SEQ_CST);
        ran = lock_run_worker(lock_cancelled_worker, &result);
        __atomic_store_n(&lock_cancel_in_open, 0, __ATOMIC_SEQ_CST);

        assert_int_equal(ran, 0);
        assert_ptr_equal(result, PTHREAD_CANCELED);
        assert_int_equal(lock_cancelled_ret, -1);
        assert_int_equal(lock_open_fds(), fds);
        assert_true(lock_free_now());

        assert_int_equal(resctrl_lock_exclusive(), PQOS_RETVAL_OK);
        assert_int_equal(resctrl_lock_release(), PQOS_RETVAL_OK);
}

/* and so is one asked for in the wait, in its first sleep, in which the holder
 * gives the lock up, so that the try after the sleep takes it
 */
static void
test_resctrl_lock_cancelled_in_the_wait_then_taken(void **state
                                                   __attribute__((unused)))
{
        const int holder = lock_hold(LOCK_EX);
        const unsigned fds = lock_open_fds();
        struct lock_watchdog watchdog;
        void *result = NULL;
        unsigned fds_after;
        int free_after;
        int fired;
        int ran;

        lock_cancelled_ret = -1;
        lock_released_holder = holder;
        __atomic_store_n(&lock_cancel_in_sleep, 1, __ATOMIC_SEQ_CST);
        lock_watchdog_start(&watchdog, holder);
        ran = lock_run_worker(lock_cancelled_worker, &result);
        fired = lock_watchdog_stop(&watchdog);
        fds_after = lock_open_fds();
        free_after = lock_free_now();
        lock_released_holder = -1;
        close(holder);

        assert_int_equal(ran, 0);
        assert_false(fired);
        assert_ptr_equal(result, PTHREAD_CANCELED);
        assert_int_equal(lock_cancelled_ret, -1);
        assert_int_equal(lock_sleeps, 1);
        assert_int_equal(fds_after, fds);
        assert_true(free_after);

        assert_int_equal(resctrl_lock_exclusive(), PQOS_RETVAL_OK);
        assert_int_equal(resctrl_lock_release(), PQOS_RETVAL_OK);
}

/* a lock whose descriptor cannot be opened fails, saying so, and leaves the
 * thread's cancellation as the caller had it, on or off
 */
static void
test_resctrl_lock_open_failure(void **state __attribute__((unused)))
{
        const int states[] = {PTHREAD_CANCEL_ENABLE, PTHREAD_CANCEL_DISABLE};
        unsigned i;

        for (i = 0; i < 2; i++) {
                int found;
                int after;
                int ret;

                (void)pthread_setcancelstate(states[i], &found);
                lock_open_errno = EACCES;
                test_log_keep_start();
                ret = resctrl_lock_exclusive();
                (void)test_log_count_stop();
                lock_open_errno = 0;
                (void)pthread_setcancelstate(found, &after);

                assert_int_equal(ret, PQOS_RETVAL_ERROR);
                assert_non_null(strstr(test_log_said(),
                                       "Could not open " RESCTRL_PATH
                                       " directory"));
                assert_int_equal(after, states[i]);
        }

        assert_int_equal(resctrl_lock_exclusive(), PQOS_RETVAL_OK);
        assert_int_equal(resctrl_lock_release(), PQOS_RETVAL_OK);
}

/* a shared lock is taken beside another shared one, and an exclusive one is
 * not
 */
static void
test_resctrl_lock_shared_beside_shared(void **state __attribute__((unused)))
{
        const int holder = lock_hold(LOCK_SH);
        struct lock_watchdog watchdog;
        int fired;
        int ret;

        assert_int_equal(resctrl_lock_shared(), PQOS_RETVAL_OK);
        assert_int_equal(resctrl_lock_release(), PQOS_RETVAL_OK);

        lock_watchdog_start(&watchdog, holder);
        test_log_count_start();
        ret = resctrl_lock_exclusive();
        (void)test_log_count_stop();
        fired = lock_watchdog_stop(&watchdog);
        close(holder);
        if (ret == PQOS_RETVAL_OK)
                (void)resctrl_lock_release();

        assert_false(fired);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
}

int
main(void)
{
        int result = 0;

        const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_resctrl_mount_default),
            cmocka_unit_test(test_resctrl_mount_l3cdp),
            cmocka_unit_test(test_resctrl_mount_l2cdp),
            cmocka_unit_test(test_resctrl_mount_mba_ctrl),
            cmocka_unit_test(test_resctrl_umount),
            cmocka_unit_test(test_resctrl_umount_error),
            cmocka_unit_test(test_resctrl_cpumask_set),
            cmocka_unit_test(test_resctrl_cpumask_unset),
            cmocka_unit_test(test_resctrl_cpumask_read),
            cmocka_unit_test(test_resctrl_cpumask_read_every_cpu),
            cmocka_unit_test(test_resctrl_cpumask_read_longer_than_the_mask),
            cmocka_unit_test(test_resctrl_cpumask_core_past_the_mask),
            cmocka_unit_test(test_resctrl_cpumask_write),
            cmocka_unit_test(test_resctrl_cpumask_write_zero),
            cmocka_unit_test_setup_teardown(
                test_resctrl_lock_keeps_the_callers_alarm, lock_setup,
                lock_teardown),
            cmocka_unit_test_setup_teardown(test_resctrl_lock_times_out,
                                            lock_setup, lock_teardown),
            cmocka_unit_test_setup_teardown(
                test_resctrl_lock_taken_once_given_up, lock_setup,
                lock_teardown),
            cmocka_unit_test_setup_teardown(
                test_resctrl_lock_not_taken_past_the_timeout, lock_setup,
                lock_teardown),
            cmocka_unit_test_setup_teardown(
                test_resctrl_lock_clock_failure_ends_the_wait, lock_setup,
                lock_teardown),
            cmocka_unit_test_setup_teardown(
                test_resctrl_lock_cancelled_in_the_wait, lock_setup,
                lock_teardown),
            cmocka_unit_test_setup_teardown(
                test_resctrl_lock_failure_logged_cancellable, lock_setup,
                lock_teardown),
            cmocka_unit_test_setup_teardown(
                test_resctrl_lock_cancelled_before_the_call, lock_setup,
                lock_teardown),
            cmocka_unit_test_setup_teardown(
                test_resctrl_lock_cancelled_as_it_is_taken, lock_setup,
                lock_teardown),
            cmocka_unit_test_setup_teardown(
                test_resctrl_lock_cancelled_in_the_wait_then_taken, lock_setup,
                lock_teardown),
            cmocka_unit_test_setup_teardown(test_resctrl_lock_open_failure,
                                            lock_setup, lock_teardown),
            cmocka_unit_test_setup_teardown(
                test_resctrl_lock_keeps_cancellation_off, lock_setup,
                lock_teardown),
            cmocka_unit_test_setup_teardown(
                test_resctrl_lock_shared_beside_shared, lock_setup,
                lock_teardown),
        };

        result += cmocka_run_group_tests(tests, NULL, NULL);

        return result;
}
