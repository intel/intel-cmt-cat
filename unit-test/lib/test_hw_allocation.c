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

#include "allocation.h"
#include "cpu_registers.h"
#include "cpuinfo.h"
#include "mock_cap.h"
#include "mock_hw_allocation.h"
#include "mock_machine.h"
#include "test.h"

/* ======== mock ======== */

int
hw_alloc_assoc_read(const unsigned lcore, unsigned *class_id)
{
        check_expected(lcore);
        *class_id = mock_type(int);

        return mock_type(int);
}

int
hw_alloc_assoc_write(const unsigned lcore, const unsigned class_id)
{
        check_expected(lcore);
        check_expected(class_id);

        return mock_type(int);
}

int
hw_alloc_reset_l3cdp(const unsigned l3cat_id_num,
                     const unsigned *l3cat_ids,
                     const int enable)
{
        check_expected(l3cat_id_num);
        assert_non_null(l3cat_ids);
        check_expected(enable);

        return mock_type(int);
}

int
hw_alloc_reset_l2cdp(const unsigned l2id_num,
                     const unsigned *l2ids,
                     const int enable)
{
        check_expected(l2id_num);
        assert_non_null(l2ids);
        check_expected(enable);

        return mock_type(int);
}

int
hw_alloc_reset_cos(const unsigned msr_start,
                   const unsigned msr_num,
                   const unsigned coreid,
                   const uint64_t msr_val)
{
        check_expected(msr_start);
        check_expected(msr_num);
        check_expected(coreid);
        check_expected(msr_val);

        return mock_type(int);
}

int
hw_alloc_reset_assoc(void)
{
        return mock_type(int);
}

/* ======== hw_l3ca_set ======== */

static void
test_hw_l3ca_set(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        struct pqos_l3ca ca[2];

        data->cap_l3ca.cdp_on = 0;

        will_return(__wrap__pqos_cap_get, data->cap);
        will_return(__wrap__pqos_cap_get, data->cpu);

        ca[0].class_id = 0;
        ca[0].cdp = 0;
        ca[0].u.ways_mask = 0xf;
        ca[1].class_id = 1;
        ca[1].cdp = 0;
        ca[1].u.ways_mask = 0xf0;

        expect_value(__wrap_msr_write, lcore, 0);
        expect_value(__wrap_msr_write, reg,
                     PQOS_MSR_L3CA_MASK_START + ca[0].class_id);
        expect_value(__wrap_msr_write, value, ca[0].u.ways_mask);
        will_return(__wrap_msr_write, PQOS_RETVAL_OK);
        expect_value(__wrap_msr_write, lcore, 0);
        expect_value(__wrap_msr_write, reg,
                     PQOS_MSR_L3CA_MASK_START + ca[1].class_id);
        expect_value(__wrap_msr_write, value, ca[1].u.ways_mask);
        will_return(__wrap_msr_write, PQOS_RETVAL_OK);

        ret = hw_l3ca_set(0, 2, ca);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_hw_l3ca_set_cdp_on(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        struct pqos_l3ca ca[2];

        data->cap_l3ca.cdp = 1;
        data->cap_l3ca.cdp_on = 1;

        will_return(__wrap__pqos_cap_get, data->cap);
        will_return(__wrap__pqos_cap_get, data->cpu);

        ca[0].class_id = 0;
        ca[0].cdp = 0;
        ca[0].u.ways_mask = 0xf;
        ca[1].class_id = 1;
        ca[1].cdp = 1;
        ca[1].u.s.data_mask = 0xf0;
        ca[1].u.s.code_mask = 0xff;

        expect_value(__wrap_msr_write, lcore, 0);
        expect_value(__wrap_msr_write, reg,
                     PQOS_MSR_L3CA_MASK_START + ca[0].class_id * 2);
        expect_value(__wrap_msr_write, value, ca[0].u.ways_mask);
        will_return(__wrap_msr_write, PQOS_RETVAL_OK);
        expect_value(__wrap_msr_write, lcore, 0);
        expect_value(__wrap_msr_write, reg,
                     PQOS_MSR_L3CA_MASK_START + ca[0].class_id * 2 + 1);
        expect_value(__wrap_msr_write, value, ca[0].u.ways_mask);
        will_return(__wrap_msr_write, PQOS_RETVAL_OK);

        expect_value(__wrap_msr_write, lcore, 0);
        expect_value(__wrap_msr_write, reg,
                     PQOS_MSR_L3CA_MASK_START + ca[1].class_id * 2);
        expect_value(__wrap_msr_write, value, ca[1].u.s.data_mask);
        will_return(__wrap_msr_write, PQOS_RETVAL_OK);
        expect_value(__wrap_msr_write, lcore, 0);
        expect_value(__wrap_msr_write, reg,
                     PQOS_MSR_L3CA_MASK_START + ca[1].class_id * 2 + 1);
        expect_value(__wrap_msr_write, value, ca[1].u.s.code_mask);
        will_return(__wrap_msr_write, PQOS_RETVAL_OK);

        ret = hw_l3ca_set(0, 2, ca);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_hw_l3ca_set_cdp_off(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        struct pqos_l3ca ca[1];

        data->cap_l3ca.cdp = 1;
        data->cap_l3ca.cdp_on = 0;

        will_return(__wrap__pqos_cap_get, data->cap);
        will_return(__wrap__pqos_cap_get, data->cpu);

        ca[0].class_id = 0;
        ca[0].cdp = 1;
        ca[0].u.s.data_mask = 0xf0;
        ca[0].u.s.code_mask = 0xff;

        ret = hw_l3ca_set(0, 1, ca);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
}

static void
test_hw_l3ca_set_unsupported(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        struct pqos_l3ca ca[1];

        data->cap_l3ca.cdp_on = 0;

        will_return(__wrap__pqos_cap_get, data->cap);
        will_return(__wrap__pqos_cap_get, data->cpu);

        ca[0].class_id = 0;
        ca[0].cdp = 0;
        ca[0].u.ways_mask = 0xf;

        ret = hw_l3ca_set(0, 1, ca);
        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);
}

/* ======== hw_l3ca_get ======== */

static void
test_hw_l3ca_get(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        unsigned l3cat_id = 0;
        unsigned max_num_ca = data->cap_l3ca.num_classes;
        unsigned num_ca;
        struct pqos_l3ca ca[4];
        unsigned class_id;

        data->cap_l3ca.cdp_on = 0;

        will_return(__wrap__pqos_cap_get, data->cap);
        will_return(__wrap__pqos_cap_get, data->cpu);

        for (class_id = 0; class_id < data->cap_l3ca.num_classes; ++class_id) {
                expect_value(__wrap_msr_read, lcore, 0);
                expect_value(__wrap_msr_read, reg,
                             PQOS_MSR_L3CA_MASK_START + class_id);
                will_return(__wrap_msr_read, 0x1 << class_id);
                will_return(__wrap_msr_read, PQOS_RETVAL_OK);
        }

        ret = hw_l3ca_get(l3cat_id, max_num_ca, &num_ca, ca);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(num_ca, data->cap_l3ca.num_classes);

        for (class_id = 0; class_id < data->cap_l3ca.num_classes; ++class_id) {
                assert_int_equal(ca[class_id].class_id, class_id);
                assert_int_equal(ca[class_id].cdp, 0);
                assert_int_equal(ca[class_id].u.ways_mask, 0x1 << class_id);
        }
}

static void
test_hw_l3ca_get_cdp(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        unsigned l3cat_id = 0;
        unsigned max_num_ca = 2;
        unsigned num_ca;
        struct pqos_l3ca ca[2];
        unsigned i;

        data->cap_l3ca.num_classes = 2;
        data->cap_l3ca.cdp = 1;
        data->cap_l3ca.cdp_on = 1;

        will_return(__wrap__pqos_cap_get, data->cap);
        will_return(__wrap__pqos_cap_get, data->cpu);

        for (i = 0; i < data->cap_l3ca.num_classes * 2; ++i) {
                expect_value(__wrap_msr_read, lcore, 0);
                expect_value(__wrap_msr_read, reg,
                             PQOS_MSR_L3CA_MASK_START + i);
                will_return(__wrap_msr_read, 0x1 << i);
                will_return(__wrap_msr_read, PQOS_RETVAL_OK);
        }

        ret = hw_l3ca_get(l3cat_id, max_num_ca, &num_ca, ca);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(num_ca, data->cap_l3ca.num_classes);

        for (i = 0; i < data->cap_l3ca.num_classes; ++i) {
                assert_int_equal(ca[i].class_id, i);
                assert_int_equal(ca[i].cdp, 1);
                assert_int_equal(ca[i].u.s.data_mask, 0x1 << (i * 2));
                assert_int_equal(ca[i].u.s.code_mask, 0x1 << (i * 2 + 1));
        }
}

static void
test_hw_l3ca_get_unsupported(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        unsigned l3cat_id = 0;
        unsigned max_num_ca = data->cap_l3ca.num_classes;
        unsigned num_ca;
        struct pqos_l3ca ca[4];

        data->cap_l3ca.cdp_on = 0;

        will_return(__wrap__pqos_cap_get, data->cap);
        will_return(__wrap__pqos_cap_get, data->cpu);

        ca[0].class_id = 0;
        ca[0].cdp = 0;
        ca[0].u.ways_mask = 0xf;

        ret = hw_l3ca_get(l3cat_id, max_num_ca, &num_ca, ca);
        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);
}

