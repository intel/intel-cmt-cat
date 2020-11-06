/*
 * BSD LICENSE
 *
 * Copyright(c) 2014-2020 Intel Corporation. All rights reserved.
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

#include "pqos.h"
#include "cpuinfo.h"
#include "cpu_registers.h"

struct test_data {
        struct pqos_cpuinfo *cpu;
        struct pqos_cap *cap;
        struct cpuinfo_config config;
        struct pqos_cap_l3ca cap_l3ca;
        struct pqos_cap_l2ca cap_l2ca;
        struct pqos_cap_mba cap_mba;
};

static inline int
test_cpuinfo_init(struct test_data *data)
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
                coreinfo->socket = i >= cores / 2 ? 1 : 0;
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

static inline int
test_config_init(struct test_data *data)
{
        struct cpuinfo_config *config = &data->config;

        config->mba_max = PQOS_MBA_LINEAR_MAX;
        config->mba_msr_reg = PQOS_MSR_MBA_MASK_START;
        config->mba_default_val = 0;

        return 0;
}

static inline int
test_cap_init(struct test_data *data, unsigned technology)
{
        struct pqos_cap *cap;
        unsigned cap_num = 0;

        cap = malloc(sizeof(*cap) +
                     PQOS_CAP_TYPE_NUMOF * sizeof(struct pqos_capability));
        if (cap == NULL)
                return -1;

        if (technology & (1 << PQOS_CAP_TYPE_L3CA)) {
                data->cap_l3ca.num_classes = 4;
                data->cap_l3ca.num_ways = 16;
                data->cap_l3ca.cdp = 0;
                data->cap_l3ca.cdp_on = 0;

                cap->capabilities[cap_num].type = PQOS_CAP_TYPE_L3CA;
                cap->capabilities[cap_num].u.l3ca = &data->cap_l3ca;
                ++cap_num;
        }

        if (technology & (1 << PQOS_CAP_TYPE_L2CA)) {
                data->cap_l2ca.num_classes = 3;
                data->cap_l2ca.num_ways = 8;
                data->cap_l2ca.cdp = 0;
                data->cap_l2ca.cdp_on = 0;

                cap->capabilities[cap_num].type = PQOS_CAP_TYPE_L2CA;
                cap->capabilities[cap_num].u.l2ca = &data->cap_l2ca;
                ++cap_num;
        }

        if (technology & (1 << PQOS_CAP_TYPE_MBA)) {
                data->cap_mba.num_classes = 4;

                data->cap_mba.throttle_max = 90;
                data->cap_mba.throttle_step = 10;
                data->cap_mba.is_linear = 1;
                data->cap_mba.ctrl = 1;
                data->cap_mba.ctrl_on = 0;

                cap->capabilities[cap_num].type = PQOS_CAP_TYPE_MBA;
                cap->capabilities[cap_num].u.mba = &data->cap_mba;
                ++cap_num;
        }

        cap->num_cap = cap_num;

        data->cap = cap;

        return 0;
}

static inline int
test_init(void **state, unsigned technology)
{
        int ret;
        struct test_data *data;

        data = calloc(1, sizeof(struct test_data));
        if (data == NULL)
                return -1;

        ret = test_cpuinfo_init(data);
        if (ret != 0)
                return ret;

        ret = test_cap_init(data, technology);
        if (ret != 0)
                return ret;

        ret = test_config_init(data);
        if (ret != 0)
                return ret;

        *state = data;

        return ret;
}

static inline int
test_init_l3ca(void **state)
{
        return test_init(state, 1 << PQOS_CAP_TYPE_L3CA);
}

static inline int
test_init_l2ca(void **state)
{
        return test_init(state, 1 << PQOS_CAP_TYPE_L2CA);
}

static inline int
test_init_mba(void **state)
{
        return test_init(state, 1 << PQOS_CAP_TYPE_MBA);
}

static inline int
test_init_unsupported(void **state)
{
        return test_init(state, 0);
}

static inline int
test_fini(void **state)
{
        struct test_data *data = (struct test_data *)*state;

        if (data != NULL) {
                if (data->cpu != NULL)
                        free(data->cpu);
                if (data->cap != NULL)
                        free(data->cap);
                free(data);
        }

        return 0;
}
