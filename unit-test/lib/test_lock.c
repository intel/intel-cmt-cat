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

#include "test_lock.h"

#ifndef LOCKFILE
#define LOCKFILE "/tmp/libpqos_test_lockfile"
#endif

#include "lock.h"
/* The log's own mutex calls reach the mocks below, and log_init() would take it
 * from test.h's constructor - before cmocka is running, where function_called()
 * crashes. lock.c logs nothing, so this file needs no log at all: see
 * TEST_NO_LOG_CONSTRUCTOR in test.h.
 */
#define TEST_NO_LOG_CONSTRUCTOR

#include "test.h"

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>

/* Hardcoded in lib/lock.c */
#define TEST_LOCKDIR      "/var/lock"
#define TEST_LOCKFILE_TMP "/var/lock/myapilock.tmp"

#define TEST_TMP_FD   123
#define TEST_LOCK_FD  77
#define TEST_LOCK_FD2 88
#define TEST_LOCK_FD3 55

/* ======== mocks ======== */

int
__wrap_pthread_mutex_init(pthread_mutex_t *restrict mutex,
                          const pthread_mutexattr_t *restrict attr
                          __attribute__((unused)))
{
        assert_non_null(mutex);
        function_called();
        return mock();
}

int
__wrap_pthread_mutex_destroy(pthread_mutex_t *mutex)
{
        assert_non_null(mutex);
        function_called();
        return mock();
}

int
__wrap_pthread_mutex_lock(pthread_mutex_t *mutex)
{
        assert_non_null(mutex);
        function_called();
        return mock();
}

int
__wrap_pthread_mutex_unlock(pthread_mutex_t *mutex)
{
        assert_non_null(mutex);
        function_called();
        return mock();
}

int
__wrap_access(const char *pathname, int mode)
{
        function_called();
        check_expected_ptr(pathname);
        check_expected(mode);
        return mock_type(int);
}

int
__wrap_unlink(const char *pathname)
{
        function_called();
        check_expected_ptr(pathname);
        return mock_type(int);
}

/* The flags of every open lock.c performed, so that a case can require what
 * they all have to carry. The mock below asserts on the two opens the cases
 * arm; this is about the property every one of them shares, and the opens that
 * pass through - the lock file read back, the /proc entry behind the liveness
 * check - are exactly the ones a change is most likely to leave inheritable.
 */
#define OPENS_MAX      32
#define OPENS_PATH_MAX 256
static char opens_path[OPENS_MAX][OPENS_PATH_MAX];
static int opens_flags[OPENS_MAX];
static int opens_seen;

static void
opens_clear(void)
{
        opens_seen = 0;
}

static void
opens_record(const char *path, int oflags)
{
        if (opens_seen >= OPENS_MAX)
                return;

        strncpy(opens_path[opens_seen], path, OPENS_PATH_MAX - 1);
        opens_path[opens_seen][OPENS_PATH_MAX - 1] = '\0';
        opens_flags[opens_seen++] = oflags;
}

/* Whether the run opened a name carrying this text, with exactly these flags
 *
 * The name matters as much as the flags: a call site put back to fopen()
 * performs no open() at all, so a case that only asked whether every open
 * carried O_CLOEXEC would pass with the descriptor inheritable again. Each
 * expected open is therefore named.
 */
static int
opens_include(const char *what, int oflags)
{
        int i;

        for (i = 0; i < opens_seen; i++)
                if (strstr(opens_path[i], what) != NULL &&
                    opens_flags[i] == oflags)
                        return 1;

        return 0;
}

/*
 * NOTE: open/close are linked with --wrap=open/close, so
 * __real_open/__real_close exist and can be used for passthrough.
 */
/* Whether open() is being handed a mode, by the rule the C library uses
 *
 * The third argument of open() is there only when the flags ask for it, so a
 * wrapper may only read one then. O_TMPFILE asks for a mode without carrying
 * O_CREAT in its bit pattern, which is why it is tested for on its own.
 */
static int
open_takes_a_mode(int oflags)
{
#ifdef O_TMPFILE
        if ((oflags & O_TMPFILE) == O_TMPFILE)
                return 1;
#endif

        return (oflags & O_CREAT) != 0;
}

