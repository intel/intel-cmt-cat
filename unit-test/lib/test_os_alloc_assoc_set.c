/*
 * BSD LICENSE
 *
 * Copyright(c) 2020-2021 Intel Corporation. All rights reserved.
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
#include "mock_resctrl.h"
#include "mock_resctrl_alloc.h"
#include "mock_resctrl_monitoring.h"

#include "os_allocation.h"

/* ======== os_alloc_assoc_set ======== */

static void
test_os_alloc_assoc_set(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        unsigned class_id = 1;
        unsigned lcore = 2;

        will_return(__wrap__pqos_cap_get, data->cap);
        will_return(__wrap__pqos_cap_get, data->cpu);

        will_return(__wrap_resctrl_lock_exclusive, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_lock_release, PQOS_RETVAL_OK);

        expect_value(__wrap_resctrl_mon_assoc_get, lcore, lcore);
        will_return(__wrap_resctrl_mon_assoc_get, PQOS_RETVAL_RESOURCE);

        expect_value(__wrap_resctrl_alloc_assoc_set, lcore, lcore);
        expect_value(__wrap_resctrl_alloc_assoc_set, class_id, class_id);
        will_return(__wrap_resctrl_alloc_assoc_set, PQOS_RETVAL_OK);

        ret = os_alloc_assoc_set(lcore, class_id);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_os_alloc_assoc_set_active_mon(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        unsigned class_id = 1;
        unsigned lcore = 2;

        will_return(__wrap__pqos_cap_get, data->cap);
        will_return(__wrap__pqos_cap_get, data->cpu);

        will_return(__wrap_resctrl_lock_exclusive, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_lock_release, PQOS_RETVAL_OK);

        expect_value(__wrap_resctrl_mon_assoc_get, lcore, lcore);
        will_return(__wrap_resctrl_mon_assoc_get, PQOS_RETVAL_OK);

        expect_value(__wrap_resctrl_alloc_assoc_set, lcore, lcore);
        expect_value(__wrap_resctrl_alloc_assoc_set, class_id, class_id);
        will_return(__wrap_resctrl_alloc_assoc_set, PQOS_RETVAL_OK);

        expect_value(__wrap_resctrl_mon_assoc_set, lcore, lcore);
        will_return(__wrap_resctrl_mon_assoc_set, PQOS_RETVAL_OK);

        ret = os_alloc_assoc_set(lcore, class_id);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_os_alloc_assoc_set_param(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        unsigned class_id = 1;
        unsigned lcore = 2;

        will_return(__wrap__pqos_cap_get, data->cap);
        will_return(__wrap__pqos_cap_get, data->cpu);

        ret = os_alloc_assoc_set(1000, class_id);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);

        will_return(__wrap__pqos_cap_get, data->cap);
        will_return(__wrap__pqos_cap_get, data->cpu);

        ret = os_alloc_assoc_set(lcore, 100);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
}

/* ======== os_alloc_assoc_set_pid ======== */

static void
test_os_alloc_assoc_set_pid(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        unsigned class_id = 1;
        pid_t task = 2;

        will_return(__wrap__pqos_cap_get, data->cap);

        will_return(__wrap_resctrl_lock_exclusive, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_lock_release, PQOS_RETVAL_OK);

        expect_value(__wrap_resctrl_mon_assoc_get_pid, task, task);
        will_return(__wrap_resctrl_mon_assoc_get_pid, PQOS_RETVAL_RESOURCE);

        expect_value(__wrap_resctrl_alloc_assoc_set_pid, task, task);
        expect_value(__wrap_resctrl_alloc_assoc_set_pid, class_id, class_id);
        will_return(__wrap_resctrl_alloc_assoc_set_pid, PQOS_RETVAL_OK);

        ret = os_alloc_assoc_set_pid(task, class_id);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_os_alloc_assoc_set_pid_active_mon(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        unsigned class_id = 1;
        pid_t task = 2;

        will_return(__wrap__pqos_cap_get, data->cap);

        will_return(__wrap_resctrl_lock_exclusive, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_lock_release, PQOS_RETVAL_OK);

        expect_value(__wrap_resctrl_mon_assoc_get_pid, task, task);
        will_return(__wrap_resctrl_mon_assoc_get_pid, PQOS_RETVAL_OK);

        expect_value(__wrap_resctrl_alloc_assoc_set_pid, task, task);
        expect_value(__wrap_resctrl_alloc_assoc_set_pid, class_id, class_id);
        will_return(__wrap_resctrl_alloc_assoc_set_pid, PQOS_RETVAL_OK);

        expect_value(__wrap_resctrl_mon_assoc_set_pid, task, task);
        will_return(__wrap_resctrl_mon_assoc_set_pid, PQOS_RETVAL_OK);

        ret = os_alloc_assoc_set_pid(task, class_id);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_os_alloc_assoc_set_pid_param(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        pid_t task = 2;

        will_return(__wrap__pqos_cap_get, data->cap);

        ret = os_alloc_assoc_set_pid(task, 100);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
}

int
main(void)
{
        int result = 0;

        const struct CMUnitTest tests_l3ca[] = {};

        const struct CMUnitTest tests_l2ca[] = {};

        const struct CMUnitTest tests_mba[] = {};

        const struct CMUnitTest tests_all[] = {
            cmocka_unit_test(test_os_alloc_assoc_set),
            cmocka_unit_test(test_os_alloc_assoc_set_param),
            cmocka_unit_test(test_os_alloc_assoc_set_active_mon),
            cmocka_unit_test(test_os_alloc_assoc_set_pid),
            cmocka_unit_test(test_os_alloc_assoc_set_pid_param),
            cmocka_unit_test(test_os_alloc_assoc_set_pid_active_mon)};

        const struct CMUnitTest tests_unsupported[] = {};

        result += cmocka_run_group_tests(tests_l3ca, test_init_l3ca, test_fini);
        result += cmocka_run_group_tests(tests_l2ca, test_init_l2ca, test_fini);
        result += cmocka_run_group_tests(tests_mba, test_init_mba, test_fini);
        result += cmocka_run_group_tests(tests_all, test_init_all, test_fini);
        result += cmocka_run_group_tests(tests_unsupported,
                                         test_init_unsupported, test_fini);

        return result;
}