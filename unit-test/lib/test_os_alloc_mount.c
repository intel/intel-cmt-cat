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

#include "mock_cap.h"
#include "os_allocation.h"
#include "test.h"

/* ======== os_alloc_mount ======== */

static void
test_os_alloc_mount_param(void **state __attribute__((unused)))
{
        int ret;

        ret = os_alloc_mount(-1, PQOS_REQUIRE_CDP_OFF, PQOS_MBA_DEFAULT);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);

        ret = os_alloc_mount(PQOS_REQUIRE_CDP_OFF, -1, PQOS_MBA_DEFAULT);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);

        ret = os_alloc_mount(PQOS_REQUIRE_CDP_OFF, PQOS_REQUIRE_CDP_OFF, -1);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);

        ret = os_alloc_mount(-1, PQOS_REQUIRE_CDP_ON, PQOS_MBA_CTRL);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);

        ret = os_alloc_mount(PQOS_REQUIRE_CDP_ON, -1, PQOS_MBA_CTRL);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);

        ret = os_alloc_mount(PQOS_REQUIRE_CDP_ON, PQOS_REQUIRE_CDP_ON, -1);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
}

static void
test_os_alloc_mount_default(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;

        will_return(__wrap__pqos_cap_get, data->cap);
        will_return(__wrap__pqos_cap_get, data->cpu);

        expect_value(__wrap_resctrl_mount, l3_cdp_cfg, PQOS_REQUIRE_CDP_OFF);
        expect_value(__wrap_resctrl_mount, l2_cdp_cfg, PQOS_REQUIRE_CDP_OFF);
        expect_value(__wrap_resctrl_mount, mba_cfg, PQOS_MBA_DEFAULT);
        will_return(__wrap_resctrl_mount, PQOS_RETVAL_OK);

        ret = os_alloc_mount(PQOS_REQUIRE_CDP_OFF, PQOS_REQUIRE_CDP_OFF,
                             PQOS_MBA_DEFAULT);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_os_alloc_mount_l2cdp(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;

        data->cap_l2ca.cdp = 1;

        will_return(__wrap__pqos_cap_get, data->cap);
        will_return(__wrap__pqos_cap_get, data->cpu);

        expect_value(__wrap_resctrl_mount, l3_cdp_cfg, PQOS_REQUIRE_CDP_OFF);
        expect_value(__wrap_resctrl_mount, l2_cdp_cfg, PQOS_REQUIRE_CDP_ON);
        expect_value(__wrap_resctrl_mount, mba_cfg, PQOS_MBA_DEFAULT);
        will_return(__wrap_resctrl_mount, PQOS_RETVAL_OK);

        ret = os_alloc_mount(PQOS_REQUIRE_CDP_OFF, PQOS_REQUIRE_CDP_ON,
                             PQOS_MBA_DEFAULT);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_os_alloc_mount_l2cdp_unsupported(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;

        data->cap_l2ca.cdp = 0;

        will_return(__wrap__pqos_cap_get, data->cap);
        will_return(__wrap__pqos_cap_get, data->cpu);

        ret = os_alloc_mount(PQOS_REQUIRE_CDP_OFF, PQOS_REQUIRE_CDP_ON,
                             PQOS_MBA_DEFAULT);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
}

static void
test_os_alloc_mount_l3cdp(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;

        data->cap_l3ca.cdp = 1;

        will_return(__wrap__pqos_cap_get, data->cap);
        will_return(__wrap__pqos_cap_get, data->cpu);

        expect_value(__wrap_resctrl_mount, l3_cdp_cfg, PQOS_REQUIRE_CDP_ON);
        expect_value(__wrap_resctrl_mount, l2_cdp_cfg, PQOS_REQUIRE_CDP_OFF);
        expect_value(__wrap_resctrl_mount, mba_cfg, PQOS_MBA_DEFAULT);
        will_return(__wrap_resctrl_mount, PQOS_RETVAL_OK);

        ret = os_alloc_mount(PQOS_REQUIRE_CDP_ON, PQOS_REQUIRE_CDP_OFF,
                             PQOS_MBA_DEFAULT);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_os_alloc_mount_l3cdp_unsupported(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;

        data->cap_l3ca.cdp = 0;

        will_return(__wrap__pqos_cap_get, data->cap);
        will_return(__wrap__pqos_cap_get, data->cpu);

        ret = os_alloc_mount(PQOS_REQUIRE_CDP_ON, PQOS_REQUIRE_CDP_OFF,
                             PQOS_MBA_DEFAULT);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
}

static void
test_os_alloc_mount_mba_ctrl_unsupported(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;

        data->cap_mba.ctrl = 0;

        will_return(__wrap__pqos_cap_get, data->cap);
        will_return(__wrap__pqos_cap_get, data->cpu);

        ret = os_alloc_mount(PQOS_REQUIRE_CDP_OFF, PQOS_REQUIRE_CDP_OFF,
                             PQOS_MBA_CTRL);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
}

int
main(void)
{
        int result = 0;

        const struct CMUnitTest tests_l3ca[] = {
            cmocka_unit_test(test_os_alloc_mount_l3cdp),
            cmocka_unit_test(test_os_alloc_mount_l3cdp_unsupported)};

        const struct CMUnitTest tests_l2ca[] = {
            cmocka_unit_test(test_os_alloc_mount_l2cdp),
            cmocka_unit_test(test_os_alloc_mount_l2cdp_unsupported)};

        const struct CMUnitTest tests_mba[] = {
            cmocka_unit_test(test_os_alloc_mount_mba_ctrl_unsupported)};

        const struct CMUnitTest tests_all[] = {
            cmocka_unit_test(test_os_alloc_mount_default),
            cmocka_unit_test(test_os_alloc_mount_param)};

        const struct CMUnitTest tests_unsupported[] = {};

        result += cmocka_run_group_tests(tests_l3ca, test_init_l3ca, test_fini);
        result += cmocka_run_group_tests(tests_l2ca, test_init_l2ca, test_fini);
        result += cmocka_run_group_tests(tests_mba, test_init_mba, test_fini);
        result += cmocka_run_group_tests(tests_all, test_init_all, test_fini);
        result += cmocka_run_group_tests(tests_unsupported,
                                         test_init_unsupported, test_fini);

        return result;
}