/*
 *  BSD  LICENSE
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

#include "common.h"
#include "log.h"
#include "pqos.h"
#include "test.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define FILE_DEAD ((FILE *)0xDEAD)

/* What the library logs, so that a case can read the message it emits. The
 * utility used to print these to stdout and its cases read them there; the
 * library logs them, and log_init() takes a callback for exactly this.
 */
static char logged[8 * 1024];

static void
log_callback(void *context __attribute__((unused)),
             const size_t size,
             const char *message)
{
        size_t used = strlen(logged);

        if (used + size + 1 < sizeof(logged)) {
                memcpy(logged + used, message, size);
                logged[used + size] = '\0';
        }
}

ssize_t
__wrap_getline(char **string, size_t *n, FILE *stream)
{
        ssize_t ret;

        assert_non_null(n);
        assert_non_null(stream);

        ret = mock_type(ssize_t);
        if (ret != -1) {
                char *data = mock_ptr_type(char *);

                *string = strdup(data);
        }

        return ret;
}

/* The descriptor the /proc case hands back from its mocked open(), which
 * nothing may read: the stream on top of it is FILE_DEAD.
 */
#define FD_DEAD 0x0DEAD

/* The name that case opens. Only this one is mocked below - every other case
 * here opens a real file, and pqos_open() itself is what several of them are
 * about, so the wrappers pass anything else straight through.
 */
#define PROC_DEAD_NAME "/proc/my_file_to_open"

/* named through a typedef, as log_printf's is: a prototype written out in a
 * .c file reads to checkpatch as an extern declaration
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

typedef int open_fn(const char *path, int oflags, ...);
open_fn __real_open;

/* Armed by a case that wants something put at a name while a call is running.
 * They are set, and race_plant() is defined, in the race section further down;
 * they appear here because the open() wrapper below is what acts on them.
 */
static const char *race_link;
static const char *race_open_link;
static void race_plant(void);

/* Variadic, as open() is
 *
 * A wrapper with a fixed mode parameter reads a third argument that a two
 * argument call never passed, and the library makes such a call: the /proc
 * names it reads are opened O_RDONLY | O_CLOEXEC with no mode at all. What the
 * flags do not ask for is not read here either.
 */
int
__wrap_open(const char *path, int oflags, ...)
{
        int mode = 0;
        int armed;
        int fd;

        if (open_takes_a_mode(oflags)) {
                va_list args;

                va_start(args, oflags);
                mode = va_arg(args, int);
                va_end(args);
        }

        if (strcmp(path, PROC_DEAD_NAME) == 0) {
                check_expected(oflags);

                return mock_type(int);
        }

        /* pqos_open() opens once, so there is no window inside it to plant
         * in: a case arms a name here to have something put at it immediately
         * after that open returns, while the descriptor is held. That is what
         * the cases below are about - what happens to the name afterwards,
         * rather than what the open resolved.
         */
        armed = race_open_link != NULL && strcmp(path, race_open_link) == 0;

        fd = __real_open(path, oflags, mode);

        /* only after an open that succeeded: what these cases are about is a
         * name changing under a descriptor that is being held, and a failed
         * open holds nothing. Planting anyway would touch the filesystem on a
         * path no case describes, and could turn an errno a case is asserting
         * on into a different one.
         */
        if (armed && fd >= 0) {
                int error = errno;

                race_link = race_open_link;
                race_open_link = NULL;
                race_plant();
                errno = error;
        }

        return fd;
}

typedef FILE *fdopen_fn(int fd, const char *mode);
fdopen_fn __real_fdopen;

FILE *
__wrap_fdopen(int fd, const char *mode)
{
        if (fd != FD_DEAD)
                return __real_fdopen(fd, mode);

        check_expected_ptr(mode);

        return mock_type(FILE *);
}

FILE *__real_fopen(const char *name, const char *mode);

FILE *
__wrap_fopen(const char *name, const char *mode)
{
        FILE *fd;

        check_expected(name);
        check_expected(mode);

        fd = mock_type(FILE *);
        if (fd == NULL || fd == FILE_DEAD)
                return fd;

        return __real_fopen(name, mode);
}

int __real_fclose(FILE *stream);

int
__wrap_fclose(FILE *stream)
{
        assert_non_null(stream);

        if (stream != FILE_DEAD)
                return __real_fclose(stream);

        return mock_type(int);
}

char *
__wrap_fgets(char *str, int n, FILE *stream)
{
        char *data;
        int len;

        assert_non_null(stream);
        assert_non_null(str);

        data = mock_ptr_type(char *);
        len = strlen(data);
        if (len == 0)
                return NULL;

        strncpy(str, data, n);

        return str;
}

static void
test_common_pqos_strcat(void **state __attribute__((unused)))
{
        char dst_param[20] = "Xx";
        const char *src_param = "Hello World!";
        size_t size_param = 6;
        char *return_value;

        return_value = pqos_strcat(dst_param, src_param, size_param);
        assert_non_null(return_value);
        assert_string_equal(return_value, "XxHell");
        assert_string_equal(dst_param, "XxHell");
}

static void
test_common_pqos_file_exists(void **state __attribute__((unused)))
{
        const char *path1 = "/proc/cpuinfo";
        const char *path2 = "./some_random_file_name_that_doesnt_exist";
        int return_value;

        return_value = pqos_file_exists(path1);
        assert_int_equal(return_value, 1);

        return_value = pqos_file_exists(path2);
        assert_int_equal(return_value, 0);
}

static void
test_common_pqos_dir_exists(void **state __attribute__((unused)))
{
        int return_value;

        return_value = pqos_dir_exists("/proc/cpuinfo");
        assert_int_equal(return_value, 0);

        return_value = pqos_dir_exists("/bin/");
        assert_int_equal(return_value, 1);

        return_value = pqos_dir_exists("/folder_that_doesnt_exist");
        assert_int_equal(return_value, 0);
}

static void
test_common_pqos_fgets(void **state __attribute__((unused)))
{
        char string[4] = {0};
        char *return_value;

        will_return(__wrap_getline, 4);
        will_return(__wrap_getline, "AbC\n");
        return_value = pqos_fgets(string, 4, FILE_DEAD);
        assert_non_null(return_value);
        assert_string_equal(string, "AbC");
        assert_string_equal(return_value, "AbC");

        will_return(__wrap_getline, 4);
        will_return(__wrap_getline, "ABC\n");
        return_value = pqos_fgets(string, 3, FILE_DEAD);
        assert_null(return_value);

        will_return(__wrap_getline, -1);
        return_value = pqos_fgets(string, 4, FILE_DEAD);
        assert_null(return_value);
}

