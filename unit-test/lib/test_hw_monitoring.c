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

#include "cpu_registers.h"
#include "hw_monitoring.h"
#include "mock_cap.h"
#include "mock_perf_monitoring.h"
#include "perf_monitoring.h"
#include "test.h"

/* ======== init ======== */

static int
test_init_mon(void **state)
{
        int ret;

        expect_any_always(__wrap_perf_mon_init, cpu);
        expect_any_always(__wrap_perf_mon_init, cap);
        will_return_always(__wrap_perf_mon_init, PQOS_RETVAL_OK);

        ret = test_init(state, 1 << PQOS_CAP_TYPE_MON);
        if (ret == 0) {
                struct test_data *data = (struct test_data *)*state;

                ret = hw_mon_init(data->cpu, data->cap, NULL);
                assert_int_equal(ret, PQOS_RETVAL_OK);
        }

        return ret;
}

static int
test_fini_mon(void **state)
{
        int ret;

        will_return_always(__wrap_perf_mon_fini, PQOS_RETVAL_OK);

        ret = hw_mon_fini();
        assert_int_equal(ret, PQOS_RETVAL_OK);

        return test_fini(state);
}

/* ======== mock ======== */

int
hw_mon_assoc_read(const unsigned lcore, pqos_rmid_t *rmid)
{
        check_expected(lcore);
        *rmid = mock_type(int);

        return mock_type(int);
}

int
hw_mon_assoc_write(const unsigned lcore, const pqos_rmid_t rmid)
{
        check_expected(lcore);
        check_expected(rmid);

        return mock_type(int);
}

int
hw_mon_assoc_unused(struct pqos_mon_poll_ctx *ctx,
                    const enum pqos_mon_event event)
{
        assert_non_null(ctx);
        check_expected(event);

        ctx->rmid = mock_type(int);

        return mock_type(int);
}

int
hw_mon_start_perf(struct pqos_mon_data *group, enum pqos_mon_event event)
{
        assert_non_null(group);
        assert_non_null(group->intl);
        check_expected(event);

        group->intl->perf.event =
            event & (PQOS_PERF_EVENT_CYCLES | PQOS_PERF_EVENT_INSTRUCTIONS |
                     PQOS_PERF_EVENT_LLC_MISS | PQOS_PERF_EVENT_LLC_REF);

        return mock_type(int);
}

int
hw_mon_start_counter(struct pqos_mon_data *group, enum pqos_mon_event event)
{
        assert_non_null(group);
        assert_non_null(group->intl);
        check_expected(event);

        group->intl->hw.event =
            event & (PQOS_MON_EVENT_L3_OCCUP | PQOS_MON_EVENT_LMEM_BW |
                     PQOS_MON_EVENT_TMEM_BW | PQOS_MON_EVENT_RMEM_BW);

        group->intl->hw.num_ctx = 1;
        group->intl->hw.ctx = (struct pqos_mon_poll_ctx *)calloc(
            1, sizeof(group->intl->hw.ctx[0]));
        group->intl->hw.ctx[0].lcore = group->cores[0];
        group->intl->hw.ctx[0].cluster = 0;
        group->intl->hw.ctx[0].rmid = 1;

        return mock_type(int);
}

int
hw_mon_read_counter(struct pqos_mon_data *group,
                    const enum pqos_mon_event event)
{
        assert_non_null(group);

        check_expected(event);

        return mock_type(int);
}

int
hw_mon_stop_perf(struct pqos_mon_data *group)
{
        assert_non_null(group);

        return mock_type(int);
}

/* ======== hw_mon_assoc_get ======== */

static void
test_hw_mon_assoc_get(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        pqos_rmid_t rmid = 1;
        unsigned lcore = 2;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);

        expect_value(hw_mon_assoc_read, lcore, lcore);
        will_return(hw_mon_assoc_read, 2);
        will_return(hw_mon_assoc_read, PQOS_RETVAL_OK);

        ret = hw_mon_assoc_get(lcore, &rmid);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(rmid, 2);
}

static void
test_hw_mon_assoc_get_param(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        pqos_rmid_t rmid = 1;
        unsigned lcore = 2;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);

        ret = hw_mon_assoc_get(200, &rmid);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
        assert_int_equal(rmid, 1);

        ret = hw_mon_assoc_get(lcore, NULL);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
}

/* ======== hw_mon_reset ======== */

static void
test_hw_mon_reset(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        struct pqos_cpuinfo *cpu = data->cpu;
        int ret;
        unsigned i;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);

        for (i = 0; i < cpu->num_cores; ++i)
                expect_value(hw_mon_assoc_write, lcore, cpu->cores[i].lcore);
        expect_value_count(hw_mon_assoc_write, rmid, 0, cpu->num_cores);
        will_return_count(hw_mon_assoc_write, PQOS_RETVAL_OK, cpu->num_cores);

        ret = hw_mon_reset();
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_hw_mon_reset_error(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        struct pqos_cpuinfo *cpu = data->cpu;
        int ret;
        unsigned i;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);

        for (i = 0; i < cpu->num_cores; ++i)
                expect_value(hw_mon_assoc_write, lcore, cpu->cores[i].lcore);
        expect_value_count(hw_mon_assoc_write, rmid, 0, cpu->num_cores);
        will_return(hw_mon_assoc_write, PQOS_RETVAL_ERROR);
        will_return_count(hw_mon_assoc_write, PQOS_RETVAL_OK,
                          cpu->num_cores - 1);

        ret = hw_mon_reset();
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
}

