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
#include <inttypes.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#ifdef __linux__
/* prctl(PR_GET_NAME) and PR_SET_NAME are Linux only, and so are the two cases
 * that rename this process to ask what a name with a space or a newline in it
 * does to the /proc record. The tree has a FreeBSD port, where this header does
 * not exist - the other library tests guard what only Linux has the same way.
 */
#include <sys/prctl.h>
#endif
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

/* The directory the lock is in, and the name of the probe file beside it. Both
 * are written out here rather than derived, because what the cases check is
 * that lib/lock.c derives them from LOCKFILE and does not name a directory of
 * its own: this binary is built with -DLOCKFILE="/tmp/libpqos_test_lockfile",
 * so a hardcoded /var/lock in the library fails every case that arms the
 * publishing - the write test would ask access() about a directory the lock is
 * not in, and link() would be crossing from one directory to another, which is
 * EXDEV when they are on different filesystems. FreeBSD's /var/tmp lock file is
 * that case.
 */
#define TEST_LOCKDIR "/tmp"
/* The probe file's name is a mkstemp() template now, so a case cannot name the
 * file: what it can require is the template, which is what says the name is not
 * one a local user could have created first.
 */
#define TEST_PROBE_TEMPLATE "/tmp/pqos-probe-XXXXXX"

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

/* what an unlink that fails leaves in errno. lock.c reads it to decide
 * whether what it could not unlink is a directory to remove instead, so a
 * case that arms a failure has to say which failure it was
 */
static int unlink_errno;

int
__wrap_unlink(const char *pathname)
{
        int ret;

        function_called();
        check_expected_ptr(pathname);
        ret = mock_type(int);
        if (ret != 0)
                errno = unlink_errno;

        return ret;
}

/* The probe file the directory check makes. The template is what the case
 * asserts on; the name mkstemp() would have chosen is not interesting, and
 * making one up here would be a name a case could predict - which is the thing
 * this is all about.
 */
/* lstat() is wrapped, and lib/common.c uses it for its own symlink checks, so
 * the mock below hands those through to the real one
 */
int __real_lstat(const char *pathname, struct stat *statbuf);

/* through a typedef, because checkpatch reads the plain declaration as an
 * extern in a .c file and NEW_TYPEDEFS is what this tree chose to ignore
 * instead
 */
typedef int fstat_fn(int fd, struct stat *statbuf);
fstat_fn __real_fstat;

/* The descriptor the mocked open() last handed lock.c for the lock file. What
 * fstat() is asked about is a descriptor and not a name now, and the number is
 * a tmpfile's rather than one a case can choose - so the mock records it.
 */
static int handed_lock_fd = -1;

/* the errno an open of the lock file is to fail with, or 0 for an open that
 * behaves. ELOOP is what O_NOFOLLOW gives for a symbolic link, which is how a
 * link at that name is now refused.
 */
static int lock_open_errno;
/* and what an open of a /proc entry fails with, 0 for one that is performed: a
 * holder's start time that cannot be read is not a holder that has gone. The
 * path narrows it to one entry, because this process reads its own record on
 * the way to publishing a lock and a case about another process's record must
 * leave that one alone; empty means every /proc entry.
 */
static int proc_open_errno;
static char proc_fail_path[64];
/* A /proc record that arrives cut short with the stream's error indicator set,
 * which fread() may do on one and the same call. NULL for a read that is
 * performed. It applies to the entry proc_fail_path names, found by the
 * descriptor the open of it returned - this process reads its own record on the
 * way to publishing a lock, and a case about another process's must leave that
 * one alone.
 */
static const char *proc_short_record;
static int proc_short_fd = -1;
static FILE *proc_short_stream;
/* whether that short record comes with the stream's error indicator set, which
 * is what fread() may do on one call - a case about a record that is merely
 * malformed clears it
 */
static int proc_short_is_error = 1;

/** the last thing lock.c wrote to stderr */
static char last_stderr[256];

/** a record as this library writes it: pid, start time and version 2 */
#define TEST_RECORD "%d %" PRIu64 " 2\n"

typedef size_t fread_fn(void *ptr, size_t size, size_t n, FILE *stream);
fread_fn __real_fread;
typedef int ferror_fn(FILE *stream);
ferror_fn __real_ferror;

size_t
__wrap_fread(void *ptr, size_t size, size_t n, FILE *stream)
{
        if (proc_short_record != NULL && proc_short_fd >= 0 &&
            fileno(stream) == proc_short_fd) {
                size_t length = strlen(proc_short_record);

                /* lock.c reads single bytes, so the count is the length */
                assert_int_equal(size, 1);
                assert_true(length <= n);
                memcpy(ptr, proc_short_record, length);
                proc_short_stream = stream;

                return length;
        }

        return __real_fread(ptr, size, n, stream);
}

int
__wrap_ferror(FILE *stream)
{
        if (proc_short_record != NULL && stream == proc_short_stream)
                return proc_short_is_error;

        return __real_ferror(stream);
}

/** the descriptor mkostemp() last handed out, which is the record's */
static int record_fd = -1;

int
__wrap_mkostemp(char *template, int flags)
{
        function_called();
        check_expected_ptr(template);
        check_expected(flags);

        errno = mock_type(int);
        record_fd = mock_type(int);

        return record_fd;
}

/* The three calls that publish the lock file: the record is written into a file
 * of its own, made readable, and given the lock's name with link(). The
 * descriptor is a number the cases invent, so write() and fchmod() on it are
 * answered rather than performed - every other descriptor is handed through,
 * which cmocka's own output depends on.
 */
typedef ssize_t write_fn(int fd, const void *buf, size_t count);
write_fn __real_write;
typedef ssize_t read_fn(int fd, void *buf, size_t count);
read_fn __real_read;

/* The most a read hands back at a time, 0 being as much as the file has. A read
 * is allowed to return fewer bytes than it was asked for, and lock.c has to
 * assemble the record rather than parse the first answer: the only reader here
 * is read_lockfile(), since the /proc record is read with fread(), which does
 * its own assembling inside the C library.
 */
static size_t read_chunk;
/* and what a read of the lock file fails with, 0 for a read that is performed:
 * a failure to read is not a record saying nothing, and the two have different
 * answers
 */
static int read_errno;

ssize_t
__wrap_read(int fd, void *buf, size_t count)
{
        if (read_errno != 0 && fd == handed_lock_fd) {
                errno = read_errno;

                return -1;
        }

        if (read_chunk > 0 && count > read_chunk)
                count = read_chunk;

        return __real_read(fd, buf, count);
}
typedef int fchmod_fn(int fd, mode_t mode);
fchmod_fn __real_fchmod;

/** the last record written to the lock file */
static char written_record[128];

ssize_t
__wrap_write(int fd, const void *buf, size_t count)
{
        if (fd != record_fd || record_fd < 0)
                return __real_write(fd, buf, count);

        function_called();

        /* kept, for the case that asks what the record says */
        snprintf(written_record, sizeof(written_record), "%.*s",
                 (int)(count < sizeof(written_record)
                           ? count
                           : sizeof(written_record) - 1),
                 (const char *)buf);

        /* as many bytes as it was given: what the cases are about is that the
         * record is written before the name exists, not how long it is
         */
        return (ssize_t)count;
}

/** the mode the lock file was published with, for the case that asks what the
 *  mode permits rather than what it is
 */
static mode_t published_mode = 0;

int
__wrap_fchmod(int fd, mode_t mode)
{
        if (fd != record_fd || record_fd < 0)
                return __real_fchmod(fd, mode);

        function_called();
        check_expected(mode);
        published_mode = mode;

        return mock_type(int);
}

int
__wrap_link(const char *from, const char *to)
{
        (void)from;

        function_called();
        check_expected_ptr(to);

        errno = mock_type(int);

        return mock_type(int);
}

/* rmdir() is what clears a directory somebody left at the lock file's name,
 * which unlink() cannot remove
 */
int
__wrap_rmdir(const char *pathname)
{
        function_called();
        check_expected_ptr(pathname);

        errno = mock_type(int);

        return mock_type(int);
}

/* What sits at the lock file's name, for the question lock.c asks of it. The
 * owner is set by the cases that describe a file somebody else wrote, even
 * though lock.c does not consult it: a case that sets it and is still refused
 * is how "the owner does not decide this" is asserted.
 */
static uid_t stat_uid = 0;
/* the type symbolically and the permission bits in octal, which is how this
 * tree writes each: S_ISREG() and S_ISLNK() are what lock.c and common.c ask
 * the type with, and 0644 is LOCKFILE_PERMS - the mode lock_init() publishes
 * the lock file with - so the mode here is one the filesystem could really hand
 * back rather than a type bit with no permissions at all
 */