static void
test_common_pqos_file_contains(void **state __attribute__((unused)))
{
        int ret_value;
        const char *search_str1 = "Test string";
        int found_param;
        const char *path = "/proc/my_file_to_open";

        /* a name under /proc is exempt from the symlink refusal, so it is
         * opened here rather than through pqos_fopen() - and the flags are
         * required to carry O_CLOEXEC, since a descriptor the library keeps
         * must not be inherited by anything the caller starts
         */
        expect_value(__wrap_open, oflags, O_RDONLY | O_CLOEXEC);
        will_return(__wrap_open, FD_DEAD);
        expect_string(__wrap_fdopen, mode, "r");
        will_return(__wrap_fdopen, FILE_DEAD);
        will_return(__wrap_fgets, "Test string");
        will_return(__wrap_fclose, 0);
        ret_value = pqos_file_contains(path, search_str1, &found_param);
        assert_int_equal(ret_value, PQOS_RETVAL_OK);
        assert_int_equal(found_param, 1);

        expect_value(__wrap_open, oflags, O_RDONLY | O_CLOEXEC);
        will_return(__wrap_open, FD_DEAD);
        expect_string(__wrap_fdopen, mode, "r");
        will_return(__wrap_fdopen, FILE_DEAD);
        will_return(__wrap_fgets, "test string");
        will_return(__wrap_fgets, "");
        will_return(__wrap_fclose, 0);
        ret_value = pqos_file_contains(path, search_str1, &found_param);
        assert_int_equal(ret_value, PQOS_RETVAL_OK);
        assert_int_equal(found_param, 0);

        /* the open failing is the file not being there */
        expect_value(__wrap_open, oflags, O_RDONLY | O_CLOEXEC);
        will_return(__wrap_open, -1);
        ret_value = pqos_file_contains(path, search_str1, &found_param);
        assert_int_equal(ret_value, PQOS_RETVAL_OK);
        assert_int_equal(found_param, 0);

        ret_value = pqos_file_contains(NULL, search_str1, &found_param);
        assert_int_equal(ret_value, PQOS_RETVAL_PARAM);
        assert_int_equal(found_param, 0);

        ret_value = pqos_file_contains("my_file_to_open", NULL, &found_param);
        assert_int_equal(ret_value, PQOS_RETVAL_PARAM);
        assert_int_equal(found_param, 0);

        ret_value = pqos_file_contains("my_file_to_open", search_str1, NULL);
        assert_int_equal(ret_value, PQOS_RETVAL_PARAM);
        assert_int_equal(found_param, 0);
}

/* pqos_fopen() and pqos_open() open the file themselves now - with O_NOFOLLOW,
 * so that the kernel is what refuses a link - and no longer reach fopen(),
 * which is why these cases work on real files instead of on the fopen() wrapper
 * above. A file is written with __real_fopen() so that the wrapper's
 * expectations stay out of the fixture.
 */
/* Every fixture lives in a directory of this process's own, so a concurrent run
 * cannot unlink or replace what this one is asserting about, and a name planted
 * beforehand cannot make ut_write() truncate somebody else's file. Same reason,
 * and the same shape, as unit-test/pqos/test_common.c.
 */
static char work_dir[PATH_MAX];
static char ut_file[PATH_MAX];
static char ut_link[PATH_MAX];
static char ut_target[PATH_MAX];
static char ut_absent[PATH_MAX];

static void
ut_path(char *buffer, size_t size, const char *name)
{
        snprintf(buffer, size, "%s/%s", work_dir, name);
        buffer[size - 1] = '\0';
}

static int
group_setup(void **state __attribute__((unused)))
{
        memset(logged, 0, sizeof(logged));
        if (log_init(-1, log_callback, NULL, LOG_VER_SUPER_VERBOSE) !=
            LOG_RETVAL_OK)
                return -1;

        snprintf(work_dir, sizeof(work_dir), "/tmp/pqos_ut_lib_common_XXXXXX");
        if (mkdtemp(work_dir) == NULL)
                return -1;

        ut_path(ut_file, sizeof(ut_file), "file");
        ut_path(ut_link, sizeof(ut_link), "link");
        ut_path(ut_target, sizeof(ut_target), "target");
        ut_path(ut_absent, sizeof(ut_absent), "absent");

        return 0;
}

static int
group_teardown(void **state __attribute__((unused)))
{
        if (work_dir[0] != '\0')
                rmdir(work_dir);
        log_fini();

        return 0;
}

/* What log_printf() was asked to do, so that a case can require it was not
 * asked at all. A message raised before the log exists is dropped inside
 * log_printf() itself, so nothing downstream can tell a guard that ran from a
 * guard that was removed: the callback sees nothing either way. Only a DEBUG
 * build asserts on it, and the default build is the one that gets run, so the
 * call is counted here instead.
 *
 * The varargs cannot be handed on as they arrived, so the message is formatted
 * here and passed as one string - the log sees the same text.
 */
static int log_printf_calls;

/* the real function, named through a typedef: a variadic prototype written out
 * in a .c file reads to checkpatch as an extern declaration
 */
typedef void log_printf_fn(int type, const char *str, ...);
log_printf_fn __real_log_printf;

void
__wrap_log_printf(int type, const char *str, ...)
{
        char message[4 * 1024];
        va_list args;

        log_printf_calls++;

        va_start(args, str);
        vsnprintf(message, sizeof(message), str, args);
        va_end(args);

        __real_log_printf(type, "%s", message);
}

/* Every case that reads what was logged starts from an empty buffer */
static void
logged_clear(void)
{
        memset(logged, 0, sizeof(logged));
}

/* Something put at a name while a call is running cannot be arranged by a
 * caller, so open() is wrapped and a case arms the name it is interested in:
 * race_open_link fires the plant immediately after the real open() returns.
 *
 * pqos_open() has no window inside it - it opens once, and the descriptor is
 * what the name resolved to at that moment - so what these cases are about is
 * what happens to the *name* afterwards, while the descriptor is held. The
 * cases about a link or a file already at the name put it there beforehand and
 * assert the answer, which needs no wrapper and cannot be skipped.
 *
 * race_wrapper_ran carries three answers, and a case that arms the hook has to
 * tell them apart: 1 is the plant done, 0 is the wrapper never reached - a C
 * library that does not route open() through it - which is a skip, and -1 is
 * the plant having run and failed, which is a broken setup and a failure. Only
 * 0 may skip; reading it as "not 1" turns a case that could not be set up into
 * a case that passed.
 */
static const char *race_target = NULL;
static const char *race_written = NULL;
static int race_wrapper_ran;

/* Puts at the armed name whatever the case armed it with */
static void
race_plant(void)
{
        race_wrapper_ran = 1;

        /* the armed name may be a directory the case created, so that it can be
         * replaced the way a directory of a path could be; for a name that is
         * not one this fails and changes nothing
         */
        rmdir(race_link);

        if (race_written != NULL) {
                /* somebody else writing to the name, rather than replacing it:
                 * the file stays the one it was, which is what the window after
                 * a create is about
                 */
                FILE *stream = __real_fopen(race_link, "a");

                if (stream == NULL ||
                    fwrite(race_written, 1, strlen(race_written), stream) !=
                        strlen(race_written) ||
                    __real_fclose(stream) != 0)
                        race_wrapper_ran = -1;
        } else {
                /* the name may hold a file this call has just created, which a
                 * link cannot be planted over
                 */
                unlink(race_link);
                if (symlink(race_target, race_link) != 0)
                        race_wrapper_ran = -1;
        }

        race_link = NULL;
}

/* lstat() is wrapped because the link flags say so, and passes everything
 * through. Nothing arms it any more: pqos_open() consults the name only to
 * explain a failure it has already decided on, so there is no answer a case
 * would want to race. fstatat() rather than __real_lstat, which would not link
 * where lstat is not an exported symbol.
 */
