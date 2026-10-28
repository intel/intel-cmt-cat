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
#include "mock_resctrl_monitoring.h"
#include "monitoring.h"
#include "os_monitoring.h"
#include "test.h"

#include <stdint.h>

/* ======== mock ======== */

/* a collection that succeeds leaves a list, as the real one does */
int
__wrap_resctrl_mon_collect_pkgids(struct pqos_mon_data *group)
{
        const int ret = mock_type(int);

        assert_non_null(group);
        if (ret == PQOS_RETVAL_OK) {
                group->intl->resctrl.pkgids =
                    calloc(1, sizeof(*group->intl->resctrl.pkgids));
                assert_non_null(group->intl->resctrl.pkgids);
                group->intl->resctrl.num_pkgids = 1;
        }

        return ret;
}

/* the stop of a group being added to must not have the name of the group's
 * directory, which resctrl_mon_stop() would remove
 */
static int
stopped_without_a_name(const LargestIntegralType value,
                       const LargestIntegralType data __attribute__((unused)))
{
        const struct pqos_mon_data *group =
            (const struct pqos_mon_data *)(uintptr_t)value;

        return group->intl->resctrl.mon_group == NULL;
}

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

/* LLC occupancy is flagged for resctrl and then the perf event fails: nothing
 * of resctrl has started, so nothing of it is stopped
 */
static void
expect_perf_fails_first(const int perf_ret)
{
        expect_value(__wrap_perf_mon_is_event_supported, event,
                     PQOS_MON_EVENT_L3_OCCUP);
        will_return(__wrap_perf_mon_is_event_supported, 0);
        expect_value(__wrap_resctrl_mon_is_event_supported, event,
                     PQOS_MON_EVENT_L3_OCCUP);
        will_return(__wrap_resctrl_mon_is_event_supported, 1);
        expect_value(__wrap_perf_mon_is_event_supported, event,
                     PQOS_PERF_EVENT_LLC_MISS);
        will_return(__wrap_perf_mon_is_event_supported, 1);
        expect_value(__wrap_perf_mon_start, event, PQOS_PERF_EVENT_LLC_MISS);
        will_return(__wrap_perf_mon_start, perf_ret);
}

static void
test_os_mon_start_events_perf_fails_first(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        struct pqos_mon_data group;
        struct pqos_mon_data_internal intl;
        unsigned cores[] = {1};

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);

        memset(&group, 0, sizeof(group));
        memset(&intl, 0, sizeof(intl));
        group.intl = &intl;
        group.num_cores = DIM(cores);
        group.cores = cores;
        group.event = PQOS_MON_EVENT_L3_OCCUP | PQOS_PERF_EVENT_LLC_MISS;

        expect_perf_fails_first(PQOS_RETVAL_ERROR);
        assert_int_equal(os_mon_start_events(&group), PQOS_RETVAL_ERROR);
        assert_int_equal(intl.resctrl.event, 0);
        assert_int_equal(intl.perf.event, 0);
}

/* The group os_mon_add_pids() passes on: the threads it adds and the name of
 * the group they are added to
 */
static void
added_to(struct pqos_mon_data *group,
         struct pqos_mon_data_internal *intl,
         pid_t *pid,
         const enum pqos_mon_event event)
{
        memset(group, 0, sizeof(*group));
        memset(intl, 0, sizeof(*intl));
        group->intl = intl;
        group->num_pids = 1;
        group->pids = pid;
        group->tid_nr = 1;
        group->tid_map = pid;
        group->event = event;
        intl->resctrl.mon_group = strdup("existing");
        assert_non_null(intl->resctrl.mon_group);
}

static void
test_os_mon_start_events_add_perf_fails_first(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        struct pqos_mon_data group;
        struct pqos_mon_data_internal intl;
        pid_t pid = 5;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);

        added_to(&group, &intl, &pid,
                 PQOS_MON_EVENT_L3_OCCUP | PQOS_PERF_EVENT_LLC_MISS);

        /* every added thread ended before its counter opened */
        expect_perf_fails_first(PQOS_RETVAL_PARAM);
        assert_int_equal(os_mon_start_events(&group), PQOS_RETVAL_PARAM);
        assert_int_equal(intl.resctrl.event, 0);

        free(intl.resctrl.mon_group);
}

static void
test_os_mon_start_events_add_fails_before_resctrl(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        struct pqos_mon_data group;
        struct pqos_mon_data_internal intl;
        pid_t pid = 5;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);

        added_to(&group, &intl, &pid, PQOS_MON_EVENT_CORE_ENERGY);

        expect_value(__wrap_resctrl_mon_is_event_supported, event,
                     PQOS_MON_EVENT_CORE_ENERGY);
        will_return(__wrap_resctrl_mon_is_event_supported, 1);
        /* the collection fails before the resctrl start, so no task is moved
         * and no directory made, and there is nothing to stop
         */
        will_return(__wrap_resctrl_mon_collect_pkgids, PQOS_RETVAL_RESOURCE);

        assert_int_equal(os_mon_start_events(&group), PQOS_RETVAL_RESOURCE);
        assert_int_equal(intl.resctrl.event, 0);
        assert_null(intl.resctrl.mon_group);
}

