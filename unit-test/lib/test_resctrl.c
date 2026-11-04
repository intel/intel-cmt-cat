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
#include <mntent.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <stdarg.h>
#include <sys/file.h>
#include <sys/syscall.h>
#include <sys/wait.h>
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
static int lock_opened_fd = -1;

int
__wrap_open(const char *path, int flags, ...)
{
        int mode = 0;
        int fd;

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
                fd = __real_open(lock_dir, flags, mode);
                __atomic_store_n(&lock_opened_fd, fd, __ATOMIC_SEQ_CST);
                return fd;
        }

        return __real_open(path, flags, mode);
}

/* flock() as it is, but where a case asks for it, the next unlock fails
 * after it has given the lock up, as one the kernel reports an error for
 */
static int lock_unlock_fails;

int
__wrap_flock(int fd, int operation)
{
        int ret = __real_flock(fd, operation);

        if ((operation & ~LOCK_NB) == LOCK_UN &&
            __atomic_exchange_n(&lock_unlock_fails, 0, __ATOMIC_SEQ_CST)) {
                errno = EIO;
                return -1;
        }

        return ret;
}

/* the keys created, counted, and as many of the next creations as a case asks
 * for refused with lock_key_create_errno; and as many of the next values set
 * refused, as with a key the process has run out of memory for
 */
static unsigned lock_key_creates;
static int lock_key_create_errno;
static unsigned lock_setspecific_fails;

int
__wrap_pthread_key_create(pthread_key_t *key, void (*destructor)(void *))
{
        const int err =
            __atomic_load_n(&lock_key_create_errno, __ATOMIC_SEQ_CST);

        __atomic_add_fetch(&lock_key_creates, 1, __ATOMIC_SEQ_CST);
        if (err != 0)
                return err;

        return __real_pthread_key_create(key, destructor);
}

int
__wrap_pthread_setspecific(pthread_key_t key, const void *value)
{
        /* a pthread function's error, which is positive */
        const int refused = ENOMEM;
        unsigned left =
            __atomic_load_n(&lock_setspecific_fails, __ATOMIC_SEQ_CST);

        while (left > 0)
                if (__atomic_compare_exchange_n(&lock_setspecific_fails, &left,
                                                left - 1, 0, __ATOMIC_SEQ_CST,
                                                __ATOMIC_SEQ_CST))
                        return refused;

        return __real_pthread_setspecific(key, value);
}

/* the sleeps of the lock's waits, and how many of them found the thread's
 * cancellation on, counted with the atomic builtins: a case's other thread
 * reads them; and what the first two asked for. A case can have the first
 * sleep:
 * - give a holder's lock up, and sleep as asked;
 * - give a holder's lock up, and last longer than the lock's timeout, as for a
 *   thread that is not run for a while;
 * - ask for the sleeping thread's own cancellation, which the wait has off;
 * - end at once, as a signal cuts a sleep short, with half of it left.
 */
static unsigned lock_sleeps;
static unsigned lock_sleeps_cancellable;
static struct timespec lock_sleeps_asked[2];
static int lock_released_holder = -1;
static int lock_overslept_holder = -1;
static int lock_cancel_in_sleep;
static int lock_sleep_interrupted;

int
__wrap_nanosleep(const struct timespec *req, struct timespec *rem)
{
        int found;
        int set;

        unsigned n;

        (void)pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &found);
        (void)pthread_setcancelstate(found, &set);
        n = __atomic_add_fetch(&lock_sleeps, 1, __ATOMIC_SEQ_CST);
        if (n <= DIM(lock_sleeps_asked))
                lock_sleeps_asked[n - 1] = *req;
        if (found == PTHREAD_CANCEL_ENABLE)
                __atomic_add_fetch(&lock_sleeps_cancellable, 1,
                                   __ATOMIC_SEQ_CST);
        if (__atomic_exchange_n(&lock_cancel_in_sleep, 0, __ATOMIC_SEQ_CST))
                (void)pthread_cancel(pthread_self());
        if (lock_released_holder != -1) {
                (void)flock(lock_released_holder, LOCK_UN);
                lock_released_holder = -1;
        }
        if (__atomic_exchange_n(&lock_sleep_interrupted, 0, __ATOMIC_SEQ_CST)) {
                if (rem != NULL) {
                        rem->tv_sec = req->tv_sec / 2;
                        rem->tv_nsec = req->tv_nsec / 2;
                }
                errno = EINTR;
                return -1;
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

/* umount2(2) as the kernel answers it: 0, or -1 and the errno queued after
 * it
 */
int
__wrap_umount2(const char *target, int flags)
{
        int ret;

        check_expected(target);
        assert_int_equal(flags, 0);

        ret = mock_type(int);
        if (ret < 0)
                errno = mock_type(int);

        return ret;
}

/* the descriptor a probe case hands the probe, while the probe is to have it
 * open, and the messages logged, and logged while it was: a log callback may
 * end the thread, so the probe closes it before it logs. And the calls made,
 * and the messages logged, with the thread's cancellation in the wrong state:
 * on from the open to the close of the descriptor, the close included, which
 * is a cancellation point, and off once it is closed
 */
static int probe_fd = -1;
static int probe_logged;
static int logged_while_open;
static int probe_calls_cancellable;
static int probe_logged_uncancellable;

/* the calling thread's cancellation state, left as it is */
static int
cancel_state(void)
{
        int found;
        int set;

        (void)pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &found);
        (void)pthread_setcancelstate(found, &set);
        return found;
}

