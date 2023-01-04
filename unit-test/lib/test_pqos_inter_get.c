/*
 * BSD LICENSE
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

#include "cap.h"
#include "mock_cap.h"
#include "test.h"

/* ======== mock  ======== */

int
_pqos_check_init(const int expect)
{
        return __wrap__pqos_check_init(expect);
}

/* ======== pqos_inter_get  ======== */

static void
test_pqos_inter_get_os(void **state __attribute__((unused)))
{
        int ret;
        enum pqos_interface interface;

        _pqos_set_inter(PQOS_INTER_OS);

        expect_function_call(__wrap_lock_get);
        expect_value(__wrap__pqos_check_init, expect, 1);
        will_return(__wrap__pqos_check_init, PQOS_RETVAL_OK);
        expect_function_call(__wrap_lock_release);

        ret = pqos_inter_get(&interface);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(interface, _pqos_get_inter());
}

static void
test_pqos_inter_get_msr(void **state __attribute__((unused)))
{
        int ret;
        enum pqos_interface interface;

        _pqos_set_inter(PQOS_INTER_MSR);

        expect_function_call(__wrap_lock_get);
        expect_value(__wrap__pqos_check_init, expect, 1);
        will_return(__wrap__pqos_check_init, PQOS_RETVAL_OK);
        expect_function_call(__wrap_lock_release);

        ret = pqos_inter_get(&interface);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(interface, _pqos_get_inter());
}

static void
test_pqos_inter_get_init(void **state __attribute__((unused)))
{
        int ret;
        enum pqos_interface interface;

        expect_function_call(__wrap_lock_get);
        expect_value(__wrap__pqos_check_init, expect, 1);
        will_return(__wrap__pqos_check_init, PQOS_RETVAL_INIT);
        expect_function_call(__wrap_lock_release);

        ret = pqos_inter_get(&interface);
        assert_int_equal(ret, PQOS_RETVAL_INIT);
}

static void
test_pqos_inter_get_param(void **state __attribute__((unused)))
{
        int ret;

        ret = pqos_inter_get(NULL);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
}

int
main(void)
{
        int result = 0;

        const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_pqos_inter_get_os),
            cmocka_unit_test(test_pqos_inter_get_msr),
            cmocka_unit_test(test_pqos_inter_get_init),
            cmocka_unit_test(test_pqos_inter_get_param),
        };

        result += cmocka_run_group_tests(tests, NULL, NULL);

        return result;
}
