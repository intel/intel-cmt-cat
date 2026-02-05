/*
 * BSD LICENSE
 *
 * Copyright(c) 2020-2026 Intel Corporation. All rights reserved.
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

#include "allocation.h"
#include "test.h"

/* ======== mock ======== */

int
hw_alloc_reset_assoc_cores(void)
{
        function_called();
        return mock_type(int);
}

int
hw_alloc_reset_assoc_channels(void)
{
        function_called();
        return mock_type(int);
}

/* ======== hw_alloc_reset_assoc ======== */

static void
test_hw_alloc_reset_assoc(void **state __attribute__((unused)))
{
        int ret;

        expect_function_call(hw_alloc_reset_assoc_cores);
        will_return(hw_alloc_reset_assoc_cores, PQOS_RETVAL_OK);

        expect_function_call(hw_alloc_reset_assoc_channels);
        will_return(hw_alloc_reset_assoc_channels, PQOS_RETVAL_OK);

        ret = hw_alloc_reset_assoc();
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_hw_alloc_reset_assoc_error(void **state __attribute__((unused)))
{
        int ret;

        /* hw_alloc_reset_assoc_cores fails */
        expect_function_call(hw_alloc_reset_assoc_cores);
        will_return(hw_alloc_reset_assoc_cores, PQOS_RETVAL_ERROR);

        expect_function_call(hw_alloc_reset_assoc_channels);
        will_return(hw_alloc_reset_assoc_channels, PQOS_RETVAL_OK);

        ret = hw_alloc_reset_assoc();
        assert_int_equal(ret, PQOS_RETVAL_ERROR);

        /* hw_alloc_reset_assoc_channels fails */
        expect_function_call(hw_alloc_reset_assoc_cores);
        will_return(hw_alloc_reset_assoc_cores, PQOS_RETVAL_OK);

        expect_function_call(hw_alloc_reset_assoc_channels);
        will_return(hw_alloc_reset_assoc_channels, PQOS_RETVAL_RESOURCE);

        ret = hw_alloc_reset_assoc();
        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);
}

int
main(void)
{
        int result = 0;

        const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_hw_alloc_reset_assoc),
            cmocka_unit_test(test_hw_alloc_reset_assoc_error)};

        result += cmocka_run_group_tests(tests, NULL, NULL);

        return result;
}