int __real_close(int fd);

/* close() as it is, noting a close of the probe's descriptor made with the
 * thread's cancellation on
 */
int
__wrap_close(int fd)
{
        if (probe_fd >= 0 && fd == probe_fd &&
            cancel_state() == PTHREAD_CANCEL_ENABLE)
                probe_calls_cancellable++;

        return __real_close(fd);
}

/* fsopen(2) and fsconfig(2) as the kernel answers them: the descriptor or 0,
 * or -1 and the errno queued after it. fsconfig() is to be given the
 * descriptor fsopen() gave, and both are to run with cancellation off
 */
long
__wrap_syscall(long number, ...)
{
        va_list ap;
        long ret;

        va_start(ap, number);
#if defined(SYS_fsopen) && defined(SYS_fsconfig)
        if (number == SYS_fsopen || number == SYS_fsconfig) {
                if (cancel_state() == PTHREAD_CANCEL_ENABLE)
                        probe_calls_cancellable++;
        }
        if (number == SYS_fsopen) {
                const char *fs_name = va_arg(ap, const char *);
                const unsigned flags = va_arg(ap, unsigned);

                check_expected(fs_name);
                /* FSOPEN_CLOEXEC */
                assert_int_equal(flags, 1);
        } else if (number == SYS_fsconfig) {
                const int fd = va_arg(ap, int);
                const unsigned cmd = va_arg(ap, unsigned);
                const char *key = va_arg(ap, const char *);
                const void *value = va_arg(ap, const void *);
                const int aux = va_arg(ap, int);

                assert_int_equal(fd, probe_fd);
                /* FSCONFIG_SET_FLAG, which the kernel refuses with EINVAL
                 * before it reads the key where a value or aux is given
                 */
                assert_int_equal(cmd, 0);
                check_expected(key);
                assert_null(value);
                assert_int_equal(aux, 0);
        } else
                fail_msg("syscall %ld not expected", number);
#else
        fail_msg("syscall %ld not expected", number);
#endif
        va_end(ap);

        ret = mock_type(long);
        if (ret < 0)
                errno = mock_type(int);

        return ret;
}

static void
probe_log_check(void *context __attribute__((unused)),
                const size_t size __attribute__((unused)),
                const char *message __attribute__((unused)))
{
        probe_logged++;
        if (probe_fd >= 0 && fcntl(probe_fd, F_GETFD) != -1)
                logged_while_open++;
        if (cancel_state() == PTHREAD_CANCEL_DISABLE)
                probe_logged_uncancellable++;
}

/* /proc/mounts as the case queued it, or NULL where it is not to be read */
FILE *
__wrap_setmntent(const char *filename, const char *type)
{
        static char buf[512];
        const char *text;

        assert_string_equal(filename, "/proc/mounts");
        assert_string_equal(type, "re");

        text = mock_ptr_type(const char *);
        if (text == NULL)
                return NULL;

        /* fmemopen() takes a buffer it may write */
        assert_true(strlen(text) < sizeof(buf));
        strcpy(buf, text);

        return fmemopen(buf, strlen(buf), "r");
}

/* lines getmntent_r() is to skip as too long, as musl's does: the line read
 * past, NULL answered and errno ERANGE
 */
static int mounts_long_lines;

/* through a typedef, as checkpatch reads the plain declaration as an extern
 * in a .c file
 */
typedef struct mntent *
getmntent_r_fn(FILE *stream, struct mntent *mnt, char *buf, int buflen);
getmntent_r_fn __real_getmntent_r;