int
__wrap_lstat(const char *pathname, struct stat *buf)
{
        return fstatat(AT_FDCWD, pathname, buf, AT_SYMLINK_NOFOLLOW);
}

char *__real_fgets(char *s, int n, FILE *stream);

static void
ut_write(const char *path, const char *text)
{
        FILE *stream = __real_fopen(path, "w");

        assert_non_null(stream);
        assert_int_equal(fwrite(text, 1, strlen(text), stream), strlen(text));
        assert_int_equal(__real_fclose(stream), 0);
}

static void
ut_assert_contents(const char *path, const char *text)
{
        FILE *stream = __real_fopen(path, "r");
        char buffer[64] = {0};

        assert_non_null(stream);
        assert_non_null(__real_fgets(buffer, sizeof(buffer), stream));
        assert_string_equal(buffer, text);
        assert_int_equal(__real_fclose(stream), 0);
}

static off_t
ut_size(const char *path)
{
        struct stat st;

        assert_return_code(stat(path, &st), 0);

        return st.st_size;
}

static void
ut_cleanup(void)
{
        unlink(ut_link);
        unlink(ut_target);
        unlink(ut_file);
}

static void
test_common_pqos_fopen(void **state __attribute__((unused)))
{
        FILE *fd;
        int error;

        ut_cleanup();

        /* an existing file is read */
        {
                char buffer[64] = {0};

                ut_write(ut_file, "contents\n");

                fd = pqos_fopen(ut_file, "r");
                assert_non_null(fd);
                assert_non_null(__real_fgets(buffer, sizeof(buffer), fd));
                assert_string_equal(buffer, "contents\n");
                assert_int_equal(pqos_fclose(fd), 0);
        }

        /* file does not exist, and errno says which */
        {
                fd = pqos_fopen(ut_absent, "r");
                error = errno;
                assert_null(fd);
                assert_int_equal(error, ENOENT);
        }

        /* a writing mode creates the file */
        {
                unlink(ut_file);
                fd = pqos_fopen(ut_file, "w");
                assert_non_null(fd);
                assert_int_equal(fwrite("written\n", 1, 8, fd), 8);
                assert_int_equal(pqos_fclose(fd), 0);
                ut_assert_contents(ut_file, "written\n");
        }

        /* a symlink is refused, and its target is left as it was - the
         * implementation this replaced truncated it through a "w" mode before
         * refusing the stream, so this case fails against that one
         */
        {
                ut_write(ut_target, "keep me\n");
                assert_return_code(symlink(ut_target, ut_link), 0);

                fd = pqos_fopen(ut_link, "w");
                assert_null(fd);
                ut_assert_contents(ut_target, "keep me\n");

                unlink(ut_link);
                unlink(ut_target);
        }

        /* a symlink is refused for a reading mode too */
        {
                ut_write(ut_target, "keep me\n");
                assert_return_code(symlink(ut_target, ut_link), 0);

                fd = pqos_fopen(ut_link, "r");
                assert_null(fd);

                unlink(ut_link);
                unlink(ut_target);
        }

        /* a link with no target does not become one */
        {
                assert_return_code(symlink(ut_target, ut_link), 0);

                fd = pqos_fopen(ut_link, "w+");
                assert_null(fd);
                assert_int_equal(access(ut_target, F_OK), -1);

                unlink(ut_link);
        }

        /* a mode that is not one C defines is refused, before the file it names
         * can be truncated by the part of it that is a mode
         */
        {
                static const char *const refused[] = {
                    "", "rw", "w+e", "z", "bw", "rbb", "+r", "wxb"};
                unsigned i;

                ut_write(ut_file, "untouched\n");

                for (i = 0; i < DIM(refused); i++) {
                        fd = pqos_fopen(ut_file, refused[i]);
                        error = errno;
                        assert_null(fd);
                        assert_int_equal(error, EINVAL);
                        ut_assert_contents(ut_file, "untouched\n");
                }
        }

        /* the binary and exclusive forms C does define are accepted */
        {
                static const char *const accepted[] = {
                    "wb", "w+b", "wb+", "wx", "wbx", "w+x", "w+bx", "wb+x"};
                unsigned i;

                for (i = 0; i < DIM(accepted); i++) {
                        unlink(ut_file);
                        fd = pqos_fopen(ut_file, accepted[i]);
                        assert_non_null(fd);
                        assert_int_equal(pqos_fclose(fd), 0);
                }
        }

        /* an exclusive mode fails on a file that is there, and leaves it alone
         */
        {
                ut_write(ut_file, "already\n");

                fd = pqos_fopen(ut_file, "wx");
                error = errno;
                assert_null(fd);
                assert_int_equal(error, EEXIST);
                ut_assert_contents(ut_file, "already\n");
        }

        /* NULL arguments are refused rather than dereferenced */
        {
                fd = pqos_fopen(NULL, "r");
                assert_null(fd);
                assert_int_equal(errno, EINVAL);

                fd = pqos_fopen(ut_file, NULL);
                assert_null(fd);
                assert_int_equal(errno, EINVAL);
        }

        /* the descriptor the stream carries is closed on exec: it can live for
         * the whole session, and nothing a child of this process does needs it
         */
        {
                unlink(ut_file);
                fd = pqos_fopen(ut_file, "w");
                assert_non_null(fd);
                assert_true(fcntl(fileno(fd), F_GETFD) & FD_CLOEXEC);
                assert_int_equal(pqos_fclose(fd), 0);
        }

        /* an append stream is where the fopen() of this platform puts one, and
         * a write goes to the end of the file whatever that is
         */
        {
                static const char *const modes[] = {"a", "ab", "a+", "a+b",
                                                    "ab+"};
                unsigned i;

                for (i = 0; i < DIM(modes); i++) {
                        FILE *reference;
                        long position;

                        ut_write(ut_file, "0123456789");

                        reference = __real_fopen(ut_file, modes[i]);
                        assert_non_null(reference);
                        position = ftell(reference);
                        assert_int_equal(__real_fclose(reference), 0);

                        fd = pqos_fopen(ut_file, modes[i]);
                        assert_non_null(fd);
                        assert_int_equal(ftell(fd), position);
                        assert_int_equal(fwrite("XY", 1, 2, fd), 2);
                        assert_int_equal(pqos_fclose(fd), 0);
                        ut_assert_contents(ut_file, "0123456789XY");
                }
        }

        ut_cleanup();
}

