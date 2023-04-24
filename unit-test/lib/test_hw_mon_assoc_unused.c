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
hw_mon_assoc_read(const unsigned lcore, pqos_rmid_t *rmid)
{
        switch (lcore) {
        case 0:
                *rmid = 0;
                break;
        case 1:
                *rmid = 1;
                break;
        case 2:
                *rmid = 2;
                break;
        case 3:
                *rmid = 3;
                break;
        case 4:
                *rmid = 2;
                break;
        default:
                *rmid = 0;
                break;
        }

        return mock_type(int);
}

/* ======== hw_mon_assoc_unused ======== */

static void
test_hw_alloc_assoc_unused(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        struct pqos_mon_poll_ctx ctx;
        struct pqos_mon_options opt;

        memset(&opt, 0, sizeof(opt));

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);

        will_return_count(hw_mon_assoc_read, PQOS_RETVAL_OK,
                          data->cpu->num_cores);

        ctx.lcore = 1;
        ctx.cluster = 0;

        ret = hw_mon_assoc_unused(&ctx, PQOS_MON_EVENT_TMEM_BW, 1, UINT32_MAX,
                                  &opt);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(ctx.rmid, 4);

        ctx.lcore = 5;
        ctx.cluster = 1;

        ret = hw_mon_assoc_unused(&ctx, PQOS_MON_EVENT_TMEM_BW, 1, UINT32_MAX,
                                  &opt);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(ctx.rmid, 1);
}

static void
test_hw_alloc_assoc_unused_invalid_cluster(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        struct pqos_mon_poll_ctx ctx;
        struct pqos_mon_options opt;

        memset(&opt, 0, sizeof(opt));

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);

        ctx.lcore = 1;
        ctx.cluster = 5;

        ret = hw_mon_assoc_unused(&ctx, PQOS_MON_EVENT_TMEM_BW, 1, UINT32_MAX,
                                  &opt);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
}

static void
test_hw_alloc_assoc_unused_range(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        struct pqos_mon_poll_ctx ctx;
        struct pqos_mon_options opt;
        unsigned rmid_min = 10;
        unsigned rmid_max = 20;

        memset(&opt, 0, sizeof(opt));

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);

        will_return_count(hw_mon_assoc_read, PQOS_RETVAL_OK,
                          data->cpu->num_cores / 2);

        ctx.lcore = 5;
        ctx.cluster = 0;

        ret = hw_mon_assoc_unused(&ctx, PQOS_MON_EVENT_TMEM_BW, rmid_min,
                                  rmid_max, &opt);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_in_range(ctx.rmid, rmid_min, rmid_max);
}

static void
test_hw_alloc_assoc_unused_not_found(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        struct pqos_mon_poll_ctx ctx;
        struct pqos_mon_options opt;
        unsigned rmid_min = 1;
        unsigned rmid_max = 2;

        memset(&opt, 0, sizeof(opt));

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);

        will_return_count(hw_mon_assoc_read, PQOS_RETVAL_OK,
                          data->cpu->num_cores / 2);

        ctx.lcore = 5;
        ctx.cluster = 0;

        ret = hw_mon_assoc_unused(&ctx, PQOS_MON_EVENT_TMEM_BW, rmid_min,
                                  rmid_max, &opt);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
}

int
main(void)
{
        int result = 0;

        const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_hw_alloc_assoc_unused),
            cmocka_unit_test(test_hw_alloc_assoc_unused_invalid_cluster),
            cmocka_unit_test(test_hw_alloc_assoc_unused_range),
            cmocka_unit_test(test_hw_alloc_assoc_unused_not_found),
        };

        result += cmocka_run_group_tests(tests, wrap_init_mon, wrap_fini_mon);

        return result;
}
