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
#include "mock_cap.h"
#include "test.h"

/* ======== mock ======== */

int
hw_alloc_assoc_read(const unsigned lcore, unsigned *class_id)
{
        switch (lcore) {
        case 0:
                *class_id = 0;
                break;
        case 1:
                *class_id = 1;
                break;
        case 2:
                *class_id = 2;
                break;
        case 3:
                *class_id = 3;
                break;
        case 4:
                *class_id = 2;
                break;
        default:
                *class_id = 0;
                break;
        }

        return mock_type(int);
}

/* ======== hw_alloc_assoc_unused ======== */

static void
test_hw_alloc_assoc_unused_l2ca(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        unsigned technology = 1 << PQOS_CAP_TYPE_L2CA;
        unsigned class_id;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);

        will_return_always(hw_alloc_assoc_read, PQOS_RETVAL_OK);

        ret = hw_alloc_assoc_unused(technology, 0, 0, 0, 0, &class_id);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(class_id, 2);

        ret = hw_alloc_assoc_unused(technology, 0, 2, 0, 0, &class_id);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(class_id, 1);
}

static void
test_hw_alloc_assoc_unused_l3ca(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        unsigned technology = 1 << PQOS_CAP_TYPE_L3CA;
        unsigned class_id;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);

        will_return_always(hw_alloc_assoc_read, PQOS_RETVAL_OK);

        ret = hw_alloc_assoc_unused(technology, 0, 0, 0, 0, &class_id);
        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);

        ret = hw_alloc_assoc_unused(technology, 1, 0, 0, 0, &class_id);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(class_id, 3);
}

static void
test_hw_alloc_assoc_unused_mba(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        unsigned technology = 1 << PQOS_CAP_TYPE_MBA;
        unsigned class_id;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);

        will_return_always(hw_alloc_assoc_read, PQOS_RETVAL_OK);

        ret = hw_alloc_assoc_unused(technology, 0, 0, 0, 0, &class_id);
        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);

        ret = hw_alloc_assoc_unused(technology, 0, 0, 1, 0, &class_id);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(class_id, 3);
}

static void
test_hw_alloc_assoc_unused_l2ca_when_l3_present(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        unsigned technology = 1 << PQOS_CAP_TYPE_L2CA;
        unsigned class_id;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);

        will_return_always(hw_alloc_assoc_read, PQOS_RETVAL_OK);

        ret = hw_alloc_assoc_unused(technology, 0, 0, 0, 0, &class_id);
        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);

        ret = hw_alloc_assoc_unused(technology, 0, 3, 0, 0, &class_id);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(class_id, 1);
}

int
main(void)
{
        int result = 0;

        const struct CMUnitTest tests_l3ca[] = {
            cmocka_unit_test(test_hw_alloc_assoc_unused_l3ca)};

        const struct CMUnitTest tests_l2ca[] = {
            cmocka_unit_test(test_hw_alloc_assoc_unused_l2ca)};

        const struct CMUnitTest tests_mba[] = {
            cmocka_unit_test(test_hw_alloc_assoc_unused_mba)};

        const struct CMUnitTest tests_all[] = {
            cmocka_unit_test(test_hw_alloc_assoc_unused_l2ca_when_l3_present)};

        result += cmocka_run_group_tests(tests_l3ca, test_init_l3ca, test_fini);
        result += cmocka_run_group_tests(tests_l2ca, test_init_l2ca, test_fini);
        result += cmocka_run_group_tests(tests_mba, test_init_mba, test_fini);
        result += cmocka_run_group_tests(tests_all, test_init_all, test_fini);

        return result;
}
