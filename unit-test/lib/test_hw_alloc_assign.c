/*
 * BSD LICENSE
 *
 * Copyright(c) 2020-2023 Intel Corporation. All rights reserved.
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
hw_alloc_assoc_write(const unsigned lcore, const unsigned class_id)
{
        check_expected(lcore);
        check_expected(class_id);

        return mock_type(int);
}

int
hw_alloc_assoc_unused(const unsigned technology,
                      unsigned l3cat_id,
                      unsigned l2cat_id,
                      unsigned mba_id,
                      unsigned smba_id,
                      unsigned *class_id)
{
        int ret;

        check_expected(technology);
        check_expected(l3cat_id);
        check_expected(l2cat_id);
        check_expected(mba_id);
        check_expected(smba_id);

        ret = mock_type(int);
        if (ret == PQOS_RETVAL_OK)
                *class_id = mock_type(int);

        return ret;
}

/* ======== hw_alloc_reset_assoc ======== */

static void
hw_alloc_assign_l2ca(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        unsigned technology = 1 << PQOS_CAP_TYPE_L2CA;
        unsigned core_array[] = {3};
        unsigned core_num = 1;
        unsigned class_id;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);

        expect_value(hw_alloc_assoc_unused, technology, technology);
        expect_value(hw_alloc_assoc_unused, l3cat_id, 0);
        expect_value(hw_alloc_assoc_unused, l2cat_id, 1);
        expect_value(hw_alloc_assoc_unused, mba_id, 0);
        expect_value(hw_alloc_assoc_unused, smba_id, 0);
        will_return(hw_alloc_assoc_unused, PQOS_RETVAL_OK);
        will_return(hw_alloc_assoc_unused, 1);

        expect_value(hw_alloc_assoc_write, lcore, core_array[0]);
        expect_value(hw_alloc_assoc_write, class_id, 1);
        will_return(hw_alloc_assoc_write, PQOS_RETVAL_OK);

        ret = hw_alloc_assign(technology, core_array, core_num, &class_id);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(class_id, 1);
}

static void
hw_alloc_assign_l3ca(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        unsigned technology = 1 << PQOS_CAP_TYPE_L3CA;
        unsigned core_array[] = {3};
        unsigned core_num = 1;
        unsigned class_id;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);

        expect_value(hw_alloc_assoc_unused, technology, technology);
        expect_value(hw_alloc_assoc_unused, l3cat_id, 0);
        expect_value(hw_alloc_assoc_unused, l2cat_id, 0);
        expect_value(hw_alloc_assoc_unused, mba_id, 0);
        expect_value(hw_alloc_assoc_unused, smba_id, 0);
        will_return(hw_alloc_assoc_unused, PQOS_RETVAL_OK);
        will_return(hw_alloc_assoc_unused, 1);

        expect_value(hw_alloc_assoc_write, lcore, core_array[0]);
        expect_value(hw_alloc_assoc_write, class_id, 1);
        will_return(hw_alloc_assoc_write, PQOS_RETVAL_OK);

        ret = hw_alloc_assign(technology, core_array, core_num, &class_id);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(class_id, 1);
}

static void
hw_alloc_assign_mba(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        unsigned technology = 1 << PQOS_CAP_TYPE_MBA;
        unsigned core_array[] = {5};
        unsigned core_num = 1;
        unsigned class_id;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);

        expect_value(hw_alloc_assoc_unused, technology, technology);
        expect_value(hw_alloc_assoc_unused, l3cat_id, 0);
        expect_value(hw_alloc_assoc_unused, l2cat_id, 0);
        expect_value(hw_alloc_assoc_unused, mba_id, 1);
        expect_value(hw_alloc_assoc_unused, smba_id, 0);
        will_return(hw_alloc_assoc_unused, PQOS_RETVAL_OK);
        will_return(hw_alloc_assoc_unused, 1);

        expect_value(hw_alloc_assoc_write, lcore, core_array[0]);
        expect_value(hw_alloc_assoc_write, class_id, 1);
        will_return(hw_alloc_assoc_write, PQOS_RETVAL_OK);

        ret = hw_alloc_assign(technology, core_array, core_num, &class_id);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(class_id, 1);
}

int
main(void)
{
        int result = 0;

        const struct CMUnitTest tests[] = {
            cmocka_unit_test(hw_alloc_assign_l2ca),
            cmocka_unit_test(hw_alloc_assign_l3ca),
            cmocka_unit_test(hw_alloc_assign_mba)};

        result += cmocka_run_group_tests(tests, test_init_l3ca, test_fini);

        return result;
}
