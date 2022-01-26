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

#include "test.h"
#include "mock_cap.h"
#include "mock_common.h"
#include "mock_resctrl.h"
#include "mock_resctrl_alloc.h"
#include "mock_resctrl_schemata.h"
#include "mock_resctrl_monitoring.h"

#include "allocation.h"
#include "os_allocation.h"

/* ======== mock ======== */

int
os_alloc_assoc_set(const unsigned lcore, const unsigned class_id)
{
        check_expected(lcore);
        check_expected(class_id);

        return mock_type(int);
}

int
os_alloc_assoc_pid(const pid_t task, const unsigned class_id)
{
        check_expected(task);
        check_expected(class_id);

        return mock_type(int);
}

int
os_alloc_reset_cores(void)
{
        return mock_type(int);
}

int
os_alloc_reset_schematas(const struct pqos_cap_l3ca *l3_cap
                         __attribute__((unused)),
                         const struct pqos_cap_l2ca *l2_cap
                         __attribute__((unused)),
                         const struct pqos_cap_mba *mba_cap
                         __attribute__((unused)))
{
        return mock_type(int);
}

int
os_alloc_reset_tasks(void)
{
        return mock_type(int);
}

int
os_alloc_mount(const enum pqos_cdp_config l3_cdp_cfg,
               const enum pqos_cdp_config l2_cdp_cfg,
               const enum pqos_mba_config mba_cfg)
{
        check_expected(l3_cdp_cfg);
        check_expected(l2_cdp_cfg);
        check_expected(mba_cfg);

        return mock_type(int);
}

int
__wrap_mkdir(const char *path, mode_t mode)
{
        check_expected(path);
        check_expected(mode);

        return mock_type(int);
}

/* ======== os_alloc_assoc_get ======== */

static void
test_os_alloc_assoc_get(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        unsigned class_id = 0;
        unsigned lcore = 2;

        will_return(__wrap__pqos_cap_get, data->cpu);

        will_return(__wrap_resctrl_lock_shared, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_lock_release, PQOS_RETVAL_OK);

        expect_value(__wrap_resctrl_alloc_assoc_get, lcore, lcore);
        will_return(__wrap_resctrl_alloc_assoc_get, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_assoc_get, 3);

        ret = os_alloc_assoc_get(lcore, &class_id);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(class_id, 3);
}

static void
test_os_alloc_assoc_get_param(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        unsigned class_id;

        will_return(__wrap__pqos_cap_get, data->cpu);

        ret = os_alloc_assoc_get(1000, &class_id);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
}

/* ======== os_alloc_assign ======== */

static void
test_os_alloc_assign(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        unsigned technology = PQOS_TECHNOLOGY_ALL;
        unsigned class_id = 0;
        unsigned core_num = 2;
        unsigned core_array[] = {1, 2};

        will_return(__wrap__pqos_cap_get, data->cap);

        expect_value(__wrap_resctrl_alloc_get_unused_group, grps_num, 3);
        will_return(__wrap_resctrl_alloc_get_unused_group, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_unused_group, 2);

        expect_value(os_alloc_assoc_set, lcore, core_array[0]);
        expect_value(os_alloc_assoc_set, class_id, 2);
        will_return(os_alloc_assoc_set, PQOS_RETVAL_OK);

        expect_value(os_alloc_assoc_set, lcore, core_array[1]);
        expect_value(os_alloc_assoc_set, class_id, 2);
        will_return(os_alloc_assoc_set, PQOS_RETVAL_OK);

        ret = os_alloc_assign(technology, core_array, core_num, &class_id);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(class_id, 2);
}

/* ======== os_alloc_release ======== */

static void
test_os_alloc_release(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        unsigned core_num = 2;
        unsigned core_array[] = {1, 2};

        will_return(__wrap__pqos_cap_get, data->cpu);

        will_return(__wrap_resctrl_lock_exclusive, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_lock_release, PQOS_RETVAL_OK);

        expect_value(__wrap_resctrl_alloc_cpumask_read, class_id, 0);
        will_return(__wrap_resctrl_alloc_cpumask_read, PQOS_RETVAL_OK);

        expect_value(__wrap_resctrl_cpumask_set, lcore, core_array[0]);
        expect_value(__wrap_resctrl_cpumask_set, lcore, core_array[1]);

        expect_value(__wrap_resctrl_alloc_cpumask_write, class_id, 0);
        will_return(__wrap_resctrl_alloc_cpumask_write, PQOS_RETVAL_OK);

        ret = os_alloc_release(core_array, core_num);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_os_alloc_release_param(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        unsigned core_num = 1;
        unsigned core_array[] = {1000};

        will_return(__wrap__pqos_cap_get, data->cpu);

        will_return(__wrap_resctrl_lock_exclusive, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_lock_release, PQOS_RETVAL_OK);

        expect_value(__wrap_resctrl_alloc_cpumask_read, class_id, 0);
        will_return(__wrap_resctrl_alloc_cpumask_read, PQOS_RETVAL_OK);

        ret = os_alloc_release(core_array, core_num);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
}

/* ======== os_l3ca_set ======== */

static void
test_os_l3ca_set_unsupported(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        struct pqos_l3ca ca[1];

        will_return(__wrap__pqos_cap_get, data->cap);
        will_return(__wrap__pqos_cap_get, data->cpu);

        ca[0].class_id = 0;
        ca[0].cdp = 0;
        ca[0].u.ways_mask = 0xf;

        ret = os_l3ca_set(0, 1, ca);
        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);
}

static void
test_os_l3ca_set_param(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        struct pqos_l3ca ca[1];

        will_return(__wrap__pqos_cap_get, data->cap);
        will_return(__wrap__pqos_cap_get, data->cpu);

        ca[0].class_id = 0;
        ca[0].cdp = 0;
        ca[0].u.ways_mask = 0xf;

        ret = os_l3ca_set(1000, 1, ca);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
}

static void
test_os_l3ca_set_cdp_off(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        unsigned num_cos = 1;
        struct pqos_l3ca ca[1];
        unsigned l3cat_id = 0;

        data->cap_l3ca.cdp_on = 0;

        ca[0].class_id = 0;
        ca[0].cdp = 1;
        ca[0].u.s.data_mask = 0xf0;
        ca[0].u.s.code_mask = 0xff;

        will_return(__wrap__pqos_cap_get, data->cap);
        will_return(__wrap__pqos_cap_get, data->cpu);

        will_return(__wrap_resctrl_lock_exclusive, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_lock_release, PQOS_RETVAL_OK);

        ret = os_l3ca_set(l3cat_id, num_cos, ca);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
}

