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

#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include "mock_cap.h"
#include "mock_cpuinfo.h"
#include "mock_os_allocation.h"
#include "mock_hw_allocation.h"
#include "mock_os_monitoring.h"
#include "mock_hw_monitoring.h"
#include "pqos.h"
#include "api.h"
#include "test.h"

#ifndef DIM
#define DIM(x) (sizeof(x) / sizeof(x[0]))
#endif

#define wrap_check_init(value, ret)                                            \
        do {                                                                   \
                /* _pqos_check_init */                                         \
                expect_value(__wrap__pqos_check_init, expect, value);          \
                will_return(__wrap__pqos_check_init, ret);                     \
                /* _pqos_api_lock */                                           \
                expect_function_call(__wrap__pqos_api_lock);                   \
                /* _pqos_api_unlock */                                         \
                expect_function_call(__wrap__pqos_api_unlock);                 \
        } while (0)

static int
setup_hw(void **state __attribute__((unused)))
{
        int ret = api_init(PQOS_INTER_MSR, PQOS_VENDOR_INTEL);

        assert_int_equal(ret, PQOS_RETVAL_OK);

        return ret;
}

static int
setup_os(void **state)
{
        struct test_data *data;

        test_init_all(state);

        data = (struct test_data *)*state;
        data->interface = PQOS_INTER_OS;

        int ret = api_init(PQOS_INTER_OS, PQOS_VENDOR_INTEL);

        assert_int_equal(ret, PQOS_RETVAL_OK);

        return ret;
}

static int
setup_os_resctrl_mon(void **state)
{
        struct test_data *data;

        test_init_all(state);

        data = (struct test_data *)*state;
        data->interface = PQOS_INTER_OS_RESCTRL_MON;

        int ret = api_init(PQOS_INTER_OS_RESCTRL_MON, PQOS_VENDOR_INTEL);

        assert_int_equal(ret, PQOS_RETVAL_OK);

        return ret;
}

/* ======== pqos_alloc_assoc_set ======== */

/* Ensure that correct error is returned when library is not initialized */
static void
test_pqos_alloc_assoc_set_init(void **state __attribute__((unused)))
{
        int ret;

        wrap_check_init(1, PQOS_RETVAL_INIT);

        ret = pqos_alloc_assoc_set(0, 0);
        assert_int_equal(ret, PQOS_RETVAL_INIT);
}

/* Check if msr API is called */
static void
test_pqos_alloc_assoc_set_hw(void **state __attribute__((unused)))
{
        int ret;

        wrap_check_init(1, PQOS_RETVAL_OK);

        expect_value(__wrap_hw_alloc_assoc_set, lcore, 0);
        expect_value(__wrap_hw_alloc_assoc_set, class_id, 0);
        will_return(__wrap_hw_alloc_assoc_set, PQOS_RETVAL_OK);

        ret = pqos_alloc_assoc_set(0, 0);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

#ifdef __linux__
/* Check if os API is called */
static void
test_pqos_alloc_assoc_set_os(void **state __attribute__((unused)))
{
        int ret;

        wrap_check_init(1, PQOS_RETVAL_OK);

        expect_value(__wrap_os_alloc_assoc_set, lcore, 0);
        expect_value(__wrap_os_alloc_assoc_set, class_id, 0);
        will_return(__wrap_os_alloc_assoc_set, PQOS_RETVAL_OK);

        ret = pqos_alloc_assoc_set(0, 0);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}
#endif

/* ======== pqos_alloc_assoc_get ======== */

static void
test_pqos_alloc_assoc_get_init(void **state __attribute__((unused)))
{
        int ret;
        unsigned class_id;

        wrap_check_init(1, PQOS_RETVAL_INIT);

        ret = pqos_alloc_assoc_get(0, &class_id);
        assert_int_equal(ret, PQOS_RETVAL_INIT);
}

static void
test_pqos_alloc_assoc_get_param_id_null(void **state __attribute__((unused)))
{
        int ret;

        ret = pqos_alloc_assoc_get(0, NULL);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
}

static void
test_pqos_alloc_assoc_get_hw(void **state __attribute__((unused)))
{
        int ret;
        unsigned id;

        wrap_check_init(1, PQOS_RETVAL_OK);

        /* hw_alloc_assoc_get */
        expect_value(__wrap_hw_alloc_assoc_get, lcore, 0);
        expect_value(__wrap_hw_alloc_assoc_get, class_id, &id);
        will_return(__wrap_hw_alloc_assoc_get, PQOS_RETVAL_OK);
        will_return(__wrap_hw_alloc_assoc_get, 5);

        ret = pqos_alloc_assoc_get(0, &id);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(id, 5);
}

static void
test_pqos_alloc_assoc_get_os(void **state __attribute__((unused)))
{
        int ret;
        unsigned id;

        wrap_check_init(1, PQOS_RETVAL_OK);

        expect_value(__wrap_os_alloc_assoc_get, lcore, 0);
        expect_value(__wrap_os_alloc_assoc_get, class_id, &id);
        will_return(__wrap_os_alloc_assoc_get, PQOS_RETVAL_OK);
        will_return(__wrap_os_alloc_assoc_get, 5);

        ret = pqos_alloc_assoc_get(0, &id);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(id, 5);
}

/* ======== pqos_alloc_assoc_set_pid ======== */

static void
test_pqos_alloc_assoc_set_pid_init(void **state __attribute__((unused)))
{
        int ret;

        wrap_check_init(1, PQOS_RETVAL_INIT);

        ret = pqos_alloc_assoc_set_pid(0, 1);
        assert_int_equal(ret, PQOS_RETVAL_INIT);
}

static void
test_pqos_alloc_assoc_set_pid_hw(void **state __attribute__((unused)))
{
        int ret;

        wrap_check_init(1, PQOS_RETVAL_OK);

        ret = pqos_alloc_assoc_set_pid(1, 2);
        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);
}

#ifdef __linux__
static void
test_pqos_alloc_assoc_set_pid_os(void **state __attribute__((unused)))
{
        int ret;

        wrap_check_init(1, PQOS_RETVAL_OK);

        expect_value(__wrap_os_alloc_assoc_set_pid, task, 1);
        expect_value(__wrap_os_alloc_assoc_set_pid, class_id, 2);
        will_return(__wrap_os_alloc_assoc_set_pid, PQOS_RETVAL_OK);

        ret = pqos_alloc_assoc_set_pid(1, 2);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}
#endif

/* ======== pqos_alloc_assoc_get_pid ======== */

static void
test_pqos_alloc_assoc_get_pid_init(void **state __attribute__((unused)))
{
        int ret;
        unsigned id;

        wrap_check_init(1, PQOS_RETVAL_INIT);

        ret = pqos_alloc_assoc_get_pid(1, &id);
        assert_int_equal(ret, PQOS_RETVAL_INIT);
}

static void
test_pqos_alloc_assoc_get_pid_param_id_null(void **state
                                            __attribute__((unused)))
{
        int ret;

        ret = pqos_alloc_assoc_get_pid(1, NULL);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
}

static void
test_pqos_alloc_assoc_get_pid_hw(void **state __attribute__((unused)))
{
        int ret;
        unsigned id;

        wrap_check_init(1, PQOS_RETVAL_OK);

        ret = pqos_alloc_assoc_get_pid(1, &id);
        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);
}

#ifdef __linux__
static void
test_pqos_alloc_assoc_get_pid_os(void **state __attribute__((unused)))
{
        int ret;
        unsigned id;

        wrap_check_init(1, PQOS_RETVAL_OK);

        expect_value(__wrap_os_alloc_assoc_get_pid, task, 1);
        expect_value(__wrap_os_alloc_assoc_get_pid, class_id, &id);
        will_return(__wrap_os_alloc_assoc_get_pid, PQOS_RETVAL_OK);
        will_return(__wrap_os_alloc_assoc_get_pid, 5);

        ret = pqos_alloc_assoc_get_pid(1, &id);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(id, 5);
}
#endif

/* ======== test_pqos_alloc_assign ======== */

static void
test_pqos_alloc_assign_init(void **state __attribute__((unused)))
{
        int ret;
        unsigned id;
        unsigned core[1];

        wrap_check_init(1, PQOS_RETVAL_INIT);

        ret = pqos_alloc_assign(1 << PQOS_CAP_TYPE_L3CA, core, 1, &id);
        assert_int_equal(ret, PQOS_RETVAL_INIT);

        wrap_check_init(1, PQOS_RETVAL_INIT);

        ret = pqos_alloc_assign(1 << PQOS_CAP_TYPE_L2CA, core, 1, &id);
        assert_int_equal(ret, PQOS_RETVAL_INIT);

        wrap_check_init(1, PQOS_RETVAL_INIT);

        ret = pqos_alloc_assign(1 << PQOS_CAP_TYPE_MBA, core, 1, &id);
        assert_int_equal(ret, PQOS_RETVAL_INIT);
}

static void
test_pqos_alloc_assign_param_technology(void **state __attribute__((unused)))
{
        int ret;
        unsigned id;
        unsigned core[1];

        ret = pqos_alloc_assign(0, core, 1, &id);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
}

static void
test_pqos_alloc_assign_param_core_null(void **state __attribute__((unused)))
{
        int ret;
        unsigned id;

        ret = pqos_alloc_assign(1 << PQOS_CAP_TYPE_L3CA, NULL, 1, &id);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
}

static void
test_pqos_alloc_assign_param_core_num(void **state __attribute__((unused)))
{
        int ret;
        unsigned id;
        unsigned core[1];

        ret = pqos_alloc_assign(1 << PQOS_CAP_TYPE_L3CA, core, 0, &id);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
}