static void
test_hw_l3ca_get_param(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        unsigned l3cat_id = 0;
        unsigned max_num_ca = 1;
        unsigned num_ca;
        struct pqos_l3ca ca[4];

        data->cap_l3ca.cdp_on = 0;

        will_return(__wrap__pqos_cap_get, data->cap);
        will_return(__wrap__pqos_cap_get, data->cpu);

        ret = hw_l3ca_get(l3cat_id, max_num_ca, &num_ca, ca);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
}

/* ======== hw_l2ca_set ======== */

static void
test_hw_l2ca_set(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        struct pqos_l2ca ca[2];

        data->cap_l2ca.cdp_on = 0;

        will_return(__wrap__pqos_cap_get, data->cap);
        will_return(__wrap__pqos_cap_get, data->cpu);

        ca[0].class_id = 0;
        ca[0].cdp = 0;
        ca[0].u.ways_mask = 0xf;
        ca[1].class_id = 1;
        ca[1].cdp = 0;
        ca[1].u.ways_mask = 0xf0;

        expect_value(__wrap_msr_write, lcore, 0);
        expect_value(__wrap_msr_write, reg,
                     PQOS_MSR_L2CA_MASK_START + ca[0].class_id);
        expect_value(__wrap_msr_write, value, ca[0].u.ways_mask);
        will_return(__wrap_msr_write, PQOS_RETVAL_OK);
        expect_value(__wrap_msr_write, lcore, 0);
        expect_value(__wrap_msr_write, reg,
                     PQOS_MSR_L2CA_MASK_START + ca[1].class_id);
        expect_value(__wrap_msr_write, value, ca[1].u.ways_mask);
        will_return(__wrap_msr_write, PQOS_RETVAL_OK);

        ret = hw_l2ca_set(0, 2, ca);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_hw_l2ca_set_cdp_on(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        struct pqos_l2ca ca[2];

        data->cap_l2ca.cdp = 1;
        data->cap_l2ca.cdp_on = 1;

        will_return(__wrap__pqos_cap_get, data->cap);
        will_return(__wrap__pqos_cap_get, data->cpu);

        ca[0].class_id = 0;
        ca[0].cdp = 0;
        ca[0].u.ways_mask = 0xf;
        ca[1].class_id = 1;
        ca[1].cdp = 1;
        ca[1].u.s.data_mask = 0xf0;
        ca[1].u.s.code_mask = 0xff;

        expect_value(__wrap_msr_write, lcore, 0);
        expect_value(__wrap_msr_write, reg,
                     PQOS_MSR_L2CA_MASK_START + ca[0].class_id * 2);
        expect_value(__wrap_msr_write, value, ca[0].u.ways_mask);
        will_return(__wrap_msr_write, PQOS_RETVAL_OK);
        expect_value(__wrap_msr_write, lcore, 0);
        expect_value(__wrap_msr_write, reg,
                     PQOS_MSR_L2CA_MASK_START + ca[0].class_id * 2 + 1);
        expect_value(__wrap_msr_write, value, ca[0].u.ways_mask);
        will_return(__wrap_msr_write, PQOS_RETVAL_OK);

        expect_value(__wrap_msr_write, lcore, 0);
        expect_value(__wrap_msr_write, reg,
                     PQOS_MSR_L2CA_MASK_START + ca[1].class_id * 2);
        expect_value(__wrap_msr_write, value, ca[1].u.s.data_mask);
        will_return(__wrap_msr_write, PQOS_RETVAL_OK);
        expect_value(__wrap_msr_write, lcore, 0);
        expect_value(__wrap_msr_write, reg,
                     PQOS_MSR_L2CA_MASK_START + ca[1].class_id * 2 + 1);
        expect_value(__wrap_msr_write, value, ca[1].u.s.code_mask);
        will_return(__wrap_msr_write, PQOS_RETVAL_OK);

        ret = hw_l2ca_set(0, 2, ca);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_hw_l2ca_set_cdp_off(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        struct pqos_l2ca ca[1];

        data->cap_l2ca.cdp = 1;
        data->cap_l2ca.cdp_on = 0;

        will_return(__wrap__pqos_cap_get, data->cap);
        will_return(__wrap__pqos_cap_get, data->cpu);

        ca[0].class_id = 0;
        ca[0].cdp = 1;
        ca[0].u.s.data_mask = 0xf0;
        ca[0].u.s.code_mask = 0xff;

        ret = hw_l2ca_set(0, 1, ca);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
}

static void
test_hw_l2ca_set_unsupported(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        struct pqos_l2ca ca[1];

        data->cap_l2ca.cdp_on = 0;

        will_return(__wrap__pqos_cap_get, data->cap);
        will_return(__wrap__pqos_cap_get, data->cpu);

        ca[0].class_id = 0;
        ca[0].cdp = 0;
        ca[0].u.ways_mask = 0xf;

        ret = hw_l2ca_set(0, 1, ca);
        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);
}

/* ======== hw_l2ca_get ======== */

static void
test_hw_l2ca_get(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        unsigned l2cat_id = 0;
        unsigned max_num_ca = data->cap_l2ca.num_classes;
        unsigned num_ca;
        struct pqos_l2ca ca[4];
        unsigned class_id;

        data->cap_l2ca.cdp_on = 0;

        will_return(__wrap__pqos_cap_get, data->cap);
        will_return(__wrap__pqos_cap_get, data->cpu);

        for (class_id = 0; class_id < data->cap_l2ca.num_classes; ++class_id) {
                expect_value(__wrap_msr_read, lcore, 0);
                expect_value(__wrap_msr_read, reg,
                             PQOS_MSR_L2CA_MASK_START + class_id);
                will_return(__wrap_msr_read, 0x1 << class_id);
                will_return(__wrap_msr_read, PQOS_RETVAL_OK);
        }

        ret = hw_l2ca_get(l2cat_id, max_num_ca, &num_ca, ca);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(num_ca, data->cap_l2ca.num_classes);

        for (class_id = 0; class_id < data->cap_l2ca.num_classes; ++class_id) {
                assert_int_equal(ca[class_id].class_id, class_id);
                assert_int_equal(ca[class_id].cdp, 0);
                assert_int_equal(ca[class_id].u.ways_mask, 0x1 << class_id);
        }
}

static void
test_hw_l2ca_get_cdp(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        unsigned l2cat_id = 0;
        unsigned max_num_ca = 2;
        unsigned num_ca;
        struct pqos_l2ca ca[2];
        unsigned i;

        data->cap_l2ca.num_classes = 2;
        data->cap_l2ca.cdp = 1;
        data->cap_l2ca.cdp_on = 1;

        will_return(__wrap__pqos_cap_get, data->cap);
        will_return(__wrap__pqos_cap_get, data->cpu);

        for (i = 0; i < data->cap_l2ca.num_classes * 2; ++i) {
                expect_value(__wrap_msr_read, lcore, 0);
                expect_value(__wrap_msr_read, reg,
                             PQOS_MSR_L2CA_MASK_START + i);
                will_return(__wrap_msr_read, 0x1 << i);
                will_return(__wrap_msr_read, PQOS_RETVAL_OK);
        }

        ret = hw_l2ca_get(l2cat_id, max_num_ca, &num_ca, ca);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(num_ca, data->cap_l2ca.num_classes);

        for (i = 0; i < data->cap_l2ca.num_classes; ++i) {
                assert_int_equal(ca[i].class_id, i);
                assert_int_equal(ca[i].cdp, 1);
                assert_int_equal(ca[i].u.s.data_mask, 0x1 << (i * 2));
                assert_int_equal(ca[i].u.s.code_mask, 0x1 << (i * 2 + 1));
        }
}

static void
test_hw_l2ca_get_unsupported(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        unsigned l2cat_id = 0;
        unsigned max_num_ca = data->cap_l2ca.num_classes;
        unsigned num_ca;
        struct pqos_l2ca ca[4];

        data->cap_l2ca.cdp_on = 0;

        will_return(__wrap__pqos_cap_get, data->cap);
        will_return(__wrap__pqos_cap_get, data->cpu);

        ca[0].class_id = 0;
        ca[0].cdp = 0;
        ca[0].u.ways_mask = 0xf;

        ret = hw_l2ca_get(l2cat_id, max_num_ca, &num_ca, ca);
        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);
}

