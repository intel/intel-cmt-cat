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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define FILE_DEAD ((FILE *)0xDEAD)

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

        return 0;
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
                static const char *const accepted[] = {"wb", "w+b", "wb+", "wx",
                                                       "w+bx"};
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

                fd = pqos_open(ut_file, O_RDONLY);
                assert_true(fd >= 0);
                assert_int_equal(close(fd), 0);
        }

        /* a symlink is refused, and what it points at is not truncated - the
         * implementation this replaced opened it first
         */
        {
                ut_write(ut_target, "keep me\n");
                assert_return_code(symlink(ut_target, ut_link), 0);

                fd = pqos_open(ut_link, O_WRONLY | O_TRUNC);
                assert_int_equal(fd, -1);
                ut_assert_contents(ut_target, "keep me\n");

                unlink(ut_link);
                unlink(ut_target);
        }

        /* a missing file leaves the caller its errno */
        {
                fd = pqos_open(ut_absent, O_RDONLY);
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
                fd = pqos_open(ut_file, O_WRONLY | O_CREAT);
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

                fd = pqos_open(work_dir, O_TMPFILE | O_RDWR);
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

                fd = pqos_open(ut_link, O_PATH);
                error = errno;
                assert_int_equal(fd, -1);
                assert_int_equal(error, ELOOP);

                /* and a regular file is still opened that way */
                fd = pqos_open(ut_target, O_PATH);
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

                fd = pqos_open(ut_file, O_RDONLY);
                assert_true(fd >= 0);
                assert_true(fcntl(fd, F_GETFD) & FD_CLOEXEC);
                assert_int_equal(close(fd), 0);
        }

        /* no name is refused rather than handed to open() */
        {
                fd = pqos_open(NULL, O_RDONLY);
                assert_int_equal(fd, -1);
                assert_int_equal(errno, EINVAL);
        }

        ut_cleanup();
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
        };

        result +=
            cmocka_run_group_tests(tests_common, group_setup, group_teardown);

        return result;
}
