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

#include "allocation.h"
#include "allocation_common.h"
#include "cap.h"
#include "erdt.h"
#include "log.h"
#include "mmio.h"
#include "mmio_common.h"
#include "utils.h"

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
                 uint16_t domain_id,
                 int num_mem_regions,
                 struct pqos_mba_mem_region *mem_regions)
{
        for (int j = 0; j < num_mem_regions; j++) {
                mem_regions[j].region_num = j;
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

/**
 * @brief Returns value of L3 Non-Contiguous CBM(Cache Bit Mask) support
 *
 * @param [in]  domain_id Resource allocation domain's ID
 *
 * @return Operation status
 * @retval struct pqos_erdt_card member non_contiguous_cbm on success
 */
static int
cap_get_mmio_l3ca_non_contiguous(uint16_t domain_id)
{
        const struct pqos_device_agent_info *dev_agent =
            get_dev_agent_by_domain(domain_id);

        if (dev_agent == NULL) {
                LOG_ERROR("domain_id is wrong\n");
                return !ERDT_CAT_NON_CONTIGUOUS_CBM_SUPPORT;
        }

        return dev_agent->card.non_contiguous_cbm;
}

/**
 * @brief Returns value of L3 Zero-length CBM(Cache Bit Mask) support
 *
 * @param [in]  domain_id Resource allocation domain's ID
 *
 * @return Operation status
 * @retval struct pqos_erdt_card member zero_length_bitmask on success
 */
static int
cap_get_mmio_l3ca_zero_length(uint16_t domain_id)
{

        const struct pqos_device_agent_info *dev_agent =
            get_dev_agent_by_domain(domain_id);

        if (dev_agent == NULL) {
                LOG_ERROR("domain_id is wrong\n");
                return !ERDT_CAT_ZERO_LENGTH_CBM_SUPPORT;
        }

        return dev_agent->card.zero_length_bitmask;
}

int
mmio_alloc_reset_mba(void)
{
        int ret;
        const struct pqos_erdt_info *erdt = _pqos_get_erdt();

        ASSERT(erdt != NULL);

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
        ASSERT(erdt != NULL);
        UNUSED_PARAM(mba_id);

        // Check if domain is valid
        if (!get_cpu_agent_by_domain(requested->domain_id))
                return PQOS_RETVAL_PARAM;

        for (unsigned i = 0; i < num_cos; i++) {
                for (int j = 0; j < requested[i].num_mem_regions; j++) {
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
                _get_regions_mba(erdt, actual[i].class_id, actual[i].domain_id,
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
        ASSERT(erdt != NULL);
        UNUSED_PARAM(mba_id);

        // Check if domain is valid
        if (!get_cpu_agent_by_domain(mba_tab->domain_id))
                return PQOS_RETVAL_PARAM;

        for (unsigned i = 0; i < max_num_cos; i++) {
                mba_tab[i].ctrl = 0;
                mba_tab[i].class_id = num_cos[i];
                mba_tab[i].mb_max = 0;

                _get_regions_mba(
                    erdt, mba_tab[i].class_id, mba_tab[i].domain_id,
                    mba_tab[i].num_mem_regions, mba_tab[i].mem_regions);
        }

        *num_cos = max_num_cos;

        return ret;
}

/**
 * =======================================
 * I/O L3 cache allocation
 * =======================================
 */
int
mmio_l3ca_set(const unsigned l3cat_id,
              const unsigned num_ca,
              const struct pqos_l3ca *ca)
{
        int ret = PQOS_RETVAL_OK;
        unsigned i = 0;
        const struct pqos_erdt_info *erdt = _pqos_get_erdt();

        ASSERT(ca != NULL);
        ASSERT(num_ca != 0);
        ASSERT(erdt != NULL);
        UNUSED_PARAM(l3cat_id);

        if (num_ca > erdt->max_clos)
                return PQOS_RETVAL_ERROR;

        // Check if all domains are valid
        for (unsigned i = 0; i < num_ca; i++) {
                if (!get_dev_agent_by_domain(ca[i].domain_id)) {
                        LOG_ERROR("Domain id %u is unavailable\n",
                                  ca[i].domain_id);
                        return PQOS_RETVAL_PARAM;
                }
        }

        for (i = 0; i < num_ca; i++) {
                /* Check L3 CBM is non-contiguous */
                if (!cap_get_mmio_l3ca_non_contiguous(ca[i].domain_id)) {
                        /* Check all COS CBM are contiguous */
                        if (!IS_CONTIGNOUS(ca[i])) {
                                LOG_ERROR("L3 CAT COS%u bit mask is not "
                                          "contiguous!\n",
                                          ca[i].class_id);
                                return PQOS_RETVAL_PARAM;
                        }
                }

                /* Check L3 CBM is zero-length bitmask */
                if (ca[i].u.ways_mask == 0 &&
                    !cap_get_mmio_l3ca_zero_length(ca[i].domain_id)) {
                        LOG_ERROR("L3 CAT COS%u bit mask is 0 and Zero-length "
                                  "bitmask is not supported in Domain id %d.\n",
                                  ca[i].class_id, ca[i].domain_id);
                        return PQOS_RETVAL_PARAM;
                }

                ret = set_iol3_cbm_clos_v1(
                    &get_dev_agent_by_domain(ca[i].domain_id)->card,
                    ca[i].class_id, ca[i].u.ways_mask);
                if (ret != PQOS_RETVAL_OK)
                        return ret;
        }

        return ret;
}

int
mmio_l3ca_get(const unsigned l3cat_id,
              const unsigned max_num_ca,
              unsigned *num_ca,
              struct pqos_l3ca *ca)
{
        int ret = PQOS_RETVAL_OK;
        unsigned i = 0;
        const struct pqos_erdt_info *erdt = _pqos_get_erdt();
        uint64_t value = 0;

        ASSERT(num_ca != NULL);
        ASSERT(ca != NULL);
        ASSERT(max_num_ca != 0);
        ASSERT(erdt != NULL);
        UNUSED_PARAM(l3cat_id);

        if (erdt->max_clos > max_num_ca)
                return PQOS_RETVAL_ERROR;

        // Check if all domains are valid
        for (unsigned i = 0; i < max_num_ca; i++) {
                if (!get_dev_agent_by_domain(ca[i].domain_id)) {
                        LOG_ERROR("Domain id %u is unavailable\n",
                                  ca[i].domain_id);
                        return PQOS_RETVAL_PARAM;
                }
        }

        for (i = 0; i < max_num_ca; i++) {
                ret = get_iol3_cbm_clos_v1(
                    &get_dev_agent_by_domain(ca[i].domain_id)->card, i,
                    REG_BLOCK_SIZE_ZERO, &value);

                if (ret != PQOS_RETVAL_OK)
                        return ret;

                ca[i].cdp = 0;
                ca[i].class_id = i;
                ca[i].u.ways_mask = value;
        }
        *num_ca = max_num_ca;

        return ret;
}

int
mmio_alloc_reset_cat(void)
{
        int ret;
        const struct pqos_erdt_info *erdt = _pqos_get_erdt();

        ASSERT(erdt != NULL);

        for (unsigned domain = 0; domain < erdt->num_dev_agents; domain++) {
                for (unsigned i = 0; i < erdt->max_clos; i++) {
                        /* Reset I/ORDT L3 CAT */
                        ret = set_iol3_cbm_clos_v1(
                            (const struct pqos_erdt_card *)&erdt
                                ->dev_agents[domain]
                                .card,
                            i, (uint64_t)IOL3_CBM_RESET_MASK);
                        if (ret != PQOS_RETVAL_OK)
                                return ret;
                }
        }

        return PQOS_RETVAL_OK;
}

int
mmio_alloc_reset(const struct pqos_alloc_config *cfg)
{
        int ret = PQOS_RETVAL_OK;

        ASSERT(cfg != NULL);

        ret = alloc_reset(cfg);
        if (ret != PQOS_RETVAL_OK) {
                LOG_ERROR("Failed to reset allocation configuration\n");
                return ret;
        }

        /* Reset Region Aware MBA */
        ret = mmio_alloc_reset_mba();
        if (ret != PQOS_RETVAL_OK) {
                LOG_ERROR("Failed to reset MBA configuration\n");
                return ret;
        }

        /* Reset I/O L3 CAT */
        ret = mmio_alloc_reset_cat();
        if (ret != PQOS_RETVAL_OK) {
                LOG_ERROR("Failed to reset L3 CAT configuration\n");
                return ret;
        }

        return PQOS_RETVAL_OK;
}