static void
test_hw_l2ca_get_param(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        unsigned l2cat_id = 0;
        unsigned max_num_ca = 1;
        unsigned num_ca;
        struct pqos_l2ca ca[4];

        data->cap_l2ca.cdp_on = 0;

        will_return(__wrap__pqos_cap_get, data->cap);
        will_return(__wrap__pqos_cap_get, data->cpu);

        ret = hw_l2ca_get(l2cat_id, max_num_ca, &num_ca, ca);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
}

/* =======+ hw_mba_set ======== */

static void
test_hw_mba_set(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        struct pqos_mba requested[3];

        will_return(__wrap__pqos_cap_get, data->cap);
        will_return(__wrap__pqos_cap_get, data->cpu);

        requested[0].class_id = 0;
        requested[0].mb_max = 50;
        requested[0].ctrl = 0;
        requested[1].class_id = 1;
        requested[1].mb_max = 44;
        requested[1].ctrl = 0;
        requested[2].class_id = 2;
        requested[2].mb_max = 26;
        requested[2].ctrl = 0;

        expect_value(__wrap_msr_write, lcore, 0);
        expect_value(__wrap_msr_write, reg, PQOS_MSR_MBA_MASK_START);
        expect_value(__wrap_msr_write, value, 50);
        will_return(__wrap_msr_write, PQOS_RETVAL_OK);
        expect_value(__wrap_msr_write, lcore, 0);
        expect_value(__wrap_msr_write, reg, PQOS_MSR_MBA_MASK_START + 1);
        expect_value(__wrap_msr_write, value, 60);
        will_return(__wrap_msr_write, PQOS_RETVAL_OK);
        expect_value(__wrap_msr_write, lcore, 0);
        expect_value(__wrap_msr_write, reg, PQOS_MSR_MBA_MASK_START + 2);
        expect_value(__wrap_msr_write, value, 70);
        will_return(__wrap_msr_write, PQOS_RETVAL_OK);

        ret = hw_mba_set(0, 3, requested, NULL);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_hw_mba_set_actual(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        struct pqos_mba requested[1];
        struct pqos_mba actual[1];

        will_return(__wrap__pqos_cap_get, data->cap);
        will_return(__wrap__pqos_cap_get, data->cpu);

        requested[0].class_id = 1;
        requested[0].mb_max = 44;
        requested[0].ctrl = 0;

        expect_value(__wrap_msr_write, lcore, 0);
        expect_value(__wrap_msr_write, reg, PQOS_MSR_MBA_MASK_START + 1);
        expect_value(__wrap_msr_write, value, 60);
        will_return(__wrap_msr_write, PQOS_RETVAL_OK);
        expect_value(__wrap_msr_read, lcore, 0);
        expect_value(__wrap_msr_read, reg, PQOS_MSR_MBA_MASK_START + 1);
        will_return(__wrap_msr_read, 60);
        will_return(__wrap_msr_read, PQOS_RETVAL_OK);

        ret = hw_mba_set(0, 1, requested, actual);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(actual[0].class_id, 1);
        assert_int_equal(actual[0].mb_max, 40);
        assert_int_equal(actual[0].ctrl, 0);
}

static void
test_hw_mba_set_unsupported(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        struct pqos_mba requested[1];

        will_return(__wrap__pqos_cap_get, data->cap);
        will_return(__wrap__pqos_cap_get, data->cpu);

        requested[0].class_id = 1;
        requested[0].mb_max = 44;
        requested[0].ctrl = 0;

        ret = hw_mba_set(0, 3, requested, NULL);
        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);
}