static void
test_os_l3ca_set_cdp_on(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        unsigned num_cos = 1;
        struct pqos_l3ca ca[1];
        unsigned l3cat_id = 0;

        data->cap_l3ca.cdp_on = 1;

        ca[0].class_id = 0;
        ca[0].cdp = 1;
        ca[0].u.s.data_mask = 0xf0;
        ca[0].u.s.code_mask = 0xff;

        will_return(__wrap__pqos_cap_get, data->cap);
        will_return(__wrap__pqos_cap_get, data->cpu);

        will_return(__wrap_resctrl_lock_exclusive, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_lock_release, PQOS_RETVAL_OK);

        expect_value(__wrap_resctrl_alloc_schemata_read, class_id,
                     ca[0].class_id);
        will_return(__wrap_resctrl_alloc_schemata_read, PQOS_RETVAL_OK);

        expect_value(__wrap_resctrl_schemata_l3ca_set, resource_id, l3cat_id);
        will_return(__wrap_resctrl_schemata_l3ca_set, PQOS_RETVAL_OK);

        expect_value(__wrap_resctrl_alloc_schemata_write, class_id,
                     ca[0].class_id);
        expect_value(__wrap_resctrl_alloc_schemata_write, technology,
                     PQOS_TECHNOLOGY_L3CA);
        will_return(__wrap_resctrl_alloc_schemata_write, PQOS_RETVAL_OK);

        ret = os_l3ca_set(l3cat_id, num_cos, ca);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_os_l3ca_set(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        unsigned num_cos = 1;
        struct pqos_l3ca ca[1];
        unsigned l3cat_id = 0;

        data->cap_l3ca.cdp_on = 0;

        ca[0].class_id = 0;
        ca[0].cdp = 0;
        ca[0].u.ways_mask = 0xf;

        will_return(__wrap__pqos_cap_get, data->cap);
        will_return(__wrap__pqos_cap_get, data->cpu);

        will_return(__wrap_resctrl_lock_exclusive, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_lock_release, PQOS_RETVAL_OK);

        expect_value(__wrap_resctrl_alloc_schemata_read, class_id,
                     ca[0].class_id);
        will_return(__wrap_resctrl_alloc_schemata_read, PQOS_RETVAL_OK);

        expect_value(__wrap_resctrl_schemata_l3ca_set, resource_id, l3cat_id);
        will_return(__wrap_resctrl_schemata_l3ca_set, PQOS_RETVAL_OK);

        expect_value(__wrap_resctrl_alloc_schemata_write, class_id,
                     ca[0].class_id);
        expect_value(__wrap_resctrl_alloc_schemata_write, technology,
                     PQOS_TECHNOLOGY_L3CA);
        will_return(__wrap_resctrl_alloc_schemata_write, PQOS_RETVAL_OK);

        ret = os_l3ca_set(l3cat_id, num_cos, ca);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

/* ======== os_l3ca_get ======== */

static void
test_os_l3ca_get_unsupported(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        unsigned max_num_ca = 16;
        struct pqos_l3ca ca[16];
        unsigned l3cat_id = 0;
        unsigned num_ca = 0;

        will_return(__wrap__pqos_cap_get, data->cap);
        will_return(__wrap__pqos_cap_get, data->cpu);

        ret = os_l3ca_get(l3cat_id, max_num_ca, &num_ca, ca);
        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);
}

static void
test_os_l3ca_get_param(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        unsigned max_num_ca = 16;
        struct pqos_l3ca ca[16];
        unsigned l3cat_id = 0;
        unsigned num_ca = 0;

        will_return(__wrap__pqos_cap_get, data->cap);
        will_return(__wrap__pqos_cap_get, data->cpu);

        ret = os_l3ca_get(1000, max_num_ca, &num_ca, ca);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);

        will_return(__wrap__pqos_cap_get, data->cap);
        will_return(__wrap__pqos_cap_get, data->cpu);

        ret = os_l3ca_get(l3cat_id, 1, &num_ca, ca);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
}

static void
test_os_l3ca_get(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        unsigned max_num_ca = 16;
        struct pqos_l3ca ca[16];
        unsigned l3cat_id = 0;
        unsigned num_ca = 0;
        unsigned i;

        will_return(__wrap__pqos_cap_get, data->cap);
        will_return(__wrap__pqos_cap_get, data->cpu);

        will_return(__wrap_resctrl_lock_shared, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_lock_release, PQOS_RETVAL_OK);

        for (i = 0; i < data->cap_l3ca.num_classes; ++i) {
                expect_value(__wrap_resctrl_alloc_schemata_read, class_id, i);
                will_return(__wrap_resctrl_alloc_schemata_read, PQOS_RETVAL_OK);

                expect_value(__wrap_resctrl_schemata_l3ca_get, resource_id,
                             l3cat_id);
                will_return(__wrap_resctrl_schemata_l3ca_get, PQOS_RETVAL_OK);
        }

        ret = os_l3ca_get(l3cat_id, max_num_ca, &num_ca, ca);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        for (i = 0; i < data->cap_l3ca.num_classes; ++i)
                assert_int_equal(ca[i].class_id, i);
}

/* ======== os_l2ca_set ======== */

static void
test_os_l2ca_set_unsupported(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        struct pqos_l2ca ca[1];

        will_return(__wrap__pqos_cap_get, data->cap);
        will_return(__wrap__pqos_cap_get, data->cpu);

        ca[0].class_id = 0;
        ca[0].cdp = 0;
        ca[0].u.ways_mask = 0xf;

        ret = os_l2ca_set(0, 1, ca);
        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);
}

static void
test_os_l2ca_set_param(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        struct pqos_l2ca ca[1];

        will_return(__wrap__pqos_cap_get, data->cap);
        will_return(__wrap__pqos_cap_get, data->cpu);

        ca[0].class_id = 0;
        ca[0].cdp = 0;
        ca[0].u.ways_mask = 0xf;

        ret = os_l2ca_set(1000, 1, ca);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
}

