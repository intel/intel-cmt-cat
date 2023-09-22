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

#include "cpu_registers.h"
#include "hw_monitoring.h"
#include "mock_cap.h"
#include "mock_perf_monitoring.h"
#include "perf_monitoring.h"
#include "test.h"

/* ======== init ======== */

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
                    const enum pqos_mon_event event,
                    pqos_rmid_t min_rmid,
                    pqos_rmid_t max_rmid,
                    const struct pqos_mon_options *opt)
{
        int ret;

        assert_non_null(ctx);
        check_expected(event);
        check_expected(min_rmid);
        check_expected(max_rmid);
        assert_non_null(opt);

        ret = mock_type(int);
        if (ret == PQOS_RETVAL_OK)
                ctx->rmid = mock_type(int);

        return ret;
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
hw_mon_start_counter(struct pqos_mon_data *group,
                     enum pqos_mon_event event,
                     const struct pqos_mon_options *opt)
{
        assert_non_null(group);
        assert_non_null(group->intl);
        check_expected(event);
        assert_non_null(opt);

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

int
hw_mon_reset_iordt(const struct pqos_cpuinfo *cpu, const int enable)
{
        assert_non_null(cpu);
        check_expected(enable);

        return mock_type(int);
}

/* ======== hw_mon_assoc_get_core ======== */

static void
test_hw_mon_assoc_get_core(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        pqos_rmid_t rmid = 1;
        unsigned lcore = 2;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);
        will_return_maybe(__wrap__pqos_get_dev, data->dev);

        expect_value(hw_mon_assoc_read, lcore, lcore);
        will_return(hw_mon_assoc_read, 2);
        will_return(hw_mon_assoc_read, PQOS_RETVAL_OK);

        ret = hw_mon_assoc_get_core(lcore, &rmid);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(rmid, 2);
}

static void
test_hw_mon_assoc_get_core_param(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        pqos_rmid_t rmid = 1;
        unsigned lcore = 2;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);
        will_return_maybe(__wrap__pqos_get_dev, data->dev);

        ret = hw_mon_assoc_get_core(200, &rmid);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
        assert_int_equal(rmid, 1);

        ret = hw_mon_assoc_get_core(lcore, NULL);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
}

/* ======== hw_mon_assoc_get_channel ======== */

static void
test_hw_mon_assoc_get_channel(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        pqos_rmid_t rmid = 1;
        pqos_channel_t channel_id = 0x201;

        data->cap_mon->iordt = 1;
        data->cap_mon->iordt_on = 1;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);
        will_return_maybe(__wrap__pqos_get_dev, data->dev);

        expect_value(__wrap_iordt_mon_assoc_read, channel_id, channel_id);
        will_return(__wrap_iordt_mon_assoc_read, PQOS_RETVAL_OK);
        will_return(__wrap_iordt_mon_assoc_read, 2);

        ret = hw_mon_assoc_get_channel(channel_id, &rmid);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(rmid, 2);
}

static void
test_hw_mon_assoc_get_channel_param(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        pqos_rmid_t rmid = 1;
        pqos_channel_t channel_id = 0x201;

        data->cap_mon->iordt = 1;
        data->cap_mon->iordt_on = 1;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);
        will_return_maybe(__wrap__pqos_get_dev, data->dev);

        /* Invalid channel Id */
        ret = hw_mon_assoc_get_channel(0xDEAD, &rmid);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
        assert_int_equal(rmid, 1);

        /* NULL param */
        ret = hw_mon_assoc_get_channel(channel_id, NULL);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);

        /* channel not supporting RMID tagging */
        ret = hw_mon_assoc_get_channel(0x202, &rmid);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);

        /* I/O RDT monitoring disabled */
        data->cap_mon->iordt = 1;
        data->cap_mon->iordt_on = 0;
        ret = hw_mon_assoc_get_channel(channel_id, &rmid);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);

        /* I/O RDT monitoring unsupported */
        data->cap_mon->iordt = 0;
        data->cap_mon->iordt_on = 0;
        ret = hw_mon_assoc_get_channel(channel_id, &rmid);
        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);
}

/* ======== hw_mon_reset ======== */

static void
hw_mon_reset_mock(const struct pqos_cpuinfo *cpu)
{
        unsigned i;

        for (i = 0; i < cpu->num_cores; ++i)
                expect_value(hw_mon_assoc_write, lcore, cpu->cores[i].lcore);
        expect_value_count(hw_mon_assoc_write, rmid, 0, cpu->num_cores);
        will_return_count(hw_mon_assoc_write, PQOS_RETVAL_OK, cpu->num_cores);
}