static void
test_hw_mba_set_non_linear(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        struct pqos_mba requested[1];

        data->cap_mba.is_linear = 0;

        will_return(__wrap__pqos_cap_get, data->cap);
        will_return(__wrap__pqos_cap_get, data->cpu);

        requested[0].class_id = 0;
        requested[0].mb_max = 50;
        requested[0].ctrl = 0;

        ret = hw_mba_set(0, 1, requested, NULL);
        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);
}

static void
test_hw_mba_set_param(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        struct pqos_mba requested[1];

        data->cap_mba.is_linear = 1;

        will_return(__wrap__pqos_cap_get, data->cap);
        will_return(__wrap__pqos_cap_get, data->cpu);

        requested[0].class_id = 100;
        requested[0].mb_max = 50;
        requested[0].ctrl = 0;

        ret = hw_mba_set(0, 1, requested, NULL);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);

        will_return(__wrap__pqos_cap_get, data->cap);
        will_return(__wrap__pqos_cap_get, data->cpu);

        requested[0].class_id = 100;
        requested[0].mb_max = 50000;
        requested[0].ctrl = 1;

        ret = hw_mba_set(0, 1, requested, NULL);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
}

/* ======== hw_mba_get ======== */

static void
test_hw_mba_get(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        unsigned mba_id = 0;
        unsigned max_num_cos = data->cap_mba.num_classes;
        unsigned num_cos;
        struct pqos_mba mba_tab[4];
        unsigned class_id;

        will_return(__wrap__pqos_cap_get, data->cap);
        will_return(__wrap__pqos_cap_get, data->cpu);

        for (class_id = 0; class_id < data->cap_mba.num_classes; ++class_id) {
                expect_value(__wrap_msr_read, lcore, 0);
                expect_value(__wrap_msr_read, reg,
                             PQOS_MSR_MBA_MASK_START + class_id);
                will_return(__wrap_msr_read, class_id * 10);
                will_return(__wrap_msr_read, PQOS_RETVAL_OK);
        }

        ret = hw_mba_get(mba_id, max_num_cos, &num_cos, mba_tab);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(num_cos, data->cap_mba.num_classes);

        for (class_id = 0; class_id < data->cap_mba.num_classes; ++class_id) {
                assert_int_equal(mba_tab[class_id].class_id, class_id);
                assert_int_equal(mba_tab[class_id].ctrl, 0);
                assert_int_equal(mba_tab[class_id].mb_max, 100 - class_id * 10);
        }
}

/* ======== hw_alloc_assoc_set ======== */

static void
test_hw_alloc_assoc_set(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        unsigned class_id = 1;
        unsigned lcore = 2;

        will_return(__wrap__pqos_cap_get, data->cap);
        will_return(__wrap__pqos_cap_get, data->cpu);

        expect_value(hw_alloc_assoc_write, lcore, lcore);
        expect_value(hw_alloc_assoc_write, class_id, class_id);
        will_return(hw_alloc_assoc_write, PQOS_RETVAL_OK);

        ret = hw_alloc_assoc_set(lcore, class_id);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_hw_alloc_assoc_set_param(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        unsigned class_id = 1;
        unsigned lcore = 2;

        will_return(__wrap__pqos_cap_get, data->cap);
        will_return(__wrap__pqos_cap_get, data->cpu);

        ret = hw_alloc_assoc_set(1000, class_id);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);

        will_return(__wrap__pqos_cap_get, data->cap);
        will_return(__wrap__pqos_cap_get, data->cpu);

        ret = hw_alloc_assoc_set(lcore, 100);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
}

static void
test_hw_alloc_assoc_set_unsupported(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        unsigned class_id = 1;
        unsigned lcore = 2;

        will_return(__wrap__pqos_cap_get, data->cap);
        will_return(__wrap__pqos_cap_get, data->cpu);

        ret = hw_alloc_assoc_set(lcore, class_id);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
}

/* ======== hw_alloc_assoc_get ======== */