static void
test_pqos_alloc_assign_param_id_null(void **state __attribute__((unused)))
{
        int ret;
        unsigned core[1];

        ret = pqos_alloc_assign(1 << PQOS_CAP_TYPE_L3CA, core, 1, NULL);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
}

static void
test_pqos_alloc_assign_hw(void **state __attribute__((unused)))
{
        int ret;
        unsigned class_id;
        unsigned core_array[1];
        unsigned core_num = 1;
        unsigned technology = 1 << PQOS_CAP_TYPE_L3CA;

        wrap_check_init(1, PQOS_RETVAL_OK);

        expect_value(__wrap_hw_alloc_assign, technology, technology);
        expect_value(__wrap_hw_alloc_assign, core_array, core_array);
        expect_value(__wrap_hw_alloc_assign, core_num, core_num);
        expect_value(__wrap_hw_alloc_assign, class_id, &class_id);
        will_return(__wrap_hw_alloc_assign, PQOS_RETVAL_OK);
        will_return(__wrap_hw_alloc_assign, 3);

        ret = pqos_alloc_assign(technology, core_array, core_num, &class_id);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(class_id, 3);
}

static void
test_pqos_alloc_assign_os(void **state __attribute__((unused)))
{
        int ret;
        unsigned class_id;
        unsigned core_array[1];
        unsigned core_num = 1;
        unsigned technology = 1 << PQOS_CAP_TYPE_L3CA;

        wrap_check_init(1, PQOS_RETVAL_OK);

        expect_value(__wrap_os_alloc_assign, technology, technology);
        expect_value(__wrap_os_alloc_assign, core_array, core_array);
        expect_value(__wrap_os_alloc_assign, core_num, core_num);
        expect_value(__wrap_os_alloc_assign, class_id, &class_id);
        will_return(__wrap_os_alloc_assign, PQOS_RETVAL_OK);
        will_return(__wrap_os_alloc_assign, 3);

        ret = pqos_alloc_assign(technology, core_array, core_num, &class_id);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(class_id, 3);
}

/* ======== test_pqos_alloc_release ======== */

static void
test_pqos_alloc_release_init(void **state __attribute__((unused)))
{
        int ret;
        unsigned core_array[1];
        unsigned core_num = 1;

        wrap_check_init(1, PQOS_RETVAL_INIT);

        ret = pqos_alloc_release(core_array, core_num);
        assert_int_equal(ret, PQOS_RETVAL_INIT);
}

static void
test_pqos_alloc_release_param(void **state __attribute__((unused)))
{
        int ret;
        unsigned core_array[1];
        unsigned core_num = 1;

        ret = pqos_alloc_release(core_array, 0);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);

        ret = pqos_alloc_release(NULL, core_num);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
}

static void
test_pqos_alloc_release_os(void **state __attribute__((unused)))
{
        int ret;
        unsigned core_array[1];
        unsigned core_num = 1;

        wrap_check_init(1, PQOS_RETVAL_OK);

        expect_value(__wrap_os_alloc_release, core_array, core_array);
        expect_value(__wrap_os_alloc_release, core_num, core_num);
        will_return(__wrap_os_alloc_release, PQOS_RETVAL_OK);

        ret = pqos_alloc_release(core_array, core_num);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_pqos_alloc_release_hw(void **state __attribute__((unused)))
{
        int ret;
        unsigned core_array[1];
        unsigned core_num = 1;

        wrap_check_init(1, PQOS_RETVAL_OK);

        expect_value(__wrap_hw_alloc_release, core_array, core_array);
        expect_value(__wrap_hw_alloc_release, core_num, core_num);
        will_return(__wrap_hw_alloc_release, PQOS_RETVAL_OK);

        ret = pqos_alloc_release(core_array, core_num);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

/* ======== test_pqos_alloc_assign_pid ======== */

static void
test_pqos_alloc_assign_pid_init(void **state __attribute__((unused)))
{
        int ret;
        unsigned technology = 1 << PQOS_CAP_TYPE_L3CA;
        unsigned class_id;
        pid_t task_array[1];
        unsigned task_num = 1;

        wrap_check_init(1, PQOS_RETVAL_INIT);

        ret =
            pqos_alloc_assign_pid(technology, task_array, task_num, &class_id);
        assert_int_equal(ret, PQOS_RETVAL_INIT);
}

static void
test_pqos_alloc_assign_pid_hw(void **state __attribute__((unused)))
{
        int ret;
        unsigned technology = 1 << PQOS_CAP_TYPE_L3CA;
        unsigned class_id;
        pid_t task_array[1];
        unsigned task_num = 1;

        wrap_check_init(1, PQOS_RETVAL_OK);

        ret =
            pqos_alloc_assign_pid(technology, task_array, task_num, &class_id);
        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);
}

static void
test_pqos_alloc_assign_pid_os(void **state __attribute__((unused)))
{
        int ret;
        unsigned technology = 1 << PQOS_CAP_TYPE_L3CA;
        pid_t task_array[1];
        unsigned task_num = 1;
        unsigned class_id;

        wrap_check_init(1, PQOS_RETVAL_OK);

        expect_value(__wrap_os_alloc_assign_pid, technology, technology);
        expect_value(__wrap_os_alloc_assign_pid, task_array, task_array);
        expect_value(__wrap_os_alloc_assign_pid, task_num, task_num);
        expect_value(__wrap_os_alloc_assign_pid, class_id, &class_id);
        will_return(__wrap_os_alloc_assign_pid, PQOS_RETVAL_OK);
        will_return(__wrap_os_alloc_assign_pid, 3);

        ret =
            pqos_alloc_assign_pid(technology, task_array, task_num, &class_id);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(class_id, 3);
}

static void
test_pqos_alloc_assign_pid_param(void **state __attribute__((unused)))
{
        int ret;
        unsigned technology = 1 << PQOS_CAP_TYPE_L3CA;
        unsigned class_id;
        pid_t task_array[1];
        unsigned task_num = 1;

        ret = pqos_alloc_assign_pid(technology, NULL, task_num, &class_id);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);

        ret = pqos_alloc_assign_pid(technology, task_array, 0, &class_id);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);

        ret = pqos_alloc_assign_pid(technology, task_array, task_num, NULL);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
}

/* ======== pqos_alloc_release_pid ======== */

static void
test_pqos_alloc_release_pid_init(void **state __attribute__((unused)))
{
        int ret;
        pid_t task_array[1];
        unsigned task_num = 1;

        wrap_check_init(1, PQOS_RETVAL_INIT);

        ret = pqos_alloc_release_pid(task_array, task_num);
        assert_int_equal(ret, PQOS_RETVAL_INIT);
}

static void
test_pqos_alloc_release_pid_param(void **state __attribute__((unused)))
{
        int ret;
        pid_t task_array[1];
        unsigned task_num = 1;

        ret = pqos_alloc_release_pid(task_array, 0);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);

        ret = pqos_alloc_release_pid(NULL, task_num);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
}

static void
test_pqos_alloc_release_pid_hw(void **state __attribute__((unused)))
{
        int ret;
        pid_t task_array[1];
        unsigned task_num = 1;

        wrap_check_init(1, PQOS_RETVAL_OK);

        ret = pqos_alloc_release_pid(task_array, task_num);
        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);
}