static void
test_os_l2ca_set_cdp_off(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        unsigned num_cos = 1;
        struct pqos_l2ca ca[1];
        unsigned l2cat_id = 0;

        data->cap_l2ca.cdp_on = 0;

        ca[0].class_id = 0;
        ca[0].cdp = 1;
        ca[0].u.s.data_mask = 0xf0;
        ca[0].u.s.code_mask = 0xff;

        will_return(__wrap__pqos_cap_get, data->cap);
        will_return(__wrap__pqos_cap_get, data->cpu);

        will_return(__wrap_resctrl_lock_exclusive, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_lock_release, PQOS_RETVAL_OK);

        ret = os_l2ca_set(l2cat_id, num_cos, ca);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
}

static void
test_os_l2ca_set_cdp_on(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        unsigned num_cos = 1;
        struct pqos_l2ca ca[1];
        unsigned l2cat_id = 0;

        data->cap_l2ca.cdp_on = 1;

        ca[0].class_id = 0;
        ca[0].cdp = 1;
        ca[0].u.s.data_mask = 0xf0;
        ca[0].u.s.code_mask = 0xff;

        will_return(__wrap__pqos_cap_get, data->cap);
        will_return(__wrap__pqos_cap_get, data->cpu);

        will_return(__wrap_resctrl_lock_exclusive, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_lock_release, PQOS_RETVAL_OK);

        expect_value(__wrap_resctrl_alloc_schemata_read, class_id,
                     ca[0].class_id);
        will_return(__wrap_resctrl_alloc_schemata_read, PQOS_RETVAL_OK);

        expect_value(__wrap_resctrl_schemata_l2ca_set, resource_id, l2cat_id);
        will_return(__wrap_resctrl_schemata_l2ca_set, PQOS_RETVAL_OK);

        expect_value(__wrap_resctrl_alloc_schemata_write, class_id,
                     ca[0].class_id);
        expect_value(__wrap_resctrl_alloc_schemata_write, technology,
                     PQOS_TECHNOLOGY_L2CA);
        will_return(__wrap_resctrl_alloc_schemata_write, PQOS_RETVAL_OK);

        ret = os_l2ca_set(l2cat_id, num_cos, ca);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_os_l2ca_set(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        unsigned num_cos = 1;
        struct pqos_l2ca ca[1];
        unsigned l2cat_id = 0;

        data->cap_l2ca.cdp_on = 0;

        ca[0].class_id = 0;
        ca[0].cdp = 0;
        ca[0].u.ways_mask = 0xf;

        will_return(__wrap__pqos_cap_get, data->cap);
        will_return(__wrap__pqos_cap_get, data->cpu);

        will_return(__wrap_resctrl_lock_exclusive, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_lock_release, PQOS_RETVAL_OK);

        expect_value(__wrap_resctrl_alloc_schemata_read, class_id,
                     ca[0].class_id);
        will_return(__wrap_resctrl_alloc_schemata_read, PQOS_RETVAL_OK);

        expect_value(__wrap_resctrl_schemata_l2ca_set, resource_id, l2cat_id);
        will_return(__wrap_resctrl_schemata_l2ca_set, PQOS_RETVAL_OK);

        expect_value(__wrap_resctrl_alloc_schemata_write, class_id,
                     ca[0].class_id);
        expect_value(__wrap_resctrl_alloc_schemata_write, technology,
                     PQOS_TECHNOLOGY_L2CA);
        will_return(__wrap_resctrl_alloc_schemata_write, PQOS_RETVAL_OK);

        ret = os_l2ca_set(l2cat_id, num_cos, ca);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

/* ======== os_l2ca_get ======== */

static void
test_os_l2ca_get_unsupported(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        unsigned max_num_ca = 16;
        struct pqos_l2ca ca[16];
        unsigned l2cat_id = 0;
        unsigned num_ca = 0;

        will_return(__wrap__pqos_cap_get, data->cap);
        will_return(__wrap__pqos_cap_get, data->cpu);

        ret = os_l2ca_get(l2cat_id, max_num_ca, &num_ca, ca);
        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);
}

static void
test_os_l2ca_get_param(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        unsigned max_num_ca = 16;
        struct pqos_l2ca ca[16];
        unsigned l2cat_id = 0;
        unsigned num_ca = 0;

        will_return(__wrap__pqos_cap_get, data->cap);
        will_return(__wrap__pqos_cap_get, data->cpu);

        ret = os_l2ca_get(1000, max_num_ca, &num_ca, ca);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);

        will_return(__wrap__pqos_cap_get, data->cap);
        will_return(__wrap__pqos_cap_get, data->cpu);

        ret = os_l2ca_get(l2cat_id, 1, &num_ca, ca);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
}

static void
test_os_l2ca_get(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        unsigned max_num_ca = 16;
        struct pqos_l2ca ca[16];
        unsigned l2cat_id = 0;
        unsigned num_ca = 0;
        unsigned i;

        will_return(__wrap__pqos_cap_get, data->cap);
        will_return(__wrap__pqos_cap_get, data->cpu);

        will_return(__wrap_resctrl_lock_shared, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_lock_release, PQOS_RETVAL_OK);

        for (i = 0; i < data->cap_l2ca.num_classes; ++i) {
                expect_value(__wrap_resctrl_alloc_schemata_read, class_id, i);
                will_return(__wrap_resctrl_alloc_schemata_read, PQOS_RETVAL_OK);

                expect_value(__wrap_resctrl_schemata_l2ca_get, resource_id,
                             l2cat_id);
                will_return(__wrap_resctrl_schemata_l2ca_get, PQOS_RETVAL_OK);
        }

        ret = os_l2ca_get(l2cat_id, max_num_ca, &num_ca, ca);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        for (i = 0; i < data->cap_l2ca.num_classes; ++i)
                assert_int_equal(ca[i].class_id, i);
}

/* ======== os_mba_set ======== */

static void
test_os_mba_set_unsupported(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        struct pqos_mba ca[1];

        will_return(__wrap__pqos_cap_get, data->cap);
        will_return(__wrap__pqos_cap_get, data->cpu);

        ca[0].class_id = 0;
        ca[0].ctrl = 0;
        ca[0].mb_max = 50;

        ret = os_mba_set(0, 1, ca, NULL);
        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);
}