static void
test_hw_alloc_assoc_get(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        unsigned class_id = 1;
        unsigned lcore = 2;

        will_return(__wrap__pqos_cap_get, data->cap);
        will_return(__wrap__pqos_cap_get, data->cpu);

        expect_value(hw_alloc_assoc_read, lcore, lcore);
        will_return(hw_alloc_assoc_read, 2);
        will_return(hw_alloc_assoc_read, PQOS_RETVAL_OK);

        ret = hw_alloc_assoc_get(lcore, &class_id);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(class_id, 2);
}

static void
test_hw_alloc_assoc_get_param(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        unsigned class_id = 1;
        unsigned lcore = 200;

        will_return(__wrap__pqos_cap_get, data->cap);
        will_return(__wrap__pqos_cap_get, data->cpu);

        ret = hw_alloc_assoc_get(lcore, &class_id);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
}

static void
test_hw_alloc_assoc_get_unsupported(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        unsigned class_id = 1;
        unsigned lcore = 2;

        will_return(__wrap__pqos_cap_get, data->cap);
        will_return(__wrap__pqos_cap_get, data->cpu);

        ret = hw_alloc_assoc_get(lcore, &class_id);
        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);
}

/* ======== hw_alloc_release ======== */

static void
test_hw_alloc_release(void **state __attribute__((unused)))
{
        int ret;
        unsigned core_array[] = {0, 1};

        expect_value(hw_alloc_assoc_write, lcore, 0);
        expect_value(hw_alloc_assoc_write, class_id, 0);
        will_return(hw_alloc_assoc_write, PQOS_RETVAL_OK);
        expect_value(hw_alloc_assoc_write, lcore, 1);
        expect_value(hw_alloc_assoc_write, class_id, 0);
        will_return(hw_alloc_assoc_write, PQOS_RETVAL_OK);

        ret = hw_alloc_release(core_array, 2);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

/* ======== hw_alloc_reset ======== */

static void
test_hw_alloc_reset_unsupported_all(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;

        will_return(__wrap__pqos_cap_get, data->cap);
        will_return(__wrap__pqos_cap_get, data->cpu);
        will_return(__wrap_cpuinfo_get_config, &data->config);

        ret = hw_alloc_reset(PQOS_REQUIRE_CDP_ANY, PQOS_REQUIRE_CDP_ANY,
                             PQOS_MBA_ANY);
        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);
}

static void
test_hw_alloc_reset_unsupported_l3ca(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;

        will_return(__wrap__pqos_cap_get, data->cap);
        will_return(__wrap__pqos_cap_get, data->cpu);
        will_return(__wrap_cpuinfo_get_config, &data->config);

        ret = hw_alloc_reset(PQOS_REQUIRE_CDP_ON, PQOS_REQUIRE_CDP_ANY,
                             PQOS_MBA_ANY);
        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);

        will_return(__wrap__pqos_cap_get, data->cap);
        will_return(__wrap__pqos_cap_get, data->cpu);
        will_return(__wrap_cpuinfo_get_config, &data->config);

        ret = hw_alloc_reset(PQOS_REQUIRE_CDP_OFF, PQOS_REQUIRE_CDP_ANY,
                             PQOS_MBA_ANY);
        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);
}

static void
test_hw_alloc_reset_unsupported_l3cdp(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;

        data->cap_l3ca.cdp = 0;

        will_return(__wrap__pqos_cap_get, data->cap);
        will_return(__wrap__pqos_cap_get, data->cpu);
        will_return(__wrap_cpuinfo_get_config, &data->config);

        ret = hw_alloc_reset(PQOS_REQUIRE_CDP_ON, PQOS_REQUIRE_CDP_ANY,
                             PQOS_MBA_ANY);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
}

static void
test_hw_alloc_reset_unsupported_l2ca(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;

        will_return(__wrap__pqos_cap_get, data->cap);
        will_return(__wrap__pqos_cap_get, data->cpu);
        will_return(__wrap_cpuinfo_get_config, &data->config);

        ret = hw_alloc_reset(PQOS_REQUIRE_CDP_ANY, PQOS_REQUIRE_CDP_ON,
                             PQOS_MBA_ANY);
        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);

        will_return(__wrap__pqos_cap_get, data->cap);
        will_return(__wrap__pqos_cap_get, data->cpu);
        will_return(__wrap_cpuinfo_get_config, &data->config);

        ret = hw_alloc_reset(PQOS_REQUIRE_CDP_ANY, PQOS_REQUIRE_CDP_OFF,
                             PQOS_MBA_ANY);
        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);
}

static void
test_hw_alloc_reset_unsupported_l2cdp(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;

        data->cap_l2ca.cdp = 0;

        will_return(__wrap__pqos_cap_get, data->cap);
        will_return(__wrap__pqos_cap_get, data->cpu);
        will_return(__wrap_cpuinfo_get_config, &data->config);

        ret = hw_alloc_reset(PQOS_REQUIRE_CDP_ANY, PQOS_REQUIRE_CDP_ON,
                             PQOS_MBA_ANY);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
}

static void
test_hw_alloc_reset_unsupported_mba(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;

        will_return(__wrap__pqos_cap_get, data->cap);
        will_return(__wrap__pqos_cap_get, data->cpu);
        will_return(__wrap_cpuinfo_get_config, &data->config);

        ret = hw_alloc_reset(PQOS_REQUIRE_CDP_ANY, PQOS_REQUIRE_CDP_ANY,
                             PQOS_MBA_DEFAULT);
        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);

        will_return(__wrap__pqos_cap_get, data->cap);
        will_return(__wrap__pqos_cap_get, data->cpu);
        will_return(__wrap_cpuinfo_get_config, &data->config);

        ret = hw_alloc_reset(PQOS_REQUIRE_CDP_ANY, PQOS_REQUIRE_CDP_ANY,
                             PQOS_MBA_CTRL);
        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);
}

static void
test_hw_alloc_reset_unsupported_mba_ctrl(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;

        data->cap_mba.ctrl = 0;

        will_return(__wrap__pqos_cap_get, data->cap);
        will_return(__wrap__pqos_cap_get, data->cpu);
        will_return(__wrap_cpuinfo_get_config, &data->config);

        ret = hw_alloc_reset(PQOS_REQUIRE_CDP_ANY, PQOS_REQUIRE_CDP_ANY,
                             PQOS_MBA_CTRL);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
}