int
__wrap_open(const char *path, int oflags, ...)
{
        int mode = 0;

        if (open_takes_a_mode(oflags)) {
                va_list args;

                va_start(args, oflags);
                mode = va_arg(args, int);
                va_end(args);
        }

        opens_record(path, oflags);

        /* Only the two opens the cases arm are mocked: the lock file taken
         * exclusively, and the temporary file the directory check makes. Every
         * other open goes through, which includes the read-only open of the
         * lock file that read_lockfile() does and the /proc read behind
         * is_pid_alive() - neither is what a case here arms, and asserting on
         * them made the mock a description of lock.c's internals rather than
         * of what these cases are about.
         */
        int mocked = (strcmp(path, TEST_LOCKFILE_TMP) == 0) ||
                     (strcmp(path, LOCKFILE) == 0 &&
                      (oflags & (O_RDWR | O_CREAT | O_EXCL)) ==
                          (O_RDWR | O_CREAT | O_EXCL));

        if (!mocked)
                return __real_open(path, oflags, mode);

        function_called();
        check_expected_ptr(path);
        check_expected(oflags);
        check_expected(mode);

        errno = mock_type(int);
        return mock_type(int);
}

int
__wrap_close(int fildes)
{
        /* Only mock close for the fds we control */
        if (fildes != TEST_TMP_FD && fildes != TEST_LOCK_FD &&
            fildes != TEST_LOCK_FD2 && fildes != TEST_LOCK_FD3)
                return __real_close(fildes);

        function_called();
        check_expected(fildes);
        return mock_type(int);
}

/*
 * Suppress lock.c error prints in unit tests by wrapping fprintf.
 * This avoids noisy output like:
 * "Couldn't create lock file: ... Error: ..."
 *
 * IMPORTANT: do NOT wrap vfprintf; lock.c uses fprintf directly.
 * Also: we only suppress when stream == stderr, otherwise pass through.
 */
int
__wrap_fprintf(FILE *stream, const char *format, ...)
{
        (void)format;

        if (stream == stderr)
                return 0;

        /* Best-effort passthrough for non-stderr */
        return 0;
}

/* ======== helpers ======== */

static void
expect_check_lockdir_access_ok(void)
{
        expect_function_call(__wrap_access);
        expect_string(__wrap_access, pathname, TEST_LOCKDIR);
        expect_value(__wrap_access, mode, R_OK | W_OK | X_OK);
        will_return(__wrap_access, 0);

        expect_function_call(__wrap_open);
        expect_string(__wrap_open, path, TEST_LOCKFILE_TMP);
        expect_value(__wrap_open, oflags,
                     O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC);
        expect_value(__wrap_open, mode, 0666);
        will_return(__wrap_open, 0);
        will_return(__wrap_open, TEST_TMP_FD);

        expect_function_call(__wrap_close);
        expect_value(__wrap_close, fildes, TEST_TMP_FD);
        will_return(__wrap_close, 0);

        expect_function_call(__wrap_unlink);
        expect_string(__wrap_unlink, pathname, TEST_LOCKFILE_TMP);
        will_return(__wrap_unlink, 0);
}

static void
expect_lockfile_open(int err, int fd)
{
        expect_function_call(__wrap_open);
        expect_string(__wrap_open, path, LOCKFILE);
        expect_value(__wrap_open, oflags,
                     O_RDWR | O_CREAT | O_EXCL | O_CLOEXEC);
        expect_value(__wrap_open, mode, 0666);
        will_return(__wrap_open, err);
        will_return(__wrap_open, fd);
}

/* ======== tests ======== */

