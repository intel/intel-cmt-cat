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

/* ======== MBM counter rollover ======== */

static void
set_mbm_counter_length(struct pqos_cap_mon *mon, const unsigned length)
{
        unsigned i;

        for (i = 0; i < mon->num_events; i++)
                if (mon->events[i].type == PQOS_MON_EVENT_LMEM_BW ||
                    mon->events[i].type == PQOS_MON_EVENT_TMEM_BW)
                        mon->events[i].counter_length = length;
}

/**
 * Teardown of the rollover cases: the MBM counters back to the fixture's 24
 * bits, which runs after a case that failed as well, so that the cases after it
 * do not read counters of another width
 */
static int
mbm_counter_length_back(void **state)
{
        const struct test_data *data = (struct test_data *)*state;

        set_mbm_counter_length(data->cap_mon, 24);
        return 0;
}

struct mbm_read {
        int ret;
        int valid_mbm_read;
        struct pqos_event_values values;
};

/**
 * Reads one MBM event of a group whose previous poll left the local and total
 * counters at prev_local and prev_total, with the counters \a width bits wide,
 * which mbm_counter_length_back() puts back
 */
static struct mbm_read
read_mbm(const struct test_data *data,
         const enum pqos_mon_event event,
         const unsigned width,
         const uint64_t prev_local,
         const uint64_t prev_total,
         const int value)
{
        unsigned cores[] = {1};
        struct pqos_mon_data group;
        struct pqos_mon_data_internal intl;
        struct pqos_mon_poll_ctx ctx;
        struct mbm_read result;

        memset(&group, 0, sizeof(group));
        memset(&intl, 0, sizeof(intl));
        memset(&ctx, 0, sizeof(ctx));
        group.intl = &intl;
        group.num_cores = 1;
        group.cores = cores;
        group.values.mbm_local = prev_local;
        group.values.mbm_total = prev_total;
        intl.hw.ctx = &ctx;
        intl.hw.num_ctx = 1;
        intl.valid_mbm_read = 1;
        ctx.lcore = cores[0];
        ctx.rmid = 2;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);

        expect_value(hw_mon_read, lcore, cores[0]);
        expect_value(hw_mon_read, rmid, ctx.rmid);
        expect_value(hw_mon_read, event,
                     event == PQOS_MON_EVENT_TMEM_BW ? 2 : 3);
        will_return(hw_mon_read, value);
        will_return(hw_mon_read, PQOS_RETVAL_OK);

        set_mbm_counter_length(data->cap_mon, width);
        result.ret = hw_mon_read_counter(&group, event);

        result.valid_mbm_read = intl.valid_mbm_read;
        result.values = group.values;

        return result;
}

/*
 * The total counter rolled over in an earlier poll and the local one has not
 * yet: total is below local, but it has not rolled over since the last poll.
 */
static void
test_hw_mon_read_counter_tmem_32bit_below_local(void **state)
{
        const struct test_data *data = (struct test_data *)*state;
        const struct pqos_monitor *pmon;
        struct mbm_read r;

        pqos_cap_get_event(data->cap, PQOS_MON_EVENT_TMEM_BW, &pmon);

        r = read_mbm(data, PQOS_MON_EVENT_TMEM_BW, 32, 0xFFFFFF00, 0x100,
                     0x200);
        assert_int_equal(r.ret, PQOS_RETVAL_OK);
        assert_int_equal(r.valid_mbm_read, 1);
        assert_int_equal(r.values.mbm_total, 0x200);
        assert_int_equal(r.values.mbm_total_delta, 0x100 * pmon->scale_factor);
}

/*
 * Total memory bandwidth monitored alone, so the local counter is never read:
 * the total counter rolls over since the last poll.
 */
static void
test_hw_mon_read_counter_tmem_32bit_rollover(void **state)
{
        const struct test_data *data = (struct test_data *)*state;
        struct mbm_read r;

        r = read_mbm(data, PQOS_MON_EVENT_TMEM_BW, 32, 0, 0xFFFFFFF0, 0x10);
        assert_int_equal(r.ret, PQOS_RETVAL_OVERFLOW);
        assert_int_equal(r.valid_mbm_read, 0);
        assert_int_equal(r.values.mbm_total, 0x10);
}

/*
 * The same rollover of the local counter, which the LMEM case reports.
 */
static void
test_hw_mon_read_counter_lmem_32bit_rollover(void **state)
{
        const struct test_data *data = (struct test_data *)*state;
        struct mbm_read r;

        r = read_mbm(data, PQOS_MON_EVENT_LMEM_BW, 32, 0xFFFFFFF0, 0, 0x10);
        assert_int_equal(r.ret, PQOS_RETVAL_OVERFLOW);
        assert_int_equal(r.valid_mbm_read, 0);
        assert_int_equal(r.values.mbm_local, 0x10);
}

/*
 * A 24-bit counter that rolls over is not reported: the check is for 32-bit
 * counters, and get_delta() takes the rollover of a 24-bit one into account.
 */
static void
test_hw_mon_read_counter_tmem_24bit_rollover(void **state)
{
        const struct test_data *data = (struct test_data *)*state;
        const struct pqos_monitor *pmon;
        struct mbm_read r;

        pqos_cap_get_event(data->cap, PQOS_MON_EVENT_TMEM_BW, &pmon);

        r = read_mbm(data, PQOS_MON_EVENT_TMEM_BW, 24, 0, 0xFFFFF0, 0x10);
        assert_int_equal(r.ret, PQOS_RETVAL_OK);
        assert_int_equal(r.valid_mbm_read, 1);
        assert_int_equal(r.values.mbm_total, 0x10);
        assert_int_equal(r.values.mbm_total_delta, 0x20 * pmon->scale_factor);
}

int
main(void)
{
        int result = 0;

        const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_hw_mon_read_counter_tmem),
            cmocka_unit_test(test_hw_mon_read_counter_lmem),
            cmocka_unit_test(test_hw_mon_read_counter_llc),
            cmocka_unit_test_teardown(
                test_hw_mon_read_counter_tmem_32bit_below_local,
                mbm_counter_length_back),
            cmocka_unit_test_teardown(
                test_hw_mon_read_counter_tmem_32bit_rollover,
                mbm_counter_length_back),
            cmocka_unit_test_teardown(
                test_hw_mon_read_counter_lmem_32bit_rollover,
                mbm_counter_length_back),
            cmocka_unit_test_teardown(
                test_hw_mon_read_counter_tmem_24bit_rollover,
                mbm_counter_length_back)};

        result += cmocka_run_group_tests(tests, wrap_init_mon, wrap_fini_mon);

        return result;
}
