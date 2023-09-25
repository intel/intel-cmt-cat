/*
 * BSD LICENSE
 *
 * Copyright(c) 2023 Intel Corporation. All rights reserved.
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

#include "monitoring.h"
#include "os_monitoring.h"
#include "test.h"

#include <dirent.h>

/* ======== mock ======== */
int
os_mon_stop_events(struct pqos_mon_data *group)
{

        assert_non_null(group);
        check_expected_ptr(group);

        free(group->intl->perf.ctx);
        group->intl->perf.ctx = NULL;

        return mock_type(int);
}

int
os_mon_start_events(struct pqos_mon_data *group)
{
        unsigned num_ctrs = 0;

        assert_non_null(group);
        check_expected_ptr(group);

        if (group->num_cores > 0)
                num_ctrs = group->num_cores;
        else if (group->tid_nr > 0)
                num_ctrs = group->tid_nr;

        group->intl->perf.ctx =
            calloc(num_ctrs, sizeof(group->intl->perf.ctx[0]));

        return mock_type(int);
}

int
os_mon_tid_exists(const pid_t pid)
{
        return pid != 0xDEAD;
}

int
__wrap_scandir(const char *restrict dirp,
               struct dirent ***restrict namelist,
               int (*filter)(const struct dirent *) __attribute__((unused)),
               int (*compar)(const struct dirent **, const struct dirent **)
                   __attribute__((unused)))
{
        if (strncmp(dirp, "/proc", 5) == 0) {
                int ret;
                pid_t pid = 0;

                ret = sscanf(dirp, "/proc/%d/task", &pid);
                if (ret != 1 || pid == 0 || pid == 0xDEAD)
                        return -1;

                *namelist = malloc(sizeof(struct dirent *));
                *namelist[0] = malloc(sizeof(struct dirent) + 10);
                sprintf((*namelist)[0]->d_name, "%d", pid);

                return 1;
        }

        return -1;
}

/* ======== os_mon_init ======== */

/**
 * @brief Test \a os_mon_init function behaviour when perf monitoring
 *        is supported for RDT events
 */
static void
test_os_mon_init_perf(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;

        expect_value(__wrap_perf_mon_init, cpu, data->cpu);
        expect_value(__wrap_perf_mon_init, cap, data->cap);
        will_return(__wrap_perf_mon_init, PQOS_RETVAL_OK);

        ret = os_mon_init(data->cpu, data->cap);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

/**
 * @brief Test \a os_mon_init function behaviour when resctrl monitoring
 *        is supported for RDT events
 */
static void
test_os_mon_init_resctrl(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;

        expect_value(__wrap_perf_mon_init, cpu, data->cpu);
        expect_value(__wrap_perf_mon_init, cap, data->cap);
        will_return(__wrap_perf_mon_init, PQOS_RETVAL_RESOURCE);
        expect_value(__wrap_resctrl_mon_init, cpu, data->cpu);
        expect_value(__wrap_resctrl_mon_init, cap, data->cap);
        will_return(__wrap_resctrl_mon_init, PQOS_RETVAL_OK);

        ret = os_mon_init(data->cpu, data->cap);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

/**
 * @brief Test \a os_mon_init function behaviour when RDT monitoring
 *        is not supported by kernel
 */
static void
test_os_mon_init_unsupported(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;

        expect_value(__wrap_perf_mon_init, cpu, data->cpu);
        expect_value(__wrap_perf_mon_init, cap, data->cap);
        will_return(__wrap_perf_mon_init, PQOS_RETVAL_RESOURCE);
        expect_value(__wrap_resctrl_mon_init, cpu, data->cpu);
        expect_value(__wrap_resctrl_mon_init, cap, data->cap);
        will_return(__wrap_resctrl_mon_init, PQOS_RETVAL_RESOURCE);

        ret = os_mon_init(data->cpu, data->cap);
        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);
}

/**
 * @brief Test \a os_mon_init function invalid parameters handling
 */
static void
test_os_mon_init_param(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;

        ret = os_mon_init(data->cpu, NULL);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);

        ret = os_mon_init(NULL, data->cap);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
}

/* ======== os_mon_fini ======== */

/**
 * @brief Test \a os_mon_init function deinicalizes all sub-modules
 */