static void
test_pqos_alloc_release_pid_os(void **state __attribute__((unused)))
{
        int ret;
        pid_t task_array[1];
        unsigned task_num = 1;

        wrap_check_init(1, PQOS_RETVAL_OK);

        expect_value(__wrap_os_alloc_release_pid, task_array, task_array);
        expect_value(__wrap_os_alloc_release_pid, task_num, task_num);
        will_return(__wrap_os_alloc_release_pid, PQOS_RETVAL_OK);

        ret = pqos_alloc_release_pid(task_array, task_num);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

/* ======== pqos_alloc_reset ======== */

static void
test_pqos_alloc_reset_init(void **state __attribute__((unused)))
{
        int ret;
        enum pqos_cdp_config l3_cdp_cfg = PQOS_REQUIRE_CDP_ANY;
        enum pqos_cdp_config l2_cdp_cfg = PQOS_REQUIRE_CDP_ANY;
        enum pqos_mba_config mba_cfg = PQOS_MBA_ANY;

        wrap_check_init(1, PQOS_RETVAL_INIT);

        ret = pqos_alloc_reset(l3_cdp_cfg, l2_cdp_cfg, mba_cfg);
        assert_int_equal(ret, PQOS_RETVAL_INIT);
}

static void
test_pqos_alloc_reset_os(void **state __attribute__((unused)))
{
        int ret;
        unsigned l3;
        unsigned l2;
        unsigned mba;

        enum pqos_cdp_config l3_cdp_cfg[] = {
            PQOS_REQUIRE_CDP_ANY, PQOS_REQUIRE_CDP_ON, PQOS_REQUIRE_CDP_OFF};
        enum pqos_cdp_config l2_cdp_cfg[] = {
            PQOS_REQUIRE_CDP_ANY, PQOS_REQUIRE_CDP_ON, PQOS_REQUIRE_CDP_OFF};
        enum pqos_mba_config mba_cfg[] = {PQOS_MBA_ANY, PQOS_MBA_DEFAULT,
                                          PQOS_MBA_CTRL};

        for (l3 = 0; l3 < DIM(l3_cdp_cfg); ++l3) {
                for (l2 = 0; l2 < DIM(l2_cdp_cfg); ++l2) {
                        for (mba = 0; mba < DIM(mba_cfg); ++mba) {
                                wrap_check_init(1, PQOS_RETVAL_OK);

                                expect_value(__wrap_os_alloc_reset, l3_cdp_cfg,
                                             l3_cdp_cfg[l3]);
                                expect_value(__wrap_os_alloc_reset, l2_cdp_cfg,
                                             l2_cdp_cfg[l2]);
                                expect_value(__wrap_os_alloc_reset, mba_cfg,
                                             mba_cfg[mba]);
                                will_return(__wrap_os_alloc_reset,
                                            PQOS_RETVAL_OK);

                                ret = pqos_alloc_reset(l3_cdp_cfg[l3],
                                                       l2_cdp_cfg[l2],
                                                       mba_cfg[mba]);
                                assert_int_equal(ret, PQOS_RETVAL_OK);
                        }
                }
        }
}

static void
test_pqos_alloc_reset_hw(void **state __attribute__((unused)))
{
        int ret;
        unsigned l3;
        unsigned l2;
        unsigned mba;

        enum pqos_cdp_config l3_cdp_cfg[] = {
            PQOS_REQUIRE_CDP_ANY, PQOS_REQUIRE_CDP_ON, PQOS_REQUIRE_CDP_OFF};
        enum pqos_cdp_config l2_cdp_cfg[] = {
            PQOS_REQUIRE_CDP_ANY, PQOS_REQUIRE_CDP_ON, PQOS_REQUIRE_CDP_OFF};
        enum pqos_mba_config mba_cfg[] = {PQOS_MBA_ANY, PQOS_MBA_DEFAULT,
                                          PQOS_MBA_CTRL};

        for (l3 = 0; l3 < DIM(l3_cdp_cfg); ++l3) {
                for (l2 = 0; l2 < DIM(l2_cdp_cfg); ++l2) {
                        for (mba = 0; mba < DIM(mba_cfg); ++mba) {
                                wrap_check_init(1, PQOS_RETVAL_OK);

                                expect_value(__wrap_hw_alloc_reset, l3_cdp_cfg,
                                             l3_cdp_cfg[l3]);
                                expect_value(__wrap_hw_alloc_reset, l2_cdp_cfg,
                                             l2_cdp_cfg[l2]);
                                expect_value(__wrap_hw_alloc_reset, mba_cfg,
                                             mba_cfg[mba]);
                                will_return(__wrap_hw_alloc_reset,
                                            PQOS_RETVAL_OK);

                                ret = pqos_alloc_reset(l3_cdp_cfg[l3],
                                                       l2_cdp_cfg[l2],
                                                       mba_cfg[mba]);
                                assert_int_equal(ret, PQOS_RETVAL_OK);
                        }
                }
        }
}

static void
test_pqos_alloc_reset_param(void **state __attribute__((unused)))
{
        int ret;
        enum pqos_cdp_config l3_cdp_cfg = PQOS_REQUIRE_CDP_ANY;
        enum pqos_cdp_config l2_cdp_cfg = PQOS_REQUIRE_CDP_ANY;
        enum pqos_mba_config mba_cfg = PQOS_MBA_ANY;

        ret = pqos_alloc_reset(-1, l2_cdp_cfg, mba_cfg);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);

        ret = pqos_alloc_reset(l3_cdp_cfg, -1, mba_cfg);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);

        ret = pqos_alloc_reset(l3_cdp_cfg, l2_cdp_cfg, -1);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
}

/* ======== pqos_pid_get_pid_assoc ======== */

static void
test_pqos_pid_get_pid_assoc_init(void **state __attribute__((unused)))
{
        unsigned *ret;
        unsigned class_id = 1;
        unsigned count;

        wrap_check_init(1, PQOS_RETVAL_INIT);

        ret = pqos_pid_get_pid_assoc(class_id, &count);
        assert_null(ret);
}

static void
test_pqos_pid_get_pid_assoc_hw(void **state __attribute__((unused)))
{
        unsigned *ret;
        unsigned class_id = 1;
        unsigned count;

        wrap_check_init(1, PQOS_RETVAL_OK);

        ret = pqos_pid_get_pid_assoc(class_id, &count);
        assert_null(ret);
}

static void
test_pqos_pid_get_pid_assoc_os(void **state __attribute__((unused)))
{
        unsigned *ret;
        unsigned class_id = 1;
        unsigned count;
        unsigned pid_array[1];

        wrap_check_init(1, PQOS_RETVAL_OK);

        expect_value(__wrap_os_pid_get_pid_assoc, class_id, class_id);
        expect_value(__wrap_os_pid_get_pid_assoc, count, &count);
        will_return(__wrap_os_pid_get_pid_assoc, pid_array);

        ret = pqos_pid_get_pid_assoc(class_id, &count);
        assert_non_null(ret);

        wrap_check_init(1, PQOS_RETVAL_OK);

        expect_value(__wrap_os_pid_get_pid_assoc, class_id, class_id);
        expect_value(__wrap_os_pid_get_pid_assoc, count, &count);
        will_return(__wrap_os_pid_get_pid_assoc, NULL);

        ret = pqos_pid_get_pid_assoc(class_id, &count);
        assert_null(ret);
}

static void
test_pqos_pid_get_pid_assoc_param(void **state __attribute__((unused)))
{
        unsigned *ret;
        unsigned class_id = 1;

        ret = pqos_pid_get_pid_assoc(class_id, NULL);
        assert_null(ret);
}

/* ======== pqos_l3ca_set ======== */

static void
test_pqos_l3ca_set_init(void **state __attribute__((unused)))
{
        int ret;
        unsigned l3cat_id = 1;
        unsigned num_cos = 1;
        struct pqos_l3ca ca[1];

        wrap_check_init(1, PQOS_RETVAL_INIT);

        ca[0].class_id = 1;
        ca[0].cdp = 0;
        ca[0].u.ways_mask = 0xf;

        ret = pqos_l3ca_set(l3cat_id, num_cos, ca);
        assert_int_equal(ret, PQOS_RETVAL_INIT);
}

static void
test_pqos_l3ca_set_hw(void **state __attribute__((unused)))
{
        int ret;
        unsigned l3cat_id = 1;
        unsigned num_cos = 1;
        struct pqos_l3ca ca[1];

        wrap_check_init(1, PQOS_RETVAL_OK);

        ca[0].class_id = 1;
        ca[0].cdp = 0;
        ca[0].u.ways_mask = 0xf;

        expect_value(__wrap_hw_l3ca_set, l3cat_id, l3cat_id);
        expect_value(__wrap_hw_l3ca_set, num_cos, num_cos);
        expect_value(__wrap_hw_l3ca_set, ca, ca);
        will_return(__wrap_hw_l3ca_set, PQOS_RETVAL_OK);

        ret = pqos_l3ca_set(l3cat_id, num_cos, ca);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_pqos_l3ca_set_hw_cdp(void **state __attribute__((unused)))
{
        int ret;
        unsigned l3cat_id = 1;
        unsigned num_cos = 1;
        struct pqos_l3ca ca[1];

        wrap_check_init(1, PQOS_RETVAL_OK);

        ca[0].class_id = 1;
        ca[0].cdp = 1;
        ca[0].u.s.data_mask = 0xf;
        ca[0].u.s.code_mask = 0xf0;

        expect_value(__wrap_hw_l3ca_set, l3cat_id, l3cat_id);
        expect_value(__wrap_hw_l3ca_set, num_cos, num_cos);
        expect_value(__wrap_hw_l3ca_set, ca, ca);
        will_return(__wrap_hw_l3ca_set, PQOS_RETVAL_OK);

        ret = pqos_l3ca_set(l3cat_id, num_cos, ca);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_pqos_l3ca_set_os(void **state __attribute__((unused)))
{
        int ret;
        unsigned l3cat_id = 1;
        unsigned num_cos = 1;
        struct pqos_l3ca ca[1];

        wrap_check_init(1, PQOS_RETVAL_OK);

        ca[0].class_id = 1;
        ca[0].cdp = 0;
        ca[0].u.ways_mask = 0xf;

        expect_value(__wrap_os_l3ca_set, l3cat_id, l3cat_id);
        expect_value(__wrap_os_l3ca_set, num_cos, num_cos);
        expect_value(__wrap_os_l3ca_set, ca, ca);
        will_return(__wrap_os_l3ca_set, PQOS_RETVAL_OK);

        ret = pqos_l3ca_set(l3cat_id, num_cos, ca);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_pqos_l3ca_set_param(void **state __attribute__((unused)))
{
        int ret;
        unsigned l3cat_id = 1;
        unsigned num_cos = 1;
        struct pqos_l3ca ca[1];

        ret = pqos_l3ca_set(l3cat_id, 0, ca);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);

        ret = pqos_l3ca_set(l3cat_id, num_cos, NULL);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);

        ca[0].class_id = 1;
        ca[0].cdp = 0;
        ca[0].u.ways_mask = 0x5;

        ret = pqos_l3ca_set(l3cat_id, num_cos, ca);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);

        ca[0].class_id = 1;
        ca[0].cdp = 0;
        ca[0].u.ways_mask = 0;

        ret = pqos_l3ca_set(l3cat_id, num_cos, ca);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);

        ca[0].class_id = 1;
        ca[0].cdp = 1;
        ca[0].u.s.data_mask = 0x5;
        ca[0].u.s.code_mask = 0xf0;

        ret = pqos_l3ca_set(l3cat_id, num_cos, ca);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);

        ca[0].class_id = 1;
        ca[0].cdp = 1;
        ca[0].u.s.data_mask = 0xf0;
        ca[0].u.s.code_mask = 0x5;

        ret = pqos_l3ca_set(l3cat_id, num_cos, ca);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
}

/* ======== pqos_l3ca_get ======== */