static mode_t stat_mode = S_IFREG | 0644;
static int stat_ret = 0;
/** what a refused fstat() fails with */
static int stat_errno = ENOENT;
/** what the close of the lock file's descriptor fails with, 0 for a real close:
 *  a close that leaves an errno of its own must not be what the caller is told
 */
static int close_errno;

int
__wrap_lstat(const char *pathname, struct stat *statbuf)
{
        /* lock.c no longer asks lstat() about the lock file - it opens it once
         * and asks fstat() about that descriptor - but lib/common.c uses
         * lstat() for its own symlink checks, so this hands those through
         */
        return __real_lstat(pathname, statbuf);
}

/* What fstat() says about the descriptor lock.c opened. The lock file's own
 * descriptor is the one the cases arm; every other descriptor - the /proc entry
 * behind the liveness check, a stream of the harness's own - is handed through.
 */
int
__wrap_fstat(int fd, struct stat *statbuf)
{
        if (fd != handed_lock_fd)
                return __real_fstat(fd, statbuf);

        if (stat_ret != 0) {
                errno = stat_errno;
                return stat_ret;
        }

        memset(statbuf, 0, sizeof(*statbuf));
        statbuf->st_uid = stat_uid;
        statbuf->st_mode = stat_mode;

        return 0;
}

/* Who the caller is. Nothing in lock.c consults it - that is the point - so it
 * is held at root here: a case describing a lock file another user wrote and
 * still being refused is what says a root caller does not decide the question
 * by ownership. Wrapped rather than left to the real geteuid() because the
 * suite does not run as root, and then the ownership question could never
 * arise.
 */
static uid_t euid = 0;

uid_t
__wrap_geteuid(void)
{
        return euid;
}

/* A descriptor holding the lock file content a case wants read back, or -1 */
static int content_fd = -1;

/**
 * @brief A file holding one line, for lock.c to read as the lock file
 *
 * @param [in] text the line
 *
 * @return the descriptor, positioned at the start
 */
static int
file_holding(const char *text)
{
        /* An anonymous file: nothing to name, so nothing for a case to predict
         * and nothing to remove afterwards - which also keeps this helper away
         * from the mkostemp() and unlink() mocks that are here for lock.c.
         *
         * A descriptor of its own, and the stream closed here. lock.c closes
         * the descriptor it is given, so a stream left open on the same one
         * would close it a second time when the process ended - onto whatever
         * number had been reused by then. dup() makes the two independent.
         */
        FILE *fp = tmpfile();
        int fd;

        assert_non_null(fp);
        assert_true(fwrite(text, 1, strlen(text), fp) == strlen(text));
        assert_int_equal(fflush(fp), 0);

        fd = dup(fileno(fp));
        assert_true(fd >= 0);
        assert_int_equal(fclose(fp), 0);
        lseek(fd, 0, SEEK_SET);

        return fd;
}

/**
 * @brief The start time of this process, read the way /proc states it
 *
 * Field 22 of /proc/self/stat, counted as the kernel documents it, and read
 * here without the arithmetic lock.c uses - so a case built on this asserts
 * what lock.c should have read rather than repeating how it reads it.
 *
 * @return the start time
 */
#ifdef __linux__
/* This process's own start time, read the way lock.c reads it. /proc/<pid>/stat
 * field 22 is a Linux interface, and so is every case below that needs a holder
 * the library will find alive: on a platform that publishes no such record
 * get_process_start_time() answers "there is none", nothing is ever recognised
 * as a live holder, and the premise of those cases cannot be built at all. They
 * are fenced one by one rather than moved together, so each stays beside the
 * cases it belongs with.
 */
static uint64_t
start_time_of(pid_t pid)
{
        char path[64];
        char buf[4096];
        char *p;
        int i, fd;
        ssize_t got;
        uint64_t value = 0;

        snprintf(path, sizeof(path), "/proc/%d/stat", (int)pid);
        fd = open(path, O_RDONLY);

        assert_true(fd >= 0);
        got = read(fd, buf, sizeof(buf) - 1);
        close(fd);
        assert_true(got > 0);
        buf[got] = '\0';

        /* the second field is the command in brackets and may hold spaces, so
         * the walk starts after the closing bracket
         */
        p = strrchr(buf, ')');
        assert_non_null(p);
        p += 2;

        /* p is now at field 3; the start time is field 22 */
        for (i = 3; i < 22; i++) {
                p = strchr(p, ' ');
                assert_non_null(p);
                p++;
        }
        assert_true(sscanf(p, "%" SCNu64, &value) == 1);

        return value;
}

static uint64_t
own_start_time(void)
{
        return start_time_of(getpid());
}

/**
 * @brief A child of this process, alive until it is stopped
 *
 * A holder the library will find alive, and one whose /proc entry the user
 * running these cases can read: pid 1 belongs to another user, and the mounts
 * these cases are about - hidepid among them - are exactly the ones where
 * reading its entry fails, so a case that modelled such a mount by arming a
 * failure for /proc/1/stat would have failed reading it for real first.
 *
 * @return the child's pid
 */
/** the child a case started, for the teardown to collect: a cmocka assertion
 * leaves the case by a long jump, so a case cannot be relied on to reach its
 * own kill - and a child that pauses for ever would outlive the whole run
 */
static pid_t case_child = -1;

static pid_t
start_a_child(void)
{
        pid_t child = fork();

        if (child == 0) {
                /* nothing cmocka knows about happens on this side: it waits to
                 * be killed, and an assertion here would be on the wrong stack
                 */
                for (;;)
                        pause();

                _exit(0);
        }

        assert_true(child > 0);
        case_child = child;

        return child;
}

/**
 * @brief Stops the child and collects it
 *
 * @param [in] child the pid start_a_child() returned
 */
static void
stop_a_child(pid_t child)
{
        assert_int_equal(kill(child, SIGKILL), 0);
        assert_true(waitpid(child, NULL, 0) == child);

        /* cleared last, so that a kill or a wait which fails leaves the child
         * for the teardown to collect: an assertion above leaves this function
         * by a long jump, and a global already cleared would have told the
         * teardown there was nothing to do
         */
        case_child = -1;
}
#endif /* __linux__ */

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

        /* Only the open the cases arm is mocked: the lock file taken
         * exclusively. The directory check makes its probe with mkstemp(),
         * which has a mock of its own. Every other open goes through, which
         * includes the read-only open of the lock file that read_lockfile()
         * does and the /proc read behind is_pid_alive() - neither is what a
         * case here arms, and asserting on them made the mock a description of
         * lock.c's internals rather than of what these cases are about.
         */
        /* A case that wants lock.c to read a particular lock file content
         * hands this the descriptor of a file it wrote itself, so the content
         * is the case's and the path stays lock.c's. Without it the read below
         * would read whatever /var/lock/libpqos happens to hold on the machine
         * running the tests.
         */
        if (proc_open_errno != 0 && strncmp(path, "/proc/", 6) == 0 &&
            (proc_fail_path[0] == '\0' || strcmp(path, proc_fail_path) == 0)) {
                errno = proc_open_errno;

                return -1;
        }

        /* the entry a short read is armed for: opened for real, and the
         * descriptor remembered so that the read of it can be told from this
         * process's read of its own record
         */
        if (proc_short_record != NULL && proc_fail_path[0] != '\0' &&
            strcmp(path, proc_fail_path) == 0) {
                proc_short_fd = __real_open(path, oflags, mode);

                /* and a stale errno behind it, which is what the real sequence
                 * leaves: the link() that brought lock_init() here failed with
                 * EEXIST, and nothing between there and the read is required to
                 * clear it. A read that reports an error without setting one of
                 * its own then has this reported in its place.
                 */
                errno = EEXIST;

                return proc_short_fd;
        }

        if (strcmp(path, LOCKFILE) == 0 && (oflags & O_ACCMODE) == O_RDONLY) {
                int fd;

                /* an open that refuses, which is how O_NOFOLLOW answers a
                 * symbolic link at this name
                 */
                if (lock_open_errno != 0) {
                        errno = lock_open_errno;
                        return -1;
                }

                if (content_fd < 0)
                        return __real_open(path, oflags, mode);

                fd = content_fd;
                content_fd = -1;
                handed_lock_fd = fd;
                lseek(fd, 0, SEEK_SET);

                return fd;
        }

        int mocked = (strcmp(path, LOCKFILE) == 0 &&
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
        /* a close that fails, which is the one thing that can overwrite the
         * errno the caller is about to print: armed for the descriptor the lock
         * file was handed on, and the file itself is closed for real so nothing
         * leaks
         */
        if (close_errno != 0 && fildes == handed_lock_fd) {
                (void)__real_close(fildes);
                errno = close_errno;

                return -1;
        }

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
        if (stream == stderr) {
                va_list args;

                /* kept rather than discarded: what lock.c says about a failure
                 * is part of what a case can require of it - a message ending
                 * in "Success" is a -1 with no errno behind it
                 */
                va_start(args, format);
                vsnprintf(last_stderr, sizeof(last_stderr), format, args);
                va_end(args);

                return 0;
        }

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

        expect_function_call(__wrap_mkostemp);
        expect_string(__wrap_mkostemp, template, TEST_PROBE_TEMPLATE);
        expect_value(__wrap_mkostemp, flags, O_CLOEXEC);
        will_return(__wrap_mkostemp, 0);
        will_return(__wrap_mkostemp, TEST_TMP_FD);

        expect_function_call(__wrap_close);
        expect_value(__wrap_close, fildes, TEST_TMP_FD);
        will_return(__wrap_close, 0);

        expect_function_call(__wrap_unlink);
        expect_string(__wrap_unlink, pathname, TEST_PROBE_TEMPLATE);
        will_return(__wrap_unlink, 0);
}

