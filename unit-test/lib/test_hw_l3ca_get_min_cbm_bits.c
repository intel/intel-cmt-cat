/*
 * BSD LICENSE
 *
 * Copyright(c) 2020-2022 Intel Corporation. All rights reserved.
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

#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include "test.h"
#include "mock_cap.h"
#include "mock_machine.h"

#include "cpu_registers.h"
#include "allocation.h"

/* ======== mock ======== */

static struct pqos_l3ca l3ca;

int
hw_alloc_assoc_unused(const unsigned technology,
                      unsigned l3cat_id,
                      unsigned l2cat_id,
                      unsigned mba_id,
                      unsigned *class_id)
{
        int ret;
        check_expected(technology);
        check_expected(l3cat_id);
        check_expected(l2cat_id);
        check_expected(mba_id);

        ret = mock_type(int);
        if (ret == PQOS_RETVAL_OK)
                *class_id = mock_type(int);

        return ret;
}

int
hw_l3ca_get(const unsigned l3cat_id __attribute__((unused)),
            const unsigned max_num_ca __attribute__((unused)),
            unsigned *num_ca,
            struct pqos_l3ca *ca)
{
        *num_ca = 1;
        ca[0] = l3ca;

        return mock_type(int);
}

int
hw_l3ca_set(const unsigned l3cat_id __attribute__((unused)),
            const unsigned num_ca,
            const struct pqos_l3ca *ca)
{
        assert_int_equal(num_ca, 1);
        l3ca = ca[0];

        return mock_type(int);
}

/* ======== hw_l3ca_get_min_cbm_bits ======== */

static void
test_hw_l3ca_get_min_cbm_bits(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        int technology = 1 << PQOS_CAP_TYPE_L3CA;
        unsigned l3cat_id = 0;
        unsigned min_cbm_bits;
        unsigned expected_min_cbm_bits = 3;

        will_return(__wrap__pqos_cap_get, data->cap);
        will_return(__wrap__pqos_cap_get, data->cpu);

        expect_value(hw_alloc_assoc_unused, technology, technology);
        expect_value(hw_alloc_assoc_unused, l3cat_id, l3cat_id);
        expect_value(hw_alloc_assoc_unused, l2cat_id, 0);
        expect_value(hw_alloc_assoc_unused, mba_id, 0);
        will_return(hw_alloc_assoc_unused, PQOS_RETVAL_OK);
        will_return(hw_alloc_assoc_unused, 1);

        /* get cos configuration */
        will_return(hw_l3ca_get, PQOS_RETVAL_OK);

        /* probe for min value */
        will_return_count(hw_l3ca_set, PQOS_RETVAL_ERROR,
                          expected_min_cbm_bits - 1);
        will_return(hw_l3ca_get, PQOS_RETVAL_OK);

        /* restore cos configuration */
        will_return(hw_l3ca_set, PQOS_RETVAL_OK);

        ret = hw_l3ca_get_min_cbm_bits(&min_cbm_bits);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(min_cbm_bits, expected_min_cbm_bits);
}

static void
test_hw_l3ca_get_min_cbm_bits_no_free_cos(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        int technology = 1 << PQOS_CAP_TYPE_L3CA;
        unsigned min_cbm_bits;
        unsigned i;

        will_return(__wrap__pqos_cap_get, data->cap);
        will_return(__wrap__pqos_cap_get, data->cpu);

        for (i = 0; i < 2; ++i) {
                expect_value(hw_alloc_assoc_unused, technology, technology);
                expect_value(hw_alloc_assoc_unused, l3cat_id, i);
                expect_value(hw_alloc_assoc_unused, l2cat_id, 0);
                expect_value(hw_alloc_assoc_unused, mba_id, 0);
                will_return(hw_alloc_assoc_unused, PQOS_RETVAL_RESOURCE);
        }

        ret = hw_l3ca_get_min_cbm_bits(&min_cbm_bits);
        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);
}

int
main(void)
{
        int result = 0;

        const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_hw_l3ca_get_min_cbm_bits),
            cmocka_unit_test(test_hw_l3ca_get_min_cbm_bits_no_free_cos)};

        result += cmocka_run_group_tests(tests, test_init_l3ca, test_fini);

        return result;
}