static void
test_pqos_l3ca_get_init(void **state __attribute__((unused)))
{
        int ret;
        unsigned l3cat_id = 1;
        unsigned max_num_ca = 1;
        unsigned num_ca;
        struct pqos_l3ca ca[1];

        wrap_check_init(1, PQOS_RETVAL_INIT);

        ret = pqos_l3ca_get(l3cat_id, max_num_ca, &num_ca, ca);
        assert_int_equal(ret, PQOS_RETVAL_INIT);
}

static void
test_pqos_l3ca_get_hw(void **state __attribute__((unused)))
{
        int ret;
        unsigned l3cat_id = 1;
        unsigned max_num_ca = 1;
        unsigned num_ca;
        struct pqos_l3ca ca[1];

        wrap_check_init(1, PQOS_RETVAL_OK);

        expect_value(__wrap_hw_l3ca_get, l3cat_id, l3cat_id);
        expect_value(__wrap_hw_l3ca_get, max_num_ca, max_num_ca);
        expect_value(__wrap_hw_l3ca_get, num_ca, &num_ca);
        expect_value(__wrap_hw_l3ca_get, ca, ca);
        will_return(__wrap_hw_l3ca_get, PQOS_RETVAL_OK);

        ret = pqos_l3ca_get(l3cat_id, max_num_ca, &num_ca, ca);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_pqos_l3ca_get_os(void **state __attribute__((unused)))
{
        int ret;
        unsigned l3cat_id = 1;
        unsigned max_num_ca = 1;
        unsigned num_ca;
        struct pqos_l3ca ca[1];

        wrap_check_init(1, PQOS_RETVAL_OK);

        expect_value(__wrap_os_l3ca_get, l3cat_id, l3cat_id);
        expect_value(__wrap_os_l3ca_get, max_num_ca, max_num_ca);
        expect_value(__wrap_os_l3ca_get, num_ca, &num_ca);
        expect_value(__wrap_os_l3ca_get, ca, ca);
        will_return(__wrap_os_l3ca_get, PQOS_RETVAL_OK);

        ret = pqos_l3ca_get(l3cat_id, max_num_ca, &num_ca, ca);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_pqos_l3ca_get_param(void **state __attribute__((unused)))
{
        int ret;
        unsigned l3cat_id = 1;
        unsigned max_num_ca = 1;
        unsigned num_ca;
        struct pqos_l3ca ca[1];

        ret = pqos_l3ca_get(l3cat_id, 0, &num_ca, ca);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);

        ret = pqos_l3ca_get(l3cat_id, max_num_ca, NULL, ca);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);

        ret = pqos_l3ca_get(l3cat_id, max_num_ca, &num_ca, NULL);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
}

/* ======== pqos_l3ca_get_min_cbm_bits ======== */

static void
test_pqos_l3ca_get_min_cbm_bits_init(void **state __attribute__((unused)))
{
        int ret;
        unsigned min_cbm_bits;

        wrap_check_init(1, PQOS_RETVAL_INIT);

        ret = pqos_l3ca_get_min_cbm_bits(&min_cbm_bits);
        assert_int_equal(ret, PQOS_RETVAL_INIT);
}

static void
test_pqos_l3ca_get_min_cbm_bits_hw(void **state __attribute__((unused)))
{
        int ret;
        unsigned min_cbm_bits;

        wrap_check_init(1, PQOS_RETVAL_OK);

        expect_value(__wrap_hw_l3ca_get_min_cbm_bits, min_cbm_bits,
                     &min_cbm_bits);
        will_return(__wrap_hw_l3ca_get_min_cbm_bits, PQOS_RETVAL_OK);

        ret = pqos_l3ca_get_min_cbm_bits(&min_cbm_bits);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_pqos_l3ca_get_min_cbm_bits_os(void **state __attribute__((unused)))
{
        int ret;
        unsigned min_cbm_bits;

        wrap_check_init(1, PQOS_RETVAL_OK);

        expect_value(__wrap_os_l3ca_get_min_cbm_bits, min_cbm_bits,
                     &min_cbm_bits);
        will_return(__wrap_os_l3ca_get_min_cbm_bits, PQOS_RETVAL_OK);

        ret = pqos_l3ca_get_min_cbm_bits(&min_cbm_bits);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_pqos_l3ca_get_min_cbm_bits_param(void **state __attribute__((unused)))
{
        int ret;

        ret = pqos_l3ca_get_min_cbm_bits(NULL);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
}

/* ======== pqos_l2ca_set ======== */

static void
test_pqos_l2ca_set_init(void **state __attribute__((unused)))
{
        int ret;
        unsigned l2id = 1;
        unsigned num_cos = 1;
        struct pqos_l2ca ca[1];

        wrap_check_init(1, PQOS_RETVAL_INIT);

        ca[0].class_id = 1;
        ca[0].cdp = 0;
        ca[0].u.ways_mask = 0xf;

        ret = pqos_l2ca_set(l2id, num_cos, ca);
        assert_int_equal(ret, PQOS_RETVAL_INIT);
}

static void
test_pqos_l2ca_set_hw(void **state __attribute__((unused)))
{
        int ret;
        unsigned l2id = 1;
        unsigned num_cos = 1;
        struct pqos_l2ca ca[1];

        wrap_check_init(1, PQOS_RETVAL_OK);

        ca[0].class_id = 1;
        ca[0].cdp = 0;
        ca[0].u.ways_mask = 0xf;

        expect_value(__wrap_hw_l2ca_set, l2id, l2id);
        expect_value(__wrap_hw_l2ca_set, num_cos, num_cos);
        expect_value(__wrap_hw_l2ca_set, ca, ca);
        will_return(__wrap_hw_l2ca_set, PQOS_RETVAL_OK);

        ret = pqos_l2ca_set(l2id, num_cos, ca);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_pqos_l2ca_set_hw_cdp(void **state __attribute__((unused)))
{
        int ret;
        unsigned l2id = 1;
        unsigned num_cos = 1;
        struct pqos_l2ca ca[1];

        wrap_check_init(1, PQOS_RETVAL_OK);

        ca[0].class_id = 1;
        ca[0].cdp = 1;
        ca[0].u.s.data_mask = 0xf;
        ca[0].u.s.code_mask = 0xf0;

        expect_value(__wrap_hw_l2ca_set, l2id, l2id);
        expect_value(__wrap_hw_l2ca_set, num_cos, num_cos);
        expect_value(__wrap_hw_l2ca_set, ca, ca);
        will_return(__wrap_hw_l2ca_set, PQOS_RETVAL_OK);

        ret = pqos_l2ca_set(l2id, num_cos, ca);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_pqos_l2ca_set_os(void **state __attribute__((unused)))
{
        int ret;
        unsigned l2id = 1;
        unsigned num_cos = 1;
        struct pqos_l2ca ca[1];

        wrap_check_init(1, PQOS_RETVAL_OK);

        ca[0].class_id = 1;
        ca[0].cdp = 0;
        ca[0].u.ways_mask = 0xf;

        expect_value(__wrap_os_l2ca_set, l2id, l2id);
        expect_value(__wrap_os_l2ca_set, num_cos, num_cos);
        expect_value(__wrap_os_l2ca_set, ca, ca);
        will_return(__wrap_os_l2ca_set, PQOS_RETVAL_OK);

        ret = pqos_l2ca_set(l2id, num_cos, ca);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_pqos_l2ca_set_param(void **state __attribute__((unused)))
{
        int ret;
        unsigned l2id = 1;
        unsigned num_cos = 1;
        struct pqos_l2ca ca[1];

        ret = pqos_l2ca_set(l2id, 0, ca);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);

        ret = pqos_l2ca_set(l2id, num_cos, NULL);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);

        ca[0].class_id = 1;
        ca[0].cdp = 0;
        ca[0].u.ways_mask = 0x5;

        ret = pqos_l2ca_set(l2id, num_cos, ca);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);

        ca[0].class_id = 1;
        ca[0].cdp = 0;
        ca[0].u.ways_mask = 0;

        ret = pqos_l2ca_set(l2id, num_cos, ca);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);

        ca[0].class_id = 1;
        ca[0].cdp = 1;
        ca[0].u.s.data_mask = 0x5;
        ca[0].u.s.code_mask = 0xf0;

        ret = pqos_l2ca_set(l2id, num_cos, ca);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);

        ca[0].class_id = 1;
        ca[0].cdp = 1;
        ca[0].u.s.data_mask = 0xf0;
        ca[0].u.s.code_mask = 0x5;

        ret = pqos_l2ca_set(l2id, num_cos, ca);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
}

/* ======== pqos_l2ca_get ======== */

static void
test_pqos_l2ca_get_init(void **state __attribute__((unused)))
{
        int ret;
        unsigned l2id = 1;
        unsigned max_num_ca = 1;
        unsigned num_ca;
        struct pqos_l2ca ca[1];

        wrap_check_init(1, PQOS_RETVAL_INIT);

        ret = pqos_l2ca_get(l2id, max_num_ca, &num_ca, ca);
        assert_int_equal(ret, PQOS_RETVAL_INIT);
}