static void
test_hw_mon_reset(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);
        will_return_maybe(__wrap__pqos_get_dev, data->dev);

        hw_mon_reset_mock(data->cpu);

        ret = hw_mon_reset(NULL);
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
        will_return_maybe(__wrap__pqos_get_dev, data->dev);

        for (i = 0; i < cpu->num_cores; ++i)
                expect_value(hw_mon_assoc_write, lcore, cpu->cores[i].lcore);
        expect_value_count(hw_mon_assoc_write, rmid, 0, cpu->num_cores);
        will_return(hw_mon_assoc_write, PQOS_RETVAL_ERROR);
        will_return_count(hw_mon_assoc_write, PQOS_RETVAL_OK,
                          cpu->num_cores - 1);

        ret = hw_mon_reset(NULL);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
}

static void
test_hw_mon_reset_unsupported(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);
        will_return_maybe(__wrap__pqos_get_dev, data->dev);

        ret = hw_mon_reset(NULL);
        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);
}

static void
test_hw_mon_reset_iordt_unsupported(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        struct pqos_mon_config cfg;

        data->cap_mon->iordt = 0;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);
        will_return_maybe(__wrap__pqos_get_dev, data->dev);

        memset(&cfg, 0, sizeof(cfg));
        cfg.l3_iordt = PQOS_REQUIRE_IORDT_ON;

        ret = hw_mon_reset(&cfg);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
}

static void
test_hw_mon_reset_iordt_enable(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        struct pqos_mon_config cfg;

        data->cap_mon->iordt = 1;
        data->cap_mon->iordt_on = 0;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);
        will_return_maybe(__wrap__pqos_get_dev, data->dev);

        expect_function_call(__wrap_iordt_mon_assoc_reset);
        will_return(__wrap_iordt_mon_assoc_reset, PQOS_RETVAL_OK);

        expect_value(hw_mon_reset_iordt, enable, 1);
        will_return(hw_mon_reset_iordt, PQOS_RETVAL_OK);
        hw_mon_reset_mock(data->cpu);

        memset(&cfg, 0, sizeof(cfg));
        cfg.l3_iordt = PQOS_REQUIRE_IORDT_ON;

        ret = hw_mon_reset(&cfg);
        assert_int_equal(ret, PQOS_RETVAL_OK);

        /* I/O RDT already enabled */
        data->cap_mon->iordt = 1;
        data->cap_mon->iordt_on = 1;
        expect_function_call(__wrap_iordt_mon_assoc_reset);
        will_return(__wrap_iordt_mon_assoc_reset, PQOS_RETVAL_OK);

        hw_mon_reset_mock(data->cpu);

        ret = hw_mon_reset(&cfg);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_hw_mon_reset_iordt_disable(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        struct pqos_mon_config cfg;

        data->cap_mon->iordt = 1;
        data->cap_mon->iordt_on = 1;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);
        will_return_maybe(__wrap__pqos_get_dev, data->dev);

        expect_function_call(__wrap_iordt_mon_assoc_reset);
        will_return(__wrap_iordt_mon_assoc_reset, PQOS_RETVAL_OK);

        expect_value(hw_mon_reset_iordt, enable, 0);
        will_return(hw_mon_reset_iordt, PQOS_RETVAL_OK);
        hw_mon_reset_mock(data->cpu);

        memset(&cfg, 0, sizeof(cfg));
        cfg.l3_iordt = PQOS_REQUIRE_IORDT_OFF;

        ret = hw_mon_reset(&cfg);
        assert_int_equal(ret, PQOS_RETVAL_OK);

        /* I/O RDT already disabled */
        data->cap_mon->iordt = 1;
        data->cap_mon->iordt_on = 0;
        hw_mon_reset_mock(data->cpu);
        ret = hw_mon_reset(&cfg);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

/* ======== hw_mon_start ======== */