static void
test_os_mon_fini(void **state __attribute__((unused)))
{
        int ret;

        will_return(__wrap_perf_mon_fini, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_mon_fini, PQOS_RETVAL_OK);

        ret = os_mon_fini();
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

/* ======== os_mon_reset ======== */

static void
test_os_mon_reset(void **state __attribute__((unused)))
{
        int ret;
        struct pqos_mon_config cfg;

        memset(&cfg, 0, sizeof(cfg));

        will_return(__wrap_resctrl_mon_reset, PQOS_RETVAL_OK);

        ret = os_mon_reset(&cfg);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

/* ======== os_mon_stop ======== */

static void
test_os_mon_stop_core(void **state __attribute__((unused)))
{
        struct pqos_mon_data group;
        struct pqos_mon_data_internal intl;
        int ret;

        memset(&group, 0, sizeof(group));
        memset(&intl, 0, sizeof(intl));
        group.intl = &intl;
        group.num_cores = 1;
        group.cores = calloc(1, sizeof(*group.cores));
        group.cores[0] = 1;

        expect_value(os_mon_stop_events, group, &group);
        will_return(os_mon_stop_events, PQOS_RETVAL_OK);

        ret = os_mon_stop(&group);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_null(group.cores);
}

static void
test_os_mon_stop_pid(void **state __attribute__((unused)))
{
        struct pqos_mon_data group;
        struct pqos_mon_data_internal intl;
        int ret;

        memset(&group, 0, sizeof(group));
        memset(&intl, 0, sizeof(intl));
        group.intl = &intl;
        group.tid_nr = 1;
        group.tid_map = calloc(1, sizeof(*group.tid_map));
        group.tid_map[0] = 1;

        expect_value(os_mon_stop_events, group, &group);
        will_return(os_mon_stop_events, PQOS_RETVAL_OK);

        ret = os_mon_stop(&group);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_null(group.tid_map);
}

static void
test_os_mon_stop_param(void **state __attribute__((unused)))
{
        struct pqos_mon_data group;
        int ret;

        memset(&group, 0, sizeof(group));

        ret = os_mon_stop(&group);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
}

/* ======== os_mon_start_cores ======== */

static void
test_os_mon_start_cores_param(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        struct pqos_mon_data group;
        int ret;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);

        memset(&group, 0, sizeof(group));

        /* invalid event */
        {
                unsigned num_cores = 1;
                unsigned cores[] = {1};
                enum pqos_mon_event event = 0xDEAD;

                ret = os_mon_start_cores(num_cores, cores, event, NULL, &group,
                                         NULL);
                assert_int_equal(ret, PQOS_RETVAL_PARAM);
        }

        /* invalid core */
        {
                unsigned num_cores = 1;
                unsigned cores[] = {1024};
                enum pqos_mon_event event = PQOS_MON_EVENT_L3_OCCUP;

                ret = os_mon_start_cores(num_cores, cores, event, NULL, &group,
                                         NULL);
                assert_int_equal(ret, PQOS_RETVAL_PARAM);
        }
}

static void
test_os_mon_start_cores_already_started(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        struct pqos_mon_data group;
        struct pqos_mon_data_internal intl;
        int ret;
        unsigned num_cores = 1;
        unsigned cores[] = {1};
        enum pqos_mon_event event = PQOS_MON_EVENT_L3_OCCUP;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);

        memset(&group, 0, sizeof(group));
        memset(&intl, 0, sizeof(intl));
        group.intl = &intl;

        expect_value(__wrap_resctrl_mon_assoc_get, lcore, cores[0]);
        will_return(__wrap_resctrl_mon_assoc_get, PQOS_RETVAL_OK);

        ret = os_mon_start_cores(num_cores, cores, event, NULL, &group, NULL);
        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);
}

static void
test_os_mon_start_cores(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        struct pqos_mon_data group;
        struct pqos_mon_data_internal intl;
        int ret;
        unsigned num_cores = 1;
        unsigned cores[] = {1};
        enum pqos_mon_event event = PQOS_MON_EVENT_L3_OCCUP;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);

        memset(&group, 0, sizeof(group));
        memset(&intl, 0, sizeof(intl));
        group.intl = &intl;

        expect_value(__wrap_resctrl_mon_assoc_get, lcore, cores[0]);
        will_return(__wrap_resctrl_mon_assoc_get, PQOS_RETVAL_RESOURCE);
        expect_value(os_mon_start_events, group, &group);
        will_return(os_mon_start_events, PQOS_RETVAL_OK);

        ret = os_mon_start_cores(num_cores, cores, event, NULL, &group, NULL);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(group.num_cores, num_cores);
        assert_int_equal(group.event, event);
        assert_non_null(group.cores);
        expect_value(os_mon_stop_events, group, &group);
        will_return(os_mon_stop_events, PQOS_RETVAL_OK);
        ret = os_mon_stop(&group);
        assert_int_equal(ret, PQOS_RETVAL_OK);

        expect_value(__wrap_resctrl_mon_assoc_get, lcore, cores[0]);
        will_return(__wrap_resctrl_mon_assoc_get, PQOS_RETVAL_RESOURCE);
        expect_value(os_mon_start_events, group, &group);
        will_return(os_mon_start_events, PQOS_RETVAL_ERROR);

        ret = os_mon_start_cores(num_cores, cores, event, NULL, &group, NULL);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
        assert_null(group.cores);
}