static void
expect_lockfile_open(int err, int fd)
{
        /* The lock file is published by linking a file that already holds the
         * record, so "creating it" is four calls and not one: a temporary file,
         * the record written to it, the permissions every other caller needs to
         * read it, and the link that gives it the name. The link is where the
         * name being taken shows up, so it carries the error this helper is
         * given - and the descriptor the caller asks for is the temporary
         * file's, because a link does not make a second object.
         */
        expect_function_call(__wrap_mkostemp);
        expect_string(__wrap_mkostemp, template, TEST_PROBE_TEMPLATE);
        expect_value(__wrap_mkostemp, flags, O_CLOEXEC);
        will_return(__wrap_mkostemp, 0);
        will_return(__wrap_mkostemp, fd >= 0 ? fd : TEST_LOCK_FD);

        expect_function_call(__wrap_write);

        expect_function_call(__wrap_fchmod);
        expect_value(__wrap_fchmod, mode, 0644);
        will_return(__wrap_fchmod, 0);

        expect_function_call(__wrap_link);
        expect_string(__wrap_link, to, LOCKFILE);
        will_return(__wrap_link, err);
        will_return(__wrap_link, err == 0 ? 0 : -1);

        if (err != 0) {
                /* the name was taken, so what was made for it goes */
                expect_function_call(__wrap_close);
                expect_value(__wrap_close, fildes, fd >= 0 ? fd : TEST_LOCK_FD);
                will_return(__wrap_close, 0);
        }

        expect_function_call(__wrap_unlink);
        expect_string(__wrap_unlink, pathname, TEST_PROBE_TEMPLATE);
        will_return(__wrap_unlink, 0);
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

        /* The one open this path performs by name: the /proc entry read for the
         * process start time. Naming it is what makes a call site put back to
         * fopen() fail here, since that performs no open() to inspect.
         *
         * The lock file is not among them any more - it is made by mkostemp()
         * and the record goes through that descriptor, so nothing opens the
         * lock file's name to write it.
         */
        assert_true(opens_include("/proc/", O_RDONLY | O_CLOEXEC));

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

        /* a stale record of this case's own, and not whatever a file of this
         * name on the host holds: one naming a live process would be held,
         * and the unlink below never asked for
         */
        content_fd = file_holding("0 0\n");

        /* the read has happened by then, and the init fails because the unlink
         * is not mocked to succeed
         */
        expect_function_call(__wrap_unlink);
        expect_string(__wrap_unlink, pathname, LOCKFILE);
        will_return(__wrap_unlink, -1);

        expect_function_call(__wrap_pthread_mutex_unlock);
        will_return(__wrap_pthread_mutex_unlock, 0);

        assert_int_equal(lock_init(), -1);

        /* the read of an existing lock file, with every flag that open carries:
         * O_NOFOLLOW so a symbolic link at the name is refused rather than
         * followed out of a world-writable directory, O_NONBLOCK so a fifo left
         * there cannot hold a privileged caller in open() for as long as the
         * attacker likes, and O_CLOEXEC like every other open here
         */
        assert_true(opens_include(LOCKFILE, O_RDONLY | O_NOFOLLOW | O_NONBLOCK |
                                                O_CLOEXEC));

        for (i = 0; i < opens_seen; i++)
                assert_int_not_equal(opens_flags[i] & O_CLOEXEC, 0);
}

static void
test_lock_probe_file_cannot_be_pre_created(void **state __attribute__((unused)))
{
        /* The directory check makes its probe with a name chosen when it runs.
         * LOCKDIR is world writable on every mainstream Linux, so a fixed name
         * was a name any local user could create first - and the O_EXCL open of
         * it then failed with EEXIST for ever, which stopped every caller of
         * this library, root included.
         */
        expect_function_call(__wrap_pthread_mutex_lock);
        will_return(__wrap_pthread_mutex_lock, 0);

        expect_function_call(__wrap_access);
        expect_string(__wrap_access, pathname, TEST_LOCKDIR);
        expect_value(__wrap_access, mode, R_OK | W_OK | X_OK);
        will_return(__wrap_access, 0);

        /* the template, and not a name: that is the point */
        expect_function_call(__wrap_mkostemp);
        expect_string(__wrap_mkostemp, template, TEST_PROBE_TEMPLATE);
        expect_value(__wrap_mkostemp, flags, O_CLOEXEC);
        will_return(__wrap_mkostemp, EACCES);
        will_return(__wrap_mkostemp, -1);

        expect_function_call(__wrap_pthread_mutex_unlock);
        will_return(__wrap_pthread_mutex_unlock, 0);

        /* a directory that really cannot be written to is still reported */
        assert_int_equal(lock_init(), -1);
}

#ifdef __linux__
static void
test_lock_of_another_owner_is_honoured_while_it_lives(void **state
                                                      __attribute__((unused)))
{
        char line[64];

        /* Nothing in this library requires root - the resctrl and perf paths
         * are the ordinary unprivileged case - so a lock file owned by another
         * user and naming a live process is a session holding the lock, and a
         * root caller has to wait for it like any other. Clearing it because a
         * root caller could not have written it would take the lock off a live
         * holder, and two sessions would drive the same counters believing they
         * each held it.
         *
         * The holder here is this process, so the file names something alive.
         */
        snprintf(line, sizeof(line), TEST_RECORD, (int)getpid(),
                 own_start_time());

        stat_ret = 0;
        stat_uid = 1000;
        stat_mode = S_IFREG | 0644;
        content_fd = file_holding(line);

        expect_function_call(__wrap_pthread_mutex_lock);
        will_return(__wrap_pthread_mutex_lock, 0);

        expect_check_lockdir_access_ok();
        expect_lockfile_open(EEXIST, -1);

        expect_function_call(__wrap_pthread_mutex_unlock);
        will_return(__wrap_pthread_mutex_unlock, 0);

        /* refused, and no unlink armed: an attempt to remove the file would
         * fail this case, which is the whole of it
         */
        assert_int_equal(lock_init(), -1);
}
#endif /* __linux__ */