static void
test_hw_alloc_reset_l3ca(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        unsigned *l3cat_ids = NULL;
        unsigned l3cat_id_num = 0;
        unsigned i;

        data->cap_l3ca.cdp = 0;
        l3cat_ids = pqos_cpu_get_l3cat_ids(data->cpu, &l3cat_id_num);

        will_return(__wrap__pqos_cap_get, data->cap);
        will_return(__wrap__pqos_cap_get, data->cpu);
        will_return(__wrap_cpuinfo_get_config, &data->config);

        for (i = 0; i < l3cat_id_num; ++i) {
                unsigned coreid;
                uint64_t msr_start = PQOS_MSR_L3CA_MASK_START;
                unsigned msr_num = data->cap_l3ca.num_classes;
                uint64_t msr_val = (1 << data->cap_l3ca.num_ways) - 1ULL;

                ret = pqos_cpu_get_one_by_l3cat_id(data->cpu, l3cat_ids[i],
                                                   &coreid);
                assert_int_equal(ret, PQOS_RETVAL_OK);

                expect_value(hw_alloc_reset_cos, msr_start, msr_start);
                expect_value(hw_alloc_reset_cos, msr_num, msr_num);
                expect_value(hw_alloc_reset_cos, coreid, coreid);
                expect_value(hw_alloc_reset_cos, msr_val, msr_val);
                will_return(hw_alloc_reset_cos, PQOS_RETVAL_OK);
        }

        will_return(hw_alloc_reset_assoc, PQOS_RETVAL_OK);

        expect_value(__wrap__pqos_cap_l3cdp_change, cdp, PQOS_REQUIRE_CDP_ANY);

        ret = hw_alloc_reset(PQOS_REQUIRE_CDP_ANY, PQOS_REQUIRE_CDP_ANY,
                             PQOS_MBA_ANY);
        assert_int_equal(ret, PQOS_RETVAL_OK);

        if (l3cat_ids != NULL)
                free(l3cat_ids);
}

static void
test_hw_alloc_reset_l3cdp_enable(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        unsigned *l3cat_ids = NULL;
        unsigned l3cat_id_num = 0;
        unsigned i;

        data->cap_l3ca.cdp = 1;
        data->cap_l3ca.cdp_on = 0;
        l3cat_ids = pqos_cpu_get_l3cat_ids(data->cpu, &l3cat_id_num);

        will_return(__wrap__pqos_cap_get, data->cap);
        will_return(__wrap__pqos_cap_get, data->cpu);
        will_return(__wrap_cpuinfo_get_config, &data->config);

        for (i = 0; i < l3cat_id_num; ++i) {
                unsigned coreid;
                uint64_t msr_start = PQOS_MSR_L3CA_MASK_START;
                unsigned msr_num = data->cap_l3ca.num_classes;
                uint64_t msr_val = (1 << data->cap_l3ca.num_ways) - 1ULL;

                ret = pqos_cpu_get_one_by_l3cat_id(data->cpu, l3cat_ids[i],
                                                   &coreid);
                assert_int_equal(ret, PQOS_RETVAL_OK);

                expect_value(hw_alloc_reset_cos, msr_start, msr_start);
                expect_value(hw_alloc_reset_cos, msr_num, msr_num);
                expect_value(hw_alloc_reset_cos, coreid, coreid);
                expect_value(hw_alloc_reset_cos, msr_val, msr_val);
                will_return(hw_alloc_reset_cos, PQOS_RETVAL_OK);
        }

        will_return(hw_alloc_reset_assoc, PQOS_RETVAL_OK);

        expect_value(hw_alloc_reset_l3cdp, l3cat_id_num, l3cat_id_num);
        expect_value(hw_alloc_reset_l3cdp, enable, 1);
        will_return(hw_alloc_reset_l3cdp, PQOS_RETVAL_OK);

        expect_value(__wrap__pqos_cap_l3cdp_change, cdp, PQOS_REQUIRE_CDP_ON);

        ret = hw_alloc_reset(PQOS_REQUIRE_CDP_ON, PQOS_REQUIRE_CDP_ANY,
                             PQOS_MBA_ANY);
        assert_int_equal(ret, PQOS_RETVAL_OK);

        if (l3cat_ids != NULL)
                free(l3cat_ids);
}

static void
test_hw_alloc_reset_l3cdp_disable(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        unsigned *l3cat_ids = NULL;
        unsigned l3cat_id_num = 0;
        unsigned i;

        data->cap_l3ca.cdp = 1;
        data->cap_l3ca.cdp_on = 1;
        l3cat_ids = pqos_cpu_get_l3cat_ids(data->cpu, &l3cat_id_num);

        will_return(__wrap__pqos_cap_get, data->cap);
        will_return(__wrap__pqos_cap_get, data->cpu);
        will_return(__wrap_cpuinfo_get_config, &data->config);

        for (i = 0; i < l3cat_id_num; ++i) {
                unsigned coreid;
                uint64_t msr_start = PQOS_MSR_L3CA_MASK_START;
                unsigned msr_num = data->cap_l3ca.num_classes * 2;
                uint64_t msr_val = (1 << data->cap_l3ca.num_ways) - 1ULL;

                ret = pqos_cpu_get_one_by_l3cat_id(data->cpu, l3cat_ids[i],
                                                   &coreid);
                assert_int_equal(ret, PQOS_RETVAL_OK);

                expect_value(hw_alloc_reset_cos, msr_start, msr_start);
                expect_value(hw_alloc_reset_cos, msr_num, msr_num);
                expect_value(hw_alloc_reset_cos, coreid, coreid);
                expect_value(hw_alloc_reset_cos, msr_val, msr_val);
                will_return(hw_alloc_reset_cos, PQOS_RETVAL_OK);
        }

        will_return(hw_alloc_reset_assoc, PQOS_RETVAL_OK);

        expect_value(hw_alloc_reset_l3cdp, l3cat_id_num, l3cat_id_num);
        expect_value(hw_alloc_reset_l3cdp, enable, 0);
        will_return(hw_alloc_reset_l3cdp, PQOS_RETVAL_OK);

        expect_value(__wrap__pqos_cap_l3cdp_change, cdp, PQOS_REQUIRE_CDP_OFF);

        ret = hw_alloc_reset(PQOS_REQUIRE_CDP_OFF, PQOS_REQUIRE_CDP_ANY,
                             PQOS_MBA_ANY);
        assert_int_equal(ret, PQOS_RETVAL_OK);

        if (l3cat_ids != NULL)
                free(l3cat_ids);
}

