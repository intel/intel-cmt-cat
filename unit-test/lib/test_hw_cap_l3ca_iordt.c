/*
 * BSD LICENSE
 *
 * Copyright(c) 2020 Intel Corporation. All rights reserved.
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

#include "hw_cap.h"
#include "test.h"

/* ======== hw_cap_l3ca_iordt ======== */

static void
test_hw_cap_l3ca_iordt_enabled(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        int enabled;

        expect_any(__wrap_msr_read, lcore);
        expect_value(__wrap_msr_read, reg, PQOS_MSR_L3_IO_QOS_CFG);
        will_return(__wrap_msr_read, PQOS_RETVAL_OK);
        will_return(__wrap_msr_read, PQOS_MSR_L3_IO_QOS_CA_EN);

        expect_any(__wrap_msr_read, lcore);
        expect_value(__wrap_msr_read, reg, PQOS_MSR_L3_IO_QOS_CFG);
        will_return(__wrap_msr_read, PQOS_RETVAL_OK);
        will_return(__wrap_msr_read, PQOS_MSR_L3_IO_QOS_CA_EN);

        ret = hw_cap_l3ca_iordt(data->cpu, &enabled);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(enabled, 1);
}

static void
test_hw_cap_l3ca_iordt_disabled(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        int enabled;

        expect_any(__wrap_msr_read, lcore);
        expect_value(__wrap_msr_read, reg, PQOS_MSR_L3_IO_QOS_CFG);
        will_return(__wrap_msr_read, PQOS_RETVAL_OK);
        will_return(__wrap_msr_read, 0);

        expect_any(__wrap_msr_read, lcore);
        expect_value(__wrap_msr_read, reg, PQOS_MSR_L3_IO_QOS_CFG);
        will_return(__wrap_msr_read, PQOS_RETVAL_OK);
        will_return(__wrap_msr_read, 0);

        ret = hw_cap_l3ca_iordt(data->cpu, &enabled);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(enabled, 0);
}

static void
test_hw_cap_l3ca_iordt_conflict(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        int enabled;

        expect_any(__wrap_msr_read, lcore);
        expect_value(__wrap_msr_read, reg, PQOS_MSR_L3_IO_QOS_CFG);
        will_return(__wrap_msr_read, PQOS_RETVAL_OK);
        will_return(__wrap_msr_read, 0);

        expect_any(__wrap_msr_read, lcore);
        expect_value(__wrap_msr_read, reg, PQOS_MSR_L3_IO_QOS_CFG);
        will_return(__wrap_msr_read, PQOS_RETVAL_OK);
        will_return(__wrap_msr_read, PQOS_MSR_L3_IO_QOS_CA_EN);

        ret = hw_cap_l3ca_iordt(data->cpu, &enabled);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
}

static void
test_hw_cap_l3ca_iordt_param(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        int enabled;

        ret = hw_cap_l3ca_iordt(NULL, &enabled);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);

        ret = hw_cap_l3ca_iordt(data->cpu, NULL);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
}

int
main(void)
{
        int result = 0;

        const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_hw_cap_l3ca_iordt_enabled),
            cmocka_unit_test(test_hw_cap_l3ca_iordt_disabled),
            cmocka_unit_test(test_hw_cap_l3ca_iordt_conflict),
            cmocka_unit_test(test_hw_cap_l3ca_iordt_param)};

        result +=
            cmocka_run_group_tests(tests, test_init_unsupported, test_fini);

        return result;
}