static void
test_stale_lock_of_another_owner_is_cleared(void **state
                                            __attribute__((unused)))
{
        /* and the same file naming nothing alive is cleared, whoever owns it:
         * lock.c does not ask who wrote the file, which is what this case says.
         * PID 0 is never a process, which is what makes this file stale rather
         * than held.
         *
         * Whether the removal is permitted is the directory's business and not
         * this module's - /run/lock is sticky, so a caller that is neither the
         * file's owner nor root cannot unlink it, and the mocked unlink() here
         * stands for a caller that can. Where it is refused, the caller is told
         * to clear the file by hand.
         */
        stat_ret = 0;
        stat_uid = 1000;
        stat_mode = S_IFREG | 0644;
        content_fd = file_holding("0 0\n");

        expect_function_call(__wrap_pthread_mutex_lock);
        will_return(__wrap_pthread_mutex_lock, 0);

        expect_check_lockdir_access_ok();
        expect_lockfile_open(EEXIST, -1);

        expect_function_call(__wrap_unlink);
        expect_string(__wrap_unlink, pathname, LOCKFILE);
        will_return(__wrap_unlink, 0);

        expect_lockfile_open(0, TEST_LOCK_FD2);

        expect_function_call(__wrap_pthread_mutex_init);
        will_return(__wrap_pthread_mutex_init, 0);
        expect_function_call(__wrap_pthread_mutex_unlock);
        will_return(__wrap_pthread_mutex_unlock, 0);

        assert_int_equal(lock_init(), 0);

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

static void
test_lock_file_that_is_a_directory_is_cleared(void **state
                                              __attribute__((unused)))
{
        /* A directory is one of the things a world writable lock directory lets
         * anybody put at this name, and unlink() cannot remove one: it fails
         * with EISDIR. Without rmdir() beside it every caller was told to
         * remove a stale lock file by hand for as long as the directory was
         * there, and the attacker could recreate it as fast as root removed it
         * - which is the denial of service this change is about, in the one
         * shape a regular file check does not cover.
         */
        stat_uid = 1000;
        stat_mode = S_IFDIR | 0755;
        /* a descriptor for the open to hand over, so that what refuses this is
         * fstat() saying "directory" about the file that was opened - and not
         * an open that happened to fail
         */
        content_fd = file_holding("");

        expect_function_call(__wrap_pthread_mutex_lock);
        will_return(__wrap_pthread_mutex_lock, 0);

        expect_check_lockdir_access_ok();
        expect_lockfile_open(EEXIST, -1);

        /* unlink() refuses it, and rmdir() takes it away */
        unlink_errno = EISDIR;
        expect_function_call(__wrap_unlink);
        expect_string(__wrap_unlink, pathname, LOCKFILE);
        will_return(__wrap_unlink, -1);

        expect_function_call(__wrap_rmdir);
        expect_string(__wrap_rmdir, pathname, LOCKFILE);
        will_return(__wrap_rmdir, 0);
        will_return(__wrap_rmdir, 0);

        expect_lockfile_open(0, TEST_LOCK_FD3);

        expect_function_call(__wrap_pthread_mutex_init);
        will_return(__wrap_pthread_mutex_init, 0);
        expect_function_call(__wrap_pthread_mutex_unlock);
        will_return(__wrap_pthread_mutex_unlock, 0);

        assert_int_equal(lock_init(), 0);

        expect_function_call(__wrap_close);
        expect_value(__wrap_close, fildes, TEST_LOCK_FD3);
        will_return(__wrap_close, 0);
        expect_function_call(__wrap_pthread_mutex_destroy);
        will_return(__wrap_pthread_mutex_destroy, 0);
        unlink_errno = 0;
        expect_function_call(__wrap_unlink);
        expect_string(__wrap_unlink, pathname, LOCKFILE);
        will_return(__wrap_unlink, 0);
        assert_int_equal(lock_fini(), 0);
}

static void
test_lock_file_is_never_published_empty(void **state __attribute__((unused)))
{
        /* Two callers racing to initialise: the first creates the name and the
         * second arrives before the record is in it. That window used to exist
         * - a create, then a write - and a regular file with nothing in it
         * reads as stale, so the second caller removed the first's lock and
         * made its own, leaving both believing they held it.
         *
         * There is no window now, and the order of the calls is what says so:
         * the record is written and the permissions set while the file has only
         * its temporary name, and link() is what publishes it. cmocka checks
         * the order of expectations, so a create that published first would
         * fail here.
         */
        expect_function_call(__wrap_pthread_mutex_lock);
        will_return(__wrap_pthread_mutex_lock, 0);

        expect_check_lockdir_access_ok();
        expect_lockfile_open(0, TEST_LOCK_FD);

        expect_function_call(__wrap_pthread_mutex_init);
        will_return(__wrap_pthread_mutex_init, 0);
        expect_function_call(__wrap_pthread_mutex_unlock);
        will_return(__wrap_pthread_mutex_unlock, 0);

        assert_int_equal(lock_init(), 0);

        /* and the name a second caller would have found taken: link() answering
         * EEXIST is that caller, and it leaves the first lock alone
         */
        expect_function_call(__wrap_close);
        expect_value(__wrap_close, fildes, TEST_LOCK_FD);
        will_return(__wrap_close, 0);
        expect_function_call(__wrap_pthread_mutex_destroy);
        will_return(__wrap_pthread_mutex_destroy, 0);
        expect_function_call(__wrap_unlink);
        expect_string(__wrap_unlink, pathname, LOCKFILE);
        will_return(__wrap_unlink, 0);
        assert_int_equal(lock_fini(), 0);
}

static void
test_lock_file_is_published_read_only_to_other_users(void **state
                                                     __attribute__((unused)))
{
        /* What the mode has to permit, which is not the same question as which
         * mode it is. LOCKDIR is world writable, so a lock file any local user
         * may write is not a lock: the holder is identified by the record, and
         * a user who overwrites a live one has it read as a dead holder - after
         * which the next caller removes it and runs beside the live session.
         *
         * Read for everybody is the other half, and it is needed for the
         * opposite reason: a caller that cannot read the record is told the
         * lock cannot be examined and refuses to clear it, for ever.
         */
        expect_function_call(__wrap_pthread_mutex_lock);
        will_return(__wrap_pthread_mutex_lock, 0);

        expect_check_lockdir_access_ok();
        expect_lockfile_open(0, TEST_LOCK_FD);

        expect_function_call(__wrap_pthread_mutex_init);
        will_return(__wrap_pthread_mutex_init, 0);
        expect_function_call(__wrap_pthread_mutex_unlock);
        will_return(__wrap_pthread_mutex_unlock, 0);

        published_mode = 0;
        assert_int_equal(lock_init(), 0);

        assert_int_equal(published_mode & 0022, 0);
        assert_int_equal(published_mode & 0444, 0444);
        assert_int_equal(published_mode & 0200, 0200);

        expect_function_call(__wrap_close);
        expect_value(__wrap_close, fildes, TEST_LOCK_FD);
        will_return(__wrap_close, 0);
        expect_function_call(__wrap_pthread_mutex_destroy);
        will_return(__wrap_pthread_mutex_destroy, 0);
        expect_function_call(__wrap_unlink);
        expect_string(__wrap_unlink, pathname, LOCKFILE);
        will_return(__wrap_unlink, 0);
        assert_int_equal(lock_fini(), 0);
}

static void
test_lock_file_that_cannot_be_examined_is_not_cleared(void **state
                                                      __attribute__((unused)))
{
        /* An open that fails for a reason which says nothing about the file -
         * this process out of descriptors, say - is not evidence that the lock
         * is stale. Treating it as such would unlink the marker of a session
         * that really is holding the lock, and a third process could then start
         * beside it. So the call refuses and the file is left alone: no unlink
         * is armed below, and an attempt to remove it fails this case.
         */
        lock_open_errno = EMFILE;

        expect_function_call(__wrap_pthread_mutex_lock);
        will_return(__wrap_pthread_mutex_lock, 0);

        expect_check_lockdir_access_ok();
        expect_lockfile_open(EEXIST, -1);

        expect_function_call(__wrap_pthread_mutex_unlock);
        will_return(__wrap_pthread_mutex_unlock, 0);

        assert_int_equal(lock_init(), -1);
}

static void
test_a_stale_file_that_cannot_be_removed_says_why(void **state
                                                  __attribute__((unused)))
{
        /* An unprivileged caller meeting another user's stale file in a sticky
         * directory: unlink() refuses it with EPERM, and the rmdir() that
         * follows - which is there for a directory at the name - answers
         * ENOTDIR, because the name is a file. What the caller is told has to
         * be the first: EPERM is why it could not be removed, where ENOTDIR is
         * only how this found out that the fallback does not apply, and a
         * message naming it sends whoever reads it after a directory that is
         * not there.
         */
        content_fd = file_holding("0 0\n");

        expect_function_call(__wrap_pthread_mutex_lock);
        will_return(__wrap_pthread_mutex_lock, 0);

        expect_check_lockdir_access_ok();
        expect_lockfile_open(EEXIST, -1);

        unlink_errno = EPERM;
        expect_function_call(__wrap_unlink);
        expect_string(__wrap_unlink, pathname, LOCKFILE);
        will_return(__wrap_unlink, -1);

        expect_function_call(__wrap_rmdir);
        expect_string(__wrap_rmdir, pathname, LOCKFILE);
        will_return(__wrap_rmdir, ENOTDIR);
        will_return(__wrap_rmdir, -1);

        expect_function_call(__wrap_pthread_mutex_unlock);
        will_return(__wrap_pthread_mutex_unlock, 0);

        assert_int_equal(lock_init(), -1);
        assert_non_null(strstr(last_stderr, strerror(EPERM)));
        assert_null(strstr(last_stderr, strerror(ENOTDIR)));
}

static void
test_a_lock_that_cannot_be_examined_says_why(void **state
                                             __attribute__((unused)))
{
        /* The file opens and fstat() refuses it - EACCES here - and the close
         * that follows fails with something else. What the caller is told has
         * to be the first: the second says nothing about the lock, and a
         * message naming it sends whoever reads it after the wrong thing.
         */
        content_fd = file_holding("4321 1234\n");
        stat_ret = -1;
        stat_errno = EACCES;
        close_errno = EIO;

        expect_function_call(__wrap_pthread_mutex_lock);
        will_return(__wrap_pthread_mutex_lock, 0);

        expect_check_lockdir_access_ok();
        expect_lockfile_open(EEXIST, -1);

        expect_function_call(__wrap_pthread_mutex_unlock);
        will_return(__wrap_pthread_mutex_unlock, 0);

        /* refused, and no unlink armed: the lock stays */
        assert_int_equal(lock_init(), -1);
        assert_non_null(strstr(last_stderr, strerror(EACCES)));
        assert_null(strstr(last_stderr, strerror(EIO)));
}

#ifdef __linux__
static void
test_live_holder_whose_record_arrives_in_pieces(void **state
                                                __attribute__((unused)))
{
        char line[64];

        /* The same live holder, with the record handed over one byte at a time.
         * A read may return fewer bytes than it was asked for - a signal
         * arriving after some have been transferred is one of the ways - and
         * parsing the first answer sees "5" where the record says "5123 987":
         * one field instead of two, which read_lockfile() reports as no record
         * at all. That is what lock_init() treats as stale, so a live holder's
         * lock would be removed and a second session would take it.
         */
        snprintf(line, sizeof(line), TEST_RECORD, (int)getpid(),
                 own_start_time());

        content_fd = file_holding(line);
        read_chunk = 1;

        expect_function_call(__wrap_pthread_mutex_lock);
        will_return(__wrap_pthread_mutex_lock, 0);

        expect_check_lockdir_access_ok();
        expect_lockfile_open(EEXIST, -1);

        expect_function_call(__wrap_pthread_mutex_unlock);
        will_return(__wrap_pthread_mutex_unlock, 0);

        /* refused, and no unlink armed - so removing the holder's lock fails
         * this case rather than passing it
         */
        assert_int_equal(lock_init(), -1);
}
#endif /* __linux__ */

#ifdef __linux__
static void
test_holder_whose_start_time_cannot_be_read_is_kept(void **state
                                                    __attribute__((unused)))
{
        char line[64];
        pid_t child;

        /* A record naming another live process - a child of this one - and the
         * /proc entry that says whether that process is the one which took the
         * lock cannot be read: EIO here, and an unreadable /proc, a hidepid
         * mount or this process being out of descriptors are the same answer.
         * "Could not be read" is not "has gone", so the lock stays.
         *
         * One value for both read a failure to look as a dead holder and
         * removed a live session's lock.
         *
         * Another process and not this one, because lock.c reads its own record
         * first on the way to publishing a lock: a failure armed for every
         * /proc entry is the case below, where that read is the one that fails.
         */
        child = start_a_child();
        snprintf(line, sizeof(line), TEST_RECORD, (int)child,
                 start_time_of(child));
        snprintf(proc_fail_path, sizeof(proc_fail_path), "/proc/%d/stat",
                 (int)child);

        content_fd = file_holding(line);
        proc_open_errno = EIO;

        expect_function_call(__wrap_pthread_mutex_lock);
        will_return(__wrap_pthread_mutex_lock, 0);

        expect_check_lockdir_access_ok();
        expect_lockfile_open(EEXIST, -1);

        expect_function_call(__wrap_pthread_mutex_unlock);
        will_return(__wrap_pthread_mutex_unlock, 0);

        /* refused, and no unlink armed, so removing it fails this case */
        assert_int_equal(lock_init(), -1);

        stop_a_child(child);
}

static void
test_holder_hidden_from_proc_is_kept(void **state __attribute__((unused)))
{
        char line[64];
        pid_t child;

        /* The ENOENT a /proc mounted with hidepid=2 gives for a live process
         * this caller may not see. No entry is not no process, and a holder
         * read as gone on the strength of it has its lock removed while it
         * runs. The kernel says the child is there, so the lock stays and the
         * caller is told it could not look.
         */
        child = start_a_child();
        snprintf(line, sizeof(line), TEST_RECORD, (int)child,
                 start_time_of(child));
        snprintf(proc_fail_path, sizeof(proc_fail_path), "/proc/%d/stat",
                 (int)child);

        content_fd = file_holding(line);
        proc_open_errno = ENOENT;

        expect_function_call(__wrap_pthread_mutex_lock);
        will_return(__wrap_pthread_mutex_lock, 0);

        expect_check_lockdir_access_ok();
        expect_lockfile_open(EEXIST, -1);

        expect_function_call(__wrap_pthread_mutex_unlock);
        will_return(__wrap_pthread_mutex_unlock, 0);

        /* refused, and no unlink armed, so removing it fails this case */
        assert_int_equal(lock_init(), -1);
        assert_non_null(strstr(last_stderr, strerror(EACCES)));

        stop_a_child(child);
}

static void
test_holder_whose_record_is_cut_short_is_kept(void **state
                                              __attribute__((unused)))
{
        char line[64];
        pid_t child;

        /* The holder's /proc entry opens and reads, and the read hands back
         * part of the record with the stream's error indicator set - one call
         * may do both. What arrives is a prefix of field 22, which parses as a
         * number: so the start time read is not the holder's, the comparison
         * says this is somebody else, and the lock is removed from a live
         * session.
         *
         * The indicator is what says the record is incomplete, and asking the
         * count alone cannot: a short read that is not an error is a record
         * this cannot tell from a complete one.
         */
        child = start_a_child();
        snprintf(line, sizeof(line), TEST_RECORD, (int)child,
                 start_time_of(child));
        snprintf(proc_fail_path, sizeof(proc_fail_path), "/proc/%d/stat",
                 (int)child);

        content_fd = file_holding(line);
        /* a prefix of a plausible record: the name closed, then a field 22 that
         * is a number and the wrong one
         */
        proc_short_record = "1 (child) S 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 "
                            "999";

        expect_function_call(__wrap_pthread_mutex_lock);
        will_return(__wrap_pthread_mutex_lock, 0);

        expect_check_lockdir_access_ok();
        expect_lockfile_open(EEXIST, -1);

        expect_function_call(__wrap_pthread_mutex_unlock);
        will_return(__wrap_pthread_mutex_unlock, 0);

        /* refused, and no unlink armed, so removing it fails this case */
        assert_int_equal(lock_init(), -1);
        /* and it says the read failed rather than repeating what the link()
         * that brought us here left behind: a stream that reports an error
         * without setting an errno of its own is this mock, and the message
         * would have said "File exists"
         */
        assert_non_null(strstr(last_stderr, strerror(EIO)));
        assert_null(strstr(last_stderr, strerror(EEXIST)));

        stop_a_child(child);
}

static void
test_a_start_time_above_32_bits_is_carried(void **state __attribute__((unused)))
{
        char line[64];
        pid_t child;
        const uint64_t big = 5000000000ULL; /* above 2^32 */

        /* /proc states field 22 as an unsigned long long, and a machine up long
         * enough has a start time that does not fit in 32 of them - about 50
         * days at 1000 Hz. This tree builds for __i386__, where a long is 32
         * bits, so the record is carried as uint64_t and written and read as
         * one.
         *
         * The case pins that width end to end: the lock file names a holder
         * with a start time above 2^32 and the /proc record states the same, so
         * the holder is recognised and the lock is left alone. It cannot fail
         * on a 64-bit host, where a long holds the value either way - what it
         * is for is the formats and the types, which a 32-bit build would
         * otherwise lose.
         */
        child = start_a_child();
        snprintf(line, sizeof(line), TEST_RECORD, (int)child, big);
        snprintf(proc_fail_path, sizeof(proc_fail_path), "/proc/%d/stat",
                 (int)child);

        content_fd = file_holding(line);
        /* fields 3 to 22, the last of them the start time: the walk skips 19
         * delimiters after the name, so the value has to be the twentieth token
         */
        proc_short_record = "1 (child) S 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 "
                            "5000000000";
        proc_short_is_error = 0;

        expect_function_call(__wrap_pthread_mutex_lock);
        will_return(__wrap_pthread_mutex_lock, 0);

        expect_check_lockdir_access_ok();
        expect_lockfile_open(EEXIST, -1);

        expect_function_call(__wrap_pthread_mutex_unlock);
        will_return(__wrap_pthread_mutex_unlock, 0);

        /* held, so refused, and no unlink armed */
        assert_int_equal(lock_init(), -1);

        stop_a_child(child);
}

static void
test_a_forged_record_is_not_a_lock(void **state __attribute__((unused)))
{
        /* Records any local user can write into a world writable directory, and
         * none of them is one this library wrote. scanf's integer conversions
         * are undefined where the value does not fit the destination, so what
         * the first two of these used to do is not defined by C at all - and
         * the fourth is the quiet one: strtoull() takes "-1" and hands back
         * UINT64_MAX, which would then have been compared against a start time
         * as if the file had said it.
         *
         * Each is read as a lock nobody holds, so it is cleared and this caller
         * takes the lock.
         */
        static const char *const forged[] = {
            "99999999999999999999 1\n", /* a pid past any integer */
            "1 99999999999999999999\n", /* a start time past 64 bits */
            "-1 1\n",                   /* no process */
            "1 -1\n",                   /* a start time that is not one */
            "1\n",                      /* one field */
            "1 1 and something else\n", /* and one too many */
        };
        unsigned i;
        char live[64];

        for (i = 0; i < sizeof(forged) / sizeof(forged[0]) + 2; i++) {
                if (i < sizeof(forged) / sizeof(forged[0])) {
                        content_fd = file_holding(forged[i]);
                } else {
                        /* and two that name a live process - this one - so that
                         * what refuses them is the shape of the record and not
                         * the liveness of what it names. A conversion that
                         * stops at the first thing it does not understand reads
                         * both as a lock this process holds, and refuses to
                         * initialise.
                         */
                        if (i == sizeof(forged) / sizeof(forged[0]))
                                snprintf(live, sizeof(live),
                                         "%d %" PRIu64 " trailing\n",
                                         (int)getpid(), own_start_time());
                        else
                                snprintf(live, sizeof(live),
                                         "+%d %" PRIu64 "\n", (int)getpid(),
                                         own_start_time());

                        content_fd = file_holding(live);
                }

                expect_function_call(__wrap_pthread_mutex_lock);
                will_return(__wrap_pthread_mutex_lock, 0);

                expect_check_lockdir_access_ok();
                expect_lockfile_open(EEXIST, -1);

                expect_function_call(__wrap_unlink);
                expect_string(__wrap_unlink, pathname, LOCKFILE);
                will_return(__wrap_unlink, 0);

                expect_lockfile_open(0, TEST_LOCK_FD2);

                expect_function_call(__wrap_pthread_mutex_init);
                will_return(__wrap_pthread_mutex_init, 0);
                expect_function_call(__wrap_pthread_mutex_unlock);
                will_return(__wrap_pthread_mutex_unlock, 0);

                assert_int_equal(lock_init(), 0);

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
}

static void
test_init_refuses_without_a_record_of_its_own(void **state
                                              __attribute__((unused)))
{
        /* No /proc at all, on the platform whose liveness check is /proc. Every
         * later check would answer the same way, so a lock published here
         * cannot be told from a stale one: the next caller removes it and runs
         * beside this session, each believing it holds the lock. Refused
         * instead.
         *
         * Nothing is armed past the directory check, so a lock file created
         * here fails this case.
         */
        proc_open_errno = ENOENT;

        expect_function_call(__wrap_pthread_mutex_lock);
        will_return(__wrap_pthread_mutex_lock, 0);

        expect_check_lockdir_access_ok();

        expect_function_call(__wrap_pthread_mutex_unlock);
        will_return(__wrap_pthread_mutex_unlock, 0);

        assert_int_equal(lock_init(), -1);
}

static void
test_init_refuses_when_its_own_start_time_cannot_be_read(
    void **state __attribute__((unused)))
{
        /* and the same failure about this process: the record it would publish
         * would name a start time that is not its own, which the next caller
         * reads as a dead holder and clears - taking the lock off this session
         * while it believes it holds it. So the lock is not taken at all.
         *
         * Nothing is armed after the directory check: a lock file created here
         * would fail this case.
         */
        proc_open_errno = EIO;

        expect_function_call(__wrap_pthread_mutex_lock);
        will_return(__wrap_pthread_mutex_lock, 0);

        expect_check_lockdir_access_ok();

        expect_function_call(__wrap_pthread_mutex_unlock);
        will_return(__wrap_pthread_mutex_unlock, 0);

        assert_int_equal(lock_init(), -1);

        /* and it says what failed: -1 from the reader promises an errno, and a
         * message ending in "Success" is what a path that sets none produces
         */
        assert_non_null(strstr(last_stderr, strerror(EIO)));
}

static void
test_init_refuses_on_a_record_it_cannot_parse(void **state
                                              __attribute__((unused)))
{
        char path[64];

        /* The entry is there and reads, and what it holds is not a record: no
         * closing parenthesis, so the walk to field 22 has nothing to walk.
         * That is "could not be read" too, and the message has to name it -
         * this path had no errno of its own, so it printed whatever the last
         * unrelated call had left behind, which is "Success" on a quiet run.
         */
        snprintf(path, sizeof(path), "/proc/%d/stat", (int)getpid());
        snprintf(proc_fail_path, sizeof(proc_fail_path), "%s", path);
        proc_short_record = "nothing that looks like a stat record";
        proc_short_is_error = 0;

        expect_function_call(__wrap_pthread_mutex_lock);
        will_return(__wrap_pthread_mutex_lock, 0);

        expect_check_lockdir_access_ok();

        expect_function_call(__wrap_pthread_mutex_unlock);
        will_return(__wrap_pthread_mutex_unlock, 0);

        assert_int_equal(lock_init(), -1);
        assert_non_null(strstr(last_stderr, strerror(EINVAL)));
        assert_null(strstr(last_stderr, strerror(0)));
}
#endif /* __linux__ */

static void
test_lock_file_that_cannot_be_read_is_not_cleared(void **state
                                                  __attribute__((unused)))
{
        /* The file opens, fstat() says it is a regular file, and the read of it
         * fails - EIO from a disk that is going, or a filesystem that has
         * stopped answering. That says nothing about who holds the lock, so it
         * is the same answer as a failure to open: the lock stays.
         *
         * Reading it as "no record, therefore stale" removed a live holder's
         * marker on the strength of this process failing to look, which is the
         * accident the three-way open was added to avoid.
         */
        content_fd = file_holding("4321 1234\n");
        read_errno = EIO;

        expect_function_call(__wrap_pthread_mutex_lock);
        will_return(__wrap_pthread_mutex_lock, 0);

        expect_check_lockdir_access_ok();
        expect_lockfile_open(EEXIST, -1);

        expect_function_call(__wrap_pthread_mutex_unlock);
        will_return(__wrap_pthread_mutex_unlock, 0);

        /* refused, and no unlink armed: an attempt to remove the file fails
         * this case rather than passing it
         */
        assert_int_equal(lock_init(), -1);
}

static void
test_lock_file_that_went_away_is_not_an_error(void **state
                                              __attribute__((unused)))
{
        /* The name was taken when link() tried it and is gone by the time it is
         * cleared, which is what a holder finalising normally does. An absent
         * name is the state the removal was asked for, so ENOENT from it is not
         * a failure: reporting one refused to initialise over a lock that no
         * longer existed.
         *
         * The open that would examine it answers ENOENT too, for the same
         * reason - the file went away - and that is already read as "nothing is
         * there".
         */
        lock_open_errno = ENOENT;
        unlink_errno = ENOENT;

        expect_function_call(__wrap_pthread_mutex_lock);
        will_return(__wrap_pthread_mutex_lock, 0);

        expect_check_lockdir_access_ok();
        expect_lockfile_open(EEXIST, -1);

        expect_function_call(__wrap_unlink);
        expect_string(__wrap_unlink, pathname, LOCKFILE);
        will_return(__wrap_unlink, -1);

        /* and the name is free, so the second attempt publishes. Through
         * mkostemp() and link(), so the ENOENT armed above stays: reset here,
         * before lock_init() runs, it let the examining open reach the real
         * file of this name, and the case pass or fail on what that held
         */
        expect_lockfile_open(0, TEST_LOCK_FD2);

        expect_function_call(__wrap_pthread_mutex_init);
        will_return(__wrap_pthread_mutex_init, 0);
        expect_function_call(__wrap_pthread_mutex_unlock);
        will_return(__wrap_pthread_mutex_unlock, 0);

        assert_int_equal(lock_init(), 0);

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

/**
 * @brief Runs the symlink case for one of the errnos an open may refuse with
 *
 * @param [in] open_errno what the open of the link fails with
 */
static void
a_symlink_at_the_name_is_cleared(int open_errno)
{
        /* whoever owns it: a symbolic link at that path is not a file this
         * library wrote, and following one is how a world writable directory
         * turns into a read somewhere else
         */

        /* A symbolic link is refused by the open itself: O_NOFOLLOW gives
         * ELOOP on Linux and EMLINK on FreeBSD, so there is no descriptor to
         * fstat and nothing to read - which is the point of opening once rather
         * than looking with lstat() and opening afterwards. What the link
         * pointed at is never touched.
         */
        euid = 1000;
        lock_open_errno = open_errno;

        expect_function_call(__wrap_pthread_mutex_lock);
        will_return(__wrap_pthread_mutex_lock, 0);

        expect_check_lockdir_access_ok();
        expect_lockfile_open(EEXIST, -1);

        expect_function_call(__wrap_unlink);
        expect_string(__wrap_unlink, pathname, LOCKFILE);
        will_return(__wrap_unlink, 0);

        expect_lockfile_open(0, TEST_LOCK_FD3);

        expect_function_call(__wrap_pthread_mutex_init);
        will_return(__wrap_pthread_mutex_init, 0);
        expect_function_call(__wrap_pthread_mutex_unlock);
        will_return(__wrap_pthread_mutex_unlock, 0);

        assert_int_equal(lock_init(), 0);

        expect_function_call(__wrap_close);
        expect_value(__wrap_close, fildes, TEST_LOCK_FD3);
        will_return(__wrap_close, 0);
        expect_function_call(__wrap_pthread_mutex_destroy);
        will_return(__wrap_pthread_mutex_destroy, 0);
        expect_function_call(__wrap_unlink);
        expect_string(__wrap_unlink, pathname, LOCKFILE);
        will_return(__wrap_unlink, 0);
        assert_int_equal(lock_fini(), 0);
}

static void
test_lock_file_that_is_a_symlink_is_not_a_lock(void **state
                                               __attribute__((unused)))
{
        /* both errnos, because which one an open of a link gives is the
         * platform's choice and the answer must not be: on the platform that
         * says EMLINK, a link that reached the "could not be examined" arm
         * would be a lock nobody can clear and a denial of service one ln -s
         * long
         */
        a_symlink_at_the_name_is_cleared(ELOOP);
        a_symlink_at_the_name_is_cleared(EMLINK);
}

#ifdef __linux__
static void
test_lock_live_holder_is_recognised_by_its_start_time(void **state
                                                      __attribute__((unused)))
{
        char line[64];

        /* A lock file naming a living process is a lock to respect, and what
         * says the process is the one that took it is its start time - field 22
         * of /proc/<pid>/stat. lock.c read field 23, the virtual memory size,
         * so this case failed to recognise its own live holder; worse, vsize
         * changes as a process allocates, so a session's own lock file could be
         * judged stale by the next invocation and removed under it.
         *
         * The holder here is this process, and the start time comes from
         * /proc/self/stat read independently of lock.c's arithmetic.
         */
        snprintf(line, sizeof(line), TEST_RECORD, (int)getpid(),
                 own_start_time());

        content_fd = file_holding(line);

        expect_function_call(__wrap_pthread_mutex_lock);
        will_return(__wrap_pthread_mutex_lock, 0);

        expect_check_lockdir_access_ok();
        expect_lockfile_open(EEXIST, -1);

        expect_function_call(__wrap_pthread_mutex_unlock);
        will_return(__wrap_pthread_mutex_unlock, 0);

        /* refused, and the file left alone: no unlink is armed above, so an
         * attempt to remove it would fail this case
         */
        assert_int_equal(lock_init(), -1);
}
#endif /* __linux__ */

#ifdef __linux__
static void
test_live_holder_whose_name_holds_a_newline(void **state
                                            __attribute__((unused)))
{
        char line[64];
        char saved[17] = {0};
        const char *newlined = "pqos\nlock";

        /* Worse than a space, and the reason the whole /proc record is read
         * rather than one line of it: this kernel does not escape a newline in
         * the process name, so "(pqos" and "lock)" land on different lines of
         * /proc/<pid>/stat. A line-wise read returns a prefix with no closing
         * parenthesis in it, the walk finds nothing, and the live holder is
         * taken for a dead one - its lock file removed and a second session
         * given a lock the first still believes it holds.
         */
        assert_int_equal(prctl(PR_GET_NAME, saved, 0, 0, 0), 0);
        assert_int_equal(prctl(PR_SET_NAME, newlined, 0, 0, 0), 0);

        snprintf(line, sizeof(line), TEST_RECORD, (int)getpid(),
                 own_start_time());

        stat_uid = 0;
        stat_mode = S_IFREG | 0644;
        content_fd = file_holding(line);

        expect_function_call(__wrap_pthread_mutex_lock);
        will_return(__wrap_pthread_mutex_lock, 0);

        expect_check_lockdir_access_ok();
        expect_lockfile_open(EEXIST, -1);

        expect_function_call(__wrap_pthread_mutex_unlock);
        will_return(__wrap_pthread_mutex_unlock, 0);

        /* refused, with no unlink armed */
        assert_int_equal(lock_init(), -1);

        assert_int_equal(prctl(PR_SET_NAME, saved, 0, 0, 0), 0);
}

static void
test_live_holder_whose_name_holds_a_space(void **state __attribute__((unused)))
{
        char line[64];
        char saved[17] = {0};
        const char *spaced = "pqos lock test";

        /* Field 2 of /proc/<pid>/stat is the process name in parentheses, and a
         * name may hold spaces - a prctl() rename, or a binary whose file name
         * has one. Counting delimiters from the start of the line then lands on
         * a different field, and the holder's start time reads back as
         * something else or as nothing at all: the live holder is judged stale,
         * its lock file removed, and a second session takes a lock the first
         * still believes it holds.
         *
         * So this process is renamed to a name with spaces in it, and the case
         * is the live holder case again.
         */
        assert_int_equal(prctl(PR_GET_NAME, saved, 0, 0, 0), 0);
        assert_int_equal(prctl(PR_SET_NAME, spaced, 0, 0, 0), 0);

        snprintf(line, sizeof(line), TEST_RECORD, (int)getpid(),
                 own_start_time());

        stat_ret = 0;
        stat_uid = 0;
        stat_mode = S_IFREG | 0644;
        content_fd = file_holding(line);

        expect_function_call(__wrap_pthread_mutex_lock);
        will_return(__wrap_pthread_mutex_lock, 0);

        expect_check_lockdir_access_ok();
        expect_lockfile_open(EEXIST, -1);

        expect_function_call(__wrap_pthread_mutex_unlock);
        will_return(__wrap_pthread_mutex_unlock, 0);

        /* refused, with no unlink armed: the holder is recognised through a
         * name the walk has to step over rather than count through
         */
        assert_int_equal(lock_init(), -1);

        assert_int_equal(prctl(PR_SET_NAME, saved, 0, 0, 0), 0);
}
#endif /* __linux__ */

/**
 * @brief Puts the mocks back to what a case that says nothing would want
 *
 * Every case that sets one of these used to set them itself, which left the
 * next case running on whatever the last one had arranged - and a content
 * descriptor that the case which set it never had read, because a file that is
 * not a lock is cleared before anything reads it, stayed open for the next case
 * to be handed by accident.
 *
 * @param [in] state unused
 *
 * @return 0, always
 */
static int
lock_case_setup(void **state __attribute__((unused)))
{
        stat_ret = 0;
        stat_errno = ENOENT;
        close_errno = 0;
        stat_uid = 0;
        stat_mode = S_IFREG | 0644;
        content_fd = -1;
        handed_lock_fd = -1;
        lock_open_errno = 0;
        proc_open_errno = 0;
        proc_fail_path[0] = '\0';
        proc_short_record = NULL;
        proc_short_fd = -1;
        proc_short_stream = NULL;
        proc_short_is_error = 1;
        last_stderr[0] = '\0';
        unlink_errno = 0;
        read_chunk = 0;
        read_errno = 0;
        euid = 0;
        opens_seen = 0;

        return 0;
}

/**
 * @brief Closes a content descriptor nothing read
 *
 * @param [in] state unused
 *
 * @return 0, always
 */
static int
lock_case_teardown(void **state __attribute__((unused)))
{
        if (content_fd >= 0) {
                close(content_fd);
                content_fd = -1;
        }

#ifdef __linux__
        /* and the child, whether or not the case got as far as stopping it: an
         * assertion in between leaves by a long jump, and this runs anyway.
         * Inside the same fence as the fixture that starts one, which is where
         * the cases that need a live holder are.
         */
        if (case_child > 0) {
                (void)kill(case_child, SIGKILL);
                (void)waitpid(case_child, NULL, 0);
                case_child = -1;
        }
#endif

        return 0;
}

/**
 * @brief Runs one init over an existing lock file that a holder keeps
 *
 * @param [in] line the record in the file
 */
static void
a_lock_that_is_held(const char *line)
{
        content_fd = file_holding(line);

        expect_function_call(__wrap_pthread_mutex_lock);
        will_return(__wrap_pthread_mutex_lock, 0);

        expect_check_lockdir_access_ok();
        expect_lockfile_open(EEXIST, -1);

        expect_function_call(__wrap_pthread_mutex_unlock);
        will_return(__wrap_pthread_mutex_unlock, 0);

        /* refused, and no unlink armed, so removing it fails the case */
        assert_int_equal(lock_init(), -1);
}

/**
 * @brief Runs one init over an existing lock file that is stale
 *
 * @param [in] line the record in the file
 */
static void
a_lock_that_is_cleared(const char *line)
{
        content_fd = file_holding(line);

        expect_function_call(__wrap_pthread_mutex_lock);
        will_return(__wrap_pthread_mutex_lock, 0);

        expect_check_lockdir_access_ok();
        expect_lockfile_open(EEXIST, -1);

        expect_function_call(__wrap_unlink);
        expect_string(__wrap_unlink, pathname, LOCKFILE);
        will_return(__wrap_unlink, 0);

        expect_lockfile_open(0, TEST_LOCK_FD2);

        expect_function_call(__wrap_pthread_mutex_init);
        will_return(__wrap_pthread_mutex_init, 0);
        expect_function_call(__wrap_pthread_mutex_unlock);
        will_return(__wrap_pthread_mutex_unlock, 0);

        assert_int_equal(lock_init(), 0);

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

#ifdef __linux__
static void
test_a_published_record_carries_its_version(void **state
                                            __attribute__((unused)))
{
        char expected[64];

        /* the version is what tells the next caller this record's second
         * number is a start time
         */
        written_record[0] = '\0';
        a_lock_that_is_cleared("0 0\n");

        snprintf(expected, sizeof(expected), TEST_RECORD, (int)getpid(),
                 own_start_time());
        assert_string_equal(written_record, expected);
}

static void
test_a_live_holder_of_an_earlier_version_keeps_the_lock(void **state
                                                        __attribute__((unused)))
{
        char line[64];
        pid_t child;

        /* Two numbers and no version: an earlier library's record, whose
         * second number was field 23 of /proc/<pid>/stat and so is not a start
         * time to compare. Compared anyway it never matches, and the live
         * session's lock was removed under it. The holder is running, so it
         * keeps the lock, and the caller is told what to do about it.
         */
        child = start_a_child();
        snprintf(line, sizeof(line), "%d 5828608\n", (int)child);

        a_lock_that_is_held(line);
        assert_non_null(strstr(last_stderr, "another version"));

        stop_a_child(child);
}

static void
test_a_record_of_a_later_version_fails_closed(void **state
                                              __attribute__((unused)))
{
        char line[64];
        pid_t child;

        /* a version this library does not write is not one it can check: its
         * second number need not be a start time at all, so one that does not
         * match this process's is no evidence the holder has gone
         */
        child = start_a_child();
        snprintf(line, sizeof(line), "%d %" PRIu64 " 3\n", (int)child,
                 start_time_of(child) + 1);

        a_lock_that_is_held(line);

        stop_a_child(child);
}

static void
test_a_holder_of_an_earlier_version_that_has_gone_is_cleared(
    void **state __attribute__((unused)))
{
        char line[64];
        pid_t child;

        /* and one whose process is not running is stale, whatever version
         * wrote it
         */
        child = start_a_child();
        stop_a_child(child);
        snprintf(line, sizeof(line), "%d 5828608\n", (int)child);

        a_lock_that_is_cleared(line);
}

static void
test_a_current_record_of_a_reused_pid_is_cleared(void **state
                                                 __attribute__((unused)))
{
        char line[64];
        pid_t child;

        /* the check the version keeps: a running process whose start time is
         * not the record's took the id after the holder had gone
         */
        child = start_a_child();
        snprintf(line, sizeof(line), TEST_RECORD, (int)child,
                 start_time_of(child) + 1);

        a_lock_that_is_cleared(line);

        stop_a_child(child);
}
#endif /* __linux__ */

#ifdef __linux__
/**
 * @brief Waits until a process reads as a zombie in /proc, or the time is up
 *
 * @param [in] pid the process
 *
 * @return whether it did
 */
static int
became_a_zombie(pid_t pid)
{
        char path[64];
        char buf[512];
        int tries;

        snprintf(path, sizeof(path), "/proc/%d/stat", (int)pid);
        for (tries = 0; tries < 200; tries++) {
                FILE *f = fopen(path, "r");
                size_t got = 0;
                char *p;

                if (f != NULL) {
                        got = fread(buf, 1, sizeof(buf) - 1, f);
                        fclose(f);
                }
                buf[got] = '\0';
                p = strrchr(buf, ')');
                if (p != NULL && p[1] == ' ' && p[2] == 'Z')
                        return 1;
                usleep(10000);
        }
        return 0;
}

static void *
runs_until_killed(void *arg)
{
        (void)arg;
        for (;;)
                pause();
        return NULL;
}

static void
test_holder_that_exited_unreaped_is_cleared(void **state
                                            __attribute__((unused)))
{
        char line[64];
        pid_t child;

        /* A holder killed without releasing the lock, whose parent has not
         * waited for it yet: its /proc entry is still there with the start time
         * it had, and nothing of it is running. Held until the reap, which may
         * never come, the lock would be lost to every later caller.
         */
        fflush(stdout);
        fflush(stderr);
        child = fork();
        assert_true(child >= 0);
        if (child == 0)
                _exit(0);
        /* tracked first, so an assertion below leaves the teardown to reap it
         */
        case_child = child;
        assert_true(became_a_zombie(child));

        snprintf(line, sizeof(line), TEST_RECORD, (int)child,
                 start_time_of(child));
        a_lock_that_is_cleared(line);

        assert_true(waitpid(child, NULL, 0) == child);
        case_child = -1;
}

static void
test_holder_whose_leader_exited_is_kept(void **state __attribute__((unused)))
{
        char line[64];
        pid_t child;

        /* and a zombie leader is not that: one that called pthread_exit() with
         * another thread still running reads as a zombie too, and its session
         * is alive in that thread
         */
        fflush(stdout);
        fflush(stderr);
        child = fork();
        assert_true(child >= 0);
        if (child == 0) {
                pthread_t worker;

                if (pthread_create(&worker, NULL, runs_until_killed, NULL) != 0)
                        _exit(1);
                pthread_exit(NULL);
        }
        case_child = child;
        assert_true(became_a_zombie(child));

        snprintf(line, sizeof(line), TEST_RECORD, (int)child,
                 start_time_of(child));
        a_lock_that_is_held(line);

        stop_a_child(child);
}
#endif /* __linux__ */

int
main(void)
{
#define LOCK_CASE(name)                                                        \
        cmocka_unit_test_setup_teardown(name, lock_case_setup,                 \
                                        lock_case_teardown)

        const struct CMUnitTest tests[] = {
            LOCK_CASE(test_lock_init_error),
            LOCK_CASE(test_lock_init_exit),
            LOCK_CASE(test_lock_get),
            LOCK_CASE(test_lock_opens_close_on_exec),
            LOCK_CASE(test_lock_probe_file_cannot_be_pre_created),
#ifdef __linux__
            LOCK_CASE(test_lock_of_another_owner_is_honoured_while_it_lives),
#endif
            LOCK_CASE(test_stale_lock_of_another_owner_is_cleared),
            LOCK_CASE(test_lock_file_that_is_a_directory_is_cleared),
            LOCK_CASE(test_lock_file_that_is_a_symlink_is_not_a_lock),
            LOCK_CASE(test_lock_file_that_cannot_be_examined_is_not_cleared),
            LOCK_CASE(test_a_lock_that_cannot_be_examined_says_why),
            LOCK_CASE(test_a_stale_file_that_cannot_be_removed_says_why),
            LOCK_CASE(test_lock_file_is_never_published_empty),
            LOCK_CASE(test_lock_file_is_published_read_only_to_other_users),
#ifdef __linux__
            LOCK_CASE(test_live_holder_whose_record_arrives_in_pieces),
#endif
            LOCK_CASE(test_lock_file_that_went_away_is_not_an_error),
            LOCK_CASE(test_lock_file_that_cannot_be_read_is_not_cleared),
#ifdef __linux__
            LOCK_CASE(test_holder_whose_start_time_cannot_be_read_is_kept),
            LOCK_CASE(test_holder_hidden_from_proc_is_kept),
            LOCK_CASE(test_holder_that_exited_unreaped_is_cleared),
            LOCK_CASE(test_holder_whose_leader_exited_is_kept),
            LOCK_CASE(test_holder_whose_record_is_cut_short_is_kept),
            LOCK_CASE(test_a_start_time_above_32_bits_is_carried),
            LOCK_CASE(test_a_forged_record_is_not_a_lock),
            LOCK_CASE(test_init_refuses_without_a_record_of_its_own),
            LOCK_CASE(test_init_refuses_when_its_own_start_time_cannot_be_read),
            LOCK_CASE(test_init_refuses_on_a_record_it_cannot_parse),
#endif
#ifdef __linux__
            LOCK_CASE(test_lock_live_holder_is_recognised_by_its_start_time),
#endif
#ifdef __linux__
            LOCK_CASE(test_live_holder_whose_name_holds_a_space),
            LOCK_CASE(test_live_holder_whose_name_holds_a_newline),
            LOCK_CASE(test_a_published_record_carries_its_version),
            LOCK_CASE(test_a_live_holder_of_an_earlier_version_keeps_the_lock),
            LOCK_CASE(test_a_record_of_a_later_version_fails_closed),
            LOCK_CASE(
                test_a_holder_of_an_earlier_version_that_has_gone_is_cleared),
            LOCK_CASE(test_a_current_record_of_a_reused_pid_is_cleared),
#endif
        };

        return cmocka_run_group_tests(tests, NULL, NULL);
}