static void
test_os_mon_start_events_add_resctrl_fails(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        struct pqos_mon_data group;
        struct pqos_mon_data_internal intl;
        pid_t pid = 5;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);

        added_to(&group, &intl, &pid, PQOS_MON_EVENT_L3_OCCUP);

        expect_value(__wrap_perf_mon_is_event_supported, event,
                     PQOS_MON_EVENT_L3_OCCUP);
        will_return(__wrap_perf_mon_is_event_supported, 0);
        expect_value(__wrap_resctrl_mon_is_event_supported, event,
                     PQOS_MON_EVENT_L3_OCCUP);
        will_return(__wrap_resctrl_mon_is_event_supported, 1);
        will_return(__wrap_resctrl_lock_exclusive, PQOS_RETVAL_OK);
        /* every added thread had ended by the time it was added */
        expect_value(__wrap_resctrl_mon_start, group, &group);
        will_return(__wrap_resctrl_mon_start, PQOS_RETVAL_PARAM);
        will_return(__wrap_resctrl_lock_release, PQOS_RETVAL_OK);
        /* the stop takes out what the start moved in, without the name of
         * the group being added to, whose directory stays
         */
        will_return(__wrap_resctrl_lock_exclusive, PQOS_RETVAL_OK);
        expect_check(__wrap_resctrl_mon_stop, group, stopped_without_a_name, 0);
        will_return(__wrap_resctrl_mon_stop, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_lock_release, PQOS_RETVAL_OK);

        assert_int_equal(os_mon_start_events(&group), PQOS_RETVAL_PARAM);
        assert_null(intl.resctrl.mon_group);
}

static void
test_os_mon_start_events_lock_fails_after_pkgids(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        struct pqos_mon_data group;
        struct pqos_mon_data_internal intl;
        pid_t pid = 5;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);

        added_to(&group, &intl, &pid, PQOS_MON_EVENT_CORE_ENERGY);

        expect_value(__wrap_resctrl_mon_is_event_supported, event,
                     PQOS_MON_EVENT_CORE_ENERGY);
        will_return(__wrap_resctrl_mon_is_event_supported, 1);
        will_return(__wrap_resctrl_mon_collect_pkgids, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_lock_exclusive, PQOS_RETVAL_ERROR);

        /* the list collected for the start that did not happen goes */
        assert_int_equal(os_mon_start_events(&group), PQOS_RETVAL_ERROR);
        assert_null(intl.resctrl.pkgids);
        assert_int_equal(intl.resctrl.num_pkgids, 0);
        assert_null(intl.resctrl.mon_group);
}

/* the resctrl lock cannot be taken for the stop: the group is taken off the
 * list of the core groups started all the same, since pqos_mon_stop() frees it
 * whatever the stop returns
 */
static void
test_os_mon_stop_events_lock_fails(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        struct pqos_mon_data group;
        struct pqos_mon_data_internal intl;
        unsigned cores[] = {1};

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);

        memset(&group, 0, sizeof(group));
        memset(&intl, 0, sizeof(intl));
        group.intl = &intl;
        group.num_cores = DIM(cores);
        group.cores = cores;
        intl.resctrl.event = PQOS_MON_EVENT_TMEM_BW;
        mock_resctrl_mon_unlisted = NULL;

        will_return(__wrap_resctrl_lock_exclusive, PQOS_RETVAL_ERROR);

        assert_int_equal(os_mon_stop_events(&group), PQOS_RETVAL_ERROR);
        assert_ptr_equal(mock_resctrl_mon_unlisted, &group);
}

int
main(void)
{
        int result = 0;

        const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_os_mon_start_events_perf),
            cmocka_unit_test(test_os_mon_start_events_resctrl),
            cmocka_unit_test(test_os_mon_start_events_unsupported),
            cmocka_unit_test(test_os_mon_start_events_perf_fails_first),
            cmocka_unit_test(test_os_mon_start_events_add_perf_fails_first),
            cmocka_unit_test(test_os_mon_start_events_add_fails_before_resctrl),
            cmocka_unit_test(test_os_mon_start_events_add_resctrl_fails),
            cmocka_unit_test(test_os_mon_start_events_lock_fails_after_pkgids),
            cmocka_unit_test(test_os_mon_stop_events_lock_fails),
        };

        result += cmocka_run_group_tests(tests, test_init_mon, test_fini);

        return result;
}