static void
test_pqos_l2ca_get_hw(void **state __attribute__((unused)))
{
        int ret;
        unsigned l2id = 1;
        unsigned max_num_ca = 1;
        unsigned num_ca;
        struct pqos_l2ca ca[1];

        wrap_check_init(1, PQOS_RETVAL_OK);

        expect_value(__wrap_hw_l2ca_get, l2id, l2id);
        expect_value(__wrap_hw_l2ca_get, max_num_ca, max_num_ca);
        expect_value(__wrap_hw_l2ca_get, num_ca, &num_ca);
        expect_value(__wrap_hw_l2ca_get, ca, ca);
        will_return(__wrap_hw_l2ca_get, PQOS_RETVAL_OK);

        ret = pqos_l2ca_get(l2id, max_num_ca, &num_ca, ca);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_pqos_l2ca_get_os(void **state __attribute__((unused)))
{
        int ret;
        unsigned l2id = 1;
        unsigned max_num_ca = 1;
        unsigned num_ca;
        struct pqos_l2ca ca[1];

        wrap_check_init(1, PQOS_RETVAL_OK);

        expect_value(__wrap_os_l2ca_get, l2id, l2id);
        expect_value(__wrap_os_l2ca_get, max_num_ca, max_num_ca);
        expect_value(__wrap_os_l2ca_get, num_ca, &num_ca);
        expect_value(__wrap_os_l2ca_get, ca, ca);
        will_return(__wrap_os_l2ca_get, PQOS_RETVAL_OK);

        ret = pqos_l2ca_get(l2id, max_num_ca, &num_ca, ca);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_pqos_l2ca_get_param(void **state __attribute__((unused)))
{
        int ret;
        unsigned l2id = 1;
        unsigned max_num_ca = 1;
        unsigned num_ca;
        struct pqos_l2ca ca[1];

        ret = pqos_l2ca_get(l2id, 0, &num_ca, ca);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);

        ret = pqos_l2ca_get(l2id, max_num_ca, NULL, ca);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);

        ret = pqos_l2ca_get(l2id, max_num_ca, &num_ca, NULL);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
}

/* ======== pqos_l2ca_get_min_cbm_bits ======== */

static void
test_pqos_l2ca_get_min_cbm_bits_init(void **state __attribute__((unused)))
{
        int ret;
        unsigned min_cbm_bits;

        wrap_check_init(1, PQOS_RETVAL_INIT);

        ret = pqos_l2ca_get_min_cbm_bits(&min_cbm_bits);
        assert_int_equal(ret, PQOS_RETVAL_INIT);
}

static void
test_pqos_l2ca_get_min_cbm_bits_hw(void **state __attribute__((unused)))
{
        int ret;
        unsigned min_cbm_bits;

        wrap_check_init(1, PQOS_RETVAL_OK);

        expect_value(__wrap_hw_l2ca_get_min_cbm_bits, min_cbm_bits,
                     &min_cbm_bits);
        will_return(__wrap_hw_l2ca_get_min_cbm_bits, PQOS_RETVAL_OK);

        ret = pqos_l2ca_get_min_cbm_bits(&min_cbm_bits);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_pqos_l2ca_get_min_cbm_bits_os(void **state __attribute__((unused)))
{
        int ret;
        unsigned min_cbm_bits;

        wrap_check_init(1, PQOS_RETVAL_OK);

        expect_value(__wrap_os_l2ca_get_min_cbm_bits, min_cbm_bits,
                     &min_cbm_bits);
        will_return(__wrap_os_l2ca_get_min_cbm_bits, PQOS_RETVAL_OK);

        ret = pqos_l2ca_get_min_cbm_bits(&min_cbm_bits);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_pqos_l2ca_get_min_cbm_bits_param(void **state __attribute__((unused)))
{
        int ret;

        ret = pqos_l2ca_get_min_cbm_bits(NULL);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
}

/* ======== pqos_mba_set ======== */

static void
test_pqos_mba_set_init(void **state __attribute__((unused)))
{
        int ret;
        unsigned mba_id = 1;
        unsigned num_cos = 1;
        struct pqos_mba requested[1];

        wrap_check_init(1, PQOS_RETVAL_INIT);

        requested[0].class_id = 1;
        requested[0].ctrl = 0;
        requested[0].mb_max = 50;

        ret = pqos_mba_set(mba_id, num_cos, requested, NULL);
        assert_int_equal(ret, PQOS_RETVAL_INIT);
}

static void
test_pqos_mba_set_os(void **state __attribute__((unused)))
{
        int ret;
        struct cpuinfo_config config;
        unsigned mba_id = 1;
        unsigned num_cos = 1;
        struct pqos_mba requested[1];

        wrap_check_init(1, PQOS_RETVAL_OK);

        will_return(__wrap_cpuinfo_get_config, &config);

        expect_value(__wrap_os_mba_set, mba_id, mba_id);
        expect_value(__wrap_os_mba_set, num_cos, num_cos);
        expect_value(__wrap_os_mba_set, requested, &requested);
        expect_value(__wrap_os_mba_set, actual, NULL);
        will_return(__wrap_os_mba_set, PQOS_RETVAL_OK);

        config.mba_max = 100;

        requested[0].class_id = 1;
        requested[0].ctrl = 0;
        requested[0].mb_max = 50;

        ret = pqos_mba_set(mba_id, num_cos, requested, NULL);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_pqos_mba_set_os_ctrl(void **state __attribute__((unused)))
{
        int ret;
        struct cpuinfo_config config;
        unsigned mba_id = 1;
        unsigned num_cos = 1;
        struct pqos_mba requested[1];

        wrap_check_init(1, PQOS_RETVAL_OK);

        will_return(__wrap_cpuinfo_get_config, &config);

        expect_value(__wrap_os_mba_set, mba_id, mba_id);
        expect_value(__wrap_os_mba_set, num_cos, num_cos);
        expect_value(__wrap_os_mba_set, requested, &requested);
        expect_value(__wrap_os_mba_set, actual, NULL);
        will_return(__wrap_os_mba_set, PQOS_RETVAL_OK);

        config.mba_max = 100;

        requested[0].class_id = 1;
        requested[0].ctrl = 1;
        requested[0].mb_max = 200;

        ret = pqos_mba_set(mba_id, num_cos, requested, NULL);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_pqos_mba_set_hw(void **state __attribute__((unused)))
{
        int ret;
        struct cpuinfo_config config;
        unsigned mba_id = 1;
        unsigned num_cos = 1;
        struct pqos_mba requested[1];

        wrap_check_init(1, PQOS_RETVAL_OK);

        will_return(__wrap_cpuinfo_get_config, &config);

        expect_value(__wrap_hw_mba_set, mba_id, mba_id);
        expect_value(__wrap_hw_mba_set, num_cos, num_cos);
        expect_value(__wrap_hw_mba_set, requested, &requested);
        expect_value(__wrap_hw_mba_set, actual, NULL);
        will_return(__wrap_hw_mba_set, PQOS_RETVAL_OK);

        config.mba_max = 100;

        requested[0].class_id = 1;
        requested[0].ctrl = 0;
        requested[0].mb_max = 50;

        ret = pqos_mba_set(mba_id, num_cos, requested, NULL);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_pqos_mba_set_param(void **state __attribute__((unused)))
{
        int ret;
        struct cpuinfo_config config;
        unsigned mba_id = 1;
        unsigned num_cos = 1;
        struct pqos_mba requested[1];

        config.mba_max = 100;

        requested[0].class_id = 1;
        requested[0].ctrl = 0;
        requested[0].mb_max = 50;

        ret = pqos_mba_set(mba_id, 0, requested, NULL);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);

        ret = pqos_mba_set(mba_id, num_cos, NULL, NULL);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);

        wrap_check_init(1, PQOS_RETVAL_OK);
        will_return(__wrap_cpuinfo_get_config, &config);

        requested[0].class_id = 1;
        requested[0].ctrl = 0;
        requested[0].mb_max = 200;

        ret = pqos_mba_set(mba_id, num_cos, requested, NULL);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);

        wrap_check_init(1, PQOS_RETVAL_OK);
        will_return(__wrap_cpuinfo_get_config, &config);

        requested[0].class_id = 1;
        requested[0].ctrl = 0;
        requested[0].mb_max = 0;

        ret = pqos_mba_set(mba_id, num_cos, requested, NULL);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
}

/* ======== pqos_mba_get ======== */

static void
test_pqos_mba_get_init(void **state __attribute__((unused)))
{
        int ret;
        unsigned mba_id = 1;
        unsigned max_num_cos = 1;
        unsigned num_cos;
        struct pqos_mba mba_tab[1];

        wrap_check_init(1, PQOS_RETVAL_INIT);

        ret = pqos_mba_get(mba_id, max_num_cos, &num_cos, mba_tab);
        assert_int_equal(ret, PQOS_RETVAL_INIT);
}

static void
test_pqos_mba_get_os(void **state __attribute__((unused)))
{
        int ret;
        unsigned mba_id = 1;
        unsigned max_num_cos = 1;
        unsigned num_cos;
        struct pqos_mba mba_tab[1];

        wrap_check_init(1, PQOS_RETVAL_OK);

        expect_value(__wrap_os_mba_get, mba_id, mba_id);
        expect_value(__wrap_os_mba_get, max_num_cos, max_num_cos);
        expect_value(__wrap_os_mba_get, num_cos, &num_cos);
        expect_value(__wrap_os_mba_get, mba_tab, mba_tab);
        will_return(__wrap_os_mba_get, PQOS_RETVAL_OK);

        ret = pqos_mba_get(mba_id, max_num_cos, &num_cos, mba_tab);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_pqos_mba_get_hw(void **state __attribute__((unused)))
{
        int ret;
        unsigned mba_id = 1;
        unsigned max_num_cos = 1;
        unsigned num_cos;
        struct pqos_mba mba_tab[1];

        wrap_check_init(1, PQOS_RETVAL_OK);

        expect_value(__wrap_hw_mba_get, mba_id, mba_id);
        expect_value(__wrap_hw_mba_get, max_num_cos, max_num_cos);
        expect_value(__wrap_hw_mba_get, num_cos, &num_cos);
        expect_value(__wrap_hw_mba_get, mba_tab, mba_tab);
        will_return(__wrap_hw_mba_get, PQOS_RETVAL_OK);

        ret = pqos_mba_get(mba_id, max_num_cos, &num_cos, mba_tab);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_pqos_mba_get_param(void **state __attribute__((unused)))
{
        int ret;
        unsigned mba_id = 1;
        unsigned max_num_cos = 1;
        unsigned num_cos;
        struct pqos_mba mba_tab[1];

        ret = pqos_mba_get(mba_id, 0, &num_cos, mba_tab);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);

        ret = pqos_mba_get(mba_id, max_num_cos, NULL, mba_tab);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);

        ret = pqos_mba_get(mba_id, max_num_cos, &num_cos, NULL);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
}

/* ======== pqos_mon_reset ======== */

static void
test_pqos_mon_reset_init(void **state __attribute__((unused)))
{
        int ret;

        wrap_check_init(1, PQOS_RETVAL_INIT);

        ret = pqos_mon_reset();
        assert_int_equal(ret, PQOS_RETVAL_INIT);
}

static void
test_pqos_mon_reset_os(void **state __attribute__((unused)))
{
        int ret;

        wrap_check_init(1, PQOS_RETVAL_OK);

        will_return(__wrap_os_mon_reset, PQOS_RETVAL_OK);

        ret = pqos_mon_reset();
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_pqos_mon_reset_hw(void **state __attribute__((unused)))
{
        int ret;

        wrap_check_init(1, PQOS_RETVAL_OK);

        will_return(__wrap_hw_mon_reset, PQOS_RETVAL_OK);

        ret = pqos_mon_reset();
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

/* ======== pqos_mon_assoc_get ======== */

static void
test_pqos_mon_assoc_get_init(void **state __attribute__((unused)))
{
        int ret;
        unsigned lcore = 1;
        pqos_rmid_t rmid;

        wrap_check_init(1, PQOS_RETVAL_INIT);

        ret = pqos_mon_assoc_get(lcore, &rmid);
        assert_int_equal(ret, PQOS_RETVAL_INIT);
}

static void
test_pqos_mon_assoc_get_hw(void **state __attribute__((unused)))
{
        int ret;
        unsigned lcore = 1;
        pqos_rmid_t rmid;

        wrap_check_init(1, PQOS_RETVAL_OK);

        expect_value(__wrap_hw_mon_assoc_get, lcore, lcore);
        expect_value(__wrap_hw_mon_assoc_get, rmid, &rmid);
        will_return(__wrap_hw_mon_assoc_get, PQOS_RETVAL_OK);

        ret = pqos_mon_assoc_get(lcore, &rmid);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_pqos_mon_assoc_get_os(void **state __attribute__((unused)))
{
        int ret;
        unsigned lcore = 1;
        pqos_rmid_t rmid;

        wrap_check_init(1, PQOS_RETVAL_OK);

        ret = pqos_mon_assoc_get(lcore, &rmid);
        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);
}

static void
test_pqos_mon_assoc_get_param(void **state __attribute__((unused)))
{
        int ret;
        unsigned lcore = 1;

        ret = pqos_mon_assoc_get(lcore, NULL);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
}

/* ======== pqos_mon_start ======== */

static void
test_pqos_mon_start_init(void **state __attribute__((unused)))
{
        int ret;
        unsigned num_cores = 1;
        unsigned cores[] = {1};
        enum pqos_mon_event event = PQOS_MON_EVENT_LMEM_BW;
        void *context = NULL;
        struct pqos_mon_data group;

        memset(&group, 0, sizeof(group));

        wrap_check_init(1, PQOS_RETVAL_INIT);

        ret = pqos_mon_start(num_cores, cores, event, context, &group);
        assert_int_equal(ret, PQOS_RETVAL_INIT);
}

static void
test_pqos_mon_start_os(void **state __attribute__((unused)))
{
        int ret;
        unsigned num_cores = 1;
        unsigned cores[] = {1};
        enum pqos_mon_event event = PQOS_MON_EVENT_LMEM_BW;
        void *context = NULL;
        struct pqos_mon_data group;

        memset(&group, 0, sizeof(group));

        wrap_check_init(1, PQOS_RETVAL_OK);

        expect_value(__wrap_os_mon_start, num_cores, num_cores);
        expect_value(__wrap_os_mon_start, cores, cores);
        expect_value(__wrap_os_mon_start, event, event);
        expect_value(__wrap_os_mon_start, context, context);
        expect_value(__wrap_os_mon_start, group, &group);
        will_return(__wrap_os_mon_start, PQOS_RETVAL_OK);

        ret = pqos_mon_start(num_cores, cores, event, context, &group);
        assert_int_equal(ret, PQOS_RETVAL_OK);

        free(group.intl);
}

static void
test_pqos_mon_start_hw(void **state __attribute__((unused)))
{
        int ret;
        unsigned num_cores = 1;
        unsigned cores[] = {1};
        enum pqos_mon_event event = PQOS_MON_EVENT_LMEM_BW;
        void *context = NULL;
        struct pqos_mon_data group;

        memset(&group, 0, sizeof(group));

        wrap_check_init(1, PQOS_RETVAL_OK);

        expect_value(__wrap_hw_mon_start, num_cores, num_cores);
        expect_value(__wrap_hw_mon_start, cores, cores);
        expect_value(__wrap_hw_mon_start, event, event);
        expect_value(__wrap_hw_mon_start, context, context);
        expect_value(__wrap_hw_mon_start, group, &group);
        will_return(__wrap_hw_mon_start, PQOS_RETVAL_OK);

        ret = pqos_mon_start(num_cores, cores, event, context, &group);
        assert_int_equal(ret, PQOS_RETVAL_OK);

        free(group.intl);
}

static void
test_pqos_mon_start_param(void **state __attribute__((unused)))
{
        int ret;
        unsigned num_cores = 1;
        unsigned cores[] = {1};
        enum pqos_mon_event event = PQOS_MON_EVENT_LMEM_BW;
        void *context = NULL;
        struct pqos_mon_data group;

        group.valid = 0x00DEAD00;
        ret = pqos_mon_start(num_cores, cores, event, context, &group);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);

        memset(&group, 0, sizeof(group));

        ret = pqos_mon_start(num_cores, cores, event, context, NULL);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);

        ret = pqos_mon_start(num_cores, NULL, event, context, &group);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);

        ret = pqos_mon_start(0, cores, event, context, &group);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);

        ret = pqos_mon_start(num_cores, cores, 0, context, &group);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);

        ret = pqos_mon_start(num_cores, cores, (uint32_t)-1, context, &group);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);

        ret = pqos_mon_start(num_cores, cores, PQOS_PERF_EVENT_IPC, context,
                             &group);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);

        ret = pqos_mon_start(num_cores, cores, PQOS_PERF_EVENT_LLC_MISS,
                             context, &group);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
}