/* ======== hw_mon_start ======== */

static void
test_hw_mon_start_mbm(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        unsigned num_cores = 1;
        unsigned cores[] = {1};
        enum pqos_mon_event event =
            PQOS_MON_EVENT_TMEM_BW | PQOS_MON_EVENT_LMEM_BW;
        struct pqos_mon_data group;
        struct pqos_mon_data_internal intl;
        int ret;

        memset(&group, 0, sizeof(struct pqos_mon_data));
        group.intl = &intl;
        memset(&intl, 0, sizeof(struct pqos_mon_data_internal));

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);

        expect_any(hw_mon_start_perf, event);
        will_return(hw_mon_start_perf, PQOS_RETVAL_OK);

        expect_value(hw_mon_assoc_read, lcore, cores[0]);
        will_return(hw_mon_assoc_read, 0);
        will_return(hw_mon_assoc_read, PQOS_RETVAL_OK);

        expect_value(hw_mon_start_counter, event, event);
        will_return(hw_mon_start_counter, PQOS_RETVAL_OK);

        ret = hw_mon_start(num_cores, cores, event, NULL, &group);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(group.num_cores, num_cores);

        /* free memory */
        expect_value(hw_mon_assoc_read, lcore, cores[0]);
        will_return(hw_mon_assoc_read, 1);
        will_return(hw_mon_assoc_read, PQOS_RETVAL_OK);
        expect_value(hw_mon_assoc_write, lcore, cores[1]);
        expect_value(hw_mon_assoc_write, rmid, 0);
        will_return(hw_mon_assoc_write, PQOS_RETVAL_OK);
        will_return(hw_mon_stop_perf, PQOS_RETVAL_OK);

        ret = hw_mon_stop(&group);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_hw_mon_start_perf(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        unsigned num_cores = 1;
        unsigned cores[] = {1};
        enum pqos_mon_event event =
            PQOS_PERF_EVENT_IPC | PQOS_MON_EVENT_TMEM_BW;
        struct pqos_mon_data group;
        struct pqos_mon_data_internal intl;
        int ret;

        memset(&group, 0, sizeof(struct pqos_mon_data));
        group.intl = &intl;
        memset(&intl, 0, sizeof(struct pqos_mon_data_internal));

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);

        expect_value(hw_mon_assoc_read, lcore, cores[0]);
        will_return(hw_mon_assoc_read, 0);
        will_return(hw_mon_assoc_read, PQOS_RETVAL_OK);

        expect_value(hw_mon_start_counter, event,
                     event | PQOS_PERF_EVENT_CYCLES |
                         PQOS_PERF_EVENT_INSTRUCTIONS);
        will_return(hw_mon_start_counter, PQOS_RETVAL_OK);

        expect_value(hw_mon_start_perf, event,
                     event | PQOS_PERF_EVENT_CYCLES |
                         PQOS_PERF_EVENT_INSTRUCTIONS);
        will_return(hw_mon_start_perf, PQOS_RETVAL_OK);

        ret = hw_mon_start(num_cores, cores, event, NULL, &group);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(group.num_cores, num_cores);

        /* free memory */
        expect_value(hw_mon_assoc_read, lcore, cores[0]);
        will_return(hw_mon_assoc_read, 1);
        will_return(hw_mon_assoc_read, PQOS_RETVAL_OK);
        expect_value(hw_mon_assoc_write, lcore, cores[1]);
        expect_value(hw_mon_assoc_write, rmid, 0);
        will_return(hw_mon_assoc_write, PQOS_RETVAL_OK);
        will_return(hw_mon_stop_perf, PQOS_RETVAL_OK);

        ret = hw_mon_stop(&group);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_hw_mon_poll(void **state __attribute__((unused)))
{
        struct pqos_mon_data group;
        int ret;

        expect_value(hw_mon_read_counter, event, PQOS_MON_EVENT_L3_OCCUP);
        will_return(hw_mon_read_counter, PQOS_RETVAL_OK);

        ret = hw_mon_poll(&group, PQOS_MON_EVENT_L3_OCCUP);
        assert_int_equal(ret, PQOS_RETVAL_OK);

        expect_value(hw_mon_read_counter, event, PQOS_MON_EVENT_LMEM_BW);
        will_return(hw_mon_read_counter, PQOS_RETVAL_OK);

        ret = hw_mon_poll(&group, PQOS_MON_EVENT_LMEM_BW);
        assert_int_equal(ret, PQOS_RETVAL_OK);

        expect_value(hw_mon_read_counter, event, PQOS_MON_EVENT_TMEM_BW);
        will_return(hw_mon_read_counter, PQOS_RETVAL_OK);

        ret = hw_mon_poll(&group, PQOS_MON_EVENT_TMEM_BW);
        assert_int_equal(ret, PQOS_RETVAL_OK);

        ret = hw_mon_poll(&group, (enum pqos_mon_event)0xFFFFFFFF);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
}

int
main(void)
{
        int result = 0;

        const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_hw_mon_assoc_get),
            cmocka_unit_test(test_hw_mon_assoc_get_param),
            cmocka_unit_test(test_hw_mon_reset),
            cmocka_unit_test(test_hw_mon_reset_error),
            cmocka_unit_test(test_hw_mon_start_mbm),
            cmocka_unit_test(test_hw_mon_start_perf),
            cmocka_unit_test(test_hw_mon_poll)};

        result += cmocka_run_group_tests(tests, test_init_mon, test_fini_mon);

        return result;
}