/* ======== os_mon_start_pids ======== */

static void
test_os_mon_start_pids_param(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        struct pqos_mon_data group;
        int ret;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);

        memset(&group, 0, sizeof(group));

        /* invalid event */
        {
                unsigned num_pids = 1;
                pid_t pids[] = {1};
                enum pqos_mon_event event = 0xDEAD;

                ret = os_mon_start_pids(num_pids, pids, event, NULL, &group);
                assert_int_equal(ret, PQOS_RETVAL_PARAM);
        }

        /* invalid pid */
        {
                unsigned num_pids = 1;
                pid_t pids[] = {0xDEAD};
                enum pqos_mon_event event = PQOS_MON_EVENT_L3_OCCUP;

                ret = os_mon_start_pids(num_pids, pids, event, NULL, &group);
                assert_int_equal(ret, PQOS_RETVAL_PARAM);
        }
}

static void
test_os_mon_start_pids(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        struct pqos_mon_data group;
        struct pqos_mon_data_internal intl;
        int ret;
        unsigned num_pids = 1;
        pid_t pids[] = {1};
        enum pqos_mon_event event = PQOS_MON_EVENT_L3_OCCUP;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);

        memset(&group, 0, sizeof(group));
        memset(&intl, 0, sizeof(intl));
        group.intl = &intl;

        expect_value(os_mon_start_events, group, &group);
        will_return(os_mon_start_events, PQOS_RETVAL_OK);

        ret = os_mon_start_pids(num_pids, pids, event, NULL, &group);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(group.num_pids, num_pids);
        assert_int_equal(group.tid_nr, num_pids);
        assert_int_equal(group.event, event);
        assert_non_null(group.tid_map);
        assert_non_null(group.pids);
        expect_value(os_mon_stop_events, group, &group);
        will_return(os_mon_stop_events, PQOS_RETVAL_OK);
        ret = os_mon_stop(&group);
        assert_int_equal(ret, PQOS_RETVAL_OK);

        expect_value(os_mon_start_events, group, &group);
        will_return(os_mon_start_events, PQOS_RETVAL_ERROR);

        ret = os_mon_start_pids(num_pids, pids, event, NULL, &group);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
        assert_null(group.tid_map);
}

/* ======== os_mon_add_pids ======== */

static void
test_os_mon_add_pids(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        struct pqos_mon_data group;
        struct pqos_mon_data_internal intl;
        int ret;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);

        /* init monitoring group */
        memset(&group, 0, sizeof(group));
        memset(&intl, 0, sizeof(intl));
        group.intl = &intl;

        /* start monitoring */
        {
                unsigned num_pids = 1;
                pid_t pids[] = {1};
                enum pqos_mon_event event = PQOS_MON_EVENT_L3_OCCUP;

                expect_value(os_mon_start_events, group, &group);
                will_return(os_mon_start_events, PQOS_RETVAL_OK);

                ret = os_mon_start_pids(num_pids, pids, event, NULL, &group);
                assert_int_equal(ret, PQOS_RETVAL_OK);
        }

        /* add non-existent */
        {
                unsigned num_pids = 1;
                pid_t pids[] = {0xDEAD};

                ret = os_mon_add_pids(num_pids, pids, &group);
                assert_int_equal(ret, PQOS_RETVAL_PARAM);
                assert_int_equal(group.num_pids, 1);
                assert_int_equal(group.tid_nr, 1);
        }

        /* add old pid */
        {
                unsigned num_pids = 1;
                pid_t pids[] = {1};

                expect_any(os_mon_start_events, group);
                will_return(os_mon_start_events, PQOS_RETVAL_OK);

                ret = os_mon_add_pids(num_pids, pids, &group);
                assert_int_equal(ret, PQOS_RETVAL_OK);
                assert_int_equal(group.num_pids, 1);
                assert_int_equal(group.tid_nr, 1);
        }

        /* add new pid */
        {
                unsigned num_pids = 1;
                pid_t pids[] = {2};

                ret = os_mon_add_pids(num_pids, pids, &group);
                assert_int_equal(ret, PQOS_RETVAL_OK);
                assert_int_equal(group.num_pids, 2);
                assert_int_equal(group.tid_nr, 2);
        }

        /* add error */
        {
                unsigned num_pids = 1;
                pid_t pids[] = {3};

                expect_any(os_mon_start_events, group);
                will_return(os_mon_start_events, PQOS_RETVAL_ERROR);

                ret = os_mon_add_pids(num_pids, pids, &group);
                assert_int_equal(ret, PQOS_RETVAL_ERROR);
                assert_int_equal(group.num_pids, 2);
                assert_int_equal(group.tid_nr, 2);
        }

        /* stop monitoring */
        {
                expect_value(os_mon_stop_events, group, &group);
                will_return(os_mon_stop_events, PQOS_RETVAL_OK);

                ret = os_mon_stop(&group);
                assert_int_equal(ret, PQOS_RETVAL_OK);
                assert_null(group.tid_map);
                assert_null(group.pids);
        }
}

