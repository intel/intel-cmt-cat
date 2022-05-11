/*
 * BSD LICENSE
 *
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
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

#ifndef __CAPS_GEN_H__
#define __CAPS_GEN_H__

#include "pqos.h"

#include <stdlib.h>

struct test_data {
        struct pqos_cpuinfo *cpu_info;
        struct pqos_capability *cap_mon;
        struct pqos_capability *cap_l3ca;
        struct pqos_capability *cap_l2ca;
        struct pqos_capability *cap_mba;
        unsigned num_socket;
};

enum generate_test_caps {
        GENERATE_CAP_MON = 0,
        GENERATE_CAP_L3CA,
        GENERATE_CAP_L2CA,
        GENERATE_CAP_MBA
};

static inline int
init_cpuinfo(struct test_data *data, unsigned num_cores, unsigned num_socket)
{
        if (num_cores % num_socket)
                return -1;
        data->num_socket = num_socket;
        unsigned socket = 0;
        struct pqos_cpuinfo *cpu_info;

        cpu_info = malloc(sizeof(*cpu_info) +
                          num_cores * sizeof(struct pqos_coreinfo));
        if (cpu_info == NULL)
                return -1;
        cpu_info->mem_size = 0;
        cpu_info->vendor = PQOS_VENDOR_INTEL;
        cpu_info->num_cores = num_cores;

        cpu_info->l2.detected = 1;
        cpu_info->l2.num_ways = 20;
        cpu_info->l2.num_sets = 1024;
        cpu_info->l2.num_partitions = 1;
        cpu_info->l2.line_size = 64;
        cpu_info->l2.total_size = 1310720;
        cpu_info->l2.way_size = 65536;

        cpu_info->l3.detected = 1;
        cpu_info->l3.num_ways = 12;
        cpu_info->l3.num_sets = 57344;
        cpu_info->l3.num_partitions = 1;
        cpu_info->l3.line_size = 64;
        cpu_info->l3.total_size = 44040192;
        cpu_info->l3.way_size = 3670016;

        unsigned i = 0;
        for (i = 0; i < num_cores; i++) {
                if ((i != 0) && (i % (num_cores / data->num_socket) == 0))
                        socket++;
                cpu_info->cores[i].lcore = i;
                cpu_info->cores[i].socket = socket;
                cpu_info->cores[i].l3_id = cpu_info->cores[i].socket;
                cpu_info->cores[i].l2_id = i / 2;
                cpu_info->cores[i].l3cat_id = cpu_info->cores[i].socket;
                cpu_info->cores[i].mba_id = cpu_info->cores[i].socket;
        }

        data->cpu_info = cpu_info;

        return 0;
}

static inline int
init_cap_mon(struct test_data *data)
{
        unsigned num_events = 6;
        struct pqos_capability *cap_mon;

        cap_mon = calloc(1, sizeof(struct pqos_capability));
        if (cap_mon == NULL)
                return -1;
        cap_mon->u.mon = malloc(sizeof(struct pqos_cap_mon) +
                                num_events * sizeof(struct pqos_monitor));
        if (cap_mon->u.mon == NULL)
                return -1;

        cap_mon->type = PQOS_CAP_TYPE_MON;
        cap_mon->u.mon->mem_size = 128;
        cap_mon->u.mon->max_rmid = 224;
        cap_mon->u.mon->l3_size = 44040192;
        cap_mon->u.mon->num_events = num_events;

        unsigned i;
        for (i = 0; i < num_events; i++) {
                if (i < 4)
                        cap_mon->u.mon->events[i].max_rmid = 224;
                else
                        cap_mon->u.mon->events[i].max_rmid = 0;
                cap_mon->u.mon->events[i].scale_factor = 1;
                cap_mon->u.mon->events[i].counter_length = 0;
        }

        cap_mon->u.mon->events[0].type = PQOS_MON_EVENT_L3_OCCUP;
        cap_mon->u.mon->events[1].type = PQOS_MON_EVENT_LMEM_BW;
        cap_mon->u.mon->events[2].type = PQOS_MON_EVENT_TMEM_BW;
        cap_mon->u.mon->events[3].type = PQOS_MON_EVENT_RMEM_BW;
        cap_mon->u.mon->events[4].type = PQOS_PERF_EVENT_LLC_MISS;
        cap_mon->u.mon->events[5].type = PQOS_PERF_EVENT_IPC;

        data->cap_mon = cap_mon;
        return 0;
}

static inline int
init_cap_l3ca(struct test_data *data)
{
        struct pqos_capability *cap_l3ca;

        cap_l3ca = calloc(1, sizeof(struct pqos_capability));
        if (cap_l3ca == NULL)
                return -1;
        cap_l3ca->u.l3ca = calloc(1, sizeof(struct pqos_cap_l3ca));
        if (cap_l3ca->u.l3ca == NULL)
                return -1;

        cap_l3ca->type = PQOS_CAP_TYPE_L3CA;
        cap_l3ca->u.l3ca->mem_size = 32;
        cap_l3ca->u.l3ca->num_classes = 6;
        cap_l3ca->u.l3ca->num_ways = 12;
        cap_l3ca->u.l3ca->way_size = 3670016;
        cap_l3ca->u.l3ca->way_contention = 3072;
        cap_l3ca->u.l3ca->cdp = 0;
        cap_l3ca->u.l3ca->cdp_on = 0;

        data->cap_l3ca = cap_l3ca;

        return 0;
}

static inline int
init_cap_l2ca(struct test_data *data)
{
        struct pqos_capability *cap_l2ca;

        cap_l2ca = calloc(1, sizeof(struct pqos_capability));
        if (cap_l2ca == NULL)
                return -1;
        cap_l2ca->u.l2ca = calloc(1, sizeof(struct pqos_cap_l2ca));
        if (cap_l2ca->u.l2ca == NULL)
                return -1;

        cap_l2ca->type = PQOS_CAP_TYPE_L2CA;
        cap_l2ca->u.l2ca->mem_size = 32;
        cap_l2ca->u.l2ca->num_classes = 4;
        cap_l2ca->u.l2ca->num_ways = 12;
        cap_l2ca->u.l2ca->way_size = 3670016;
        cap_l2ca->u.l2ca->way_contention = 3072;
        cap_l2ca->u.l2ca->cdp = 0;
        cap_l2ca->u.l2ca->cdp_on = 0;

        data->cap_l2ca = cap_l2ca;

        return 0;
}

static inline int
init_cap_mba(struct test_data *data)
{
        struct pqos_capability *cap_mba;

        cap_mba = calloc(1, sizeof(struct pqos_capability));
        if (cap_mba == NULL)
                return -1;
        cap_mba->u.mba = calloc(1, sizeof(struct pqos_cap_mba));
        if (cap_mba->u.mba == NULL)
                return -1;

        cap_mba->type = PQOS_CAP_TYPE_MBA;
        cap_mba->u.mba->mem_size = 28;
        cap_mba->u.mba->num_classes = 4;
        cap_mba->u.mba->throttle_max = 90;
        cap_mba->u.mba->throttle_step = 10;
        cap_mba->u.mba->is_linear = 1;
        cap_mba->u.mba->ctrl = -1;
        cap_mba->u.mba->ctrl_on = 0;

        data->cap_mba = cap_mba;

        return 0;
}

static inline int
fini_caps(void **state)
{
        struct test_data *data = (struct test_data *)*state;

        if (data != NULL) {
                if (data->cpu_info != NULL) {
                        free(data->cpu_info);
                        data->cpu_info = NULL;
                }
                if (data->cap_mon != NULL) {
                        if (data->cap_mon->u.mon != NULL)
                                free(data->cap_mon->u.mon);
                        free(data->cap_mon);
                        data->cap_mon = NULL;
                }
                if (data->cap_l3ca != NULL) {
                        if (data->cap_l3ca->u.l3ca != NULL)
                                free(data->cap_l3ca->u.l3ca);
                        free(data->cap_l3ca);
                        data->cap_l3ca = NULL;
                }
                if (data->cap_l2ca != NULL) {
                        if (data->cap_l2ca->u.l2ca != NULL)
                                free(data->cap_l2ca->u.l2ca);
                        free(data->cap_l2ca);
                        data->cap_l2ca = NULL;
                }
                if (data->cap_mba != NULL) {
                        if (data->cap_mba->u.mba != NULL)
                                free(data->cap_mba->u.mba);
                        free(data->cap_mba);
                        data->cap_mba = NULL;
                }
                free(data);
        }
        return 0;
}

static inline int
init_caps(void **state, unsigned data_to_generate)
{
        int ret = 0;
        struct test_data *data;

        data = calloc(1, sizeof(struct test_data));
        if (data == NULL)
                return -1;

        ret = init_cpuinfo(data, 4, 2);
        if (ret != 0)
                return ret;

        if (data_to_generate & (1 << GENERATE_CAP_MON)) {
                ret = init_cap_mon(data);
                if (ret != 0)
                        return ret;
        }

        if (data_to_generate & (1 << GENERATE_CAP_L3CA)) {
                ret = init_cap_l3ca(data);
                if (ret != 0)
                        return ret;
        }

        if (data_to_generate & (1 << GENERATE_CAP_L2CA)) {
                ret = init_cap_l2ca(data);
                if (ret != 0)
                        return ret;
        }

        if (data_to_generate & (1 << GENERATE_CAP_MBA)) {
                ret = init_cap_mba(data);
                if (ret != 0)
                        return ret;
        }

        *state = data;
        return ret;
}

#endif /* __CAPS_GEN_H__ */
