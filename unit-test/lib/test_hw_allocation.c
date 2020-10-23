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

#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include "allocation.h"
#include "mock_cap.h"
#include "mock_machine.h"

struct pqos_data {
        struct pqos_cpuinfo *cpu;
        struct pqos_cap *cap;
        struct pqos_cap_l3ca cap_l3ca;
};

static int
cpuinfo_init(struct pqos_data *data)
{
        unsigned i;
        unsigned cores = 8;
        struct pqos_cpuinfo *cpu;

        cpu = malloc(sizeof(*cpu) + cores * sizeof(struct pqos_coreinfo));
        if (cpu == NULL)
                return -1;

        cpu->num_cores = cores;
        cpu->vendor = PQOS_VENDOR_INTEL;
        for (i = 0; i < cores; ++i) {
                struct pqos_coreinfo *coreinfo = &cpu->cores[i];

                coreinfo->lcore = i;
                coreinfo->socket = i > cores / 2 ? 1 : 0;
                coreinfo->l3_id = coreinfo->socket;
                coreinfo->l2_id = i / 2;
                coreinfo->l3cat_id = coreinfo->socket;
                coreinfo->mba_id = coreinfo->socket;
        }

        /* L3 cache info */
        cpu->l3.detected = 1;
        cpu->l3.num_ways = 16;

        /* L2 cache info */
        cpu->l3.detected = 1;
        cpu->l3.num_ways = 12;

        data->cpu = cpu;

        return 0;
}

static int
cap_init(struct pqos_data *data, unsigned technology)
{
        struct pqos_cap *cap;
        unsigned cap_num = 0;

        cap = malloc(sizeof(*cap) +
                     PQOS_CAP_TYPE_NUMOF * sizeof(struct pqos_capability));
        if (cap == NULL)
                return -1;

        data->cap_l3ca.num_classes = 4;
        data->cap_l3ca.num_ways = 16;
        data->cap_l3ca.cdp = 0;
        data->cap_l3ca.cdp_on = 0;

        if (technology & (1 << PQOS_CAP_TYPE_L3CA)) {
                cap->capabilities[cap_num].type = PQOS_CAP_TYPE_L3CA;
                cap->capabilities[cap_num].u.l3ca = &data->cap_l3ca;
                ++cap_num;
        }

        cap->num_cap = cap_num;

        data->cap = cap;

        return 0;
}

static int
test_init(void **state, unsigned technology)
{
        int ret;
        struct pqos_data *data;

        data = calloc(1, sizeof(struct pqos_data));
        if (data == NULL)
                return -1;

        ret = cpuinfo_init(data);
        if (ret != 0)
                return ret;

        ret = cap_init(data, technology);
        if (ret != 0)
                return ret;

        *state = data;

        return ret;
}

static int
test_init_l3ca(void **state)
{
        return test_init(state, 1 << PQOS_CAP_TYPE_L3CA);
}

static int
test_init_unsupported(void **state)
{
        return test_init(state, 0);
}

static int
test_fini(void **state)
{
        struct pqos_data *data = (struct pqos_data *)*state;

        if (data != NULL) {
                if (data->cpu != NULL)
                        free(data->cpu);
                if (data->cap != NULL)
                        free(data->cap);
                free(data);
        }

        return 0;
}

/* ======== hw_l3ca_set ======== */

static void
test_hw_alloc_set(void **state)
{
        struct pqos_data *data = (struct pqos_data *)*state;
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
        expect_value(__wrap_msr_write, reg, 0xC90 + ca[0].class_id);
        expect_value(__wrap_msr_write, value, ca[0].u.ways_mask);
        will_return(__wrap_msr_write, PQOS_RETVAL_OK);
        expect_value(__wrap_msr_write, lcore, 0);
        expect_value(__wrap_msr_write, reg, 0xC90 + ca[1].class_id);
        expect_value(__wrap_msr_write, value, ca[1].u.ways_mask);
        will_return(__wrap_msr_write, PQOS_RETVAL_OK);

        ret = hw_l3ca_set(0, 2, ca);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_hw_alloc_set_cdp_on(void **state)
{
        struct pqos_data *data = (struct pqos_data *)*state;
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
        expect_value(__wrap_msr_write, reg, 0xC90 + ca[0].class_id * 2);
        expect_value(__wrap_msr_write, value, ca[0].u.ways_mask);
        will_return(__wrap_msr_write, PQOS_RETVAL_OK);
        expect_value(__wrap_msr_write, lcore, 0);
        expect_value(__wrap_msr_write, reg, 0xC90 + ca[0].class_id * 2 + 1);
        expect_value(__wrap_msr_write, value, ca[0].u.ways_mask);
        will_return(__wrap_msr_write, PQOS_RETVAL_OK);

        expect_value(__wrap_msr_write, lcore, 0);
        expect_value(__wrap_msr_write, reg, 0xC90 + ca[1].class_id * 2);
        expect_value(__wrap_msr_write, value, ca[1].u.s.data_mask);
        will_return(__wrap_msr_write, PQOS_RETVAL_OK);
        expect_value(__wrap_msr_write, lcore, 0);
        expect_value(__wrap_msr_write, reg, 0xC90 + ca[1].class_id * 2 + 1);
        expect_value(__wrap_msr_write, value, ca[1].u.s.code_mask);
        will_return(__wrap_msr_write, PQOS_RETVAL_OK);

        ret = hw_l3ca_set(0, 2, ca);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_hw_alloc_set_cdp_off(void **state)
{
        struct pqos_data *data = (struct pqos_data *)*state;
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
test_hw_alloc_set_unsupported(void **state)
{
        struct pqos_data *data = (struct pqos_data *)*state;
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

int
main(void)
{
        int result = 0;

        const struct CMUnitTest tests_l3ca[] = {
            cmocka_unit_test(test_hw_alloc_set),
            cmocka_unit_test(test_hw_alloc_set_cdp_on),
            cmocka_unit_test(test_hw_alloc_set_cdp_off)};

        const struct CMUnitTest tests_unsupported[] = {
            cmocka_unit_test(test_hw_alloc_set_unsupported)};

        result += cmocka_run_group_tests(tests_l3ca, test_init_l3ca, test_fini);
        result += cmocka_run_group_tests(tests_unsupported,
                                         test_init_unsupported, test_fini);

        return result;
}