static void
test_hw_mon_start_param(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        struct pqos_mon_data group;
        struct pqos_mon_data_internal intl;
        int ret;
        unsigned num_cores = 1;
        unsigned cores[] = {1};
        enum pqos_mon_event event = PQOS_MON_EVENT_LMEM_BW;
        struct pqos_mon_options opt;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);
        will_return_maybe(__wrap__pqos_get_dev, data->dev);

        memset(&opt, 0, sizeof(opt));
        memset(&group, 0, sizeof(struct pqos_mon_data));
        group.intl = &intl;
        memset(&intl, 0, sizeof(struct pqos_mon_data_internal));

        /* Invalid event */
        ret = hw_mon_start_cores(num_cores, cores, 0xDEAD, NULL, &group, &opt);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);

        /* Core already monitored */
        expect_value(hw_mon_assoc_read, lcore, cores[0]);
        will_return(hw_mon_assoc_read, 1);
        will_return(hw_mon_assoc_read, PQOS_RETVAL_OK);
        ret = hw_mon_start_cores(num_cores, cores, event, NULL, &group, &opt);
        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);

        /* Invalid core */
        cores[0] = 1000000;
        ret = hw_mon_start_cores(num_cores, cores, 0xDEAD, NULL, &group, &opt);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
}

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
        struct pqos_mon_options opt;

        memset(&opt, 0, sizeof(opt));

        memset(&group, 0, sizeof(struct pqos_mon_data));
        group.intl = &intl;
        memset(&intl, 0, sizeof(struct pqos_mon_data_internal));

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);
        will_return_maybe(__wrap__pqos_get_dev, data->dev);

        expect_any(hw_mon_start_perf, event);
        will_return(hw_mon_start_perf, PQOS_RETVAL_OK);

        expect_value(hw_mon_assoc_read, lcore, cores[0]);
        will_return(hw_mon_assoc_read, 0);
        will_return(hw_mon_assoc_read, PQOS_RETVAL_OK);

        expect_value(hw_mon_start_counter, event, event);
        will_return(hw_mon_start_counter, PQOS_RETVAL_OK);

        ret = hw_mon_start_cores(num_cores, cores, event, NULL, &group, &opt);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(group.num_cores, num_cores);

        /* free memory */
        expect_value(hw_mon_assoc_read, lcore, cores[0]);
        will_return(hw_mon_assoc_read, 1);
        will_return(hw_mon_assoc_read, PQOS_RETVAL_OK);
        expect_value(hw_mon_assoc_write, lcore, cores[0]);
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
        struct pqos_mon_options opt;

        memset(&opt, 0, sizeof(opt));

        memset(&group, 0, sizeof(struct pqos_mon_data));
        group.intl = &intl;
        memset(&intl, 0, sizeof(struct pqos_mon_data_internal));

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);
        will_return_maybe(__wrap__pqos_get_dev, data->dev);

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

        ret = hw_mon_start_cores(num_cores, cores, event, NULL, &group, &opt);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(group.num_cores, num_cores);

        /* free memory */
        expect_value(hw_mon_assoc_read, lcore, cores[0]);
        will_return(hw_mon_assoc_read, 1);
        will_return(hw_mon_assoc_read, PQOS_RETVAL_OK);
        expect_value(hw_mon_assoc_write, lcore, cores[0]);
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

/* ======== hw_mon_start_channels ======== */

static void
test_hw_mon_start_channels_unsupported(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        pqos_channel_t channel_id = 0x201;
        enum pqos_mon_event event =
            PQOS_PERF_EVENT_IPC | PQOS_MON_EVENT_TMEM_BW;
        struct pqos_mon_data group;
        struct pqos_mon_data_internal intl;
        struct pqos_mon_options opt;
        int ret;

        memset(&group, 0, sizeof(struct pqos_mon_data));
        group.intl = &intl;
        memset(&intl, 0, sizeof(struct pqos_mon_data_internal));
        memset(&opt, 0, sizeof(opt));

        data->cap_mon->iordt = 0;
        data->cap_mon->iordt_on = 0;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);
        will_return_maybe(__wrap__pqos_get_dev, data->dev);

        ret = hw_mon_start_channels(1, &channel_id, event, NULL, &group, &opt);
        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);
}

static void
test_hw_mon_start_channels_disabled(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        pqos_channel_t channel_id = 0x201;
        enum pqos_mon_event event =
            PQOS_PERF_EVENT_IPC | PQOS_MON_EVENT_TMEM_BW;
        struct pqos_mon_data group;
        struct pqos_mon_data_internal intl;
        struct pqos_mon_options opt;
        int ret;

        memset(&group, 0, sizeof(struct pqos_mon_data));
        group.intl = &intl;
        memset(&intl, 0, sizeof(struct pqos_mon_data_internal));
        memset(&opt, 0, sizeof(opt));

        data->cap_mon->iordt = 1;
        data->cap_mon->iordt_on = 0;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);
        will_return_maybe(__wrap__pqos_get_dev, data->dev);

        ret = hw_mon_start_channels(1, &channel_id, event, NULL, &group, &opt);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
}

static void
test_hw_mon_start_channels_param(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        pqos_channel_t channel_id = 0x201;
        enum pqos_mon_event event = PQOS_MON_EVENT_TMEM_BW;
        struct pqos_mon_data group;
        struct pqos_mon_data_internal intl;
        struct pqos_mon_options opt;
        int ret;

        memset(&group, 0, sizeof(struct pqos_mon_data));
        group.intl = &intl;
        memset(&intl, 0, sizeof(struct pqos_mon_data_internal));
        memset(&opt, 0, sizeof(opt));

        data->cap_mon->iordt = 1;
        data->cap_mon->iordt_on = 1;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);
        will_return_maybe(__wrap__pqos_get_dev, data->dev);

        /* Invalid event */
        ret = hw_mon_start_channels(1, &channel_id, 0xDEAD, NULL, &group, &opt);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);

        /* Unsupported event */
        ret = hw_mon_start_channels(1, &channel_id, PQOS_PERF_EVENT_IPC, NULL,
                                    &group, &opt);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);

        /* Channel already monitored */
        expect_value(__wrap_iordt_mon_assoc_read, channel_id, channel_id);
        will_return(__wrap_iordt_mon_assoc_read, PQOS_RETVAL_OK);
        will_return(__wrap_iordt_mon_assoc_read, 2);
        ret = hw_mon_start_channels(1, &channel_id, event, NULL, &group, &opt);
        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);

        /* No rmid tagging support */
        channel_id = 0x202;
        ret = hw_mon_start_channels(1, &channel_id, event, NULL, &group, &opt);
        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);

        /* Invalid channel */
        channel_id = 0xDEAD;
        ret = hw_mon_start_channels(1, &channel_id, event, NULL, &group, &opt);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
}

