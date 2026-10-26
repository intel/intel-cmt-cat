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

#include "mock_cap.h"
#include "os_allocation.h"
#include "resctrl_schemata.h"
#include "test.h"

/* ======== mock ======== */

/* the MBA values of the schemata read, from the queue: the one the shared
 * mock gives leaves them as they were, which here is uninitialised
 */
int
__wrap_resctrl_schemata_mba_get(const struct resctrl_schemata *schemata,
                                unsigned resource_id,
                                struct pqos_mba *ca)
{
        int ret;

        assert_non_null(schemata);
        assert_non_null(ca);
        check_expected(resource_id);

        ret = mock_type(int);
        if (ret == PQOS_RETVAL_OK)
                ca->mb_max = mock_type(unsigned);

        return ret;
}

/* ======== os_alloc_mount ======== */

static void
test_os_alloc_mount_param(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);

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

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);

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

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);

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

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);

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

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);

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

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);

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

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);

        ret = os_alloc_mount(PQOS_REQUIRE_CDP_OFF, PQOS_REQUIRE_CDP_OFF,
                             PQOS_MBA_CTRL);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
}

/**
 * MBA CTRL asked for where its support is unknown: the request reaches the
 * mount, and the mount's result is checked from the schemata of class 0,
 * whose MBA value of domain 0 is \a mb_max, set when \a mounted
 */
static void
assert_mount_mba_ctrl_unknown(struct test_data *data,
                              const int mounted,
                              const unsigned mb_max,
                              const int expected)
{
        int ret;

        data->cap_mba.ctrl = -1;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);

        expect_value(__wrap_resctrl_mount, l3_cdp_cfg, PQOS_REQUIRE_CDP_OFF);
        expect_value(__wrap_resctrl_mount, l2_cdp_cfg, PQOS_REQUIRE_CDP_OFF);
        expect_value(__wrap_resctrl_mount, mba_cfg, PQOS_MBA_CTRL);
        will_return(__wrap_resctrl_mount,
                    mounted ? PQOS_RETVAL_OK : PQOS_RETVAL_ERROR);

        if (mounted) {
                expect_value(__wrap_resctrl_alloc_schemata_read, class_id, 0);
                will_return(__wrap_resctrl_alloc_schemata_read, PQOS_RETVAL_OK);
                expect_value(__wrap_resctrl_schemata_mba_get, resource_id, 0);
                will_return(__wrap_resctrl_schemata_mba_get, PQOS_RETVAL_OK);
                will_return(__wrap_resctrl_schemata_mba_get, mb_max);
        }
        /* a mount that is not what was asked for is taken down again */
        if (mounted && expected != PQOS_RETVAL_OK)
                will_return(__wrap_resctrl_umount, PQOS_RETVAL_OK);

        ret = os_alloc_mount(PQOS_REQUIRE_CDP_OFF, PQOS_REQUIRE_CDP_OFF,
                             PQOS_MBA_CTRL);
        assert_int_equal(ret, expected);

        data->cap_mba.ctrl = 1;
}

static void
test_os_alloc_mount_mba_ctrl_unknown(void **state)
{
        struct test_data *data = (struct test_data *)*state;

        /* mounted, and the schemata in MBps, above the throttling maximum,
         * which is 0 here: no cpuinfo is read in this binary
         */
        assert_mount_mba_ctrl_unknown(data, 1, 2000, PQOS_RETVAL_OK);
}

static void
test_os_alloc_mount_mba_ctrl_unknown_refused(void **state)
{
        struct test_data *data = (struct test_data *)*state;

        /* the kernel refuses the mount: its error, not the up-front PARAM */
        assert_mount_mba_ctrl_unknown(data, 0, 0, PQOS_RETVAL_ERROR);
}

static void
test_os_alloc_mount_mba_ctrl_not_enabled(void **state)
{
        struct test_data *data = (struct test_data *)*state;

        /* mounted, but the schemata still at the throttling maximum: MBA
         * CTRL did not take, and resctrl is unmounted
         */
        assert_mount_mba_ctrl_unknown(data, 1, 0, PQOS_RETVAL_ERROR);
}

/**
 * Mounted, MBA CTRL did not take, and the unmount answers \a umount_ret
 */
static void
assert_not_enabled_unmounted(struct test_data *data,
                             const int umount_ret,
                             const int expected)
{
        int ret;

        data->cap_mba.ctrl = -1;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);

        expect_value(__wrap_resctrl_mount, l3_cdp_cfg, PQOS_REQUIRE_CDP_OFF);
        expect_value(__wrap_resctrl_mount, l2_cdp_cfg, PQOS_REQUIRE_CDP_OFF);
        expect_value(__wrap_resctrl_mount, mba_cfg, PQOS_MBA_CTRL);
        will_return(__wrap_resctrl_mount, PQOS_RETVAL_OK);
        expect_value(__wrap_resctrl_alloc_schemata_read, class_id, 0);
        will_return(__wrap_resctrl_alloc_schemata_read, PQOS_RETVAL_OK);
        expect_value(__wrap_resctrl_schemata_mba_get, resource_id, 0);
        will_return(__wrap_resctrl_schemata_mba_get, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_schemata_mba_get, 0);
        will_return(__wrap_resctrl_umount, umount_ret);

        ret = os_alloc_mount(PQOS_REQUIRE_CDP_OFF, PQOS_REQUIRE_CDP_OFF,
                             PQOS_MBA_CTRL);
        assert_int_equal(ret, expected);

        data->cap_mba.ctrl = 1;
}

static void
test_os_alloc_mount_mba_ctrl_not_enabled_left_mounted(void **state)
{
        /* the unmount fails: a mount is left that may be this one or one
         * another manager has mounted since, which PQOS_RETVAL_BUSY says, so
         * that the caller leaves it alone, as os_alloc_mount_back() does,
         * rather than take down a mount that may not be its own
         */
        assert_not_enabled_unmounted((struct test_data *)*state,
                                     PQOS_RETVAL_ERROR, PQOS_RETVAL_BUSY);
}

static void
test_os_alloc_mount_mba_ctrl_not_enabled_mount_gone(void **state)
{
        /* nothing is mounted there any more: no mount of this call's is left,
         * and the error is the mount's own
         */
        assert_not_enabled_unmounted((struct test_data *)*state,
                                     PQOS_RETVAL_RESOURCE, PQOS_RETVAL_ERROR);
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
            cmocka_unit_test(test_os_alloc_mount_mba_ctrl_unsupported),
            cmocka_unit_test(test_os_alloc_mount_mba_ctrl_unknown),
            cmocka_unit_test(test_os_alloc_mount_mba_ctrl_unknown_refused),
            cmocka_unit_test(test_os_alloc_mount_mba_ctrl_not_enabled),
            cmocka_unit_test(
                test_os_alloc_mount_mba_ctrl_not_enabled_left_mounted),
            cmocka_unit_test(
                test_os_alloc_mount_mba_ctrl_not_enabled_mount_gone)};

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
