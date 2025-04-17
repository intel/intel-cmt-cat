/*
 * BSD LICENSE
 *
 * Copyright(c) 2014-2023 Intel Corporation. All rights reserved.
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
 *
 */

/**
 * @brief Implementation of MBA related PQoS API
 *
 */

#include "mmio_allocation.h"

#include "cap.h"
#include "log.h"
#include "mmio.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief Populate mem_regions data structure for a given CLOS
 *
 * @param [in]  erdt erdt structure to obtain information for all domains
 * @param [in]  class_id COS to extract MBA information for
 * @param [in]  domain_id domain to extract MBA information for
 * @param [out] num_mem_regions how many mem_regions to populate
 * @param [out] mem_regions mem regions to save MBA information
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 */
static int
_get_regions_mba(const struct pqos_erdt_info *erdt,
                 unsigned class_id,
                 unsigned domain_id,
                 int num_mem_regions,
                 struct pqos_mba_mem_region *mem_regions)
{
        for (int j = 0; j < num_mem_regions - 1; j++) {
                get_mba_optimal_bw_region_clos_v1(
                    (const struct pqos_erdt_marc *)&erdt->cpu_agents[domain_id]
                        .marc,
                    j, class_id,
                    (unsigned *)&mem_regions[j]
                        .bw_ctrl_val[PQOS_BW_CTRL_TYPE_OPT_IDX]);

                get_mba_min_bw_region_clos_v1(
                    (const struct pqos_erdt_marc *)&erdt->cpu_agents[domain_id]
                        .marc,
                    j, class_id,
                    (unsigned *)&mem_regions[j]
                        .bw_ctrl_val[PQOS_BW_CTRL_TYPE_MIN_IDX]);

                get_mba_max_bw_region_clos_v1(
                    (const struct pqos_erdt_marc *)&erdt->cpu_agents[domain_id]
                        .marc,
                    j, class_id,
                    (unsigned *)&mem_regions[j]
                        .bw_ctrl_val[PQOS_BW_CTRL_TYPE_MAX_IDX]);
        }

        return PQOS_RETVAL_OK;
}

int
mmio_alloc_reset_mba(const unsigned mba_ids_num,
                     const unsigned *mba_ids,
                     const int enable)
{
        int ret;
        const struct pqos_erdt_info *erdt = _pqos_get_erdt();

        UNUSED_PARAM(mba_ids_num);
        UNUSED_PARAM(mba_ids);
        UNUSED_PARAM(enable);

        for (unsigned domain = 0; domain < erdt->num_cpu_agents; domain++) {
                for (unsigned i = 0; i < erdt->max_clos; i++) {
                        for (int j = 0; j < PQOS_MAX_MEM_REGIONS; j++) {
                                ret = set_mba_optimal_bw_region_clos_v1(
                                    (const struct pqos_erdt_marc *)&erdt
                                        ->cpu_agents[domain]
                                        .marc,
                                    j, i, MBA_MAX_BW);
                                if (ret != PQOS_RETVAL_OK)
                                        return ret;

                                ret = set_mba_min_bw_region_clos_v1(
                                    (const struct pqos_erdt_marc *)&erdt
                                        ->cpu_agents[domain]
                                        .marc,
                                    j, i, MBA_MAX_BW);
                                if (ret != PQOS_RETVAL_OK)
                                        return ret;

                                ret = set_mba_max_bw_region_clos_v1(
                                    (const struct pqos_erdt_marc *)&erdt
                                        ->cpu_agents[domain]
                                        .marc,
                                    j, i, MBA_MAX_BW);
                                if (ret != PQOS_RETVAL_OK)
                                        return ret;
                        }
                }
        }

        return PQOS_RETVAL_OK;
}

int
mmio_mba_set(const unsigned mba_id,
             const unsigned num_cos,
             const struct pqos_mba *requested,
             struct pqos_mba *actual)
{
        int ret;
        int current_bw;
        const struct pqos_erdt_info *erdt = _pqos_get_erdt();

        ASSERT(actual != NULL);
        ASSERT(num_cos != 0);
        UNUSED_PARAM(mba_id);

        for (unsigned i = 0; i < num_cos; i++) {
                for (int j = 0; j < requested[i].num_mem_regions - 1; j++) {
                        if (requested[i].mem_regions[j].region_num == -1)
                                continue;

                        current_bw =
                            requested[i]
                                .mem_regions[j]
                                .bw_ctrl_val[PQOS_BW_CTRL_TYPE_OPT_IDX];
                        if (current_bw != -1) {
                                ret = set_mba_optimal_bw_region_clos_v1(
                                    (const struct pqos_erdt_marc *)&erdt
                                        ->cpu_agents[requested[i].domain_id]
                                        .marc,
                                    j, requested[i].class_id, current_bw);

                                if (ret != PQOS_RETVAL_OK)
                                        return ret;
                        }

                        current_bw =
                            requested[i]
                                .mem_regions[j]
                                .bw_ctrl_val[PQOS_BW_CTRL_TYPE_MIN_IDX];
                        if (requested[i]
                                .mem_regions[j]
                                .bw_ctrl_val[PQOS_BW_CTRL_TYPE_MIN_IDX] != -1) {
                                ret = set_mba_min_bw_region_clos_v1(
                                    (const struct pqos_erdt_marc *)&erdt
                                        ->cpu_agents[requested[i].domain_id]
                                        .marc,
                                    j, requested[i].class_id, current_bw);

                                if (ret != PQOS_RETVAL_OK)
                                        return ret;
                        }

                        current_bw =
                            requested[i]
                                .mem_regions[j]
                                .bw_ctrl_val[PQOS_BW_CTRL_TYPE_MAX_IDX];
                        if (requested[i]
                                .mem_regions[j]
                                .bw_ctrl_val[PQOS_BW_CTRL_TYPE_MAX_IDX] != -1) {
                                ret = set_mba_max_bw_region_clos_v1(
                                    (const struct pqos_erdt_marc *)&erdt
                                        ->cpu_agents[requested[i].domain_id]
                                        .marc,
                                    j, requested[i].class_id, current_bw);

                                if (ret != PQOS_RETVAL_OK)
                                        return ret;
                        }
                }

                if (actual == NULL)
                        continue;

                actual[i] = requested[i];
                _get_regions_mba((const struct pqos_erdt_info *)&erdt,
                                 actual[i].class_id, actual[i].domain_id,
                                 actual[i].num_mem_regions,
                                 actual[i].mem_regions);
        }

        return PQOS_RETVAL_OK;
}

int
mmio_mba_get(const unsigned mba_id,
             const unsigned max_num_cos,
             unsigned *num_cos,
             struct pqos_mba *mba_tab)
{
        const struct pqos_erdt_info *erdt = _pqos_get_erdt();
        int ret = PQOS_RETVAL_OK;

        ASSERT(num_cos != NULL);
        ASSERT(mba_tab != NULL);
        ASSERT(max_num_cos != 0);
        UNUSED_PARAM(mba_id);

        for (unsigned i = 0; i < max_num_cos; i++) {
                mba_tab[i].ctrl = 0;
                mba_tab[i].class_id = i;
                mba_tab[i].mb_max = 0;

                _get_regions_mba((const struct pqos_erdt_info *)&erdt,
                                 mba_tab[i].class_id, mba_tab[i].domain_id,
                                 mba_tab[i].num_mem_regions,
                                 mba_tab[i].mem_regions);
        }

        *num_cos = max_num_cos;

        return ret;
}
