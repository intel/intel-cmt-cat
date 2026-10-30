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

#include "mock_cap.h"
#include "mock_resctrl.h"
#include "mock_resctrl_alloc.h"
#include "mock_resctrl_monitoring.h"
#include "os_allocation.h"
#include "test.h"

#include <string.h>

/* ======== os_alloc_assoc_set ======== */

static void
test_os_alloc_assoc_set(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        unsigned class_id = 1;
        unsigned lcore = 2;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);

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

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);

        will_return(__wrap_resctrl_lock_exclusive, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_lock_release, PQOS_RETVAL_OK);

        expect_value(__wrap_resctrl_mon_assoc_get, lcore, lcore);
        will_return(__wrap_resctrl_mon_assoc_get, PQOS_RETVAL_OK);

        expect_value(__wrap_resctrl_alloc_assoc_set, lcore, lcore);
        expect_value(__wrap_resctrl_alloc_assoc_set, class_id, class_id);
        will_return(__wrap_resctrl_alloc_assoc_set, PQOS_RETVAL_OK);

        expect_value(__wrap_resctrl_mon_assoc_set, lcore, lcore);
        expect_string(__wrap_resctrl_mon_assoc_set, name, "test");
        will_return(__wrap_resctrl_mon_assoc_set, PQOS_RETVAL_OK);

        ret = os_alloc_assoc_set(lcore, class_id);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

/* moves core 2, whose monitoring is started, to class 1, the group's restore
 * there failing with mon_ret; returns what was logged
 */
static const char *
assoc_set_mon_fails(const int mon_ret)
{
        const unsigned class_id = 1;
        const unsigned lcore = 2;

        will_return(__wrap_resctrl_lock_exclusive, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_lock_release, PQOS_RETVAL_OK);

        expect_value(__wrap_resctrl_mon_assoc_get, lcore, lcore);
        will_return(__wrap_resctrl_mon_assoc_get, PQOS_RETVAL_OK);

        expect_value(__wrap_resctrl_alloc_assoc_set, lcore, lcore);
        expect_value(__wrap_resctrl_alloc_assoc_set, class_id, class_id);
        will_return(__wrap_resctrl_alloc_assoc_set, PQOS_RETVAL_OK);

        expect_value(__wrap_resctrl_mon_assoc_set, lcore, lcore);
        expect_string(__wrap_resctrl_mon_assoc_set, name, "test");
        will_return(__wrap_resctrl_mon_assoc_set, mon_ret);

        test_log_keep_start();
        assert_int_equal(os_alloc_assoc_set(lcore, class_id), PQOS_RETVAL_OK);
        (void)test_log_count_stop();

        return test_log_said();
}

/* a restore that finds every free RMID in the kernel's limbo says so, which
 * the association leaves to its caller; another failure is said as before
 */
static void
test_os_alloc_assoc_set_mon_restore_fails(void **state)
{
        struct test_data *data = (struct test_data *)*state;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);
        assert_string_equal(assoc_set_mon_fails(PQOS_RETVAL_BUSY),
                            "WARN: Could not assign core 2 back to monitoring "
                            "group: " RESCTRL_MON_LIMBO "\n");
        assert_string_equal(
            assoc_set_mon_fails(PQOS_RETVAL_ERROR),
            "WARN: Could not assign core 2 back to monitoring group\n");
}

static void
test_os_alloc_assoc_set_param(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        unsigned class_id = 1;
        unsigned lcore = 2;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);

        ret = os_alloc_assoc_set(1000, class_id);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);

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

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);

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

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);

        will_return(__wrap_resctrl_lock_exclusive, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_lock_release, PQOS_RETVAL_OK);

        expect_value(__wrap_resctrl_mon_assoc_get_pid, task, task);
        will_return(__wrap_resctrl_mon_assoc_get_pid, PQOS_RETVAL_OK);

        expect_value(__wrap_resctrl_alloc_assoc_set_pid, task, task);
        expect_value(__wrap_resctrl_alloc_assoc_set_pid, class_id, class_id);
        will_return(__wrap_resctrl_alloc_assoc_set_pid, PQOS_RETVAL_OK);

        expect_value(__wrap_resctrl_mon_assoc_set_pid, task, task);
        expect_string(__wrap_resctrl_mon_assoc_set_pid, name, "test");
        will_return(__wrap_resctrl_mon_assoc_set_pid, PQOS_RETVAL_OK);

        ret = os_alloc_assoc_set_pid(task, class_id);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

/* a task whose monitoring group cannot be looked up is not moved: the move
 * would take it out of a group it may be in, with no name to put it back in
 */
static void
test_os_alloc_assoc_set_pid_mon_lookup_error(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        unsigned class_id = 1;
        pid_t task = 2;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);

        will_return(__wrap_resctrl_lock_exclusive, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_lock_release, PQOS_RETVAL_OK);

        expect_value(__wrap_resctrl_mon_assoc_get_pid, task, task);
        will_return(__wrap_resctrl_mon_assoc_get_pid, PQOS_RETVAL_ERROR);
        /* and no write to a tasks file: resctrl_alloc_assoc_set_pid() is not
         * expected
         */

        ret = os_alloc_assoc_set_pid(task, class_id);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
}

/* a task moved that cannot be put back in its monitoring group has been
 * taken out of its monitoring: the call fails
 */