static void
test_os_mba_set_param(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        struct pqos_mba ca[1];

        will_return(__wrap__pqos_cap_get, data->cap);
        will_return(__wrap__pqos_cap_get, data->cpu);

        ca[0].class_id = 0;
        ca[0].ctrl = 0;
        ca[0].mb_max = 50;

        ret = os_mba_set(1000, 1, ca, NULL);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
}

static void
test_os_mba_set_ctrl_off(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        unsigned num_cos = 1;
        struct pqos_mba ca[1];
        unsigned mbat_id = 0;

        data->cap_mba.ctrl_on = 0;

        ca[0].class_id = 0;
        ca[0].ctrl = 1;
        ca[0].mb_max = 10000;

        will_return(__wrap__pqos_cap_get, data->cap);
        will_return(__wrap__pqos_cap_get, data->cpu);

        will_return(__wrap_resctrl_lock_exclusive, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_lock_release, PQOS_RETVAL_OK);

        ret = os_mba_set(mbat_id, num_cos, ca, NULL);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
}

static void
test_os_mba_set_ctrl_on(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        unsigned num_cos = 1;
        struct pqos_mba ca[1];
        unsigned mbat_id = 0;

        data->cap_mba.ctrl_on = 1;

        ca[0].class_id = 0;
        ca[0].ctrl = 1;
        ca[0].mb_max = 10000;

        will_return(__wrap__pqos_cap_get, data->cap);
        will_return(__wrap__pqos_cap_get, data->cpu);

        will_return(__wrap_resctrl_lock_exclusive, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_lock_release, PQOS_RETVAL_OK);

        expect_value(__wrap_resctrl_alloc_schemata_read, class_id,
                     ca[0].class_id);
        will_return(__wrap_resctrl_alloc_schemata_read, PQOS_RETVAL_OK);

        expect_value(__wrap_resctrl_schemata_mba_set, resource_id, mbat_id);
        will_return(__wrap_resctrl_schemata_mba_set, PQOS_RETVAL_OK);

        expect_value(__wrap_resctrl_alloc_schemata_write, class_id,
                     ca[0].class_id);
        expect_value(__wrap_resctrl_alloc_schemata_write, technology,
                     PQOS_TECHNOLOGY_MBA);
        will_return(__wrap_resctrl_alloc_schemata_write, PQOS_RETVAL_OK);

        ret = os_mba_set(mbat_id, num_cos, ca, NULL);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_os_mba_set(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        unsigned num_cos = 1;
        struct pqos_mba ca[1];
        unsigned mbat_id = 0;

        data->cap_mba.ctrl_on = 0;

        ca[0].class_id = 0;
        ca[0].ctrl = 0;
        ca[0].mb_max = 60;

        will_return(__wrap__pqos_cap_get, data->cap);
        will_return(__wrap__pqos_cap_get, data->cpu);

        will_return(__wrap_resctrl_lock_exclusive, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_lock_release, PQOS_RETVAL_OK);

        expect_value(__wrap_resctrl_alloc_schemata_read, class_id,
                     ca[0].class_id);
        will_return(__wrap_resctrl_alloc_schemata_read, PQOS_RETVAL_OK);

        expect_value(__wrap_resctrl_schemata_mba_set, resource_id, mbat_id);
        will_return(__wrap_resctrl_schemata_mba_set, PQOS_RETVAL_OK);

        expect_value(__wrap_resctrl_alloc_schemata_write, class_id,
                     ca[0].class_id);
        expect_value(__wrap_resctrl_alloc_schemata_write, technology,
                     PQOS_TECHNOLOGY_MBA);
        will_return(__wrap_resctrl_alloc_schemata_write, PQOS_RETVAL_OK);

        ret = os_mba_set(mbat_id, num_cos, ca, NULL);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

/* ======== os_mba_get ======== */

static void
test_os_mba_get_unsupported(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        unsigned max_num_ca = 16;
        struct pqos_mba ca[16];
        unsigned mbat_id = 0;
        unsigned num_ca = 0;

        will_return(__wrap__pqos_cap_get, data->cap);
        will_return(__wrap__pqos_cap_get, data->cpu);

        ret = os_mba_get(mbat_id, max_num_ca, &num_ca, ca);
        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);
}

static void
test_os_mba_get_param(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        unsigned max_num_ca = 16;
        struct pqos_mba ca[16];
        unsigned mbat_id = 0;
        unsigned num_ca = 0;

        will_return(__wrap__pqos_cap_get, data->cap);
        will_return(__wrap__pqos_cap_get, data->cpu);

        ret = os_mba_get(1000, max_num_ca, &num_ca, ca);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);

        will_return(__wrap__pqos_cap_get, data->cap);
        will_return(__wrap__pqos_cap_get, data->cpu);

        ret = os_mba_get(mbat_id, 1, &num_ca, ca);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
}

static void
test_os_mba_get(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        unsigned max_num_ca = 16;
        struct pqos_mba ca[16];
        unsigned mbat_id = 0;
        unsigned num_ca = 0;
        unsigned i;

        will_return(__wrap__pqos_cap_get, data->cap);
        will_return(__wrap__pqos_cap_get, data->cpu);

        will_return(__wrap_resctrl_lock_shared, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_lock_release, PQOS_RETVAL_OK);

        for (i = 0; i < data->cap_mba.num_classes; ++i) {
                expect_value(__wrap_resctrl_alloc_schemata_read, class_id, i);
                will_return(__wrap_resctrl_alloc_schemata_read, PQOS_RETVAL_OK);

                expect_value(__wrap_resctrl_schemata_mba_get, resource_id,
                             mbat_id);
                will_return(__wrap_resctrl_schemata_mba_get, PQOS_RETVAL_OK);
        }

        ret = os_mba_get(mbat_id, max_num_ca, &num_ca, ca);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        for (i = 0; i < data->cap_mba.num_classes; ++i)
                assert_int_equal(ca[i].class_id, i);
}

/* ======== os_alloc_assoc_get_pid ======== */

static void
test_os_alloc_assoc_get_pid(void **state __attribute__((unused)))
{
        int ret;
        pid_t task = 0;
        unsigned class_id = 2;

        will_return(__wrap_resctrl_lock_shared, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_lock_release, PQOS_RETVAL_OK);

        expect_value(__wrap_resctrl_alloc_assoc_get_pid, task, task);
        will_return(__wrap_resctrl_alloc_assoc_get_pid, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_assoc_get_pid, 3);

        ret = os_alloc_assoc_get_pid(task, &class_id);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(class_id, 3);
}