/* ======== pqos_mon_stop ======== */

static void
test_pqos_mon_stop_init(void **state __attribute__((unused)))
{
        int ret;
        struct pqos_mon_data group;

        memset(&group, 0, sizeof(group));
        group.valid = 0x00DEAD00;

        wrap_check_init(1, PQOS_RETVAL_INIT);

        ret = pqos_mon_stop(&group);
        assert_int_equal(ret, PQOS_RETVAL_INIT);
}

static void
test_pqos_mon_stop_hw(void **state __attribute__((unused)))
{
        int ret;
        struct pqos_mon_data group;

        memset(&group, 0, sizeof(group));
        group.valid = 0x00DEAD00;

        wrap_check_init(1, PQOS_RETVAL_OK);

        expect_value(__wrap_hw_mon_stop, group, &group);
        will_return(__wrap_hw_mon_stop, PQOS_RETVAL_OK);

        ret = pqos_mon_stop(&group);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_pqos_mon_stop_os(void **state __attribute__((unused)))
{
        int ret;
        struct pqos_mon_data group;

        memset(&group, 0, sizeof(group));
        group.valid = 0x00DEAD00;

        wrap_check_init(1, PQOS_RETVAL_OK);

        expect_value(__wrap_os_mon_stop, group, &group);
        will_return(__wrap_os_mon_stop, PQOS_RETVAL_OK);

        ret = pqos_mon_stop(&group);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_pqos_mon_stop_param(void **state __attribute__((unused)))
{
        int ret;
        struct pqos_mon_data group;

        memset(&group, 0, sizeof(group));
        group.valid = 0x00DEAD00;
        ret = pqos_mon_stop(NULL);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);

        memset(&group, 0, sizeof(group));
        ret = pqos_mon_stop(&group);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
}

/* ======== pqos_mon_poll ======== */

static void
test_pqos_mon_poll_init(void **state __attribute__((unused)))
{
        int ret;
        struct pqos_mon_data group;
        unsigned num_groups = 1;
        struct pqos_mon_data *groups[] = {&group};

        memset(&group, 0, sizeof(group));
        group.valid = 0x00DEAD00;
        group.event = PQOS_MON_EVENT_LMEM_BW;

        wrap_check_init(1, PQOS_RETVAL_INIT);

        ret = pqos_mon_poll(groups, num_groups);
        assert_int_equal(ret, PQOS_RETVAL_INIT);
}

static void
test_pqos_mon_poll(void **state __attribute__((unused)))
{
        int ret;
        struct pqos_mon_data group;
        unsigned num_groups = 1;
        struct pqos_mon_data *groups[] = {&group};

        memset(&group, 0, sizeof(group));
        group.valid = 0x00DEAD00;
        group.event = PQOS_MON_EVENT_LMEM_BW;

        wrap_check_init(1, PQOS_RETVAL_OK);

        expect_value(__wrap_pqos_mon_poll_events, group, &group);
        will_return(__wrap_pqos_mon_poll_events, PQOS_RETVAL_OK);

        ret = pqos_mon_poll(groups, num_groups);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_pqos_mon_poll_param(void **state __attribute__((unused)))
{
        int ret;
        struct pqos_mon_data group;
        unsigned num_groups = 1;
        struct pqos_mon_data *groups[] = {&group};

        memset(&group, 0, sizeof(group));
        group.valid = 0x00DEAD00;
        group.event = PQOS_MON_EVENT_LMEM_BW;

        ret = pqos_mon_poll(groups, 0);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);

        ret = pqos_mon_poll(NULL, num_groups);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);

        memset(&group, 0, sizeof(group));
        group.event = PQOS_MON_EVENT_LMEM_BW;
        ret = pqos_mon_poll(groups, num_groups);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);

        memset(&group, 0, sizeof(group));
        group.event = PQOS_MON_EVENT_LMEM_BW;
        ret = pqos_mon_poll(groups, num_groups);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);

        groups[0] = NULL;
        ret = pqos_mon_poll(groups, num_groups);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
}