struct mntent *
__wrap_getmntent_r(FILE *stream, struct mntent *mnt, char *buf, int buflen)
{
        int c;

        if (mounts_long_lines == 0)
                return __real_getmntent_r(stream, mnt, buf, buflen);

        mounts_long_lines--;
        do
                c = fgetc(stream);
        while (c != EOF && c != '\n');
        errno = ERANGE;

        return NULL;
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

        /* the mount is still there */
        expect_string(__wrap_umount2, target, "/sys/fs/resctrl");
        will_return(__wrap_umount2, -1);
        will_return(__wrap_umount2, EBUSY);

        ret = resctrl_umount();
        assert_int_equal(ret, PQOS_RETVAL_ERROR);

        /* nothing is mounted there, or there is no such path */
        expect_string(__wrap_umount2, target, "/sys/fs/resctrl");
        will_return(__wrap_umount2, -1);
        will_return(__wrap_umount2, EINVAL);

        ret = resctrl_umount();
        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);

        expect_string(__wrap_umount2, target, "/sys/fs/resctrl");
        will_return(__wrap_umount2, -1);
        will_return(__wrap_umount2, ENOENT);

        ret = resctrl_umount();
        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);
}

/* ======== resctrl_mount_option_supported ======== */

static void
test_resctrl_mount_option_param(void **state __attribute__((unused)))
{
        int supported;

        assert_int_equal(resctrl_mount_option_supported(NULL, &supported),
                         PQOS_RETVAL_PARAM);
        assert_int_equal(resctrl_mount_option_supported("mba_MBps", NULL),
                         PQOS_RETVAL_PARAM);
}

#if defined(SYS_fsopen) && defined(SYS_fsconfig)

/**
 * The kernel's answers, in turn: to fsopen(), to the flag no kernel has and to
 * the option asked about; a negative answer is -errno, and 1 is no call
 */
static void
assert_mount_option(const int fsopen_err,
                    const int bogus_err,
                    const int option_err,
                    const int expected)
{
        int supported = 2;
        int fd = -1;
        int ret;

        expect_string(__wrap_syscall, fs_name, "resctrl");
        if (fsopen_err != 0) {
                will_return(__wrap_syscall, -1);
                will_return(__wrap_syscall, -fsopen_err);
        } else {
                /* a descriptor of its own for the probe to close */
                fd = open("/dev/null", O_RDONLY | O_CLOEXEC);
                assert_true(fd >= 0);
                will_return(__wrap_syscall, fd);

                expect_string(__wrap_syscall, key, "pqos_no_such_option");
                will_return(__wrap_syscall, bogus_err == 0 ? 0 : -1);
                if (bogus_err != 0)
                        will_return(__wrap_syscall, -bogus_err);

                if (option_err != 1) {
                        expect_string(__wrap_syscall, key, "mba_MBps");
                        will_return(__wrap_syscall, option_err == 0 ? 0 : -1);
                        if (option_err != 0)
                                will_return(__wrap_syscall, -option_err);
                }
        }

        probe_fd = fd;
        probe_logged = 0;
        logged_while_open = 0;
        probe_calls_cancellable = 0;
        probe_logged_uncancellable = 0;
        /* the probe logs at debug level, which only super verbose passes */
        (void)log_init(-1, probe_log_check, NULL, LOG_VER_SUPER_VERBOSE);
        ret = resctrl_mount_option_supported("mba_MBps", &supported);
        test_log_init_silent();
        probe_fd = -1;
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(supported, expected);
        assert_int_equal(logged_while_open, 0);
        /* cancellation off for the descriptor's life, and on again after */
        assert_int_equal(probe_calls_cancellable, 0);
        assert_int_equal(probe_logged_uncancellable, 0);
        assert_int_equal(cancel_state(), PTHREAD_CANCEL_ENABLE);
        /* an answer not known is said, so the check above saw a message */
        if (expected == -1)
                assert_true(probe_logged > 0);

        /* the probe closed it: closing it again fails */
        if (fd >= 0)
                assert_int_equal(close(fd), -1);
}

static void
test_resctrl_mount_option_supported(void **state __attribute__((unused)))
{
        /* taken, and refused, where the made-up flag is refused */
        assert_mount_option(0, -EINVAL, 0, 1);
        assert_mount_option(0, -EINVAL, -EINVAL, 0);
        /* another error for the option: not known */
        assert_mount_option(0, -EINVAL, -EPERM, -1);
}

static void
test_resctrl_mount_option_not_asked(void **state __attribute__((unused)))
{
        /* fsopen() fails: no resctrl, or not permitted */
        assert_mount_option(-ENODEV, 0, 1, -1);
        assert_mount_option(-EPERM, 0, 1, -1);
        /* a resctrl that takes the made-up flag parses options only when it
         * mounts, and one that answers it otherwise cannot be read either:
         * the option is not asked
         */
        assert_mount_option(0, 0, 1, -1);
        assert_mount_option(0, -ENOMEM, 1, -1);
}
#endif

/* ======== resctrl_mounted_with ======== */

/**
 * resctrl's own line, after another filesystem's whose options name mba_MBps
 */
static const char *mounts_resctrl_mbps =
    "tmpfs /run tmpfs rw,nosuid,mode=755 0 0\n"
    "resctrl /sys/fs/resctrl resctrl rw,relatime,mba_MBps 0 0\n";