static void
test_os_alloc_assoc_set_pid_mon_restore_error(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        unsigned class_id = 1;
        pid_t task = 2;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);

        will_return(__wrap_resctrl_lock_exclusive, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_lock_release, PQOS_RETVAL_OK);

        expect_value(__wrap_resctrl_mon_assoc_get_pid, task, task);
        will_return(__wrap_resctrl_mon_assoc_get_pid, PQOS_RETVAL_OK);

        expect_value(__wrap_resctrl_alloc_assoc_set_pid, task, task);
        expect_value(__wrap_resctrl_alloc_assoc_set_pid, class_id, class_id);
        will_return(__wrap_resctrl_alloc_assoc_set_pid, PQOS_RETVAL_OK);

        expect_value(__wrap_resctrl_mon_assoc_set_pid, task, task);
        expect_string(__wrap_resctrl_mon_assoc_set_pid, name, "test");
        will_return(__wrap_resctrl_mon_assoc_set_pid, PQOS_RETVAL_ERROR);

        ret = os_alloc_assoc_set_pid(task, class_id);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
}

/* a task that ended after its move is in no monitoring group to be put back
 * in: that is no failure
 */
static void
test_os_alloc_assoc_set_pid_mon_restore_task_ended(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        unsigned class_id = 1;
        pid_t task = 2;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);

        will_return(__wrap_resctrl_lock_exclusive, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_lock_release, PQOS_RETVAL_OK);

        expect_value(__wrap_resctrl_mon_assoc_get_pid, task, task);
        will_return(__wrap_resctrl_mon_assoc_get_pid, PQOS_RETVAL_OK);

        expect_value(__wrap_resctrl_alloc_assoc_set_pid, task, task);
        expect_value(__wrap_resctrl_alloc_assoc_set_pid, class_id, class_id);
        will_return(__wrap_resctrl_alloc_assoc_set_pid, PQOS_RETVAL_OK);

        expect_value(__wrap_resctrl_mon_assoc_set_pid, task, task);
        expect_string(__wrap_resctrl_mon_assoc_set_pid, name, "test");
        will_return(__wrap_resctrl_mon_assoc_set_pid, PQOS_RETVAL_PARAM);

        ret = os_alloc_assoc_set_pid(task, class_id);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

/* moves task 2, whose monitoring is started, to class 1, the group's restore
 * there failing with mon_ret, which the call then fails with as the move has
 * taken the task out of its monitoring; returns what was logged
 */
static const char *
assoc_set_pid_mon_fails(const int mon_ret)
{
        const unsigned class_id = 1;
        const pid_t task = 2;

        will_return(__wrap_resctrl_lock_exclusive, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_lock_release, PQOS_RETVAL_OK);

        expect_value(__wrap_resctrl_mon_assoc_get_pid, task, task);
        will_return(__wrap_resctrl_mon_assoc_get_pid, PQOS_RETVAL_OK);

        expect_value(__wrap_resctrl_alloc_assoc_set_pid, task, task);
        expect_value(__wrap_resctrl_alloc_assoc_set_pid, class_id, class_id);
        will_return(__wrap_resctrl_alloc_assoc_set_pid, PQOS_RETVAL_OK);

        expect_value(__wrap_resctrl_mon_assoc_set_pid, task, task);
        expect_string(__wrap_resctrl_mon_assoc_set_pid, name, "test");
        will_return(__wrap_resctrl_mon_assoc_set_pid, mon_ret);

        test_log_keep_start();
        assert_int_equal(os_alloc_assoc_set_pid(task, class_id), mon_ret);
        (void)test_log_count_stop();

        return test_log_said();
}

/* a task's restore that fails is an error, which names the kernel's limbo
 * when every free RMID is in it
 */
static void
test_os_alloc_assoc_set_pid_mon_restore_fails(void **state)
{
        struct test_data *data = (struct test_data *)*state;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);
        assert_string_equal(assoc_set_pid_mon_fails(PQOS_RETVAL_BUSY),
                            "ERROR: Could not assign task 2 back to "
                            "monitoring group: " RESCTRL_MON_LIMBO "\n");
        assert_string_equal(
            assoc_set_pid_mon_fails(PQOS_RETVAL_ERROR),
            "ERROR: Could not assign task 2 back to monitoring group\n");
}

static void
test_os_alloc_assoc_set_pid_param(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        pid_t task = 2;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);

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
            cmocka_unit_test(test_os_alloc_assoc_set_mon_restore_fails),
            cmocka_unit_test(test_os_alloc_assoc_set_pid),
            cmocka_unit_test(test_os_alloc_assoc_set_pid_param),
            cmocka_unit_test(test_os_alloc_assoc_set_pid_active_mon),
            cmocka_unit_test(test_os_alloc_assoc_set_pid_mon_lookup_error),
            cmocka_unit_test(test_os_alloc_assoc_set_pid_mon_restore_error),
            cmocka_unit_test(
                test_os_alloc_assoc_set_pid_mon_restore_task_ended),
            cmocka_unit_test(test_os_alloc_assoc_set_pid_mon_restore_fails)};

        const struct CMUnitTest tests_unsupported[] = {};

        result += cmocka_run_group_tests(tests_l3ca, test_init_l3ca, test_fini);
        result += cmocka_run_group_tests(tests_l2ca, test_init_l2ca, test_fini);
        result += cmocka_run_group_tests(tests_mba, test_init_mba, test_fini);
        result += cmocka_run_group_tests(tests_all, test_init_all, test_fini);
        result += cmocka_run_group_tests(tests_unsupported,
                                         test_init_unsupported, test_fini);

        return result;
}
