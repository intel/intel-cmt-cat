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

        expect_string(__wrap_fopen, name, path);
        expect_string(__wrap_fopen, mode, "r");
        will_return(__wrap_fopen, FILE_DEAD);
        will_return(__wrap_fgets, "Test string");
        will_return(__wrap_fclose, 0);
        ret_value = pqos_file_contains(path, search_str1, &found_param);
        assert_int_equal(ret_value, PQOS_RETVAL_OK);
        assert_int_equal(found_param, 1);

        expect_string(__wrap_fopen, name, path);
        expect_string(__wrap_fopen, mode, "r");
        will_return(__wrap_fopen, FILE_DEAD);
        will_return(__wrap_fgets, "test string");
        will_return(__wrap_fgets, "");
        will_return(__wrap_fclose, 0);
        ret_value = pqos_file_contains(path, search_str1, &found_param);
        assert_int_equal(ret_value, PQOS_RETVAL_OK);
        assert_int_equal(found_param, 0);

        expect_string(__wrap_fopen, name, path);
        expect_string(__wrap_fopen, mode, "r");
        will_return(__wrap_fopen, NULL);
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

/* The window between pqos_open()'s lstat() and its open() cannot be hit by a
 * caller, so lstat() is wrapped: the first call on the armed name answers
 * truthfully and then puts a symlink there, which is exactly the race the
 * O_NOFOLLOW is for. The pass-through is fstatat() rather than __real_lstat,
 * which would not link where lstat is not an exported symbol; a C library that
 * does not route lstat() through the wrapper at all - it is exported from glibc
 * 2.33 - leaves race_wrapper_ran clear, and the cases below skip rather than
 * pretend.
 */