/* ======== pqos_mon_start_pids ======== */

static void
test_pqos_mon_start_pids_init(void **state __attribute__((unused)))
{
        int ret;
        unsigned num_pids = 1;
        pid_t pids[] = {1};
        enum pqos_mon_event event = PQOS_MON_EVENT_LMEM_BW;
        void *context = NULL;
        struct pqos_mon_data group;

        memset(&group, 0, sizeof(group));

        wrap_check_init(1, PQOS_RETVAL_INIT);

        ret = pqos_mon_start_pids(num_pids, pids, event, context, &group);
        assert_int_equal(ret, PQOS_RETVAL_INIT);
}

static void
test_pqos_mon_start_pids_os(void **state __attribute__((unused)))
{
        int ret;
        unsigned num_pids = 1;
        pid_t pids[] = {1};
        enum pqos_mon_event event = PQOS_MON_EVENT_LMEM_BW;
        void *context = NULL;
        struct pqos_mon_data group;

        memset(&group, 0, sizeof(group));

        wrap_check_init(1, PQOS_RETVAL_OK);

        expect_value(__wrap_os_mon_start_pids, num_pids, num_pids);
        expect_value(__wrap_os_mon_start_pids, event, event);
        expect_value(__wrap_os_mon_start_pids, context, context);
        expect_value(__wrap_os_mon_start_pids, group, &group);
        will_return(__wrap_os_mon_start_pids, PQOS_RETVAL_OK);

        ret = pqos_mon_start_pids(num_pids, pids, event, context, &group);
        assert_int_equal(ret, PQOS_RETVAL_OK);

        if (group.intl != NULL)
                free(group.intl);
}

static void
test_pqos_mon_start_pids_hw(void **state __attribute__((unused)))
{
        int ret;
        unsigned num_pids = 1;
        pid_t pids[] = {1};
        enum pqos_mon_event event = PQOS_MON_EVENT_LMEM_BW;
        void *context = NULL;
        struct pqos_mon_data group;

        memset(&group, 0, sizeof(group));

        wrap_check_init(1, PQOS_RETVAL_OK);

        ret = pqos_mon_start_pids(num_pids, pids, event, context, &group);
        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);
}

static void
test_pqos_mon_start_pids_param(void **state __attribute__((unused)))
{
        int ret;
        unsigned num_pids = 1;
        pid_t pids[] = {1};
        enum pqos_mon_event event = PQOS_MON_EVENT_LMEM_BW;
        void *context = NULL;
        struct pqos_mon_data group;

        group.valid = 0x00DEAD00;
        ret = pqos_mon_start_pids(num_pids, pids, event, context, &group);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);

        memset(&group, 0, sizeof(group));

        ret = pqos_mon_start_pids(num_pids, pids, event, context, NULL);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);

        ret = pqos_mon_start_pids(num_pids, NULL, event, context, &group);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);

        ret = pqos_mon_start_pids(0, pids, event, context, &group);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);

        ret = pqos_mon_start_pids(num_pids, pids, 0, context, &group);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);

        ret =
            pqos_mon_start_pids(num_pids, pids, (uint32_t)-1, context, &group);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);

        ret = pqos_mon_start_pids(num_pids, pids, PQOS_PERF_EVENT_IPC, context,
                                  &group);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);

        ret = pqos_mon_start_pids(num_pids, pids, PQOS_PERF_EVENT_LLC_MISS,
                                  context, &group);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
}

/* ======== pqos_mon_start_pid ======== */

static void
test_pqos_mon_start_pid_os(void **state __attribute__((unused)))
{
        int ret;
        pid_t pid = 1;
        enum pqos_mon_event event = PQOS_MON_EVENT_LMEM_BW;
        void *context = NULL;
        struct pqos_mon_data group;

        memset(&group, 0, sizeof(group));

        wrap_check_init(1, PQOS_RETVAL_OK);

        expect_value(__wrap_os_mon_start_pids, num_pids, 1);
        expect_value(__wrap_os_mon_start_pids, event, event);
        expect_value(__wrap_os_mon_start_pids, context, context);
        expect_value(__wrap_os_mon_start_pids, group, &group);
        will_return(__wrap_os_mon_start_pids, PQOS_RETVAL_OK);

        ret = pqos_mon_start_pid(pid, event, context, &group);
        assert_int_equal(ret, PQOS_RETVAL_OK);

        if (group.intl != NULL)
                free(group.intl);
}

static void
test_pqos_mon_start_pid_hw(void **state __attribute__((unused)))
{
        int ret;
        pid_t pid = 1;
        enum pqos_mon_event event = PQOS_MON_EVENT_LMEM_BW;
        void *context = NULL;
        struct pqos_mon_data group;

        memset(&group, 0, sizeof(group));

        wrap_check_init(1, PQOS_RETVAL_OK);

        ret = pqos_mon_start_pid(pid, event, context, &group);
        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);
}

/* ======== pqos_mon_add_pids ======== */

static void
test_pqos_mon_add_pids_init(void **state __attribute__((unused)))
{
        int ret;
        unsigned num_pids = 1;
        pid_t pids[] = {1};
        struct pqos_mon_data group;

        memset(&group, 0, sizeof(group));
        group.valid = 0x00DEAD00;

        wrap_check_init(1, PQOS_RETVAL_INIT);

        ret = pqos_mon_add_pids(num_pids, pids, &group);
        assert_int_equal(ret, PQOS_RETVAL_INIT);
}

static void
test_pqos_mon_add_pids_os(void **state __attribute__((unused)))
{
        int ret;
        unsigned num_pids = 1;
        pid_t pids[] = {1};
        struct pqos_mon_data group;

        memset(&group, 0, sizeof(group));
        group.valid = 0x00DEAD00;

        wrap_check_init(1, PQOS_RETVAL_OK);

        expect_value(__wrap_os_mon_add_pids, num_pids, num_pids);
        expect_value(__wrap_os_mon_add_pids, pids, pids);
        expect_value(__wrap_os_mon_add_pids, group, &group);
        will_return(__wrap_os_mon_add_pids, PQOS_RETVAL_OK);

        ret = pqos_mon_add_pids(num_pids, pids, &group);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_pqos_mon_add_pids_hw(void **state __attribute__((unused)))
{
        int ret;
        unsigned num_pids = 1;
        pid_t pids[] = {1};
        struct pqos_mon_data group;

        memset(&group, 0, sizeof(group));
        group.valid = 0x00DEAD00;

        wrap_check_init(1, PQOS_RETVAL_OK);

        ret = pqos_mon_add_pids(num_pids, pids, &group);
        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);
}

static void
test_pqos_mon_add_pids_param(void **state __attribute__((unused)))
{
        int ret;
        unsigned num_pids = 1;
        pid_t pids[] = {1};
        struct pqos_mon_data group;

        memset(&group, 0, sizeof(group));

        ret = pqos_mon_add_pids(num_pids, pids, &group);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);

        group.valid = 0x00DEAD00;

        ret = pqos_mon_add_pids(0, pids, &group);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);

        ret = pqos_mon_add_pids(num_pids, NULL, &group);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);

        ret = pqos_mon_add_pids(num_pids, pids, NULL);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
}

/* ======== pqos_mon_remove_pids ======== */

static void
test_pqos_mon_remove_pids_init(void **state __attribute__((unused)))
{
        int ret;
        unsigned num_pids = 1;
        pid_t pids[] = {1};
        struct pqos_mon_data group;

        memset(&group, 0, sizeof(group));
        group.valid = 0x00DEAD00;

        wrap_check_init(1, PQOS_RETVAL_INIT);

        ret = pqos_mon_remove_pids(num_pids, pids, &group);
        assert_int_equal(ret, PQOS_RETVAL_INIT);
}

static void
test_pqos_mon_remove_pids_os(void **state __attribute__((unused)))
{
        int ret;
        unsigned num_pids = 1;
        pid_t pids[] = {1};
        struct pqos_mon_data group;

        memset(&group, 0, sizeof(group));
        group.valid = 0x00DEAD00;

        wrap_check_init(1, PQOS_RETVAL_OK);

        expect_value(__wrap_os_mon_remove_pids, num_pids, num_pids);
        expect_value(__wrap_os_mon_remove_pids, pids, pids);
        expect_value(__wrap_os_mon_remove_pids, group, &group);
        will_return(__wrap_os_mon_remove_pids, PQOS_RETVAL_OK);

        ret = pqos_mon_remove_pids(num_pids, pids, &group);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_pqos_mon_remove_pids_hw(void **state __attribute__((unused)))
{
        int ret;
        unsigned num_pids = 1;
        pid_t pids[] = {1};
        struct pqos_mon_data group;

        memset(&group, 0, sizeof(group));
        group.valid = 0x00DEAD00;

        wrap_check_init(1, PQOS_RETVAL_OK);

        ret = pqos_mon_remove_pids(num_pids, pids, &group);
        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);
}