static void
test_common_pqos_open(void **state __attribute__((unused)))
{
        int fd;
        int error;

        ut_cleanup();

        /* an existing file is opened */
        {
                ut_write(ut_file, "contents\n");

                fd = pqos_open(ut_file, O_RDONLY, 0);
                assert_true(fd >= 0);
                assert_int_equal(close(fd), 0);
        }

        /* a symlink is refused, and what it points at is not truncated - the
         * implementation this replaced opened it first
         */
        {
                ut_write(ut_target, "keep me\n");
                assert_return_code(symlink(ut_target, ut_link), 0);

                fd = pqos_open(ut_link, O_WRONLY | O_TRUNC, 0);
                assert_int_equal(fd, -1);
                ut_assert_contents(ut_target, "keep me\n");

                unlink(ut_link);
                unlink(ut_target);
        }

        /* a missing file leaves the caller its errno */
        {
                fd = pqos_open(ut_absent, O_RDONLY, 0);
                error = errno;
                assert_int_equal(fd, -1);
                assert_int_equal(error, ENOENT);
        }

        /* O_CREAT is given a mode, so the file it creates is readable rather
         * than opened with whatever the stack held
         */
        {
                struct stat st;
                mode_t saved;

                unlink(ut_file);
                /* nothing masked out, so what is asserted is the mode the
                 * helper supplies rather than what this process's umask
                 * happens to leave of it
                 */
                saved = umask(0);
                fd = pqos_open(ut_file, O_WRONLY | O_CREAT, 0666);
                (void)umask(saved);

                assert_true(fd >= 0);
                assert_int_equal(close(fd), 0);
                assert_int_equal(stat(ut_file, &st), 0);
                assert_int_equal(st.st_mode & 07777, 0666);
        }

#ifdef O_TMPFILE
        /* O_TMPFILE asks open() for a mode as O_CREAT does, without carrying
         * O_CREAT in its bit pattern, so it has a branch of its own in the
         * helper and needs a case of its own here. The name it takes is the
         * directory to create the unnamed file in. A filesystem that will not
         * do it says so rather than failing the case.
         */
        {
                struct stat st;
                mode_t saved = umask(0);

                fd = pqos_open(work_dir, O_TMPFILE | O_RDWR, 0666);
                error = errno;
                (void)umask(saved);

                if (fd == -1 && (error == EOPNOTSUPP || error == EINVAL ||
                                 error == EISDIR)) {
                        print_message("O_TMPFILE unsupported here (%s), "
                                      "mode not checked\n",
                                      strerror(error));
                } else {
                        assert_true(fd >= 0);
                        assert_int_equal(fstat(fd, &st), 0);
                        assert_int_equal(st.st_mode & 07777, 0666);
                        assert_int_equal(close(fd), 0);
                }
        }
#endif

#ifdef O_PATH
        /* O_PATH is the flag O_NOFOLLOW does not refuse a link for - the pair
         * opens the link itself - so the descriptor is asked what it describes
         */
        {
                ut_write(ut_target, "keep me\n");
                assert_return_code(symlink(ut_target, ut_link), 0);

                fd = pqos_open(ut_link, O_PATH, 0);
                error = errno;
                assert_int_equal(fd, -1);
                assert_int_equal(error, ELOOP);

                /* and a regular file is still opened that way */
                fd = pqos_open(ut_target, O_PATH, 0);
                assert_true(fd >= 0);
                assert_int_equal(close(fd), 0);

                unlink(ut_link);
                unlink(ut_target);
        }
#endif
        /* and it is closed on exec - pqos_open() opens /dev/mem, which a child
         * of this process has no business inheriting
         */
        {
                ut_write(ut_file, "contents\n");

                fd = pqos_open(ut_file, O_RDONLY, 0);
                assert_true(fd >= 0);
                assert_true(fcntl(fd, F_GETFD) & FD_CLOEXEC);
                assert_int_equal(close(fd), 0);
        }

        /* no name is refused rather than handed to open() */
        {
                fd = pqos_open(NULL, O_RDONLY, 0);
                assert_int_equal(fd, -1);
                assert_int_equal(errno, EINVAL);
        }

        ut_cleanup();
}

/* The contract pqos_open() took over from the utility's safe_open(), which the
 * library's own cases did not cover before: what it does with a name that is
 * not there yet, with the flag combinations a caller may pick, and with a name
 * that changes under the call. The reasoning behind each of these was settled
 * in review of PSWD-19861 and is kept with them.
 */

/* A file the caller offered to create does not exist yet, which is the whole
 * point of asking for it: "pqos --log-file=<new path>" names a log the tool is
 * meant to open for the first time.
 */