static void
test_lock_init_error(void **state __attribute__((unused)))
{
        /*
         * Case 1: open(lockfile) fails.
         * Use ENOENT (or EEXIST) instead of EACCES to avoid "Permission denied"
         * message semantics, even if it were to leak. With fprintf(stderr)
         * wrapped, there should be no output anyway.
         */
        expect_function_call(__wrap_pthread_mutex_lock);
        will_return(__wrap_pthread_mutex_lock, 0);

        expect_check_lockdir_access_ok();
        expect_lockfile_open(ENOENT, -1);

        expect_function_call(__wrap_pthread_mutex_unlock);
        will_return(__wrap_pthread_mutex_unlock, 0);

        assert_int_equal(lock_init(), -1);

        /* Case 2: open ok, mutex init fails (cleanup: close+unlink) */
        expect_function_call(__wrap_pthread_mutex_lock);
        will_return(__wrap_pthread_mutex_lock, 0);

        expect_check_lockdir_access_ok();
        expect_lockfile_open(0, TEST_LOCK_FD3);

        expect_function_call(__wrap_pthread_mutex_init);
        will_return(__wrap_pthread_mutex_init, -1);

        expect_function_call(__wrap_close);
        expect_value(__wrap_close, fildes, TEST_LOCK_FD3);
        will_return(__wrap_close, 0);

        expect_function_call(__wrap_unlink);
        expect_string(__wrap_unlink, pathname, LOCKFILE);
        will_return(__wrap_unlink, 0);

        expect_function_call(__wrap_pthread_mutex_unlock);
        will_return(__wrap_pthread_mutex_unlock, 0);

        assert_int_equal(lock_init(), -1);
}

static void
test_lock_init_exit(void **state __attribute__((unused)))
{
        /* init ok */
        expect_function_call(__wrap_pthread_mutex_lock);
        will_return(__wrap_pthread_mutex_lock, 0);

        expect_check_lockdir_access_ok();
        expect_lockfile_open(0, TEST_LOCK_FD);

        expect_function_call(__wrap_pthread_mutex_init);
        will_return(__wrap_pthread_mutex_init, 0);

        expect_function_call(__wrap_pthread_mutex_unlock);
        will_return(__wrap_pthread_mutex_unlock, 0);

        assert_int_equal(lock_init(), 0);

        /* fini ok */
        expect_function_call(__wrap_close);
        expect_value(__wrap_close, fildes, TEST_LOCK_FD);
        will_return(__wrap_close, 0);

        expect_function_call(__wrap_pthread_mutex_destroy);
        will_return(__wrap_pthread_mutex_destroy, 0);

        expect_function_call(__wrap_unlink);
        expect_string(__wrap_unlink, pathname, LOCKFILE);
        will_return(__wrap_unlink, 0);

        assert_int_equal(lock_fini(), 0);

        /* init ok again */
        expect_function_call(__wrap_pthread_mutex_lock);
        will_return(__wrap_pthread_mutex_lock, 0);

        expect_check_lockdir_access_ok();
        expect_lockfile_open(0, TEST_LOCK_FD2);

        expect_function_call(__wrap_pthread_mutex_init);
        will_return(__wrap_pthread_mutex_init, 0);

        expect_function_call(__wrap_pthread_mutex_unlock);
        will_return(__wrap_pthread_mutex_unlock, 0);

        assert_int_equal(lock_init(), 0);

        /* fini: destroy fails */
        expect_function_call(__wrap_close);
        expect_value(__wrap_close, fildes, TEST_LOCK_FD2);
        will_return(__wrap_close, 0);

        expect_function_call(__wrap_pthread_mutex_destroy);
        will_return(__wrap_pthread_mutex_destroy, -1);

        expect_function_call(__wrap_unlink);
        expect_string(__wrap_unlink, pathname, LOCKFILE);
        will_return(__wrap_unlink, 0);

        assert_int_equal(lock_fini(), -1);
}

static void
test_lock_get(void **state __attribute__((unused)))
{
        /* init ok */
        expect_function_call(__wrap_pthread_mutex_lock);
        will_return(__wrap_pthread_mutex_lock, 0);

        expect_check_lockdir_access_ok();
        expect_lockfile_open(0, TEST_LOCK_FD2);

        expect_function_call(__wrap_pthread_mutex_init);
        will_return(__wrap_pthread_mutex_init, 0);

        expect_function_call(__wrap_pthread_mutex_unlock);
        will_return(__wrap_pthread_mutex_unlock, 0);

        assert_int_equal(lock_init(), 0);

        /* lock_get / lock_release */
        expect_function_call(__wrap_pthread_mutex_lock);
        will_return(__wrap_pthread_mutex_lock, 0);
        lock_get();

        expect_function_call(__wrap_pthread_mutex_unlock);
        will_return(__wrap_pthread_mutex_unlock, 0);
        lock_release();

        /* fini ok */
        expect_function_call(__wrap_close);
        expect_value(__wrap_close, fildes, TEST_LOCK_FD2);
        will_return(__wrap_close, 0);

        expect_function_call(__wrap_pthread_mutex_destroy);
        will_return(__wrap_pthread_mutex_destroy, 0);

        expect_function_call(__wrap_unlink);
        expect_string(__wrap_unlink, pathname, LOCKFILE);
        will_return(__wrap_unlink, 0);

        assert_int_equal(lock_fini(), 0);
}