static void
test_pqos_mon_remove_pids_param(void **state __attribute__((unused)))
{
        int ret;
        unsigned num_pids = 1;
        pid_t pids[] = {1};
        struct pqos_mon_data group;

        memset(&group, 0, sizeof(group));

        ret = pqos_mon_remove_pids(num_pids, pids, &group);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);

        group.valid = 0x00DEAD00;

        ret = pqos_mon_remove_pids(0, pids, &group);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);

        ret = pqos_mon_remove_pids(num_pids, NULL, &group);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);

        ret = pqos_mon_remove_pids(num_pids, pids, NULL);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
}

int
main(void)
{
        int result = 0;

        const struct CMUnitTest tests_init[] = {
            cmocka_unit_test(test_pqos_alloc_assoc_set_init),
            cmocka_unit_test(test_pqos_alloc_assoc_get_init),
            cmocka_unit_test(test_pqos_alloc_assoc_set_pid_init),
            cmocka_unit_test(test_pqos_alloc_assoc_get_pid_init),
            cmocka_unit_test(test_pqos_alloc_assign_init),
            cmocka_unit_test(test_pqos_alloc_release_init),
            cmocka_unit_test(test_pqos_alloc_assign_pid_init),
            cmocka_unit_test(test_pqos_alloc_release_pid_init),
            cmocka_unit_test(test_pqos_alloc_reset_init),
            cmocka_unit_test(test_pqos_pid_get_pid_assoc_init),
            cmocka_unit_test(test_pqos_l3ca_set_init),
            cmocka_unit_test(test_pqos_l3ca_get_init),
            cmocka_unit_test(test_pqos_l3ca_get_min_cbm_bits_init),
            cmocka_unit_test(test_pqos_l2ca_set_init),
            cmocka_unit_test(test_pqos_l2ca_get_init),
            cmocka_unit_test(test_pqos_l2ca_get_min_cbm_bits_init),
            cmocka_unit_test(test_pqos_mba_set_init),
            cmocka_unit_test(test_pqos_mba_get_init),
            cmocka_unit_test(test_pqos_mon_reset_init),
            cmocka_unit_test(test_pqos_mon_assoc_get_init),
            cmocka_unit_test(test_pqos_mon_start_init),
            cmocka_unit_test(test_pqos_mon_stop_init),
            cmocka_unit_test(test_pqos_mon_poll_init),
            cmocka_unit_test(test_pqos_mon_start_pids_init),
            cmocka_unit_test(test_pqos_mon_add_pids_init),
            cmocka_unit_test(test_pqos_mon_remove_pids_init)};

        const struct CMUnitTest tests_param[] = {
            cmocka_unit_test(test_pqos_alloc_assoc_get_param_id_null),
            cmocka_unit_test(test_pqos_alloc_assoc_get_pid_param_id_null),
            cmocka_unit_test(test_pqos_alloc_assign_param_technology),
            cmocka_unit_test(test_pqos_alloc_assign_param_core_null),
            cmocka_unit_test(test_pqos_alloc_assign_param_core_num),
            cmocka_unit_test(test_pqos_alloc_assign_param_id_null),
            cmocka_unit_test(test_pqos_alloc_release_param),
            cmocka_unit_test(test_pqos_alloc_assign_pid_param),
            cmocka_unit_test(test_pqos_alloc_release_pid_param),
            cmocka_unit_test(test_pqos_alloc_reset_param),
            cmocka_unit_test(test_pqos_pid_get_pid_assoc_param),
            cmocka_unit_test(test_pqos_l3ca_set_param),
            cmocka_unit_test(test_pqos_l3ca_get_param),
            cmocka_unit_test(test_pqos_l3ca_get_min_cbm_bits_param),
            cmocka_unit_test(test_pqos_l2ca_set_param),
            cmocka_unit_test(test_pqos_l2ca_get_param),
            cmocka_unit_test(test_pqos_l2ca_get_min_cbm_bits_param),
            cmocka_unit_test(test_pqos_mba_set_param),
            cmocka_unit_test(test_pqos_mba_get_param),
            cmocka_unit_test(test_pqos_mon_assoc_get_param),
            cmocka_unit_test(test_pqos_mon_start_param),
            cmocka_unit_test(test_pqos_mon_stop_param),
            cmocka_unit_test(test_pqos_mon_poll_param),
            cmocka_unit_test(test_pqos_mon_start_pids_param),
            cmocka_unit_test(test_pqos_mon_add_pids_param),
            cmocka_unit_test(test_pqos_mon_remove_pids_param)};

        const struct CMUnitTest tests_hw[] = {
            cmocka_unit_test(test_pqos_alloc_assoc_set_hw),
            cmocka_unit_test(test_pqos_alloc_assoc_get_hw),
            cmocka_unit_test(test_pqos_alloc_assoc_set_pid_hw),
            cmocka_unit_test(test_pqos_alloc_assoc_get_pid_hw),
            cmocka_unit_test(test_pqos_alloc_assign_hw),
            cmocka_unit_test(test_pqos_alloc_release_hw),
            cmocka_unit_test(test_pqos_alloc_assign_pid_hw),
            cmocka_unit_test(test_pqos_alloc_release_pid_hw),
            cmocka_unit_test(test_pqos_alloc_reset_hw),
            cmocka_unit_test(test_pqos_pid_get_pid_assoc_hw),
            cmocka_unit_test(test_pqos_l3ca_set_hw),
            cmocka_unit_test(test_pqos_l3ca_set_hw_cdp),
            cmocka_unit_test(test_pqos_l3ca_get_hw),
            cmocka_unit_test(test_pqos_l3ca_get_min_cbm_bits_hw),
            cmocka_unit_test(test_pqos_l2ca_set_hw),
            cmocka_unit_test(test_pqos_l2ca_set_hw_cdp),
            cmocka_unit_test(test_pqos_l2ca_get_hw),
            cmocka_unit_test(test_pqos_l2ca_get_min_cbm_bits_hw),
            cmocka_unit_test(test_pqos_mba_set_hw),
            cmocka_unit_test(test_pqos_mba_get_hw),
            cmocka_unit_test(test_pqos_mon_reset_hw),
            cmocka_unit_test(test_pqos_mon_assoc_get_hw),
            cmocka_unit_test(test_pqos_mon_start_hw),
            cmocka_unit_test(test_pqos_mon_stop_hw),
            cmocka_unit_test(test_pqos_mon_poll),
            cmocka_unit_test(test_pqos_mon_start_pids_hw),
            cmocka_unit_test(test_pqos_mon_start_pid_hw),
            cmocka_unit_test(test_pqos_mon_add_pids_hw),
            cmocka_unit_test(test_pqos_mon_remove_pids_hw)};

#ifdef __linux__
        const struct CMUnitTest tests_os[] = {
            cmocka_unit_test(test_pqos_alloc_assoc_set_os),
            cmocka_unit_test(test_pqos_alloc_assoc_get_os),
            cmocka_unit_test(test_pqos_alloc_assoc_set_pid_os),
            cmocka_unit_test(test_pqos_alloc_assoc_get_pid_os),
            cmocka_unit_test(test_pqos_alloc_assign_os),
            cmocka_unit_test(test_pqos_alloc_release_os),
            cmocka_unit_test(test_pqos_alloc_assign_pid_os),
            cmocka_unit_test(test_pqos_alloc_release_pid_os),
            cmocka_unit_test(test_pqos_alloc_reset_os),
            cmocka_unit_test(test_pqos_pid_get_pid_assoc_os),
            cmocka_unit_test(test_pqos_l3ca_set_os),
            cmocka_unit_test(test_pqos_l3ca_get_os),
            cmocka_unit_test(test_pqos_l3ca_get_min_cbm_bits_os),
            cmocka_unit_test(test_pqos_l2ca_set_os),
            cmocka_unit_test(test_pqos_l2ca_get_os),
            cmocka_unit_test(test_pqos_l2ca_get_min_cbm_bits_os),
            cmocka_unit_test(test_pqos_mba_set_os),
            cmocka_unit_test(test_pqos_mba_set_os_ctrl),
            cmocka_unit_test(test_pqos_mba_get_os),
            cmocka_unit_test(test_pqos_mon_reset_os),
            cmocka_unit_test(test_pqos_mon_assoc_get_os),
            cmocka_unit_test(test_pqos_mon_start_os),
            cmocka_unit_test(test_pqos_mon_stop_os),
            cmocka_unit_test(test_pqos_mon_poll),
            cmocka_unit_test(test_pqos_mon_start_pids_os),
            cmocka_unit_test(test_pqos_mon_start_pid_os),
            cmocka_unit_test(test_pqos_mon_add_pids_os),
            cmocka_unit_test(test_pqos_mon_remove_pids_os)};
#endif

        result += cmocka_run_group_tests(tests_init, NULL, NULL);
        result += cmocka_run_group_tests(tests_param, NULL, NULL);
        result += cmocka_run_group_tests(tests_hw, setup_hw, NULL);
#ifdef __linux__
        result += cmocka_run_group_tests(tests_os, setup_os, test_fini);
        result += cmocka_run_group_tests(tests_os, setup_os_resctrl_mon, test_fini);
#endif

        return result;
}
