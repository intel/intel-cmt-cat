/*
 * BSD  LICENSE
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
#include "mock_common.h"
#include "pqos.h"
#include "test.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

FILE *
pqos_fopen(const char *name, const char *mode)
{
        return __wrap_pqos_fopen(name, mode);
}

int
pqos_fclose(FILE *fd)
{
        return __wrap_pqos_fclose(fd);
}

int
__wrap_feof(FILE *stream)
{
        assert_non_null(stream);

        return mock_type(int);
}

static void
test_common_pqos_fread_uint64(void **state __attribute__((unused)))
{
        int return_value;
        uint64_t value_param = 123456789;
        uint64_t expected_value;
        const char *path = "/tmp/path";

        expect_string(__wrap_pqos_fopen, name, path);
        expect_string(__wrap_pqos_fopen, mode, "r");
        will_return(__wrap_pqos_fopen, "9999999999999999");
        expect_function_call(__wrap_pqos_fclose);
        will_return(__wrap_pqos_fclose, 0);
        expected_value = 9999999999999999;
        return_value = pqos_fread_uint64(path, 10, &value_param);
        assert_int_equal(return_value, PQOS_RETVAL_OK);
        assert_int_equal(value_param, expected_value);

        expect_string(__wrap_pqos_fopen, name, path);
        expect_string(__wrap_pqos_fopen, mode, "r");
        will_return(__wrap_pqos_fopen, "");
        expect_function_call(__wrap_pqos_fclose);
        will_return(__wrap_pqos_fclose, 0);
        return_value = pqos_fread_uint64(path, 16, &value_param);
        assert_int_equal(return_value, PQOS_RETVAL_ERROR);

        expect_string(__wrap_pqos_fopen, name, path);
        expect_string(__wrap_pqos_fopen, mode, "r");
        will_return(__wrap_pqos_fopen, "fffffffffffffffe");
        expect_function_call(__wrap_pqos_fclose);
        will_return(__wrap_pqos_fclose, 0);
        expected_value = 0xFFFFFFFFFFFFFFFE;
        return_value = pqos_fread_uint64(path, 16, &value_param);
        assert_int_equal(return_value, PQOS_RETVAL_OK);
        assert_int_equal(value_param, expected_value);

        expect_string(__wrap_pqos_fopen, name, path);
        expect_string(__wrap_pqos_fopen, mode, "r");
        will_return(__wrap_pqos_fopen, "\n");
        expect_function_call(__wrap_pqos_fclose);
        will_return(__wrap_pqos_fclose, 0);
        return_value = pqos_fread_uint64(path, 16, &value_param);
        assert_int_equal(return_value, PQOS_RETVAL_ERROR);

        expect_string(__wrap_pqos_fopen, name, path);
        expect_string(__wrap_pqos_fopen, mode, "r");
        will_return(__wrap_pqos_fopen, "invalid");
        expect_function_call(__wrap_pqos_fclose);
        will_return(__wrap_pqos_fclose, 0);
        return_value = pqos_fread_uint64(path, 16, &value_param);
        assert_int_equal(return_value, PQOS_RETVAL_ERROR);

        expect_string(__wrap_pqos_fopen, name, path);
        expect_string(__wrap_pqos_fopen, mode, "r");
        will_return(__wrap_pqos_fopen, NULL);
        return_value = pqos_fread_uint64(path, 16, &value_param);
        assert_int_equal(return_value, PQOS_RETVAL_ERROR);
}

/* ======== pqos_fread_uint ======== */

static void
test_common_pqos_fread_uint(void **state __attribute__((unused)))
{
        int ret;
        unsigned value;
        const char *path = "/tmp/path";

        expect_string(__wrap_pqos_fopen, name, path);
        expect_string(__wrap_pqos_fopen, mode, "r");
        will_return(__wrap_pqos_fopen, "123\n");
        expect_function_call(__wrap_pqos_fclose);
        will_return(__wrap_pqos_fclose, 0);

        ret = pqos_fread_uint(path, &value);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(value, 123);
}

static void
test_common_pqos_fread_uint_error(void **state __attribute__((unused)))
{
        int ret;
        unsigned value;
        const char *path = "/tmp/path";

        /* could not open file */
        expect_string(__wrap_pqos_fopen, name, path);
        expect_string(__wrap_pqos_fopen, mode, "r");
        will_return(__wrap_pqos_fopen, NULL);

        ret = pqos_fread_uint(path, &value);
        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);
}

static void
test_common_pqos_fread_uint_invalid(void **state __attribute__((unused)))
{
        int ret;
        unsigned value;
        const char *path = "/tmp/path";

        expect_string(__wrap_pqos_fopen, name, path);
        expect_string(__wrap_pqos_fopen, mode, "r");
        will_return(__wrap_pqos_fopen, "invalid\n");
        expect_function_call(__wrap_pqos_fclose);
        will_return(__wrap_pqos_fclose, 0);

        ret = pqos_fread_uint(path, &value);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
}

int
main(void)
{
        int result = 0;

        const struct CMUnitTest tests_common_fread_uint64[] = {
            cmocka_unit_test(test_common_pqos_fread_uint64),
            cmocka_unit_test(test_common_pqos_fread_uint),
            cmocka_unit_test(test_common_pqos_fread_uint_error),
            cmocka_unit_test(test_common_pqos_fread_uint_invalid),
        };

        result += cmocka_run_group_tests(tests_common_fread_uint64, NULL, NULL);

        return result;
}
