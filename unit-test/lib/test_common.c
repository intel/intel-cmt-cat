/*
 *  BSD  LICENSE
 *
 * Copyright(c) 2022-2023 Intel Corporation. All rights reserved.
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

static void
test_common_pqos_fopen(void **state __attribute__((unused)))
{
        FILE *fd;

        /* file does not exists */
        {
                const char *path = "/proc/file_that_doesnt_exist";

                fd = pqos_fopen(path, "r");
                assert_null(fd);
        }

        /* directory */
        {
                const char *path = "/proc";

                expect_string(__wrap_fopen, name, path);
                expect_string(__wrap_fopen, mode, "r");
                will_return(__wrap_fopen, NULL);

                fd = pqos_fopen(path, "r");
                assert_null(fd);
        }

        /* symlink */
        {
                const char *path = "/tmp/pqos_ut_symlink";

                unlink(path);
                assert_return_code(symlink("/proc/cpuinfo", path), 0);

                expect_string(__wrap_fopen, name, path);
                expect_string(__wrap_fopen, mode, "r");
                will_return(__wrap_fopen, (FILE *)1);

                fd = pqos_fopen(path, "r");
                assert_null(fd);

                unlink(path);
        }

        /* normal file */
        {
                const char *path = "/proc/cpuinfo";

                expect_string(__wrap_fopen, name, path);
                expect_string(__wrap_fopen, mode, "r");
                will_return(__wrap_fopen, (FILE *)1);

                fd = pqos_fopen(path, "r");
                assert_non_null(fd);

                pqos_fclose(fd);
        }
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
        };

        result += cmocka_run_group_tests(tests_common, NULL, NULL);

        return result;
}
