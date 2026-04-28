/*
 * BSD LICENSE
 *
 * Copyright(c) 2024-2026 Intel Corporation. All rights reserved.
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

#include "monitoring.h"
#include "test.h"

int __wrap__pqos_check_init(const int expect);

int
_pqos_check_init(const int expect)
{
        return __wrap__pqos_check_init(expect);
}

#define GROUP_VALID_MARKER (0x00DEAD00)

static void
expect_init_ok(void)
{
        expect_function_call(__wrap_lock_get);
        expect_value(__wrap__pqos_check_init, expect, 1);
        will_return(__wrap__pqos_check_init, PQOS_RETVAL_OK);
        expect_function_call(__wrap_lock_release);
}

static void
test_pqos_mon_get_tel_value_null(void **state __attribute__((unused)))
{
        double val = 0.0;
        int ret;

        ret = pqos_mon_get_tel_value(NULL, PQOS_MON_EVENT_CORE_ENERGY, &val);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);

        ret = pqos_mon_get_tel_value(NULL, PQOS_MON_EVENT_CORE_ENERGY, NULL);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
}

static void
test_pqos_mon_get_tel_value_invalid_group(void **state __attribute__((unused)))
{
        struct pqos_mon_data group;
        double val = 0.0;
        int ret;

        memset(&group, 0, sizeof(group));

        ret = pqos_mon_get_tel_value(&group, PQOS_MON_EVENT_CORE_ENERGY, &val);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
}

static void
test_pqos_mon_get_tel_value_event_not_selected(void **state
                                               __attribute__((unused)))
{
        struct pqos_mon_data group;
        struct pqos_mon_data_internal intl;
        double val = 0.0;
        int ret;

        memset(&group, 0, sizeof(group));
        memset(&intl, 0, sizeof(intl));

        group.intl = &intl;
        group.valid = GROUP_VALID_MARKER;
        group.event = PQOS_MON_EVENT_CORE_ENERGY;

        ret = pqos_mon_get_tel_value(&group, PQOS_MON_EVENT_ACTIVITY, &val);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
}

static void
test_pqos_mon_get_tel_value_invalid_event(void **state __attribute__((unused)))
{
        struct pqos_mon_data group;
        struct pqos_mon_data_internal intl;
        double val = 0.0;
        int ret;

        memset(&group, 0, sizeof(group));
        memset(&intl, 0, sizeof(intl));

        group.intl = &intl;
        group.valid = GROUP_VALID_MARKER;
        group.event = PQOS_MON_EVENT_CORE_ENERGY;

        ret = pqos_mon_get_tel_value(&group, PQOS_MON_EVENT_L3_OCCUP, &val);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
}

static void
test_pqos_mon_get_tel_value_not_valid(void **state __attribute__((unused)))
{
        struct pqos_mon_data group;
        struct pqos_mon_data_internal intl;
        double val = 0.0;
        int ret;

        memset(&group, 0, sizeof(group));
        memset(&intl, 0, sizeof(intl));

        group.intl = &intl;
        group.valid = GROUP_VALID_MARKER;
        group.event = PQOS_MON_EVENT_CORE_ENERGY;
        intl.resctrl.tel[PQOS_TEL_SLOT_CORE_ENERGY].valid = 0;

        expect_init_ok();

        ret = pqos_mon_get_tel_value(&group, PQOS_MON_EVENT_CORE_ENERGY, &val);
        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);
}

static void
test_pqos_mon_get_tel_value_core_energy_ok(void **state __attribute__((unused)))
{
        struct pqos_mon_data group;
        struct pqos_mon_data_internal intl;
        double val = 0.0;
        int ret;

        memset(&group, 0, sizeof(group));
        memset(&intl, 0, sizeof(intl));

        group.intl = &intl;
        group.valid = GROUP_VALID_MARKER;
        group.event = PQOS_MON_EVENT_CORE_ENERGY;
        intl.resctrl.tel[PQOS_TEL_SLOT_CORE_ENERGY].valid = 1;
        intl.resctrl.tel[PQOS_TEL_SLOT_CORE_ENERGY].current = 42.5;

        expect_init_ok();

        ret = pqos_mon_get_tel_value(&group, PQOS_MON_EVENT_CORE_ENERGY, &val);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_float_equal(val, 42.5, 0.001);
}

static void
test_pqos_mon_get_tel_value_activity_ok(void **state __attribute__((unused)))
{
        struct pqos_mon_data group;
        struct pqos_mon_data_internal intl;
        double val = 0.0;
        int ret;

        memset(&group, 0, sizeof(group));
        memset(&intl, 0, sizeof(intl));

        group.intl = &intl;
        group.valid = GROUP_VALID_MARKER;
        group.event = PQOS_MON_EVENT_ACTIVITY;
        intl.resctrl.tel[PQOS_TEL_SLOT_ACTIVITY].valid = 1;
        intl.resctrl.tel[PQOS_TEL_SLOT_ACTIVITY].current = 11.25;

        expect_init_ok();

        ret = pqos_mon_get_tel_value(&group, PQOS_MON_EVENT_ACTIVITY, &val);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_float_equal(val, 11.25, 0.001);
}

static void
test_pqos_mon_get_tel_value_power_ok(void **state __attribute__((unused)))
{
        struct pqos_mon_data group;
        struct pqos_mon_data_internal intl;
        double val = 0.0;
        int ret;

        memset(&group, 0, sizeof(group));
        memset(&intl, 0, sizeof(intl));

        group.intl = &intl;
        group.valid = GROUP_VALID_MARKER;
        group.event = PQOS_MON_EVENT_POWER;
        intl.resctrl.tel[PQOS_TEL_SLOT_POWER].valid = 1;
        intl.resctrl.tel[PQOS_TEL_SLOT_POWER].current = 99.75;

        expect_init_ok();

        ret = pqos_mon_get_tel_value(&group, PQOS_MON_EVENT_POWER, &val);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_float_equal(val, 99.75, 0.001);
}

int
main(void)
{
        int result = 0;

        const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_pqos_mon_get_tel_value_null),
            cmocka_unit_test(test_pqos_mon_get_tel_value_invalid_group),
            cmocka_unit_test(test_pqos_mon_get_tel_value_event_not_selected),
            cmocka_unit_test(test_pqos_mon_get_tel_value_invalid_event),
            cmocka_unit_test(test_pqos_mon_get_tel_value_not_valid),
            cmocka_unit_test(test_pqos_mon_get_tel_value_core_energy_ok),
            cmocka_unit_test(test_pqos_mon_get_tel_value_activity_ok),
            cmocka_unit_test(test_pqos_mon_get_tel_value_power_ok),
        };

        result += cmocka_run_group_tests(tests, NULL, NULL);

        return result;
}