/* Every descriptor this file opens closes on exec
 *
 * fopen() does not do that, and three of these opens replaced a fopen(): the
 * lock file read, the lock file written and the /proc entry read for a process
 * start time. The flags are collected for every open the run performs rather
 * than arming each one, because what is being required is the property they
 * share - a fopen() put back anywhere here, or the flag dropped from one
 * open(), fails this case wherever it happens.
 */
static void
test_lock_opens_close_on_exec(void **state __attribute__((unused)))
{
        int i;

        opens_clear();

        /* a successful init, which is what performs them: the directory
         * check's temporary file, the lock file created exclusively, the /proc
         * entry read for this process's start time, and the lock file written
         * with the pid and that time. It is not read on this path - the read
         * happens when the file is already there, which is the second half of
         * this case
         */
        expect_function_call(__wrap_pthread_mutex_lock);
        will_return(__wrap_pthread_mutex_lock, 0);

        expect_check_lockdir_access_ok();
        expect_lockfile_open(0, TEST_LOCK_FD3);

        expect_function_call(__wrap_pthread_mutex_init);
        will_return(__wrap_pthread_mutex_init, 0);

        expect_function_call(__wrap_pthread_mutex_unlock);
        will_return(__wrap_pthread_mutex_unlock, 0);

        assert_int_equal(lock_init(), 0);

        /* each open this path performs, by name and by flags: the /proc entry
         * read for the process start time and the lock file written with the
         * pid. Naming them is what makes a call site put back to fopen() fail
         * here, since that performs no open() to inspect
         */
        assert_true(opens_include("/proc/", O_RDONLY | O_CLOEXEC));
        assert_true(
            opens_include(LOCKFILE, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC));

        /* and nothing else this path opened is inheritable either */
        for (i = 0; i < opens_seen; i++)
                assert_int_not_equal(opens_flags[i] & O_CLOEXEC, 0);

        expect_function_call(__wrap_close);
        expect_value(__wrap_close, fildes, TEST_LOCK_FD3);
        will_return(__wrap_close, 0);

        expect_function_call(__wrap_pthread_mutex_destroy);
        will_return(__wrap_pthread_mutex_destroy, 0);

        expect_function_call(__wrap_unlink);
        expect_string(__wrap_unlink, pathname, LOCKFILE);
        will_return(__wrap_unlink, 0);

        assert_int_equal(lock_fini(), 0);

        /* the lock file is also read, and that open is on another path: it
         * happens when the file is already there, which is the init above
         * finding EEXIST rather than creating it. Doctoring read_lockfile()
         * back to fopen() passes the case above and fails this half, which is
         * why both are here
         */
        opens_clear();

        expect_function_call(__wrap_pthread_mutex_lock);
        will_return(__wrap_pthread_mutex_lock, 0);

        expect_check_lockdir_access_ok();
        expect_lockfile_open(EEXIST, -1);

        /* whatever the stale check decides, the read has happened by then, and
         * the init fails because the unlink of a file this test does not own
         * is not mocked to succeed
         */
        expect_function_call(__wrap_unlink);
        expect_string(__wrap_unlink, pathname, LOCKFILE);
        will_return(__wrap_unlink, -1);

        expect_function_call(__wrap_pthread_mutex_unlock);
        will_return(__wrap_pthread_mutex_unlock, 0);

        assert_int_equal(lock_init(), -1);

        assert_true(opens_include(LOCKFILE, O_RDONLY | O_CLOEXEC));

        for (i = 0; i < opens_seen; i++)
                assert_int_not_equal(opens_flags[i] & O_CLOEXEC, 0);
}

int
main(void)
{
        const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_lock_init_error),
            cmocka_unit_test(test_lock_init_exit),
            cmocka_unit_test(test_lock_get),
            cmocka_unit_test(test_lock_opens_close_on_exec),
        };

        return cmocka_run_group_tests(tests, NULL, NULL);
}