static void
test_hw_alloc_reset_l2ca(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        unsigned *l2ids = NULL;
        unsigned l2id_num = 0;
        unsigned i;

        data->cap_l2ca.cdp = 0;
        l2ids = pqos_cpu_get_l2ids(data->cpu, &l2id_num);

        will_return(__wrap__pqos_cap_get, data->cap);
        will_return(__wrap__pqos_cap_get, data->cpu);
        will_return(__wrap_cpuinfo_get_config, &data->config);

        for (i = 0; i < l2id_num; ++i) {
                unsigned coreid;
                uint64_t msr_start = PQOS_MSR_L2CA_MASK_START;
                unsigned msr_num = data->cap_l2ca.num_classes;
                uint64_t msr_val = (1 << data->cap_l2ca.num_ways) - 1ULL;

                ret = pqos_cpu_get_one_by_l2id(data->cpu, l2ids[i], &coreid);
                assert_int_equal(ret, PQOS_RETVAL_OK);

                expect_value(hw_alloc_reset_cos, msr_start, msr_start);
                expect_value(hw_alloc_reset_cos, msr_num, msr_num);
                expect_value(hw_alloc_reset_cos, coreid, coreid);
                expect_value(hw_alloc_reset_cos, msr_val, msr_val);
                will_return(hw_alloc_reset_cos, PQOS_RETVAL_OK);
        }

        will_return(hw_alloc_reset_assoc, PQOS_RETVAL_OK);

        expect_value(__wrap__pqos_cap_l2cdp_change, cdp, PQOS_REQUIRE_CDP_ANY);

        ret = hw_alloc_reset(PQOS_REQUIRE_CDP_ANY, PQOS_REQUIRE_CDP_ANY,
                             PQOS_MBA_ANY);
        assert_int_equal(ret, PQOS_RETVAL_OK);

        if (l2ids != NULL)
                free(l2ids);
}

static void
test_hw_alloc_reset_l2cdp_enable(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        unsigned *l2ids = NULL;
        unsigned l2id_num = 0;
        unsigned i;

        data->cap_l2ca.cdp = 1;
        data->cap_l2ca.cdp_on = 0;
        l2ids = pqos_cpu_get_l2ids(data->cpu, &l2id_num);

        will_return(__wrap__pqos_cap_get, data->cap);
        will_return(__wrap__pqos_cap_get, data->cpu);
        will_return(__wrap_cpuinfo_get_config, &data->config);

        for (i = 0; i < l2id_num; ++i) {
                unsigned coreid;
                uint64_t msr_start = PQOS_MSR_L2CA_MASK_START;
                unsigned msr_num = data->cap_l2ca.num_classes;
                uint64_t msr_val = (1 << data->cap_l2ca.num_ways) - 1ULL;

                ret = pqos_cpu_get_one_by_l2id(data->cpu, l2ids[i], &coreid);
                assert_int_equal(ret, PQOS_RETVAL_OK);

                expect_value(hw_alloc_reset_cos, msr_start, msr_start);
                expect_value(hw_alloc_reset_cos, msr_num, msr_num);
                expect_value(hw_alloc_reset_cos, coreid, coreid);
                expect_value(hw_alloc_reset_cos, msr_val, msr_val);
                will_return(hw_alloc_reset_cos, PQOS_RETVAL_OK);
        }

        will_return(hw_alloc_reset_assoc, PQOS_RETVAL_OK);

        expect_value(hw_alloc_reset_l2cdp, l2id_num, l2id_num);
        expect_value(hw_alloc_reset_l2cdp, enable, 1);
        will_return(hw_alloc_reset_l2cdp, PQOS_RETVAL_OK);

        expect_value(__wrap__pqos_cap_l2cdp_change, cdp, PQOS_REQUIRE_CDP_ON);

        ret = hw_alloc_reset(PQOS_REQUIRE_CDP_ANY, PQOS_REQUIRE_CDP_ON,
                             PQOS_MBA_ANY);
        assert_int_equal(ret, PQOS_RETVAL_OK);

        if (l2ids != NULL)
                free(l2ids);
}

static void
test_hw_alloc_reset_l2cdp_disable(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        unsigned *l2ids = NULL;
        unsigned l2id_num = 0;
        unsigned i;

        data->cap_l2ca.cdp = 1;
        data->cap_l2ca.cdp_on = 1;
        l2ids = pqos_cpu_get_l2ids(data->cpu, &l2id_num);

        will_return(__wrap__pqos_cap_get, data->cap);
        will_return(__wrap__pqos_cap_get, data->cpu);
        will_return(__wrap_cpuinfo_get_config, &data->config);

        for (i = 0; i < l2id_num; ++i) {
                unsigned coreid;
                uint64_t msr_start = PQOS_MSR_L2CA_MASK_START;
                unsigned msr_num = data->cap_l2ca.num_classes * 2;
                uint64_t msr_val = (1 << data->cap_l2ca.num_ways) - 1ULL;

                ret = pqos_cpu_get_one_by_l2id(data->cpu, l2ids[i], &coreid);
                assert_int_equal(ret, PQOS_RETVAL_OK);

                expect_value(hw_alloc_reset_cos, msr_start, msr_start);
                expect_value(hw_alloc_reset_cos, msr_num, msr_num);
                expect_value(hw_alloc_reset_cos, coreid, coreid);
                expect_value(hw_alloc_reset_cos, msr_val, msr_val);
                will_return(hw_alloc_reset_cos, PQOS_RETVAL_OK);
        }

        will_return(hw_alloc_reset_assoc, PQOS_RETVAL_OK);

        expect_value(hw_alloc_reset_l2cdp, l2id_num, l2id_num);
        expect_value(hw_alloc_reset_l2cdp, enable, 0);
        will_return(hw_alloc_reset_l2cdp, PQOS_RETVAL_OK);

        expect_value(__wrap__pqos_cap_l2cdp_change, cdp, PQOS_REQUIRE_CDP_OFF);

        ret = hw_alloc_reset(PQOS_REQUIRE_CDP_ANY, PQOS_REQUIRE_CDP_OFF,
                             PQOS_MBA_ANY);
        assert_int_equal(ret, PQOS_RETVAL_OK);

        if (l2ids != NULL)
                free(l2ids);
}

