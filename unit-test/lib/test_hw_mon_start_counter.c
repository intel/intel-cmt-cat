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

#include "hw_monitoring.h"
#include "mock_cap.h"
#include "mock_perf_monitoring.h"
#include "test.h"

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

                ret = hw_mon_init(data->cpu, data->cap);
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
hw_mon_assoc_write(const unsigned lcore, const pqos_rmid_t rmid)
{
        check_expected(lcore);
        check_expected(rmid);

        return mock_type(int);
}

int
hw_mon_assoc_unused(struct pqos_mon_poll_ctx *ctx,
                    const enum pqos_mon_event event,
                    pqos_rmid_t min_rmid,
                    pqos_rmid_t max_rmid,
                    const struct pqos_mon_options *opt)
{
        assert_non_null(ctx);
        check_expected(event);
        assert_int_equal(min_rmid, 1);
        assert_int_equal(max_rmid, UINT32_MAX);
        assert_non_null(opt);

        ctx->rmid = mock_type(int);

        return mock_type(int);
}

/* ======== hw_mon_start_counter ======== */

static void
test_hw_mon_start_counter(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        unsigned num_cores = 1;
        unsigned cores[] = {1};
        struct pqos_mon_data group;
        struct pqos_mon_data_internal intl;
        enum pqos_mon_event event = PQOS_MON_EVENT_TMEM_BW;
        int ret;
        struct pqos_mon_options opt;

        memset(&opt, 0, sizeof(opt));

        memset(&group, 0, sizeof(struct pqos_mon_data));
        group.intl = &intl;
        group.num_cores = num_cores;
        group.cores = cores;
        memset(&intl, 0, sizeof(struct pqos_mon_data_internal));

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);

        expect_value(hw_mon_assoc_unused, event, event);
        will_return(hw_mon_assoc_unused, 1);
        will_return(hw_mon_assoc_unused, PQOS_RETVAL_OK);

        expect_value(hw_mon_assoc_write, lcore, cores[0]);
        expect_value(hw_mon_assoc_write, rmid, 1);
        will_return(hw_mon_assoc_write, PQOS_RETVAL_OK);

        ret = hw_mon_start_counter(&group, event, &opt);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(group.intl->hw.event, event);
        assert_int_equal(group.intl->hw.num_ctx, 1);
        assert_int_equal(group.intl->hw.ctx[0].rmid, 1);
        assert_int_equal(group.intl->hw.ctx[0].lcore, cores[0]);
        assert_int_equal(group.intl->hw.ctx[0].cluster, 0);

        free(group.intl->hw.ctx);
}

static void
test_hw_mon_start_counter_core_group(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        unsigned num_cores = 2;
        unsigned cores[] = {1, 5};
        struct pqos_mon_data group;
        struct pqos_mon_data_internal intl;
        enum pqos_mon_event event = PQOS_MON_EVENT_LMEM_BW;
        int ret;
        struct pqos_mon_options opt;

        memset(&opt, 0, sizeof(opt));

        memset(&group, 0, sizeof(struct pqos_mon_data));
        group.intl = &intl;
        group.num_cores = num_cores;
        group.cores = cores;
        memset(&intl, 0, sizeof(struct pqos_mon_data_internal));

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);

        expect_value(hw_mon_assoc_unused, event, event);
        will_return(hw_mon_assoc_unused, 1);
        will_return(hw_mon_assoc_unused, PQOS_RETVAL_OK);

        expect_value(hw_mon_assoc_unused, event, event);
        will_return(hw_mon_assoc_unused, 2);
        will_return(hw_mon_assoc_unused, PQOS_RETVAL_OK);

        expect_value(hw_mon_assoc_write, lcore, cores[0]);
        expect_value(hw_mon_assoc_write, rmid, 1);
        will_return(hw_mon_assoc_write, PQOS_RETVAL_OK);

        expect_value(hw_mon_assoc_write, lcore, cores[1]);
        expect_value(hw_mon_assoc_write, rmid, 2);
        will_return(hw_mon_assoc_write, PQOS_RETVAL_OK);

        ret = hw_mon_start_counter(&group, event, &opt);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(group.intl->hw.event, event);
        assert_int_equal(group.intl->hw.num_ctx, 2);
        assert_int_equal(group.intl->hw.ctx[0].rmid, 1);
        assert_int_equal(group.intl->hw.ctx[0].lcore, cores[0]);
        assert_int_equal(group.intl->hw.ctx[0].cluster, 0);
        assert_int_equal(group.intl->hw.ctx[1].rmid, 2);
        assert_int_equal(group.intl->hw.ctx[1].lcore, cores[1]);
        assert_int_equal(group.intl->hw.ctx[1].cluster, 1);

        free(group.intl->hw.ctx);
}

int
main(void)
{
        int result = 0;

        const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_hw_mon_start_counter),
            cmocka_unit_test(test_hw_mon_start_counter_core_group)};

        result += cmocka_run_group_tests(tests, test_init_mon, test_fini_mon);

        return result;
}
