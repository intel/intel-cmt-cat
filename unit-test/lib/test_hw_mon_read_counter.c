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

#include "hw_monitoring.h"
#include "mock_cap.h"
#include "mock_perf_monitoring.h"
#include "test.h"

static int
wrap_init_mon(void **state)
{
        int ret;

        expect_any_always(__wrap_perf_mon_init, cpu);
        expect_any_always(__wrap_perf_mon_init, cap);
        will_return_always(__wrap_perf_mon_init, PQOS_RETVAL_OK);

        ret = test_init(state, 1 << PQOS_CAP_TYPE_MON);
        if (ret == 0) {
                struct test_data *data = (struct test_data *)*state;

                ret = hw_mon_init(data->cpu, data->cap);
                assert_int_equal(ret, PQOS_RETVAL_OK);
        }

        return ret;
}

static int
wrap_fini_mon(void **state)
{
        int ret;

        will_return_always(__wrap_perf_mon_fini, PQOS_RETVAL_OK);

        ret = hw_mon_fini();
        assert_int_equal(ret, PQOS_RETVAL_OK);

        return test_fini(state);
}

/* ======== mock ======== */

int
hw_mon_read(const unsigned lcore,
            const pqos_rmid_t rmid,
            const unsigned event,
            uint64_t *value)
{
        check_expected(lcore);
        check_expected(rmid);
        check_expected(event);

        *value = mock_type(int);

        return mock_type(int);
}

/* ======== hw_mon_start_counter ======== */

static void
test_hw_mon_read_counter_tmem(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        unsigned num_cores = 1;
        unsigned cores[] = {1};
        struct pqos_mon_data group;
        struct pqos_mon_data_internal intl;
        struct pqos_mon_poll_ctx ctx;
        enum pqos_mon_event event = PQOS_MON_EVENT_TMEM_BW;
        const struct pqos_monitor *pmon;
        int ret;

        pqos_cap_get_event(data->cap, event, &pmon);

        memset(&group, 0, sizeof(struct pqos_mon_data));
        group.intl = &intl;
        group.num_cores = num_cores;
        group.cores = cores;
        memset(&intl, 0, sizeof(struct pqos_mon_data_internal));
        intl.hw.ctx = &ctx;
        intl.hw.num_ctx = 1;
        memset(&ctx, 0, sizeof(struct pqos_mon_poll_ctx));
        ctx.lcore = cores[0];
        ctx.cluster = 0;
        ctx.rmid = 2;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);

        expect_value(hw_mon_read, lcore, cores[0]);
        expect_value(hw_mon_read, rmid, ctx.rmid);
        expect_value(hw_mon_read, event, 2);
        will_return(hw_mon_read, 5);
        will_return(hw_mon_read, PQOS_RETVAL_OK);

        ret = hw_mon_read_counter(&group, event);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(group.values.mbm_total, 5);
        assert_int_equal(group.values.mbm_total_delta, 0);

        group.intl->valid_mbm_read = 1;

        expect_value(hw_mon_read, lcore, cores[0]);
        expect_value(hw_mon_read, rmid, ctx.rmid);
        expect_value(hw_mon_read, event, 2);
        will_return(hw_mon_read, 10);
        will_return(hw_mon_read, PQOS_RETVAL_OK);

        ret = hw_mon_read_counter(&group, event);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(group.values.mbm_total, 10);
        assert_int_equal(group.values.mbm_total_delta, 5 * pmon->scale_factor);
}

static void
test_hw_mon_read_counter_lmem(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        unsigned num_cores = 1;
        unsigned cores[] = {1};
        struct pqos_mon_data group;
        struct pqos_mon_data_internal intl;
        struct pqos_mon_poll_ctx ctx;
        enum pqos_mon_event event = PQOS_MON_EVENT_LMEM_BW;
        const struct pqos_monitor *pmon;
        int ret;

        pqos_cap_get_event(data->cap, event, &pmon);

        memset(&group, 0, sizeof(struct pqos_mon_data));
        group.intl = &intl;
        group.num_cores = num_cores;
        group.cores = cores;
        memset(&intl, 0, sizeof(struct pqos_mon_data_internal));
        intl.hw.ctx = &ctx;
        intl.hw.num_ctx = 1;
        memset(&ctx, 0, sizeof(struct pqos_mon_poll_ctx));
        ctx.lcore = cores[0];
        ctx.cluster = 0;
        ctx.rmid = 2;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);

        expect_value(hw_mon_read, lcore, cores[0]);
        expect_value(hw_mon_read, rmid, ctx.rmid);
        expect_value(hw_mon_read, event, 3);
        will_return(hw_mon_read, 5);
        will_return(hw_mon_read, PQOS_RETVAL_OK);

        ret = hw_mon_read_counter(&group, event);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(group.values.mbm_local, 5);
        assert_int_equal(group.values.mbm_local_delta, 0);

        group.intl->valid_mbm_read = 1;

        expect_value(hw_mon_read, lcore, cores[0]);
        expect_value(hw_mon_read, rmid, ctx.rmid);
        expect_value(hw_mon_read, event, 3);
        will_return(hw_mon_read, 10);
        will_return(hw_mon_read, PQOS_RETVAL_OK);

        ret = hw_mon_read_counter(&group, event);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(group.values.mbm_local, 10);
        assert_int_equal(group.values.mbm_local_delta, 5 * pmon->scale_factor);
}

static void
test_hw_mon_read_counter_llc(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        unsigned num_cores = 1;
        unsigned cores[] = {1};
        struct pqos_mon_data group;
        struct pqos_mon_data_internal intl;
        struct pqos_mon_poll_ctx ctx;
        enum pqos_mon_event event = PQOS_MON_EVENT_L3_OCCUP;
        const struct pqos_monitor *pmon;
        int ret;

        pqos_cap_get_event(data->cap, event, &pmon);

        memset(&group, 0, sizeof(struct pqos_mon_data));
        group.intl = &intl;
        group.num_cores = num_cores;
        group.cores = cores;
        memset(&intl, 0, sizeof(struct pqos_mon_data_internal));
        intl.hw.ctx = &ctx;
        intl.hw.num_ctx = 1;
        memset(&ctx, 0, sizeof(struct pqos_mon_poll_ctx));
        ctx.lcore = cores[0];
        ctx.cluster = 0;
        ctx.rmid = 2;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);

        expect_value(hw_mon_read, lcore, cores[0]);
        expect_value(hw_mon_read, rmid, ctx.rmid);
        expect_value(hw_mon_read, event, 1);
        will_return(hw_mon_read, 5);
        will_return(hw_mon_read, PQOS_RETVAL_OK);

        ret = hw_mon_read_counter(&group, event);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(group.values.llc, 5 * pmon->scale_factor);
}

int
main(void)
{
        int result = 0;

        const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_hw_mon_read_counter_tmem),
            cmocka_unit_test(test_hw_mon_read_counter_lmem),
            cmocka_unit_test(test_hw_mon_read_counter_llc)};

        result += cmocka_run_group_tests(tests, wrap_init_mon, wrap_fini_mon);

        return result;
}