static const char *mounts_resctrl_default =
    "fuse.x /mnt/x fuse.x rw,mba_MBps,user_id=0 0 0\n"
    "resctrl /sys/fs/resctrl resctrl rw,relatime,cdp,mba_MBps_x 0 0\n";

static const char *mounts_none = "proc /proc proc rw,relatime 0 0\n";

static void
assert_mounted_with(const char *mounts, const int expected)
{
        int found = 2;

        will_return(__wrap_setmntent, mounts);
        assert_int_equal(resctrl_mounted_with("mba_MBps", &found),
                         PQOS_RETVAL_OK);
        assert_int_equal(found, expected);
}

static void
test_resctrl_mounted_with(void **state __attribute__((unused)))
{
        int found;

        assert_mounted_with(mounts_resctrl_mbps, 1);
        /* another filesystem's option, and one whose name only begins with
         * mba_MBps, are not resctrl mounted with it
         */
        assert_mounted_with(mounts_resctrl_default, 0);
        assert_mounted_with(mounts_none, 0);
        /* /proc/mounts not read: not mounted with it, as for a file that is
         * not there
         */
        assert_mounted_with(NULL, 0);
        /* a line before resctrl's too long for the buffer, as musl reads one:
         * the lines after it are read all the same
         */
        mounts_long_lines = 1;
        assert_mounted_with(mounts_resctrl_mbps, 1);
        assert_int_equal(mounts_long_lines, 0);
        /* and where it is the last line, the end of the file ends the read */
        mounts_long_lines = 1;
        assert_mounted_with(mounts_none, 0);
        assert_int_equal(mounts_long_lines, 0);

        assert_int_equal(resctrl_mounted_with(NULL, &found), PQOS_RETVAL_PARAM);
        assert_int_equal(resctrl_mounted_with("mba_MBps", NULL),
                         PQOS_RETVAL_PARAM);
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
        lock_sleep_interrupted = 0;
        memset(lock_sleeps_asked, 0, sizeof(lock_sleeps_asked));
        lock_clock_fails_in = 0;
        lock_clock_frozen = 0;
        lock_opens = 0;
        lock_open_errno = 0;
        lock_cancel_in_open = 0;
        lock_opened_fd = -1;
        lock_unlock_fails = 0;
        lock_key_create_errno = 0;
        lock_setspecific_fails = 0;

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

/* 1 where another descriptor's lock of \a type, not waiting, is refused with
 * EWOULDBLOCK, as it is while the lock is held against it
 */
static int
lock_refused(const int type)
{
        const int fd = open(lock_dir, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
        int refused;

        assert_true(fd >= 0);
        refused = flock(fd, type | LOCK_NB) != 0 && errno == EWOULDBLOCK;
        close(fd);

        return refused;
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
 * clock is frozen for the case. It is held until it is released
 */
static void
test_resctrl_lock_taken_once_given_up(void **state __attribute__((unused)))
{
        const int holder = lock_hold(LOCK_EX);
        struct lock_watchdog watchdog;
        int fired;
        int held;
        int ret;

        lock_released_holder = holder;
        __atomic_store_n(&lock_clock_frozen, 1, __ATOMIC_SEQ_CST);
        lock_watchdog_start(&watchdog, holder);
        ret = resctrl_lock_exclusive();
        fired = lock_watchdog_stop(&watchdog);
        __atomic_store_n(&lock_clock_frozen, 0, __ATOMIC_SEQ_CST);
        lock_released_holder = -1;
        close(holder);
        held = lock_refused(LOCK_SH);

        assert_false(fired);
        assert_int_equal(lock_sleeps, 1);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(resctrl_lock_release(), PQOS_RETVAL_OK);
        assert_true(held);
        assert_false(lock_refused(LOCK_EX));
}

/* a lock that was taken is held until it is released: an exclusive one
 * against another descriptor's shared and exclusive locks, a shared one
 * against an exclusive lock alone; and neither after its release. Each is
 * released before anything is asserted, so that a case after a failed one
 * does not find it held
 */
static void
test_resctrl_lock_held_until_released(void **state __attribute__((unused)))
{
        int ex_against_sh;
        int ex_against_ex;
        int sh_against_ex;
        int sh_against_sh;
        int taken;

        taken = resctrl_lock_exclusive();
        ex_against_sh = lock_refused(LOCK_SH);
        ex_against_ex = lock_refused(LOCK_EX);
        if (taken == PQOS_RETVAL_OK)
                (void)resctrl_lock_release();
        assert_int_equal(taken, PQOS_RETVAL_OK);
        assert_true(ex_against_sh);
        assert_true(ex_against_ex);
        assert_false(lock_refused(LOCK_EX));

        taken = resctrl_lock_shared();
        sh_against_ex = lock_refused(LOCK_EX);
        sh_against_sh = lock_refused(LOCK_SH);
        if (taken == PQOS_RETVAL_OK)
                (void)resctrl_lock_release();
        assert_int_equal(taken, PQOS_RETVAL_OK);
        assert_true(sh_against_ex);
        assert_false(sh_against_sh);
        assert_false(lock_refused(LOCK_EX));
}

/* a sleep of the wait that a signal cuts short is slept out, the time it had
 * left asked for again, before the lock is tried again: here the holder gives
 * the lock up in that sleep, so a try straight after the signal would take it
 * at once, with one sleep made. The clock is frozen for the case
 */
static void
test_resctrl_lock_interrupted_sleep_slept_out(void **state
                                              __attribute__((unused)))
{
        const int holder = lock_hold(LOCK_EX);
        struct lock_watchdog watchdog;
        int fired;
        int ret;

        lock_released_holder = holder;
        lock_sleep_interrupted = 1;
        __atomic_store_n(&lock_clock_frozen, 1, __ATOMIC_SEQ_CST);
        lock_watchdog_start(&watchdog, holder);
        ret = resctrl_lock_exclusive();
        fired = lock_watchdog_stop(&watchdog);
        __atomic_store_n(&lock_clock_frozen, 0, __ATOMIC_SEQ_CST);
        lock_released_holder = -1;
        close(holder);
        if (ret == PQOS_RETVAL_OK)
                (void)resctrl_lock_release();

        assert_false(fired);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(lock_sleeps, 2);
        assert_int_equal(lock_sleeps_asked[1].tv_sec,
                         lock_sleeps_asked[0].tv_sec / 2);
        assert_int_equal(lock_sleeps_asked[1].tv_nsec,
                         lock_sleeps_asked[0].tv_nsec / 2);
        assert_true(lock_sleeps_asked[0].tv_nsec > 0);
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

/* what resctrl_lock_exclusive() gave a worker that ends holding the lock */
static int lock_ended_ret = -1;

/* takes the lock and ends its thread with it held, as a locked section that
 * calls pthread_exit() does
 */
static void *
lock_exit_worker(void *arg __attribute__((unused)))
{
        lock_ended_ret = resctrl_lock_exclusive();
        pthread_exit(NULL);
}

/* takes the lock and is cancelled with it held, at a cancellation point of
 * its locked section
 */
static void *
lock_cancel_holding_worker(void *arg __attribute__((unused)))
{
        lock_ended_ret = resctrl_lock_exclusive();
        (void)pthread_cancel(pthread_self());
        pthread_testcancel();

        return NULL;
}

/* a log callback that ends the thread it is called on, as an application's
 * callback may
 */
static void
lock_log_ends_the_thread(void *context __attribute__((unused)),
                         const size_t size __attribute__((unused)),
                         const char *message __attribute__((unused)))
{
        pthread_exit(NULL);
}

/* what a worker returns where it goes on past the point it was to end at */
#define LOCK_WORKER_WENT_ON ((void *)1)

/* takes the lock and logs in its locked section, where the callback ends the
 * thread; a worker the log has not ended returns LOCK_WORKER_WENT_ON
 */
static void *
lock_log_exit_worker(void *arg __attribute__((unused)))
{
        lock_ended_ret = resctrl_lock_exclusive();
        LOG_ERROR("a locked section logs\n");

        return LOCK_WORKER_WENT_ON;
}

/* runs \a worker, which takes the lock and ends with it held, giving what it
 * returned, and the log back to the silent one afterwards
 */
static void *
lock_run_ending_worker(void *(*worker)(void *))
{
        void *result = NULL;

        lock_ended_ret = -1;
        assert_int_equal(lock_run_worker(worker, &result), 0);
        test_log_init_silent();

        return result;
}

/* the worker took the lock, and its end gave it up: its descriptor closed, no
 * other descriptor refused the lock, and the lock taken again at once
 */
static void
assert_lock_given_up(const unsigned fds)
{
        assert_int_equal(lock_ended_ret, PQOS_RETVAL_OK);
        assert_int_equal(lock_open_fds(), fds);
        assert_int_equal(lock_refused(LOCK_EX), 0);
        assert_int_equal(resctrl_lock_exclusive(), PQOS_RETVAL_OK);
        assert_int_equal(resctrl_lock_release(), PQOS_RETVAL_OK);
        assert_int_equal(lock_sleeps, 0);
}

/* a thread that calls pthread_exit() holding the lock gives it up as it ends,
 * without a release
 */
static void
test_resctrl_lock_given_up_at_thread_exit(void **state __attribute__((unused)))
{
        const unsigned fds = lock_open_fds();

        assert_null(lock_run_ending_worker(lock_exit_worker));
        assert_lock_given_up(fds);
}

/* a thread cancelled holding the lock gives it up as it ends */
static void
test_resctrl_lock_given_up_when_cancelled_holding_it(void **state
                                                     __attribute__((unused)))
{
        const unsigned fds = lock_open_fds();

        assert_ptr_equal(lock_run_ending_worker(lock_cancel_holding_worker),
                         PTHREAD_CANCELED);
        assert_lock_given_up(fds);
}

/* a log callback that ends the thread in a locked section, as one called from
 * a log while the lock is held, has the lock given up as the thread ends
 */
static void
test_resctrl_lock_given_up_when_a_log_callback_ends_the_thread(
    void **state __attribute__((unused)))
{
        const unsigned fds = lock_open_fds();

        assert_int_equal(
            log_init(-1, lock_log_ends_the_thread, NULL, LOG_VER_DEFAULT),
            LOG_RETVAL_OK);
        assert_null(lock_run_ending_worker(lock_log_exit_worker));
        assert_lock_given_up(fds);
}

/* what a release gave a worker */
static int lock_released_ret = -1;

/* takes the lock, has its cancellation asked for, as it would be by another
 * thread meanwhile, and releases the lock; ends at its next cancellation point
 */
static void *
lock_release_cancelled_worker(void *arg __attribute__((unused)))
{
        lock_ended_ret = resctrl_lock_exclusive();
        (void)pthread_cancel(pthread_self());
        lock_released_ret = resctrl_lock_release();
        pthread_testcancel();

        return LOCK_WORKER_WENT_ON;
}

/* a release with a cancellation pending gives the lock up and closes its
 * descriptor before the cancellation is acted on: close() is a cancellation
 * point, and the thread would otherwise end with the descriptor open and
 * nothing left to close it
 */
static void
test_resctrl_lock_release_not_cut_short_by_a_cancellation(
    void **state __attribute__((unused)))
{
        const unsigned fds = lock_open_fds();

        lock_released_ret = -1;
        assert_ptr_equal(lock_run_ending_worker(lock_release_cancelled_worker),
                         PTHREAD_CANCELED);
        assert_int_equal(lock_released_ret, PQOS_RETVAL_OK);
        assert_lock_given_up(fds);
}

/* the stage a worker that releases before it ends has reached, and the
 * descriptor it locked with
 */
static int lock_stage;
static int lock_worker_fd = -1;

static void
lock_stage_wait(const int stage)
{
        while (__atomic_load_n(&lock_stage, __ATOMIC_SEQ_CST) < stage)
                (void)sched_yield();
}

/* takes the lock and releases it, then waits for the case to take the lock
 * itself before it ends
 */
static void *
lock_release_then_exit_worker(void *arg __attribute__((unused)))
{
        lock_ended_ret = resctrl_lock_exclusive();
        lock_worker_fd = __atomic_load_n(&lock_opened_fd, __ATOMIC_SEQ_CST);
        lock_released_ret = resctrl_lock_release();
        __atomic_store_n(&lock_stage, 1, __ATOMIC_SEQ_CST);
        lock_stage_wait(2);
        pthread_exit(NULL);
}

/* a thread that released the lock before it ends gives nothing up as it ends:
 * not the lock another holder has taken since, on the very descriptor number
 * the thread's own had
 */
static void
test_resctrl_lock_released_not_given_up_again_at_exit(void **state
                                                      __attribute__((unused)))
{
        const unsigned fds = lock_open_fds();
        pthread_t thread;
        int ret;

        lock_stage = 0;
        lock_worker_fd = -1;
        lock_released_ret = -1;
        assert_int_equal(
            pthread_create(&thread, NULL, lock_release_then_exit_worker, NULL),
            0);
        lock_stage_wait(1);
        ret = resctrl_lock_exclusive();
        __atomic_store_n(&lock_stage, 2, __ATOMIC_SEQ_CST);
        assert_int_equal(pthread_join(thread, NULL), 0);

        assert_int_equal(lock_ended_ret, PQOS_RETVAL_OK);
        assert_int_equal(lock_released_ret, PQOS_RETVAL_OK);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        /* the number the worker's descriptor had, reused */
        assert_int_equal(lock_opened_fd, lock_worker_fd);
        assert_int_equal(lock_refused(LOCK_EX), 1);
        assert_int_equal(resctrl_lock_release(), PQOS_RETVAL_OK);
        assert_int_equal(lock_open_fds(), fds);
        assert_int_equal(lock_refused(LOCK_EX), 0);
}

/* a child forked while the lock is held, whose thread ends, closes its copy of
 * the descriptor without giving up the lock, which is the parent's and is
 * held on the open file description the two share
 */
static void
test_resctrl_lock_kept_when_a_forked_child_ends(void **state
                                                __attribute__((unused)))
{
        const unsigned fds = lock_open_fds();
        int status = -1;
        pid_t child;

        assert_int_equal(resctrl_lock_exclusive(), PQOS_RETVAL_OK);
        (void)fflush(NULL);
        child = fork();
        if (child == 0)
                pthread_exit(NULL);
        assert_true(child > 0);
        assert_int_equal(waitpid(child, &status, 0), child);
        assert_true(WIFEXITED(status));
        assert_int_equal(WEXITSTATUS(status), 0);

        assert_int_equal(lock_refused(LOCK_EX), 1);
        assert_int_equal(resctrl_lock_release(), PQOS_RETVAL_OK);
        assert_int_equal(lock_open_fds(), fds);
        assert_int_equal(lock_refused(LOCK_EX), 0);
}

/* what the log callback found as the release warned that its unlock failed */
static int lock_warned_fds = -1;
static int lock_warned_refused = -1;

static void
lock_log_checks_given_up(void *context __attribute__((unused)),
                         const size_t size __attribute__((unused)),
                         const char *message __attribute__((unused)))
{
        if (lock_warned_fds >= 0)
                return;
        lock_warned_fds = (int)lock_open_fds();
        lock_warned_refused = lock_refused(LOCK_EX);
}

/* a release whose unlock fails warns of it with the descriptor already
 * closed and the lock free, as a callback that ends the thread there leaves
 * nothing behind
 */
static void
test_resctrl_lock_closed_before_the_unlock_warning(void **state
                                                   __attribute__((unused)))
{
        const unsigned fds = lock_open_fds();
        int ret;

        lock_warned_fds = -1;
        lock_warned_refused = -1;
        assert_int_equal(resctrl_lock_exclusive(), PQOS_RETVAL_OK);
        assert_int_equal(
            log_init(-1, lock_log_checks_given_up, NULL, LOG_VER_DEFAULT),
            LOG_RETVAL_OK);
        lock_unlock_fails = 1;
        ret = resctrl_lock_release();
        test_log_init_silent();

        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(lock_unlock_fails, 0);
        assert_int_equal(lock_warned_fds, (int)fds);
        assert_int_equal(lock_warned_refused, 0);
}

/* takes the lock and releases it twice, then takes it a third time and ends
 * holding it
 */
static void *
lock_twice_then_exit_worker(void *arg __attribute__((unused)))
{
        int i;

        for (i = 0; i < 2; i++) {
                if (resctrl_lock_exclusive() != PQOS_RETVAL_OK ||
                    resctrl_lock_release() != PQOS_RETVAL_OK)
                        return LOCK_WORKER_WENT_ON;
        }
        lock_ended_ret = resctrl_lock_exclusive();
        pthread_exit(NULL);
}

/* a thread whose key value cannot be set locks and releases as before, is
 * told once, and tries again at each lock: once set, the thread gives the lock
 * up as it ends
 */
static void
test_resctrl_lock_holder_recorded_once_it_can_be(void **state
                                                 __attribute__((unused)))
{
        const unsigned fds = lock_open_fds();
        void *result;

        lock_setspecific_fails = 2;
        test_log_count_start();
        result = lock_run_ending_worker(lock_twice_then_exit_worker);

        assert_null(result);
        assert_int_equal(*test_log_count(), 1);
        assert_int_equal(lock_setspecific_fails, 0);
        assert_lock_given_up(fds);
}

/* a key that cannot be created leaves the lock as it was, said once for the
 * thread, and the next lock creates it: a thread that ends holding the lock
 * afterwards gives it up. The first of the lock's cases, as the key, once
 * made, is kept for the process: the count of creations shows it was not made
 * before
 */
static void
test_resctrl_lock_key_created_once_it_can_be(void **state
                                             __attribute__((unused)))
{
        unsigned fds;

        assert_int_equal(lock_key_creates, 0);
        lock_key_create_errno = EAGAIN;
        test_log_count_start();
        assert_int_equal(resctrl_lock_exclusive(), PQOS_RETVAL_OK);
        assert_int_equal(resctrl_lock_release(), PQOS_RETVAL_OK);
        assert_int_equal(resctrl_lock_shared(), PQOS_RETVAL_OK);
        assert_int_equal(resctrl_lock_release(), PQOS_RETVAL_OK);
        assert_int_equal(test_log_count_stop(), 1);
        assert_int_equal(lock_key_creates, 2);

        lock_key_create_errno = 0;
        fds = lock_open_fds();
        assert_null(lock_run_ending_worker(lock_exit_worker));
        assert_int_equal(lock_key_creates, 3);
        assert_lock_given_up(fds);
        assert_int_equal(lock_key_creates, 3);
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

/* the cancellation case's worker with its cancellation asynchronous through
 * the lock, as a caller may have it: a cancellation pending is acted on as
 * soon as the lock puts the caller's state back
 */
static void *
lock_cancelled_async_worker(void *arg __attribute__((unused)))
{
        int type;

        (void)pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, &type);
        lock_cancelled_ret = resctrl_lock_exclusive();
        (void)pthread_setcanceltype(type, &type);
        pthread_testcancel();

        return NULL;
}

/* and so it is for a caller whose cancellation is asynchronous: the lock's
 * handler is in before the caller's state is put back, where the cancellation
 * is acted on at once
 */
static void
test_resctrl_lock_cancelled_as_it_is_taken_async(void **state
                                                 __attribute__((unused)))
{
        const unsigned fds = lock_open_fds();
        void *result = NULL;
        int ran;

        lock_cancelled_ret = -1;
        __atomic_store_n(&lock_cancel_in_open, 1, __ATOMIC_SEQ_CST);
        ran = lock_run_worker(lock_cancelled_async_worker, &result);
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
 * gives the lock up, so that the try after the sleep takes it, however late
 * its thread runs: the clock is frozen for the case, which a timeout would
 * otherwise fail with the lock correctly not taken
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
        __atomic_store_n(&lock_clock_frozen, 1, __ATOMIC_SEQ_CST);
        lock_watchdog_start(&watchdog, holder);
        ran = lock_run_worker(lock_cancelled_worker, &result);
        fired = lock_watchdog_stop(&watchdog);
        __atomic_store_n(&lock_clock_frozen, 0, __ATOMIC_SEQ_CST);
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
#if defined(SYS_fsopen) && defined(SYS_fsconfig)
            cmocka_unit_test(test_resctrl_mount_option_supported),
            cmocka_unit_test(test_resctrl_mount_option_not_asked),
#endif
            cmocka_unit_test(test_resctrl_mount_option_param),
            cmocka_unit_test(test_resctrl_mounted_with),
            cmocka_unit_test(test_resctrl_cpumask_set),
            cmocka_unit_test(test_resctrl_cpumask_unset),
            cmocka_unit_test(test_resctrl_cpumask_read),
            cmocka_unit_test(test_resctrl_cpumask_read_every_cpu),
            cmocka_unit_test(test_resctrl_cpumask_read_longer_than_the_mask),
            cmocka_unit_test(test_resctrl_cpumask_core_past_the_mask),
            cmocka_unit_test(test_resctrl_cpumask_write),
            cmocka_unit_test(test_resctrl_cpumask_write_zero),
            cmocka_unit_test_setup_teardown(
                test_resctrl_lock_key_created_once_it_can_be, lock_setup,
                lock_teardown),
            cmocka_unit_test_setup_teardown(
                test_resctrl_lock_keeps_the_callers_alarm, lock_setup,
                lock_teardown),
            cmocka_unit_test_setup_teardown(test_resctrl_lock_times_out,
                                            lock_setup, lock_teardown),
            cmocka_unit_test_setup_teardown(
                test_resctrl_lock_taken_once_given_up, lock_setup,
                lock_teardown),
            cmocka_unit_test_setup_teardown(
                test_resctrl_lock_held_until_released, lock_setup,
                lock_teardown),
            cmocka_unit_test_setup_teardown(
                test_resctrl_lock_interrupted_sleep_slept_out, lock_setup,
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
                test_resctrl_lock_cancelled_as_it_is_taken_async, lock_setup,
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
            cmocka_unit_test_setup_teardown(
                test_resctrl_lock_given_up_at_thread_exit, lock_setup,
                lock_teardown),
            cmocka_unit_test_setup_teardown(
                test_resctrl_lock_given_up_when_cancelled_holding_it,
                lock_setup, lock_teardown),
            cmocka_unit_test_setup_teardown(
                test_resctrl_lock_given_up_when_a_log_callback_ends_the_thread,
                lock_setup, lock_teardown),
            cmocka_unit_test_setup_teardown(
                test_resctrl_lock_release_not_cut_short_by_a_cancellation,
                lock_setup, lock_teardown),
            cmocka_unit_test_setup_teardown(
                test_resctrl_lock_released_not_given_up_again_at_exit,
                lock_setup, lock_teardown),
            cmocka_unit_test_setup_teardown(
                test_resctrl_lock_kept_when_a_forked_child_ends, lock_setup,
                lock_teardown),
            cmocka_unit_test_setup_teardown(
                test_resctrl_lock_closed_before_the_unlock_warning, lock_setup,
                lock_teardown),
            cmocka_unit_test_setup_teardown(
                test_resctrl_lock_holder_recorded_once_it_can_be, lock_setup,
                lock_teardown),
        };

        result += cmocka_run_group_tests(tests, NULL, NULL);

        return result;
}