/* ======== os_alloc_assign_pid ======== */

static void
test_os_alloc_assign_pid(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        unsigned technology = PQOS_TECHNOLOGY_ALL;
        unsigned class_id = 0;
        unsigned task_num = 2;
        pid_t task_array[] = {1, 2};

        will_return(__wrap__pqos_cap_get, data->cap);

        will_return(__wrap_resctrl_lock_exclusive, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_lock_release, PQOS_RETVAL_OK);

        expect_value(__wrap_resctrl_alloc_get_unused_group, grps_num, 3);
        will_return(__wrap_resctrl_alloc_get_unused_group, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_unused_group, 2);

        expect_value(__wrap_resctrl_alloc_task_write, task, task_array[0]);
        expect_value(__wrap_resctrl_alloc_task_write, class_id, 2);
        will_return(__wrap_resctrl_alloc_task_write, PQOS_RETVAL_OK);

        expect_value(__wrap_resctrl_alloc_task_write, task, task_array[1]);
        expect_value(__wrap_resctrl_alloc_task_write, class_id, 2);
        will_return(__wrap_resctrl_alloc_task_write, PQOS_RETVAL_OK);

        ret = os_alloc_assign_pid(technology, task_array, task_num, &class_id);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(class_id, 2);
}

/* ======== os_alloc_release_pid ======== */

static void
test_os_alloc_release_pid(void **state __attribute__((unused)))
{
        int ret;
        unsigned task_num = 2;
        pid_t task_array[] = {1, 2};

        will_return(__wrap_resctrl_lock_exclusive, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_lock_release, PQOS_RETVAL_OK);

        expect_value(__wrap_resctrl_alloc_task_write, task, task_array[0]);
        expect_value(__wrap_resctrl_alloc_task_write, class_id, 0);
        will_return(__wrap_resctrl_alloc_task_write, PQOS_RETVAL_OK);

        expect_value(__wrap_resctrl_alloc_task_write, task, task_array[1]);
        expect_value(__wrap_resctrl_alloc_task_write, class_id, 0);
        will_return(__wrap_resctrl_alloc_task_write, PQOS_RETVAL_OK);

        ret = os_alloc_release_pid(task_array, task_num);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

/* ======== os_alloc_reset ======== */

static void
test_os_alloc_reset_unsupported_all(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;

        will_return(__wrap__pqos_cap_get, data->cap);

        ret = os_alloc_reset(PQOS_REQUIRE_CDP_ANY, PQOS_REQUIRE_CDP_ANY,
                             PQOS_MBA_ANY);
        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);
}

static void
test_os_alloc_reset_unsupported_l3ca(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;

        will_return(__wrap__pqos_cap_get, data->cap);

        ret = os_alloc_reset(PQOS_REQUIRE_CDP_ON, PQOS_REQUIRE_CDP_ANY,
                             PQOS_MBA_ANY);
        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);

        will_return(__wrap__pqos_cap_get, data->cap);

        ret = os_alloc_reset(PQOS_REQUIRE_CDP_OFF, PQOS_REQUIRE_CDP_ANY,
                             PQOS_MBA_ANY);
        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);
}

static void
test_os_alloc_reset_unsupported_l3cdp(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;

        data->cap_l3ca.cdp = 0;

        will_return(__wrap__pqos_cap_get, data->cap);

        ret = os_alloc_reset(PQOS_REQUIRE_CDP_ON, PQOS_REQUIRE_CDP_ANY,
                             PQOS_MBA_ANY);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
}

static void
test_os_alloc_reset_unsupported_l2ca(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;

        will_return(__wrap__pqos_cap_get, data->cap);

        ret = os_alloc_reset(PQOS_REQUIRE_CDP_ANY, PQOS_REQUIRE_CDP_ON,
                             PQOS_MBA_ANY);
        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);

        will_return(__wrap__pqos_cap_get, data->cap);

        ret = os_alloc_reset(PQOS_REQUIRE_CDP_ANY, PQOS_REQUIRE_CDP_OFF,
                             PQOS_MBA_ANY);
        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);
}

static void
test_os_alloc_reset_unsupported_l2cdp(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;

        data->cap_l2ca.cdp = 0;

        will_return(__wrap__pqos_cap_get, data->cap);

        ret = os_alloc_reset(PQOS_REQUIRE_CDP_ANY, PQOS_REQUIRE_CDP_ON,
                             PQOS_MBA_ANY);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
}

static void
test_os_alloc_reset_unsupported_mba(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;

        will_return(__wrap__pqos_cap_get, data->cap);

        ret = os_alloc_reset(PQOS_REQUIRE_CDP_ANY, PQOS_REQUIRE_CDP_ANY,
                             PQOS_MBA_DEFAULT);
        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);

        will_return(__wrap__pqos_cap_get, data->cap);

        ret = os_alloc_reset(PQOS_REQUIRE_CDP_ANY, PQOS_REQUIRE_CDP_ANY,
                             PQOS_MBA_CTRL);
        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);
}

static void
test_os_alloc_reset_unsupported_mba_ctrl(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;

        data->cap_mba.ctrl = 0;

        will_return(__wrap__pqos_cap_get, data->cap);

        ret = os_alloc_reset(PQOS_REQUIRE_CDP_ANY, PQOS_REQUIRE_CDP_ANY,
                             PQOS_MBA_CTRL);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
}