static void
test_common_pqos_open_creates_a_missing_file(void **state
                                             __attribute__((unused)))
{
        struct stat st;
        int fd;

        unlink(ut_file);
        logged_clear();

        fd = pqos_open(ut_file, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
        assert_true(fd >= 0);
        assert_int_equal(write(fd, "logged\n", 7), 7);
        assert_int_equal(close(fd), 0);

        assert_int_equal(lstat(ut_file, &st), 0);
        assert_true(S_ISREG(st.st_mode));
        assert_int_equal(st.st_size, 7);
        assert_int_equal(unlink(ut_file), 0);
}

/* The symlink refusal cannot rest on the flags a caller happened to choose:
 * O_CREAT | O_EXCL answers EEXIST for a link, O_PATH opens the link itself and
 * O_DIRECTORY answers ENOTDIR, so every one of them has to arrive as ELOOP.
 */
static void
test_common_pqos_open_refuses_a_symlink_whatever_the_flags(
    void **state __attribute__((unused)))
{
        static const int flag_sets[] = {
            O_RDONLY,
            O_WRONLY | O_CREAT,
            O_WRONLY | O_CREAT | O_EXCL,
            O_WRONLY | O_CREAT | O_TRUNC,
#ifdef O_PATH
            O_PATH,
#endif
#ifdef O_DIRECTORY
            O_RDONLY | O_DIRECTORY,
#endif
        };
        unsigned i;

        for (i = 0; i < DIM(flag_sets); i++) {
                int fd;
                int error;

                unlink(ut_link);
                ut_write(ut_target, "keep me\n");
                assert_return_code(symlink(ut_target, ut_link), 0);
                logged_clear();
                errno = 0;

                fd = pqos_open(ut_link, flag_sets[i], S_IRUSR | S_IWUSR);
                error = errno;

                assert_int_equal(fd, -1);
                assert_int_equal(error, ELOOP);
                assert_non_null(strstr(logged, "is a symlink"));
                /* and what the link pointed at was not touched */
                ut_assert_contents(ut_target, "keep me\n");

                assert_int_equal(unlink(ut_link), 0);
                assert_int_equal(unlink(ut_target), 0);
        }
}

/* A link with no target is still a link, and refusing it must not create the
 * file it names - which a creating mode would otherwise do.
 */
static void
test_common_pqos_open_refuses_a_dangling_symlink(void **state
                                                 __attribute__((unused)))
{
        int fd;
        int error;

        unlink(ut_link);
        unlink(ut_target);
        assert_return_code(symlink(ut_target, ut_link), 0);
        logged_clear();
        errno = 0;

        fd = pqos_open(ut_link, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
        error = errno;

        assert_int_equal(fd, -1);
        assert_int_equal(error, ELOOP);
        assert_non_null(strstr(logged, "is a symlink"));
        assert_int_equal(access(ut_target, F_OK), -1);

        assert_int_equal(unlink(ut_link), 0);
}

/* An ELOOP can come from a loop in a directory of the path rather than from the
 * name itself, and the name is then not a link. Reporting one as the other
 * would send a caller looking at the wrong file.
 */
static void
test_common_pqos_open_looping_parent_is_not_a_link(void **state
                                                   __attribute__((unused)))
{
        char loop[PATH_MAX];
        char inside[PATH_MAX];
        int fd;

        /* both derived from the work directory rather than one from the other,
         * which would compose two PATH_MAX buffers and cannot be shown to fit
         */
        ut_path(loop, sizeof(loop), "loop");
        ut_path(inside, sizeof(inside), "loop/inside.txt");
        unlink(loop);
        assert_return_code(symlink(loop, loop), 0);
        logged_clear();
        errno = 0;

        fd = pqos_open(inside, O_RDONLY, 0);

        assert_int_equal(fd, -1);
        /* the leaf was never a link, so nothing is reported as one */
        assert_null(strstr(logged, "is a symlink"));

        assert_int_equal(unlink(loop), 0);
}

/* A link at the name under O_EXCL, where the refusal arrives as EEXIST rather
 * than ELOOP and would otherwise be reported as "the file exists". Nothing is
 * raced here: an exclusive create is one open, so the link is put there first
 * and what is being checked is which errno the caller is given.
 */
static void
test_common_pqos_open_names_a_late_symlink_under_o_excl(void **state
                                                        __attribute__((unused)))
{
        int fd;
        int error;

        unlink(ut_link);
        unlink(ut_target);
        assert_return_code(symlink(ut_target, ut_link), 0);
        logged_clear();
        errno = 0;

        fd = pqos_open(ut_link, O_WRONLY | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
        error = errno;

        assert_int_equal(fd, -1);
        assert_int_equal(error, ELOOP);
        assert_non_null(strstr(logged, "is a symlink"));
        /* and the name the link pointed at was not created */
        assert_int_equal(access(ut_target, F_OK), -1);

        assert_int_equal(unlink(ut_link), 0);
}

/* O_DIRECTORY turns the refusal into ENOTDIR, so an errno is not what decides
 * whether a name is a link - the name is. A link to a directory is what a
 * caller passing O_DIRECTORY runs into, and it has to arrive as ELOOP like any
 * other link.
 */
static void
test_common_pqos_open_names_a_link_under_o_directory(void **state
                                                     __attribute__((unused)))
{
#ifdef O_DIRECTORY
        char dir[PATH_MAX];
        int fd;
        int error;

        ut_path(dir, sizeof(dir), "race_dir");
        rmdir(dir);
        unlink(dir);
        assert_return_code(symlink(work_dir, dir), 0);
        logged_clear();
        errno = 0;

        fd = pqos_open(dir, O_RDONLY | O_DIRECTORY, 0);
        error = errno;

        assert_int_equal(fd, -1);
        /* the open said ENOTDIR; what is reported is what the name is */
        assert_int_equal(error, ELOOP);
        assert_non_null(strstr(logged, "is a symlink"));

        assert_int_equal(unlink(dir), 0);
#else
        skip();
#endif
}

/* What O_CREAT means here is what open() means by it: create the file, or open
 * what the name refers to.
 *
 * An earlier version of this function opened twice for a create that is not
 * exclusive - once without O_CREAT to see whether anything was there, then with
 * O_CREAT | O_EXCL - so that a file appearing in between was refused with
 * EEXIST rather than opened. It was taken out for two reasons, and this case
 * pins what replaced it. Removing O_CREAT for the first open changes what the
 * kernel makes of the rest, measured: O_RDONLY | O_CREAT | O_DIRECTORY is
 * EINVAL from open() and opens the directory without it. And the protection was
 * narrower than it appeared, since a file already at the name when the call
 * started was opened either way.
 *
 * A caller that needs the file to be new asks for O_EXCL, which is passed
 * through and refuses an existing name - the case below this one.
 */
static void
test_common_pqos_open_opens_a_file_that_is_there(void **state
                                                 __attribute__((unused)))
{
        static const char *const theirs = "already at the name\n";
        int fd;

        unlink(ut_file);
        ut_write(ut_file, theirs);
        logged_clear();
        errno = 0;

        fd = pqos_open(ut_file, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);

        assert_true(fd >= 0);
        assert_int_equal(close(fd), 0);
        /* opened, not refused, and not emptied either - O_TRUNC was not asked
         * for
         */
        ut_assert_contents(ut_file, theirs);
        assert_null(strstr(logged, "is a symlink"));

        /* and O_EXCL is how a caller says the file has to be new */
        errno = 0;
        fd = pqos_open(ut_file, O_WRONLY | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
        assert_int_equal(fd, -1);
        assert_int_equal(errno, EEXIST);

        assert_int_equal(unlink(ut_file), 0);
}

/* The caller's flags reach the kernel as given, so a combination open() refuses
 * is refused here too.
 *
 * O_RDONLY | O_CREAT | O_DIRECTORY is the one that caught this: open() answers
 * EINVAL for it, measured, and an earlier version of pqos_open() took O_CREAT
 * off in order to look before it created - which turned that refusal into an
 * open directory and a descriptor the caller was never meant to get. O_NOFOLLOW
 * and O_CLOEXEC are added now, and the only flag removed is O_TRUNC where
 * O_PATH is set with it, which open() ignores there anyway; O_CREAT in
 * particular travels untouched, which is why this case asks for a combination
 * whose meaning depends on it.
 *
 * What is compared is the platform's own answer rather than a fixed errno: the
 * case asks open() first and requires pqos_open() to agree, and skips where a
 * platform allows the pair. pqos.h documents the same example the same way.
 */
static void
test_common_pqos_open_passes_the_flags_as_given(void **state
                                                __attribute__((unused)))
{
#ifdef O_DIRECTORY
        char dir[PATH_MAX];
        int fd;
        int error;

        ut_path(dir, sizeof(dir), "flags_dir");
        rmdir(dir);
        unlink(dir);
        assert_int_equal(mkdir(dir, S_IRWXU), 0);
        logged_clear();

        /* what a direct open() says about these flags, on this platform */
        fd = open(dir, O_RDONLY | O_CREAT | O_DIRECTORY, S_IRUSR | S_IWUSR);
        error = errno;
        if (fd >= 0) {
                /* a platform that allows the pair has nothing to check here */
                assert_int_equal(close(fd), 0);
                assert_int_equal(rmdir(dir), 0);
                skip();
        }

        errno = 0;
        fd =
            pqos_open(dir, O_RDONLY | O_CREAT | O_DIRECTORY, S_IRUSR | S_IWUSR);

        /* the same refusal, for the same reason */
        assert_int_equal(fd, -1);
        assert_int_equal(errno, error);
        assert_null(strstr(logged, "is a symlink"));

        /* and without the flag the kernel objects to, the directory opens -
         * so what was refused above is the combination, not the directory
         */
        fd = pqos_open(dir, O_RDONLY | O_DIRECTORY, 0);
        assert_true(fd >= 0);
        assert_int_equal(close(fd), 0);

        assert_int_equal(rmdir(dir), 0);
#else
        skip();
#endif
}

/* Both of these run before pqos_init() in the utility - the log file and the
 * configuration file are opened with them, and the log the library writes to is
 * the first of those - so a refused link has to arrive as a return value and an
 * errno, with nothing said to a log that does not exist yet. log_printf() holds
 * the rest of the library to the opposite rule and a DEBUG build asserts it, so
 * without the ask in LOG_ERROR_IF_INIT() this ordering aborts the tool instead
 * of reporting the link.
 *
 * That assertion is where this case bites, and it is compiled in by
 * "make DEBUG=y" in this directory: built that way, removing the ask makes this
 * case abort here rather than fail an assertion below. Without DEBUG the case
 * still holds the contract - a refusal, the right errno, nothing logged - but a
 * regression would pass it, log_printf() returning quietly on its own.
 */
static void
test_common_refuses_a_symlink_before_the_log_exists(void **state
                                                    __attribute__((unused)))
{
        int fd;
        int error;
        int before;
        FILE *stream;

        ut_write(ut_target, "keep me\n");
        assert_return_code(symlink(ut_target, ut_link), 0);
        logged_clear();

        assert_int_equal(log_fini(), LOG_RETVAL_OK);
        before = log_printf_calls;

        errno = 0;
        fd = pqos_open(ut_link, O_WRONLY, 0);
        error = errno;
        assert_int_equal(fd, -1);
        assert_int_equal(error, ELOOP);

        errno = 0;
        stream = pqos_fopen(ut_link, "w");
        error = errno;
        assert_null(stream);
        assert_int_equal(error, ELOOP);

        /* log_printf() was not called, which is the claim: a guard that had
         * been removed would call it and be dropped inside it, writing nothing
         * either way, so the absence of output is not evidence on its own. Only
         * a DEBUG build turns that call into an abort, and the default build is
         * the one that gets run
         */
        assert_int_equal(log_printf_calls, before);

        assert_int_equal(
            log_init(-1, log_callback, NULL, LOG_VER_SUPER_VERBOSE),
            LOG_RETVAL_OK);

        /* nothing was written to a log that was not there, and the file the
         * link pointed at is as it was
         */
        assert_int_equal(strlen(logged), 0);
        ut_assert_contents(ut_target, "keep me\n");

        /* and the same refusal with a log to write to does call it, so the
         * count above is the guard working rather than the counter not
         */
        before = log_printf_calls;
        errno = 0;
        fd = pqos_open(ut_link, O_WRONLY, 0);
        assert_int_equal(fd, -1);
        assert_true(log_printf_calls > before);
        assert_non_null(strstr(logged, "is a symlink"));

        assert_int_equal(unlink(ut_link), 0);
        assert_int_equal(unlink(ut_target), 0);
}

/* A link is ELOOP wherever this runs, whichever of the two opened it: what
 * open() answers is ELOOP on Linux, EMLINK on FreeBSD and EEXIST for the
 * O_CREAT | O_EXCL an 'x' mode asks for, and a caller printing strerror() would
 * otherwise read out a different reason on each platform. The tool's own copy
 * of this normalized it too, and its message is what users have seen.
 */
static void
test_common_names_a_symlink_the_same_way_in_both(void **state
                                                 __attribute__((unused)))
{
        static const char *const modes[] = {"r", "w", "a", "w+", "wx"};
        unsigned i;
        int error;
        FILE *stream;

        ut_write(ut_target, "keep me\n");
        assert_return_code(symlink(ut_target, ut_link), 0);

        for (i = 0; i < DIM(modes); i++) {
                logged_clear();
                errno = 0;

                stream = pqos_fopen(ut_link, modes[i]);
                error = errno;

                assert_null(stream);
                assert_int_equal(error, ELOOP);
                assert_non_null(strstr(logged, "is a symlink"));
                ut_assert_contents(ut_target, "keep me\n");
        }

        assert_int_equal(unlink(ut_link), 0);
        assert_int_equal(unlink(ut_target), 0);
}

/* What O_TRUNC empties, and what it leaves alone. The truncation is part of the
 * open now, so the file emptied is the file the name resolved to at the moment
 * it was opened - the file open() itself would have truncated - and no other.
 *
 * A name replaced before the call is therefore served as the replacement rather
 * than refused: there is no window to notice, no comparison, and no EAGAIN. The
 * comparison this replaced could only see a replacement when the platform gave
 * it away, since a name unlinked and created again can carry the inode number
 * it had, so what it protected was never what it appeared to protect.
 *
 * The file the name used to refer to is kept reachable through a second name,
 * so that "and no other" is asserted rather than assumed.
 */
static void
test_common_pqos_open_truncates_what_it_opened(void **state
                                               __attribute__((unused)))
{
        static const char *const kept = "the file the name used to name\n";
        static const char *const raced = "the file that took the name\n";
        char hard[PATH_MAX];
        int fd;

        ut_path(hard, sizeof(hard), "hard_name");
        unlink(hard);
        ut_write(ut_link, kept);
        /* one inode, two names: the second outlives the rename below */
        assert_return_code(link(ut_link, hard), 0);
        ut_write(ut_target, raced);
        /* the replacement takes the name atomically, as another process
         * could
         */
        assert_return_code(rename(ut_target, ut_link), 0);
        logged_clear();
        errno = 0;

        fd = pqos_open(ut_link, O_WRONLY | O_TRUNC, 0);

        assert_true(fd >= 0);
        assert_int_equal(close(fd), 0);

        /* the file the name refers to is the one that was emptied */
        assert_int_equal(ut_size(ut_link), 0);
        /* and the one it used to refer to still has what it had */
        ut_assert_contents(hard, kept);
        assert_null(strstr(logged, "changed while"));

        assert_int_equal(unlink(ut_link), 0);
        assert_int_equal(unlink(hard), 0);
}

/* Truncating needs a descriptor that can write, since the truncation is done
 * through it. O_RDONLY | O_TRUNC is unspecified in POSIX - Linux empties the
 * file, FreeBSD does not - and either way ftruncate() would refuse the
 * descriptor, so the request is refused rather than half honoured.
 */
static void
test_common_pqos_open_refuses_a_read_only_truncate(void **state
                                                   __attribute__((unused)))
{
        int fd;
        int error;

        ut_write(ut_file, "untouched\n");
        errno = 0;

        fd = pqos_open(ut_file, O_RDONLY | O_TRUNC, 0);
        error = errno;

        assert_int_equal(fd, -1);
        assert_int_equal(error, EINVAL);
        ut_assert_contents(ut_file, "untouched\n");

        /* and the writable form of the same request works, on the file it named
         */
        fd = pqos_open(ut_file, O_WRONLY | O_TRUNC, 0);
        assert_true(fd >= 0);
        assert_int_equal(close(fd), 0);
        assert_int_equal(ut_size(ut_file), 0);

        /* a symlink is still named a symlink, whatever the flags say: the name
         * is looked at before the access mode is judged, so a caller guarding
         * against a planted link is told about the link rather than about its
         * own flags
         */
        {
                ut_write(ut_target, "keep me\n");
                assert_return_code(symlink(ut_target, ut_link), 0);
                logged_clear();
                errno = 0;

                fd = pqos_open(ut_link, O_RDONLY | O_TRUNC, 0);
                error = errno;

                assert_int_equal(fd, -1);
                assert_int_equal(error, ELOOP);
                assert_non_null(strstr(logged, "is a symlink"));
                ut_assert_contents(ut_target, "keep me\n");

                assert_int_equal(unlink(ut_link), 0);
                assert_int_equal(unlink(ut_target), 0);
        }

        assert_int_equal(unlink(ut_file), 0);
}

/* O_TRUNC is what open() would have applied, and open() applies it to a regular
 * file: it is ignored for a FIFO, which "pqos -o <fifo>" relies on, and
 * ftruncate() would answer EINVAL for one. So the file type decides, and a FIFO
 * opens as it did before this function deferred the truncation at all.
 */
static void
test_common_pqos_open_leaves_a_fifo_alone(void **state __attribute__((unused)))
{
        int fd;

        unlink(ut_file);
        assert_return_code(mkfifo(ut_file, S_IRUSR | S_IWUSR), 0);
        logged_clear();
        errno = 0;

        /* O_NONBLOCK so that opening it does not wait for the other end */
        fd = pqos_open(ut_file, O_RDWR | O_NONBLOCK | O_TRUNC, 0);
        assert_true(fd >= 0);
        assert_int_equal(strlen(logged), 0);
        assert_int_equal(close(fd), 0);

        assert_int_equal(unlink(ut_file), 0);
}

/* A request this cannot honour is named as such whether or not the file is
 * there: the flags are judged before the name's existence is answered, so a
 * missing name does not turn a refused flag combination into ENOENT. The order
 * of the three answers is a symbolic link first, then the flags, then
 * existence.
 */
static void
test_common_pqos_open_judges_flags_before_existence(void **state
                                                    __attribute__((unused)))
{
        int fd;
        int error;

        unlink(ut_absent);
        errno = 0;

        fd = pqos_open(ut_absent, O_RDONLY | O_TRUNC, 0);
        error = errno;
        assert_int_equal(fd, -1);
        assert_int_equal(error, EINVAL);

        /* a missing name the caller did not offer to create is still ENOENT */
        errno = 0;
        fd = pqos_open(ut_absent, O_WRONLY, 0);
        error = errno;
        assert_int_equal(fd, -1);
        assert_int_equal(error, ENOENT);
        assert_int_equal(access(ut_absent, F_OK), -1);
}

/* What the identity comparison is and is not. It catches a name that came to
 * refer to another file during the call; it cannot catch a hard link that was
 * already there, because a link is not a second file - both stats describe the
 * one inode, and truncating through either name empties it. The case is here so
 * that the promise is not read as more than it is.
 */
static void
test_common_pqos_open_truncates_through_a_hard_link(void **state
                                                    __attribute__((unused)))
{
        int fd;

        unlink(ut_link);
        ut_write(ut_target, "8 bytes\n");
        assert_return_code(link(ut_target, ut_link), 0);

        fd = pqos_open(ut_link, O_WRONLY | O_TRUNC, 0);
        assert_true(fd >= 0);
        assert_int_equal(close(fd), 0);

        /* the target is empty: the name that was opened is the same file */
        assert_int_equal(ut_size(ut_target), 0);

        assert_int_equal(unlink(ut_link), 0);
        assert_int_equal(unlink(ut_target), 0);
}

/* A link planted at the name after this created the file, which is served
 * rather than refused: the descriptor is the file that was created, the link
 * cannot reach through a descriptor, and what it points at stays untouched by a
 * write through one.
 *
 * This is where the redesign shows. What this replaced looked at the name again
 * after the open, found a link and answered ELOOP - refusing a descriptor that
 * was never in doubt, and telling a caller that the file it had just created
 * was a symbolic link. The name is not asked about what was opened any more.
 *
 * The plant lands immediately after the open that created the file, which is
 * the only open on the name: the harness fires it as that call returns.
 */
static void
test_common_pqos_open_names_a_planted_link(void **state __attribute__((unused)))
{
        static const char *const written = "into the file that was created\n";
        int fd;

        unlink(ut_link);
        ut_write(ut_target, "keep me\n");

        race_target = ut_target;
        race_open_link = ut_link;
        race_wrapper_ran = 0;
        logged_clear();
        errno = 0;

        fd = pqos_open(ut_link, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);

        race_open_link = NULL;

        /* a call that failed is a failure here, not an unsupported harness. The
         * plant fires only after an open that succeeded, so a regression that
         * makes pqos_open() fail leaves race_wrapper_ran clear, and judging the
         * plant first would report that as "this C library does not route
         * open() through the wrapper" and skip. The open is asserted first, and
         * the descriptor is closed on the path that really is unsupported.
         */
        assert_true(fd >= 0);

        /* -1 is the plant itself having failed - a symlink, unlink or write
         * that did not happen - which is a broken setup rather than an
         * unsupported harness. Skipping on it would turn a case that could not
         * be set up into a case that passed.
         */
        assert_int_not_equal(race_wrapper_ran, -1);

        if (race_wrapper_ran == 0) {
                assert_int_equal(close(fd), 0);
                unlink(ut_link);
                unlink(ut_target);
                skip();
        }

        assert_int_equal(write(fd, written, strlen(written)),
                         (int)strlen(written));
        assert_int_equal(close(fd), 0);

        /* nothing was said about a symlink, and the write reached the file this
         * created rather than the one the link names
         */
        assert_null(strstr(logged, "is a symlink"));
        assert_null(strstr(logged, "changed while"));
        ut_assert_contents(ut_target, "keep me\n");

        unlink(ut_link);
        assert_int_equal(unlink(ut_target), 0);
}

/* A write that lands the moment after this created the file is kept. The
 * truncation the caller asked for was part of the create - one open, on a file
 * that did not exist yet - so there is nothing left to apply afterwards, and
 * nothing that could erase what another process wrote once the name appeared.
 *
 * A deferred truncation is what would have that window, which is why this case
 * exists: the write lands immediately after the open that created the file.
 */
static void
test_common_pqos_open_keeps_an_early_write(void **state __attribute__((unused)))
{
        static const char *const theirs = "written by somebody else\n";
        int fd;

        unlink(ut_file);

        race_open_link = ut_file;
        race_written = theirs;
        race_wrapper_ran = 0;
        logged_clear();

        fd = pqos_open(ut_file, O_WRONLY | O_CREAT | O_TRUNC, 0600);

        race_open_link = NULL;
        race_written = NULL;

        /* a call that failed is a failure here, not an unsupported harness. The
         * plant fires only after an open that succeeded, so a regression that
         * makes pqos_open() fail leaves race_wrapper_ran clear, and judging the
         * plant first would report that as "this C library does not route
         * open() through the wrapper" and skip. The open is asserted first, and
         * the descriptor is closed on the path that really is unsupported.
         */
        assert_true(fd >= 0);

        /* -1 is the plant itself having failed - a symlink, unlink or write
         * that did not happen - which is a broken setup rather than an
         * unsupported harness. Skipping on it would turn a case that could not
         * be set up into a case that passed.
         */
        assert_int_not_equal(race_wrapper_ran, -1);

        if (race_wrapper_ran == 0) {
                assert_int_equal(close(fd), 0);
                unlink(ut_file);
                skip();
        }

        assert_int_equal(close(fd), 0);

        /* what they wrote is still there: this call created the file, so it had
         * nothing of its own to empty
         */
        ut_assert_contents(ut_file, theirs);

        assert_int_equal(unlink(ut_file), 0);
}

/* A trailing slash used to disarm the check, and the promise has to hold
 * however a caller spells the name. The kernel resolves the last component of
 * "link/" as a directory, so the link stops being the final component:
 * lstat("link/")
 * describes the target and open("link/", O_NOFOLLOW) follows the link. Both
 * functions therefore ask about the component that was named, with the slashes
 * taken off - while a name that really is a directory, or is nothing but
 * slashes, still opens.
 */
static void
test_common_pqos_open_refuses_a_link_behind_a_slash(void **state
                                                    __attribute__((unused)))
{
#ifdef O_DIRECTORY
        char dir[PATH_MAX];
        char slashed[PATH_MAX];
        char dir_slashed[PATH_MAX];
        FILE *stream;
        int fd;
        int error;

        ut_path(dir, sizeof(dir), "slash_dir");
        rmdir(dir);
        unlink(dir);
        unlink(ut_link);
        assert_int_equal(mkdir(dir, S_IRWXU), 0);
        assert_return_code(symlink(dir, ut_link), 0);
        snprintf(slashed, sizeof(slashed), "%s/", ut_link);
        snprintf(dir_slashed, sizeof(dir_slashed), "%s/", dir);

        /* the link, named with the slash that used to hide it */
        logged_clear();
        errno = 0;
        fd = pqos_open(slashed, O_RDONLY | O_DIRECTORY, 0);
        error = errno;
        assert_int_equal(fd, -1);
        assert_int_equal(error, ELOOP);
        assert_non_null(strstr(logged, "is a symlink"));

        /* and through the other function, which has no leaf check of its own */
        logged_clear();
        errno = 0;
        stream = pqos_fopen(slashed, "r");
        error = errno;
        assert_null(stream);
        assert_int_equal(error, ELOOP);
        assert_non_null(strstr(logged, "is a symlink"));

        /* a directory named with a trailing slash is still a directory */
        fd = pqos_open(dir_slashed, O_RDONLY | O_DIRECTORY, 0);
        assert_true(fd >= 0);
        assert_int_equal(close(fd), 0);

        /* and a name that is nothing but slashes is the root, which no link can
         * be
         */
        fd = pqos_open("/", O_RDONLY | O_DIRECTORY, 0);
        assert_true(fd >= 0);
        assert_int_equal(close(fd), 0);
        fd = pqos_open("//", O_RDONLY | O_DIRECTORY, 0);
        assert_true(fd >= 0);
        assert_int_equal(close(fd), 0);

        /* The window the leaf check used to leave open is gone rather than
         * covered: pqos_open() takes the slashes off and opens the component
         * that was named, once, so between deciding what to open and opening it
         * there is nothing left to overtake. A case arming a plant here would
         * have no call to fire on - the harness watches open(), and there is
         * only the one - so what used to be raced is asserted above instead,
         * with the link already at the name.
         */
        assert_int_equal(unlink(ut_link), 0);
#else
        skip();
#endif
}

/* The lowest descriptor number free at this moment
 *
 * A descriptor a call leaked is one missing from the pool, so the number a
 * dup() hands out afterwards is a number a leak moves. It is given straight
 * back, so asking does not change what the next call gets.
 */
static int
ut_free_fd(void)
{
        int fd = dup(1);

        assert_true(fd >= 0);
        assert_int_equal(close(fd), 0);

        return fd;
}

static void
test_common_pqos_fopen_refuses_a_null_mode_and_keeps_the_pool(
    void **state __attribute__((unused)))
{
        char dir[PATH_MAX];
        char slashed[PATH_MAX];
        char twice[PATH_MAX];
        FILE *stream;
        int free_before;
        int error;

        ut_path(dir, sizeof(dir), "null_mode_dir");
        rmdir(dir);
        unlink(dir);
        assert_int_equal(mkdir(dir, S_IRWXU), 0);
        snprintf(slashed, sizeof(slashed), "%s/", dir);
        snprintf(twice, sizeof(twice), "%s//", dir);

        free_before = ut_free_fd();

        /* the name as written, which the mode check has always refused */
        errno = 0;
        stream = pqos_fopen(dir, NULL);
        error = errno;
        assert_null(stream);
        assert_int_equal(error, EINVAL);

        /* and the same directory named with one trailing slash and with two.
         * This is a name that can be opened, so a mode judged after the name
         * had been dispatched on would answer NULL with a descriptor already
         * spent on it - three such calls cost three descriptors
         */
        errno = 0;
        stream = pqos_fopen(slashed, NULL);
        error = errno;
        assert_null(stream);
        assert_int_equal(error, EINVAL);

        errno = 0;
        stream = pqos_fopen(twice, NULL);
        error = errno;
        assert_null(stream);
        assert_int_equal(error, EINVAL);

        assert_int_equal(ut_free_fd(), free_before);

        assert_int_equal(rmdir(dir), 0);
}

int
main(void)
{
        int result = 0;

        const struct CMUnitTest tests_common[] = {
            cmocka_unit_test(test_common_pqos_strcat),
            cmocka_unit_test(test_common_pqos_file_exists),
            cmocka_unit_test(test_common_pqos_dir_exists),
            cmocka_unit_test(test_common_pqos_fgets),
            cmocka_unit_test(test_common_pqos_file_contains),
            cmocka_unit_test(test_common_pqos_fopen),
            cmocka_unit_test(test_common_pqos_open),
            cmocka_unit_test(test_common_pqos_open_creates_a_missing_file),
            cmocka_unit_test(
                test_common_pqos_open_refuses_a_symlink_whatever_the_flags),
            cmocka_unit_test(test_common_pqos_open_refuses_a_dangling_symlink),
            cmocka_unit_test(
                test_common_pqos_open_looping_parent_is_not_a_link),
            cmocka_unit_test(
                test_common_pqos_open_names_a_late_symlink_under_o_excl),
            cmocka_unit_test(
                test_common_pqos_open_names_a_link_under_o_directory),
            cmocka_unit_test(test_common_pqos_open_opens_a_file_that_is_there),
            cmocka_unit_test(test_common_pqos_open_passes_the_flags_as_given),
            cmocka_unit_test(
                test_common_refuses_a_symlink_before_the_log_exists),
            cmocka_unit_test(test_common_names_a_symlink_the_same_way_in_both),
            cmocka_unit_test(test_common_pqos_open_truncates_what_it_opened),
            cmocka_unit_test(
                test_common_pqos_open_refuses_a_read_only_truncate),
            cmocka_unit_test(test_common_pqos_open_leaves_a_fifo_alone),
            cmocka_unit_test(
                test_common_pqos_open_judges_flags_before_existence),
            cmocka_unit_test(
                test_common_pqos_open_truncates_through_a_hard_link),
            cmocka_unit_test(test_common_pqos_open_names_a_planted_link),
            cmocka_unit_test(test_common_pqos_open_keeps_an_early_write),
            cmocka_unit_test(
                test_common_pqos_open_refuses_a_link_behind_a_slash),
            cmocka_unit_test(
                test_common_pqos_fopen_refuses_a_null_mode_and_keeps_the_pool),
        };

        result +=
            cmocka_run_group_tests(tests_common, group_setup, group_teardown);

        return result;
}