static const char *race_link = NULL;
static const char *race_target = NULL;
static const char *race_replacement = NULL;
static const char *race_written = NULL;
static int race_regular;
static int race_skip;
static int race_early;
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
        } else if (race_replacement != NULL) {
                /* an atomic swap by a file that already exists, so the name
                 * comes to refer to another inode. Unlinking the name and
                 * creating it again would not do: the inode number is free by
                 * then and the filesystem hands it straight back, leaving the
                 * identity comparison looking at what it expected
                 */
                if (rename(race_replacement, race_link) != 0)
                        race_wrapper_ran = -1;
        } else if (race_regular) {
                int raced = creat(race_link, S_IRUSR | S_IWUSR);

                if (raced == -1)
                        race_wrapper_ran = -1;
                else
                        close(raced);
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

int
__wrap_lstat(const char *pathname, struct stat *buf)
{
        int armed = race_link != NULL && strcmp(pathname, race_link) == 0;
        int ret;
        int error;

        /* a case interested in a later window says how many calls on the name
         * to let past untouched: pqos_open() lstat()s a name it creates a
         * second time, after the open, and that window is a different one
         */
        if (armed && race_skip > 0) {
                race_skip--;
                armed = 0;
        }

        /* planted before the answer where the case is about what this call
         * reports, and after it where the case is about what the open() that
         * follows finds - which is the window O_NOFOLLOW is for
         */
        if (armed && race_early)
                race_plant();

        ret = fstatat(AT_FDCWD, pathname, buf, AT_SYMLINK_NOFOLLOW);
        error = errno;

        if (armed && !race_early)
                race_plant();

        errno = error;

        return ret;
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

/* The window the O_NOFOLLOW is for: a link planted after the lstat() has to be
 * refused by the open rather than followed, and the file it points at must be
 * left alone.
 */
static void
test_common_pqos_open_refuses_a_late_symlink(void **state
                                             __attribute__((unused)))
{
        int fd;
        int error;

        unlink(ut_link);
        unlink(ut_target);
        race_target = ut_target;
        race_link = ut_link;
        race_regular = 0;
        race_wrapper_ran = 0;
        logged_clear();
        errno = 0;

        fd = pqos_open(ut_link, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
        error = errno;

        race_link = NULL;
        if (race_wrapper_ran != 1) {
                unlink(ut_link);
                unlink(ut_target);
                skip();
        }

        assert_int_equal(fd, -1);
        assert_int_equal(error, ELOOP);
        assert_non_null(strstr(logged, "is a symlink"));
        assert_int_equal(access(ut_target, F_OK), -1);

        assert_int_equal(unlink(ut_link), 0);
}

/* The same race under O_EXCL, where the refusal arrives as EEXIST rather than
 * ELOOP and would otherwise be reported as "the file exists".
 */
static void
test_common_pqos_open_names_a_late_symlink_under_o_excl(void **state
                                                        __attribute__((unused)))
{
        int fd;
        int error;

        unlink(ut_link);
        unlink(ut_target);
        race_target = ut_target;
        race_link = ut_link;
        race_regular = 0;
        race_wrapper_ran = 0;
        logged_clear();
        errno = 0;

        fd = pqos_open(ut_link, O_WRONLY | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
        error = errno;

        race_link = NULL;
        if (race_wrapper_ran != 1) {
                unlink(ut_link);
                unlink(ut_target);
                skip();
        }

        assert_int_equal(fd, -1);
        assert_int_equal(error, ELOOP);
        assert_non_null(strstr(logged, "is a symlink"));

        assert_int_equal(unlink(ut_link), 0);
}

/* O_DIRECTORY turns the refusal into ENOTDIR, so an errno is not what decides
 * whether a name is a link - the name is. Here a directory is replaced by a
 * link to one inside the window, which is the shape a path component takes.
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
        assert_int_equal(mkdir(dir, S_IRWXU), 0);
        race_target = work_dir;
        race_link = dir;
        race_regular = 0;
        race_wrapper_ran = 0;
        logged_clear();
        errno = 0;

        fd = pqos_open(dir, O_RDONLY | O_DIRECTORY, 0);
        error = errno;

        race_link = NULL;
        if (race_wrapper_ran != 1) {
                rmdir(dir);
                unlink(dir);
                skip();
        }

        assert_int_equal(fd, -1);
        /* the open said ENOTDIR; what is reported is what the name is */
        assert_int_equal(error, ELOOP);
        assert_non_null(strstr(logged, "is a symlink"));

        assert_int_equal(unlink(dir), 0);
#else
        skip();
#endif
}

/* A regular file appearing in the window is not a link, and adopting it would
 * hand the caller a file it never named. It is reported as the name having
 * changed, which is what happened.
 */
static void
test_common_pqos_open_does_not_adopt_a_late_file(void **state
                                                 __attribute__((unused)))
{
        int fd;
        int error;

        unlink(ut_link);
        race_target = NULL;
        race_link = ut_link;
        race_regular = 1;
        race_wrapper_ran = 0;
        logged_clear();
        errno = 0;

        fd = pqos_open(ut_link, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
        error = errno;

        race_link = NULL;
        race_regular = 0;
        if (race_wrapper_ran != 1) {
                unlink(ut_link);
                skip();
        }

        assert_int_equal(fd, -1);
        assert_int_equal(error, EEXIST);
        assert_null(strstr(logged, "is a symlink"));

        assert_int_equal(unlink(ut_link), 0);
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

/* A name replaced under the call is refused - and where the caller asked for
 * O_TRUNC that has to mean refused before anything is emptied. open() applies
 * O_TRUNC to whatever the name refers to by then, so the file that took the
 * name's place would come back empty and the refusal would arrive too late to
 * matter. The truncation is done through the descriptor this call identified.
 */
static void
test_common_pqos_open_does_not_empty_a_late_file(void **state
                                                 __attribute__((unused)))
{
        static const char *const raced = "the file that took the name\n";
        int fd;
        int error;

        ut_write(ut_link, "the file the caller named\n");
        ut_write(ut_target, raced);

        race_target = NULL;
        race_link = ut_link;
        race_replacement = ut_target;
        race_wrapper_ran = 0;
        logged_clear();
        errno = 0;

        fd = pqos_open(ut_link, O_WRONLY | O_TRUNC, 0);
        error = errno;

        race_link = NULL;
        race_replacement = NULL;
        if (race_wrapper_ran != 1) {
                unlink(ut_link);
                skip();
        }

        assert_int_equal(fd, -1);
        assert_int_equal(error, EAGAIN);

        /* what the refusal is worth: the file that took the name still has
         * what it had
         */
        ut_assert_contents(ut_link, raced);

        assert_int_equal(unlink(ut_link), 0);
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

/* A link planted at the name after this created the file. The descriptor is the
 * regular file it made, so the identity comparison would find a mismatch and
 * call it a change - true, but less than the caller is promised: a name found
 * to be a symbolic link is ELOOP however the call arrived at it. The window is
 * the second lstat() of the name, the one that follows the open, and what
 * matters here is the answer that lstat() gives rather than what the open
 * finds, so the harness plants the link before answering.
 */
static void
test_common_pqos_open_names_a_planted_link(void **state __attribute__((unused)))
{
        int fd;
        int error;

        unlink(ut_link);
        ut_write(ut_target, "keep me\n");

        race_target = ut_target;
        race_link = ut_link;
        race_skip = 1;
        race_early = 1;
        race_wrapper_ran = 0;
        logged_clear();
        errno = 0;

        fd = pqos_open(ut_link, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
        error = errno;

        race_link = NULL;
        race_skip = 0;
        race_early = 0;
        if (race_wrapper_ran != 1) {
                unlink(ut_link);
                unlink(ut_target);
                skip();
        }

        assert_int_equal(fd, -1);
        assert_int_equal(error, ELOOP);
        assert_non_null(strstr(logged, "is a symlink"));
        assert_null(strstr(logged, "changed while"));

        /* and what the link points at was not touched */
        ut_assert_contents(ut_target, "keep me\n");

        unlink(ut_link);
        assert_int_equal(unlink(ut_target), 0);
}

/* The window a deferred truncation opens after a create, which open() does not
 * have: O_EXCL made the file, so it was empty and there is nothing to truncate,
 * but the name is visible from the moment open() returns. Emptying it after
 * that would erase what another process wrote in between - data the create
 * open() performs in one step would have kept. The write lands on the second
 * lstat() of the name, the one that follows the open.
 */
static void
test_common_pqos_open_keeps_an_early_write(void **state __attribute__((unused)))
{
        static const char *const theirs = "written by somebody else\n";
        int fd;

        unlink(ut_file);

        race_link = ut_file;
        race_written = theirs;
        race_skip = 1;
        race_early = 1;
        race_wrapper_ran = 0;
        logged_clear();

        fd = pqos_open(ut_file, O_WRONLY | O_CREAT | O_TRUNC, 0600);

        race_link = NULL;
        race_written = NULL;
        race_skip = 0;
        race_early = 0;
        if (race_wrapper_ran != 1) {
                unlink(ut_file);
                skip();
        }

        assert_true(fd >= 0);
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

        /* and the window the check used to leave open: the leaf is what gets
         * opened now, so a link put there after the lstat() is refused by the
         * O_NOFOLLOW on that open - a check of the leaf followed by an open of
         * the name as written could only have been advisory
         */
        assert_int_equal(rmdir(dir), 0);
        assert_int_equal(mkdir(dir, S_IRWXU), 0);
        race_target = work_dir;
        race_link = dir;
        race_regular = 0;
        race_wrapper_ran = 0;
        logged_clear();
        errno = 0;

        fd = pqos_open(dir_slashed, O_RDONLY | O_DIRECTORY, 0);
        error = errno;

        race_link = NULL;
        if (race_wrapper_ran == 1) {
                assert_int_equal(fd, -1);
                assert_int_equal(error, ELOOP);
                assert_non_null(strstr(logged, "is a symlink"));
                unlink(dir);
        } else {
                if (fd >= 0)
                        assert_int_equal(close(fd), 0);
                rmdir(dir);
        }

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
            cmocka_unit_test(test_common_pqos_open_refuses_a_late_symlink),
            cmocka_unit_test(
                test_common_pqos_open_names_a_late_symlink_under_o_excl),
            cmocka_unit_test(
                test_common_pqos_open_names_a_link_under_o_directory),
            cmocka_unit_test(test_common_pqos_open_does_not_adopt_a_late_file),
            cmocka_unit_test(
                test_common_refuses_a_symlink_before_the_log_exists),
            cmocka_unit_test(test_common_names_a_symlink_the_same_way_in_both),
            cmocka_unit_test(test_common_pqos_open_does_not_empty_a_late_file),
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