static void
test_os_alloc_reset_light(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;

        will_return(__wrap__pqos_cap_get, data->cap);

        will_return(__wrap_resctrl_lock_exclusive, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_lock_release, PQOS_RETVAL_OK);

        will_return(os_alloc_reset_cores, PQOS_RETVAL_OK);
        will_return(os_alloc_reset_schematas, PQOS_RETVAL_OK);
        will_return(os_alloc_reset_tasks, PQOS_RETVAL_OK);

        ret = os_alloc_reset(PQOS_REQUIRE_CDP_ANY, PQOS_REQUIRE_CDP_ANY,
                             PQOS_MBA_ANY);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_os_alloc_reset_prep(struct test_data *data)
{
        unsigned num_grps;

        resctrl_alloc_get_grps_num(data->cap, &num_grps);

        will_return(__wrap__pqos_cap_get, data->cap);

        if (num_grps > 1) {
                expect_string(__wrap_pqos_dir_exists, path,
                              "/sys/fs/resctrl/COS1");
                will_return(__wrap_pqos_dir_exists, 0);
                expect_string(__wrap_mkdir, path, "/sys/fs/resctrl/COS1");
                expect_value(__wrap_mkdir, mode, 0755);
                will_return(__wrap_mkdir, 0);
        }
        if (num_grps > 2) {
                expect_string(__wrap_pqos_dir_exists, path,
                              "/sys/fs/resctrl/COS2");
                will_return(__wrap_pqos_dir_exists, 0);
                expect_string(__wrap_mkdir, path, "/sys/fs/resctrl/COS2");
                expect_value(__wrap_mkdir, mode, 0755);
                will_return(__wrap_mkdir, 0);
        }
        if (num_grps > 3) {
                expect_string(__wrap_pqos_dir_exists, path,
                              "/sys/fs/resctrl/COS3");
                will_return(__wrap_pqos_dir_exists, 0);
                expect_string(__wrap_mkdir, path, "/sys/fs/resctrl/COS3");
                expect_value(__wrap_mkdir, mode, 0755);
                will_return(__wrap_mkdir, 0);
        }
}

static void
test_os_alloc_reset_full(struct test_data *data,
                         enum pqos_cdp_config l3_cdp_cfg,
                         enum pqos_cdp_config l2_cdp_cfg,
                         enum pqos_mba_config mba_cfg)
{

        will_return(__wrap__pqos_cap_get, data->cap);

        will_return(__wrap_resctrl_lock_exclusive, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_lock_release, PQOS_RETVAL_OK);

        /* check if monitoring is inactive */
        will_return(__wrap_resctrl_mon_active, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_mon_active, 0);

        if (l3_cdp_cfg != PQOS_REQUIRE_CDP_ANY)
                expect_value(__wrap__pqos_cap_l3cdp_change, cdp, l3_cdp_cfg);
        if (l2_cdp_cfg != PQOS_REQUIRE_CDP_ANY)
                expect_value(__wrap__pqos_cap_l2cdp_change, cdp, l2_cdp_cfg);
        if (mba_cfg != PQOS_MBA_ANY)
                expect_value(__wrap__pqos_cap_mba_change, cfg, mba_cfg);

        if (l3_cdp_cfg == PQOS_REQUIRE_CDP_ANY)
                l3_cdp_cfg = PQOS_REQUIRE_CDP_OFF;
        if (l2_cdp_cfg == PQOS_REQUIRE_CDP_ANY)
                l2_cdp_cfg = PQOS_REQUIRE_CDP_OFF;
        if (mba_cfg == PQOS_MBA_ANY)
                mba_cfg = PQOS_MBA_DEFAULT;

        will_return(os_alloc_reset_cores, PQOS_RETVAL_OK);

        /* remount */
        will_return(__wrap_resctrl_umount, PQOS_RETVAL_OK);
        expect_value(os_alloc_mount, l3_cdp_cfg, l3_cdp_cfg);
        expect_value(os_alloc_mount, l2_cdp_cfg, l2_cdp_cfg);
        expect_value(os_alloc_mount, mba_cfg, mba_cfg);
        will_return(os_alloc_mount, PQOS_RETVAL_OK);

        /* init resctrl */
        test_os_alloc_reset_prep(data);
}

static void
test_os_alloc_reset_l3cdp_enable(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        enum pqos_cdp_config l3_cdp_cfg = PQOS_REQUIRE_CDP_ON;
        enum pqos_cdp_config l2_cdp_cfg = PQOS_REQUIRE_CDP_ANY;
        enum pqos_mba_config mba_cfg = PQOS_MBA_ANY;

        data->cap_l3ca.cdp = 1;
        data->cap_l3ca.cdp_on = 0;

        test_os_alloc_reset_full(data, l3_cdp_cfg, l2_cdp_cfg, mba_cfg);

        ret = os_alloc_reset(l3_cdp_cfg, l2_cdp_cfg, mba_cfg);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_os_alloc_reset_l3cdp_disable(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        enum pqos_cdp_config l3_cdp_cfg = PQOS_REQUIRE_CDP_OFF;
        enum pqos_cdp_config l2_cdp_cfg = PQOS_REQUIRE_CDP_ANY;
        enum pqos_mba_config mba_cfg = PQOS_MBA_ANY;

        data->cap_l3ca.cdp = 1;
        data->cap_l3ca.cdp_on = 1;

        test_os_alloc_reset_full(data, l3_cdp_cfg, l2_cdp_cfg, mba_cfg);

        ret = os_alloc_reset(l3_cdp_cfg, l2_cdp_cfg, mba_cfg);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_os_alloc_reset_l3cdp_mon(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;

        data->cap_l3ca.cdp = 1;
        data->cap_l3ca.cdp_on = 0;

        will_return(__wrap__pqos_cap_get, data->cap);

        will_return(__wrap_resctrl_lock_exclusive, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_lock_release, PQOS_RETVAL_OK);

        /* check if monitoring is active */
        will_return(__wrap_resctrl_mon_active, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_mon_active, 1);

        ret = os_alloc_reset(PQOS_REQUIRE_CDP_ON, PQOS_REQUIRE_CDP_ANY,
                             PQOS_MBA_ANY);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
}

static void
test_os_alloc_reset_l2cdp_enable(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        enum pqos_cdp_config l3_cdp_cfg = PQOS_REQUIRE_CDP_ANY;
        enum pqos_cdp_config l2_cdp_cfg = PQOS_REQUIRE_CDP_ON;
        enum pqos_mba_config mba_cfg = PQOS_MBA_ANY;

        data->cap_l2ca.cdp = 1;
        data->cap_l2ca.cdp_on = 0;

        test_os_alloc_reset_full(data, l3_cdp_cfg, l2_cdp_cfg, mba_cfg);

        ret = os_alloc_reset(l3_cdp_cfg, l2_cdp_cfg, mba_cfg);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_os_alloc_reset_l2cdp_disable(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        enum pqos_cdp_config l3_cdp_cfg = PQOS_REQUIRE_CDP_ANY;
        enum pqos_cdp_config l2_cdp_cfg = PQOS_REQUIRE_CDP_OFF;
        enum pqos_mba_config mba_cfg = PQOS_MBA_ANY;

        data->cap_l2ca.cdp = 1;
        data->cap_l2ca.cdp_on = 1;

        test_os_alloc_reset_full(data, l3_cdp_cfg, l2_cdp_cfg, mba_cfg);

        ret = os_alloc_reset(l3_cdp_cfg, l2_cdp_cfg, mba_cfg);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_os_alloc_reset_l2cdp_mon(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;

        data->cap_l2ca.cdp = 1;
        data->cap_l2ca.cdp_on = 0;

        will_return(__wrap__pqos_cap_get, data->cap);

        will_return(__wrap_resctrl_lock_exclusive, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_lock_release, PQOS_RETVAL_OK);

        /* check if monitoring is active */
        will_return(__wrap_resctrl_mon_active, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_mon_active, 1);

        ret = os_alloc_reset(PQOS_REQUIRE_CDP_ANY, PQOS_REQUIRE_CDP_ON,
                             PQOS_MBA_ANY);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
}

static void
test_os_alloc_reset_mba_ctrl_enable(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        enum pqos_cdp_config l3_cdp_cfg = PQOS_REQUIRE_CDP_ANY;
        enum pqos_cdp_config l2_cdp_cfg = PQOS_REQUIRE_CDP_ANY;
        enum pqos_mba_config mba_cfg = PQOS_MBA_CTRL;

        data->cap_mba.ctrl = 1;
        data->cap_mba.ctrl_on = 0;

        test_os_alloc_reset_full(data, l3_cdp_cfg, l2_cdp_cfg, mba_cfg);

        ret = os_alloc_reset(l3_cdp_cfg, l2_cdp_cfg, mba_cfg);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_os_alloc_reset_mba_ctrl_disable(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        enum pqos_cdp_config l3_cdp_cfg = PQOS_REQUIRE_CDP_ANY;
        enum pqos_cdp_config l2_cdp_cfg = PQOS_REQUIRE_CDP_ANY;
        enum pqos_mba_config mba_cfg = PQOS_MBA_DEFAULT;

        data->cap_mba.ctrl = 1;
        data->cap_mba.ctrl_on = 1;

        test_os_alloc_reset_full(data, l3_cdp_cfg, l2_cdp_cfg, mba_cfg);

        ret = os_alloc_reset(l3_cdp_cfg, l2_cdp_cfg, mba_cfg);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_os_alloc_reset_mba_ctrl_mon(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;

        data->cap_mba.ctrl = 1;
        data->cap_mba.ctrl_on = 0;

        will_return(__wrap__pqos_cap_get, data->cap);

        will_return(__wrap_resctrl_lock_exclusive, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_lock_release, PQOS_RETVAL_OK);

        /* check if monitoring is active */
        will_return(__wrap_resctrl_mon_active, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_mon_active, 1);

        ret = os_alloc_reset(PQOS_REQUIRE_CDP_ANY, PQOS_REQUIRE_CDP_ANY,
                             PQOS_MBA_CTRL);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
}

static void
test_os_alloc_init_mounted(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        unsigned num_grps;

        expect_string(__wrap_pqos_file_exists, path, "/sys/fs/resctrl/cpus");
        will_return(__wrap_pqos_file_exists, 1);

        will_return(__wrap__pqos_cap_get, data->cap);

        resctrl_alloc_get_grps_num(data->cap, &num_grps);

        if (num_grps >= 1) {
                expect_string(__wrap_pqos_dir_exists, path,
                              "/sys/fs/resctrl/COS1");
                will_return(__wrap_pqos_dir_exists, 1);
        }
        if (num_grps >= 2) {
                expect_string(__wrap_pqos_dir_exists, path,
                              "/sys/fs/resctrl/COS2");
                will_return(__wrap_pqos_dir_exists, 1);
        }
        if (num_grps >= 3) {
                expect_string(__wrap_pqos_dir_exists, path,
                              "/sys/fs/resctrl/COS3");
                will_return(__wrap_pqos_dir_exists, 1);
        }

        will_return(__wrap_resctrl_alloc_init, PQOS_RETVAL_OK);

        ret = os_alloc_init(data->cpu, data->cap);
        assert_int_equal(ret, PQOS_RETVAL_OK);

        will_return(__wrap_resctrl_alloc_fini, PQOS_RETVAL_OK);

        ret = os_alloc_fini();
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_os_alloc_init_unmounted(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;

        expect_string(__wrap_pqos_file_exists, path, "/sys/fs/resctrl/cpus");
        will_return(__wrap_pqos_file_exists, 0);

        expect_value(os_alloc_mount, l3_cdp_cfg, PQOS_REQUIRE_CDP_OFF);
        expect_value(os_alloc_mount, l2_cdp_cfg, PQOS_REQUIRE_CDP_OFF);
        expect_value(os_alloc_mount, mba_cfg, PQOS_MBA_DEFAULT);
        will_return(os_alloc_mount, PQOS_RETVAL_OK);

        will_return(__wrap_resctrl_alloc_init, PQOS_RETVAL_OK);

        test_os_alloc_reset_prep(data);

        ret = os_alloc_init(data->cpu, data->cap);
        assert_int_equal(ret, PQOS_RETVAL_OK);

        will_return(__wrap_resctrl_alloc_fini, PQOS_RETVAL_OK);

        ret = os_alloc_fini();
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

/* ======== os_l3ca_get_min_cbm_bits ======== */

static void
test_os_l3ca_get_min_cbm_bits(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        unsigned min_cbm_bits = 0;

        will_return(__wrap__pqos_cap_get, data->cap);

        expect_string(__wrap_pqos_fread_uint64, fname,
                      "/sys/fs/resctrl/info/L3/min_cbm_bits");
        expect_value(__wrap_pqos_fread_uint64, base, 10);
        will_return(__wrap_pqos_fread_uint64, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_fread_uint64, 2);

        ret = os_l3ca_get_min_cbm_bits(&min_cbm_bits);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(min_cbm_bits, 2);
}

static void
test_os_l3ca_get_min_cbm_bits_unsupported(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        unsigned min_cbm_bits;

        will_return(__wrap__pqos_cap_get, data->cap);

        ret = os_l3ca_get_min_cbm_bits(&min_cbm_bits);
        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);
}

/* ======== os_l2ca_get_min_cbm_bits ======== */

static void
test_os_l2ca_get_min_cbm_bits(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        unsigned min_cbm_bits = 0;

        will_return(__wrap__pqos_cap_get, data->cap);

        expect_string(__wrap_pqos_fread_uint64, fname,
                      "/sys/fs/resctrl/info/L2/min_cbm_bits");
        expect_value(__wrap_pqos_fread_uint64, base, 10);
        will_return(__wrap_pqos_fread_uint64, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_fread_uint64, 2);

        ret = os_l2ca_get_min_cbm_bits(&min_cbm_bits);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(min_cbm_bits, 2);
}

static void
test_os_l2ca_get_min_cbm_bits_unsupported(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        unsigned min_cbm_bits;

        will_return(__wrap__pqos_cap_get, data->cap);

        ret = os_l2ca_get_min_cbm_bits(&min_cbm_bits);
        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);
}

int
main(void)
{
        int result = 0;

        const struct CMUnitTest tests_l3ca[] = {
            cmocka_unit_test(test_os_l3ca_set_param),
            cmocka_unit_test(test_os_l3ca_set),
            cmocka_unit_test(test_os_l3ca_set_cdp_on),
            cmocka_unit_test(test_os_l3ca_set_cdp_off),
            cmocka_unit_test(test_os_l3ca_get_param),
            cmocka_unit_test(test_os_l3ca_get),
            cmocka_unit_test(test_os_alloc_reset_unsupported_l3cdp),
            cmocka_unit_test(test_os_alloc_reset_unsupported_l2ca),
            cmocka_unit_test(test_os_alloc_reset_unsupported_mba),
            cmocka_unit_test(test_os_alloc_reset_light),
            cmocka_unit_test(test_os_alloc_reset_l3cdp_enable),
            cmocka_unit_test(test_os_alloc_reset_l3cdp_disable),
            cmocka_unit_test(test_os_alloc_reset_l3cdp_mon),
            cmocka_unit_test(test_os_alloc_init_mounted),
            cmocka_unit_test(test_os_alloc_init_unmounted),
            cmocka_unit_test(test_os_l3ca_get_min_cbm_bits),
            cmocka_unit_test(test_os_l2ca_get_min_cbm_bits_unsupported)};

        const struct CMUnitTest tests_l2ca[] = {
            cmocka_unit_test(test_os_l2ca_set_param),
            cmocka_unit_test(test_os_l2ca_set),
            cmocka_unit_test(test_os_l2ca_set_cdp_on),
            cmocka_unit_test(test_os_l2ca_set_cdp_off),
            cmocka_unit_test(test_os_l2ca_get_param),
            cmocka_unit_test(test_os_l2ca_get),
            cmocka_unit_test(test_os_alloc_reset_unsupported_l3ca),
            cmocka_unit_test(test_os_alloc_reset_unsupported_l2cdp),
            cmocka_unit_test(test_os_alloc_reset_unsupported_mba),
            cmocka_unit_test(test_os_alloc_reset_light),
            cmocka_unit_test(test_os_alloc_reset_l2cdp_enable),
            cmocka_unit_test(test_os_alloc_reset_l2cdp_disable),
            cmocka_unit_test(test_os_alloc_reset_l2cdp_mon),
            cmocka_unit_test(test_os_l3ca_get_min_cbm_bits_unsupported),
            cmocka_unit_test(test_os_l2ca_get_min_cbm_bits)};

        const struct CMUnitTest tests_mba[] = {
            cmocka_unit_test(test_os_mba_set_param),
            cmocka_unit_test(test_os_mba_set),
            cmocka_unit_test(test_os_mba_set_ctrl_on),
            cmocka_unit_test(test_os_mba_set_ctrl_off),
            cmocka_unit_test(test_os_mba_get_param),
            cmocka_unit_test(test_os_mba_get),
            cmocka_unit_test(test_os_alloc_reset_unsupported_l3ca),
            cmocka_unit_test(test_os_alloc_reset_unsupported_l2ca),
            cmocka_unit_test(test_os_alloc_reset_unsupported_mba_ctrl),
            cmocka_unit_test(test_os_alloc_reset_light),
            cmocka_unit_test(test_os_alloc_reset_mba_ctrl_enable),
            cmocka_unit_test(test_os_alloc_reset_mba_ctrl_disable),
            cmocka_unit_test(test_os_alloc_reset_mba_ctrl_mon),
            cmocka_unit_test(test_os_l3ca_get_min_cbm_bits_unsupported),
            cmocka_unit_test(test_os_l2ca_get_min_cbm_bits_unsupported)};

        const struct CMUnitTest tests_all[] = {
            cmocka_unit_test(test_os_alloc_assoc_get),
            cmocka_unit_test(test_os_alloc_assoc_get_param),
            cmocka_unit_test(test_os_alloc_assign),
            cmocka_unit_test(test_os_alloc_release),
            cmocka_unit_test(test_os_alloc_release_param),
            cmocka_unit_test(test_os_alloc_assoc_get_pid),
            cmocka_unit_test(test_os_alloc_assign_pid),
            cmocka_unit_test(test_os_alloc_release_pid),
            cmocka_unit_test(test_os_alloc_reset_light)};

        const struct CMUnitTest tests_unsupported[] = {
            cmocka_unit_test(test_os_l3ca_set_unsupported),
            cmocka_unit_test(test_os_l3ca_get_unsupported),
            cmocka_unit_test(test_os_l2ca_set_unsupported),
            cmocka_unit_test(test_os_l2ca_get_unsupported),
            cmocka_unit_test(test_os_mba_set_unsupported),
            cmocka_unit_test(test_os_mba_get_unsupported),
            cmocka_unit_test(test_os_alloc_reset_unsupported_all)};

        result += cmocka_run_group_tests(tests_l3ca, test_init_l3ca, test_fini);
        result += cmocka_run_group_tests(tests_l2ca, test_init_l2ca, test_fini);
        result += cmocka_run_group_tests(tests_mba, test_init_mba, test_fini);
        result += cmocka_run_group_tests(tests_all, test_init_all, test_fini);
        result += cmocka_run_group_tests(tests_unsupported,
                                         test_init_unsupported, test_fini);

        return result;
}