/* ======== os_mon_remove_pids ======== */

static void
test_os_mon_remove_pids(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        struct pqos_mon_data group;
        struct pqos_mon_data_internal intl;
        int ret;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);

        /* init monitoring group */
        memset(&group, 0, sizeof(group));
        memset(&intl, 0, sizeof(intl));
        group.intl = &intl;

        /* start monitoring */
        {
                unsigned num_pids = 2;
                pid_t pids[] = {1, 2};
                enum pqos_mon_event event = PQOS_MON_EVENT_L3_OCCUP;

                expect_value(os_mon_start_events, group, &group);
                will_return(os_mon_start_events, PQOS_RETVAL_OK);

                ret = os_mon_start_pids(num_pids, pids, event, NULL, &group);
                assert_int_equal(ret, PQOS_RETVAL_OK);
        }

        /* remove pids */
        {
                unsigned num_pids = 1;
                pid_t pids[] = {1};

                expect_any(os_mon_stop_events, group);
                will_return(os_mon_stop_events, PQOS_RETVAL_OK);

                ret = os_mon_remove_pids(num_pids, pids, &group);
                assert_int_equal(ret, PQOS_RETVAL_OK);
                assert_int_equal(group.num_pids, 1);
                assert_int_equal(group.tid_nr, 1);
        }

        /* remove non-existent */
        {
                unsigned num_pids = 1;
                pid_t pids[] = {3};

                expect_any(os_mon_stop_events, group);
                will_return(os_mon_stop_events, PQOS_RETVAL_OK);

                ret = os_mon_remove_pids(num_pids, pids, &group);
                assert_int_equal(ret, PQOS_RETVAL_OK);
                assert_int_equal(group.num_pids, 1);
                assert_int_equal(group.tid_nr, 1);
        }

        /* stop monitoring */
        {
                expect_value(os_mon_stop_events, group, &group);
                will_return(os_mon_stop_events, PQOS_RETVAL_OK);

                ret = os_mon_stop(&group);
                assert_int_equal(ret, PQOS_RETVAL_OK);
                assert_null(group.tid_map);
                assert_null(group.pids);
        }
}

int
main(void)
{
        int result = 0;

        const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_os_mon_init_perf),
            cmocka_unit_test(test_os_mon_init_resctrl),
            cmocka_unit_test(test_os_mon_init_unsupported),
            cmocka_unit_test(test_os_mon_init_param),
            cmocka_unit_test(test_os_mon_fini),
            cmocka_unit_test(test_os_mon_reset),
            cmocka_unit_test(test_os_mon_stop_core),
            cmocka_unit_test(test_os_mon_stop_pid),
            cmocka_unit_test(test_os_mon_stop_param),
            cmocka_unit_test(test_os_mon_start_cores_param),
            cmocka_unit_test(test_os_mon_start_cores_already_started),
            cmocka_unit_test(test_os_mon_start_cores),
            cmocka_unit_test(test_os_mon_start_pids_param),
            cmocka_unit_test(test_os_mon_start_pids),
            cmocka_unit_test(test_os_mon_add_pids),
            cmocka_unit_test(test_os_mon_remove_pids),
        };

        result += cmocka_run_group_tests(tests, test_init_mon, test_fini);

        return result;
}
