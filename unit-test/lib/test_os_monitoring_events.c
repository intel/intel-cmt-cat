/*
 * BSD LICENSE
 *
 * Copyright(c) 2023-2026 Intel Corporation. All rights reserved.
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

#include "mock_os_monitoring.h"
#include "monitoring.h"
#include "os_monitoring.h"
#include "test.h"

/* ======== os_mon_start_events ======== */

static void
test_os_mon_start_events_perf(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        struct pqos_mon_data group;
        struct pqos_mon_data_internal intl;
        enum pqos_mon_event event = PQOS_MON_EVENT_L3_OCCUP;
        unsigned num_cores = 1;
        unsigned cores[] = {1};
        int ret;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);

        memset(&group, 0, sizeof(group));
        memset(&intl, 0, sizeof(intl));
        group.intl = &intl;
        group.num_cores = num_cores;
        group.cores = cores;
        group.event = event;

        /* success */
        {
                expect_value(__wrap_perf_mon_is_event_supported, event, event);
                will_return(__wrap_perf_mon_is_event_supported, 1);
                expect_value(__wrap_perf_mon_start, event, event);
                will_return(__wrap_perf_mon_start, PQOS_RETVAL_OK);

                ret = os_mon_start_events(&group);
                assert_int_equal(ret, PQOS_RETVAL_OK);
                assert_int_equal(intl.perf.event, event);
                assert_int_equal(intl.resctrl.event, 0);

                expect_value(__wrap_perf_mon_stop, event, event);
                will_return(__wrap_perf_mon_stop, PQOS_RETVAL_OK);

                ret = os_mon_stop_events(&group);
                assert_int_equal(ret, PQOS_RETVAL_OK);
        }

        /* error */
        {
                expect_value(__wrap_perf_mon_is_event_supported, event, event);
                will_return(__wrap_perf_mon_is_event_supported, 1);
                expect_value(__wrap_perf_mon_start, event, event);
                will_return(__wrap_perf_mon_start, PQOS_RETVAL_ERROR);

                ret = os_mon_start_events(&group);
                assert_int_equal(ret, PQOS_RETVAL_ERROR);
        }
}

static void
test_os_mon_start_events_resctrl(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        struct pqos_mon_data group;
        struct pqos_mon_data_internal intl;
        enum pqos_mon_event event = PQOS_MON_EVENT_L3_OCCUP;
        unsigned num_cores = 1;
        unsigned cores[] = {1};
        int ret;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);

        memset(&group, 0, sizeof(group));
        memset(&intl, 0, sizeof(intl));
        group.intl = &intl;
        group.num_cores = num_cores;
        group.cores = cores;
        group.event = event;

        /* success */
        {
                expect_value(__wrap_perf_mon_is_event_supported, event, event);
                will_return(__wrap_perf_mon_is_event_supported, 0);
                expect_value(__wrap_resctrl_mon_is_event_supported, event,
                             event);
                will_return(__wrap_resctrl_mon_is_event_supported, 1);
                will_return(__wrap_resctrl_lock_exclusive, PQOS_RETVAL_OK);
                expect_value(__wrap_resctrl_mon_start, group, &group);
                will_return(__wrap_resctrl_lock_release, PQOS_RETVAL_OK);
                will_return(__wrap_resctrl_mon_start, PQOS_RETVAL_OK);

                ret = os_mon_start_events(&group);
                assert_int_equal(ret, PQOS_RETVAL_OK);
                assert_int_equal(intl.perf.event, 0);
                assert_int_equal(intl.resctrl.event, event);

                will_return(__wrap_resctrl_lock_exclusive, PQOS_RETVAL_OK);
                expect_value(__wrap_resctrl_mon_stop, group, &group);
                will_return(__wrap_resctrl_mon_stop, PQOS_RETVAL_OK);
                will_return(__wrap_resctrl_lock_release, PQOS_RETVAL_OK);

                ret = os_mon_stop_events(&group);
                assert_int_equal(ret, PQOS_RETVAL_OK);
                assert_int_equal(intl.perf.event, 0);
                assert_int_equal(intl.resctrl.event, 0);
        }

        /* error */
        {
                expect_value(__wrap_perf_mon_is_event_supported, event, event);
                will_return(__wrap_perf_mon_is_event_supported, 0);
                expect_value(__wrap_resctrl_mon_is_event_supported, event,
                             event);
                will_return(__wrap_resctrl_mon_is_event_supported, 1);
                will_return(__wrap_resctrl_lock_exclusive, PQOS_RETVAL_OK);
                expect_value(__wrap_resctrl_mon_start, group, &group);
                will_return(__wrap_resctrl_mon_start, PQOS_RETVAL_ERROR);
                will_return(__wrap_resctrl_lock_release, PQOS_RETVAL_OK);
                /* stop */
                will_return(__wrap_resctrl_lock_exclusive, PQOS_RETVAL_OK);
                expect_value(__wrap_resctrl_mon_stop, group, &group);
                will_return(__wrap_resctrl_mon_stop, PQOS_RETVAL_OK);
                will_return(__wrap_resctrl_lock_release, PQOS_RETVAL_OK);

                ret = os_mon_start_events(&group);
                assert_int_equal(ret, PQOS_RETVAL_ERROR);
        }
}

static void
test_os_mon_start_events_unsupported(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        struct pqos_mon_data group;
        struct pqos_mon_data_internal intl;
        enum pqos_mon_event event = PQOS_MON_EVENT_L3_OCCUP;
        unsigned num_cores = 1;
        unsigned cores[] = {1};
        int ret;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);

        memset(&group, 0, sizeof(group));
        memset(&intl, 0, sizeof(intl));
        group.intl = &intl;
        group.num_cores = num_cores;
        group.cores = cores;
        group.event = event;

        expect_value(__wrap_perf_mon_is_event_supported, event, event);
        will_return(__wrap_perf_mon_is_event_supported, 0);
        expect_value(__wrap_resctrl_mon_is_event_supported, event, event);
        will_return(__wrap_resctrl_mon_is_event_supported, 0);

        ret = os_mon_start_events(&group);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
}

int
main(void)
{
        int result = 0;

        const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_os_mon_start_events_perf),
            cmocka_unit_test(test_os_mon_start_events_resctrl),
            cmocka_unit_test(test_os_mon_start_events_unsupported),
        };

        result += cmocka_run_group_tests(tests, test_init_mon, test_fini);

        return result;
}