static void
test_hw_alloc_reset_mba(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        unsigned *mba_ids = NULL;
        unsigned mba_id_num = 0;
        unsigned i;

        data->cap_mba.ctrl = 0;
        mba_ids = pqos_cpu_get_mba_ids(data->cpu, &mba_id_num);

        will_return(__wrap__pqos_cap_get, data->cap);
        will_return(__wrap__pqos_cap_get, data->cpu);
        will_return(__wrap_cpuinfo_get_config, &data->config);

        for (i = 0; i < mba_id_num; ++i) {
                unsigned coreid;
                uint64_t msr_start = data->config.mba_msr_reg;
                unsigned msr_num = data->cap_mba.num_classes;
                uint64_t msr_val = data->config.mba_default_val;

                ret =
                    pqos_cpu_get_one_by_mba_id(data->cpu, mba_ids[i], &coreid);
                assert_int_equal(ret, PQOS_RETVAL_OK);

                expect_value(hw_alloc_reset_cos, msr_start, msr_start);
                expect_value(hw_alloc_reset_cos, msr_num, msr_num);
                expect_value(hw_alloc_reset_cos, coreid, coreid);
                expect_value(hw_alloc_reset_cos, msr_val, msr_val);
                will_return(hw_alloc_reset_cos, PQOS_RETVAL_OK);
        }

        will_return(hw_alloc_reset_assoc, PQOS_RETVAL_OK);

        ret = hw_alloc_reset(PQOS_REQUIRE_CDP_ANY, PQOS_REQUIRE_CDP_ANY,
                             PQOS_MBA_ANY);
        assert_int_equal(ret, PQOS_RETVAL_OK);

        if (mba_ids != NULL)
                free(mba_ids);
}

int
main(void)
{
        int result = 0;

        const struct CMUnitTest tests_l3ca[] = {
            cmocka_unit_test(test_hw_l3ca_set),
            cmocka_unit_test(test_hw_l3ca_set_cdp_on),
            cmocka_unit_test(test_hw_l3ca_set_cdp_off),
            cmocka_unit_test(test_hw_l3ca_get),
            cmocka_unit_test(test_hw_l3ca_get_cdp),
            cmocka_unit_test(test_hw_l3ca_get_param),
            cmocka_unit_test(test_hw_alloc_assoc_set),
            cmocka_unit_test(test_hw_alloc_assoc_set_param),
            cmocka_unit_test(test_hw_alloc_assoc_get),
            cmocka_unit_test(test_hw_alloc_assoc_get_param),
            cmocka_unit_test(test_hw_alloc_reset_unsupported_l2ca),
            cmocka_unit_test(test_hw_alloc_reset_unsupported_mba),
            cmocka_unit_test(test_hw_alloc_reset_unsupported_l3cdp),
            cmocka_unit_test(test_hw_alloc_reset_l3ca),
            cmocka_unit_test(test_hw_alloc_reset_l3cdp_enable),
            cmocka_unit_test(test_hw_alloc_reset_l3cdp_disable)};

        const struct CMUnitTest tests_l2ca[] = {
            cmocka_unit_test(test_hw_l2ca_set),
            cmocka_unit_test(test_hw_l2ca_set_cdp_on),
            cmocka_unit_test(test_hw_l2ca_set_cdp_off),
            cmocka_unit_test(test_hw_l2ca_get),
            cmocka_unit_test(test_hw_l2ca_get_cdp),
            cmocka_unit_test(test_hw_l2ca_get_param),
            cmocka_unit_test(test_hw_alloc_assoc_set),
            cmocka_unit_test(test_hw_alloc_assoc_set_param),
            cmocka_unit_test(test_hw_alloc_assoc_get),
            cmocka_unit_test(test_hw_alloc_reset_unsupported_l3ca),
            cmocka_unit_test(test_hw_alloc_reset_unsupported_mba),
            cmocka_unit_test(test_hw_alloc_reset_unsupported_l2cdp),
            cmocka_unit_test(test_hw_alloc_reset_l2ca),
            cmocka_unit_test(test_hw_alloc_reset_l2cdp_enable),
            cmocka_unit_test(test_hw_alloc_reset_l2cdp_disable)};

        const struct CMUnitTest tests_mba[] = {
            cmocka_unit_test(test_hw_alloc_assoc_set),
            cmocka_unit_test(test_hw_alloc_assoc_set_param),
            cmocka_unit_test(test_hw_alloc_assoc_get),
            cmocka_unit_test(test_hw_alloc_reset_unsupported_l3ca),
            cmocka_unit_test(test_hw_alloc_reset_unsupported_l2ca),
            cmocka_unit_test(test_hw_alloc_reset_unsupported_mba_ctrl),
            cmocka_unit_test(test_hw_alloc_reset_mba),
            cmocka_unit_test(test_hw_mba_set),
            cmocka_unit_test(test_hw_mba_set_actual),
            cmocka_unit_test(test_hw_mba_set_non_linear),
            cmocka_unit_test(test_hw_mba_set_param),
            cmocka_unit_test(test_hw_mba_get)};

        const struct CMUnitTest tests_unsupported[] = {
            cmocka_unit_test(test_hw_l3ca_set_unsupported),
            cmocka_unit_test(test_hw_l3ca_get_unsupported),
            cmocka_unit_test(test_hw_l2ca_set_unsupported),
            cmocka_unit_test(test_hw_l2ca_get_unsupported),
            cmocka_unit_test(test_hw_mba_set_unsupported),
            cmocka_unit_test(test_hw_alloc_assoc_set_unsupported),
            cmocka_unit_test(test_hw_alloc_assoc_get_unsupported),
            cmocka_unit_test(test_hw_alloc_release),
            cmocka_unit_test(test_hw_alloc_reset_unsupported_all)};

        result += cmocka_run_group_tests(tests_l3ca, test_init_l3ca, test_fini);
        result += cmocka_run_group_tests(tests_l2ca, test_init_l2ca, test_fini);
        result += cmocka_run_group_tests(tests_mba, test_init_mba, test_fini);
        result += cmocka_run_group_tests(tests_unsupported,
                                         test_init_unsupported, test_fini);

        return result;
}
