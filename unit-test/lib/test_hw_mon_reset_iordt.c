/*
 * BSD LICENSE
 *
 * Copyright(c) 2023 Intel Corporation. All rights reserved.
 * All rights reserved.
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
#include "mock_machine.h"
#include "test.h"

/* ======== hw_mon_reset_iordt ======== */

static void
test_hw_mon_reset_iordt_enable(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        unsigned *sockets = NULL;
        unsigned sockets_num;
        int ret;
        unsigned i;

        data->cap_l3ca.iordt = 1;
        data->cap_l3ca.iordt_on = 0;

        sockets = pqos_cpu_get_sockets(data->cpu, &sockets_num);
        for (i = 0; i < sockets_num; i++) {
                unsigned lcore;

                pqos_cpu_get_one_core(data->cpu, sockets[i], &lcore);

                expect_value(__wrap_msr_read, lcore, lcore);
                expect_value(__wrap_msr_read, reg, PQOS_MSR_L3_IO_QOS_CFG);
                will_return(__wrap_msr_read, PQOS_RETVAL_OK);
                will_return(__wrap_msr_read, 0);

                expect_value(__wrap_msr_write, lcore, lcore);
                expect_value(__wrap_msr_write, reg, PQOS_MSR_L3_IO_QOS_CFG);
                expect_value(__wrap_msr_write, value,
                             PQOS_MSR_L3_IO_QOS_MON_EN);
                will_return(__wrap_msr_write, PQOS_RETVAL_OK);
        }

        ret = hw_mon_reset_iordt(data->cpu, 1);
        assert_int_equal(ret, PQOS_RETVAL_OK);

        if (sockets != NULL)
                free(sockets);
}

static void
test_hw_mon_reset_iordt_disable(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        unsigned *sockets = NULL;
        unsigned sockets_num;
        int ret;
        unsigned i;

        data->cap_l3ca.iordt = 1;
        data->cap_l3ca.iordt_on = 1;

        sockets = pqos_cpu_get_sockets(data->cpu, &sockets_num);
        for (i = 0; i < sockets_num; i++) {
                unsigned lcore;

                pqos_cpu_get_one_core(data->cpu, sockets[i], &lcore);

                expect_value(__wrap_msr_read, lcore, lcore);
                expect_value(__wrap_msr_read, reg, PQOS_MSR_L3_IO_QOS_CFG);
                will_return(__wrap_msr_read, PQOS_RETVAL_OK);
                will_return(__wrap_msr_read, UINT64_MAX);

                expect_value(__wrap_msr_write, lcore, lcore);
                expect_value(__wrap_msr_write, reg, PQOS_MSR_L3_IO_QOS_CFG);
                expect_value(__wrap_msr_write, value,
                             UINT64_MAX & (~PQOS_MSR_L3_IO_QOS_MON_EN));
                will_return(__wrap_msr_write, PQOS_RETVAL_OK);
        }

        ret = hw_mon_reset_iordt(data->cpu, 0);
        assert_int_equal(ret, PQOS_RETVAL_OK);

        if (sockets != NULL)
                free(sockets);
}

static void
test_hw_mon_reset_iordt_error_read(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;

        data->cap_mon->iordt = 1;
        data->cap_mon->iordt_on = 1;

        expect_value(__wrap_msr_read, lcore, 0);
        expect_value(__wrap_msr_read, reg, PQOS_MSR_L3_IO_QOS_CFG);
        will_return(__wrap_msr_read, PQOS_RETVAL_ERROR);

        ret = hw_mon_reset_iordt(data->cpu, 0);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
}

static void
test_hw_mon_reset_iordt_error_write(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;

        data->cap_mon->iordt = 1;
        data->cap_mon->iordt_on = 1;

        expect_value(__wrap_msr_read, lcore, 0);
        expect_value(__wrap_msr_read, reg, PQOS_MSR_L3_IO_QOS_CFG);
        will_return(__wrap_msr_read, PQOS_RETVAL_OK);
        will_return(__wrap_msr_read, 0);

        expect_value(__wrap_msr_write, lcore, 0);
        expect_value(__wrap_msr_write, reg, PQOS_MSR_L3_IO_QOS_CFG);
        expect_value(__wrap_msr_write, value, 0);
        will_return(__wrap_msr_write, PQOS_RETVAL_ERROR);

        ret = hw_mon_reset_iordt(data->cpu, 0);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
}

int
main(void)
{
        int result = 0;

        const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_hw_mon_reset_iordt_enable),
            cmocka_unit_test(test_hw_mon_reset_iordt_disable),
            cmocka_unit_test(test_hw_mon_reset_iordt_error_read),
            cmocka_unit_test(test_hw_mon_reset_iordt_error_write)};

        result += cmocka_run_group_tests(tests, test_init_mon, test_fini);

        return result;
}