static void
test_hw_mon_start_channels(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        pqos_channel_t channel_id = 0x201;
        enum pqos_mon_event event = PQOS_MON_EVENT_TMEM_BW;
        struct pqos_mon_data group;
        struct pqos_mon_data_internal intl;
        struct pqos_mon_options opt;
        int ret;

        memset(&group, 0, sizeof(struct pqos_mon_data));
        group.intl = &intl;
        memset(&intl, 0, sizeof(struct pqos_mon_data_internal));
        memset(&opt, 0, sizeof(opt));

        data->cap_mon->iordt = 1;
        data->cap_mon->iordt_on = 1;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);
        will_return_maybe(__wrap__pqos_get_dev, data->dev);

        /* Channel is not monitored */
        expect_value(__wrap_iordt_mon_assoc_read, channel_id, channel_id);
        will_return(__wrap_iordt_mon_assoc_read, PQOS_RETVAL_OK);
        will_return(__wrap_iordt_mon_assoc_read, 0);

        expect_value(hw_mon_assoc_unused, event, event);
        expect_value(hw_mon_assoc_unused, min_rmid, 0);
        expect_value(hw_mon_assoc_unused, max_rmid, 32);
        will_return(hw_mon_assoc_unused, PQOS_RETVAL_OK);
        will_return(hw_mon_assoc_unused, 0);

        expect_value(__wrap_iordt_get_numa, channel_id, channel_id);
        will_return(__wrap_iordt_get_numa, PQOS_RETVAL_OK);
        will_return(__wrap_iordt_get_numa, 0);

        expect_value(__wrap_iordt_mon_assoc_write, channel_id, channel_id);
        expect_value(__wrap_iordt_mon_assoc_write, rmid, 0);
        will_return(__wrap_iordt_mon_assoc_write, PQOS_RETVAL_OK);

        ret = hw_mon_start_channels(1, &channel_id, event, NULL, &group, &opt);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(group.num_channels, 1);
        assert_int_equal(group.channels[0], channel_id);

        /* Free memory */
        expect_value(__wrap_iordt_mon_assoc_write, channel_id, channel_id);
        expect_value(__wrap_iordt_mon_assoc_write, rmid, 0);
        will_return(__wrap_iordt_mon_assoc_write, PQOS_RETVAL_OK);
        will_return(hw_mon_stop_perf, PQOS_RETVAL_OK);

        ret = hw_mon_stop(&group);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

int
main(void)
{
        int result = 0;

        const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_hw_mon_assoc_get_core),
            cmocka_unit_test(test_hw_mon_assoc_get_core_param),
            cmocka_unit_test(test_hw_mon_assoc_get_channel),
            cmocka_unit_test(test_hw_mon_assoc_get_channel_param),
            cmocka_unit_test(test_hw_mon_reset),
            cmocka_unit_test(test_hw_mon_reset_error),
            cmocka_unit_test(test_hw_mon_start_param),
            cmocka_unit_test(test_hw_mon_start_mbm),
            cmocka_unit_test(test_hw_mon_start_perf),
            cmocka_unit_test(test_hw_mon_poll),
            cmocka_unit_test(test_hw_mon_reset_iordt_disable),
            cmocka_unit_test(test_hw_mon_reset_iordt_enable),
            cmocka_unit_test(test_hw_mon_reset_iordt_unsupported),
            cmocka_unit_test(test_hw_mon_start_channels_unsupported),
            cmocka_unit_test(test_hw_mon_start_channels_disabled),
            cmocka_unit_test(test_hw_mon_start_channels_param),
            cmocka_unit_test(test_hw_mon_start_channels)};

        const struct CMUnitTest tests_unsupported[] = {
            cmocka_unit_test(test_hw_mon_reset_unsupported)};

        result += cmocka_run_group_tests(tests, test_init_mon, test_fini);
        result += cmocka_run_group_tests(tests_unsupported,
                                         test_init_unsupported, test_fini);

        return result;
}
