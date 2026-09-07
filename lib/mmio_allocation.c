/*
 * BSD LICENSE
 *
 * Copyright(c) 2014-2026 Intel Corporation. All rights reserved.
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

#include <inttypes.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

/* The device domain list of log_domain_without_l3ca(). One log line carries at
 * most AP_BUFFER_SIZE - 1 bytes, 319, and the text around the list takes about
 * 150 of them, so the list is kept well inside what is left. log_printf() hands
 * the length the message needed, rather than the length it wrote, to its
 * callback and to write(), so a message that did not fit would be read past the
 * end of its buffer.
 */
#define DOMAIN_LIST_SIZE 128

/* What ends the list where the domains did not all fit in DOMAIN_LIST_SIZE */
#define DOMAIN_LIST_MORE " and more"

/**
 * @brief Add one domain ID to a list built for a diagnostic
 *
 * The buffer always keeps room for DOMAIN_LIST_MORE behind the last entry that
 * fitted, so a list that ran out of room says so rather than ending as though
 * the platform had no further domains.
 *
 * @param [out]    domains buffer of DOMAIN_LIST_SIZE bytes to build the list in
 * @param [in,out] used how much of the buffer the list occupies
 * @param [in]     domain_id domain to add
 *
 * @return whether another domain can still be added
 */
static int
append_domain(char *domains, size_t *used, uint16_t domain_id)
{
        /* room an entry may take, leaving DOMAIN_LIST_MORE always able to
         * follow the last one that fitted
         */
        const size_t entry_limit = DOMAIN_LIST_SIZE - sizeof(DOMAIN_LIST_MORE);
        int len = snprintf(domains + *used, entry_limit - *used, "%s0x%x",
                           *used > 0 ? ", " : "", domain_id);

        if (len < 0 || (size_t)len >= entry_limit - *used) {
                /* *used is below entry_limit, so the reserved room is there */
                memcpy(domains + *used, DOMAIN_LIST_MORE,
                       sizeof(DOMAIN_LIST_MORE));
                *used += sizeof(DOMAIN_LIST_MORE) - 1;
                return 0;
        }

        *used += (size_t)len;

        return 1;
}

/**
 * @brief Populate a single mem_region data structure for a given CLOS
 *        using the region_num already stored in the mem_region.
 *
 * @param [in]     cpu_agent CPU agent containing the domain's MBA registers
 * @param [in]     class_id CLOS to extract MBA information for
 * @param [in,out] mem_region mem region whose region_num identifies the region
 *                            and whose bw_ctrl_val fields are populated
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 */
static int
_get_region_mba(const struct pqos_cpu_agent_info *cpu_agent,
                unsigned class_id,
                struct pqos_mba_mem_region *mem_region)
{
        int ret;
        int region_num = mem_region->region_num;

        ret = get_mba_optimal_bw_region_clos_v1(
            &cpu_agent->marc, region_num, class_id,
            (unsigned *)&mem_region->bw_ctrl_val[PQOS_BW_CTRL_TYPE_OPT_IDX]);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        ret = get_mba_min_bw_region_clos_v1(
            &cpu_agent->marc, region_num, class_id,
            (unsigned *)&mem_region->bw_ctrl_val[PQOS_BW_CTRL_TYPE_MIN_IDX]);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        ret = get_mba_max_bw_region_clos_v1(
            &cpu_agent->marc, region_num, class_id,
            (unsigned *)&mem_region->bw_ctrl_val[PQOS_BW_CTRL_TYPE_MAX_IDX]);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        return PQOS_RETVAL_OK;
}

/**
 * @brief Populate mem_regions data structure for a given CLOS
 *
 * @param [in]  cpu_agent CPU agent containing the domain's MBA registers
 * @param [in]  class_id CLOS to extract MBA information for
 * @param [out] num_mem_regions how many mem_regions to populate
 * @param [out] mem_regions mem regions to save MBA information
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 */
static int
_get_regions_mba(const struct pqos_cpu_agent_info *cpu_agent,
                 unsigned class_id,
                 int num_mem_regions,
                 struct pqos_mba_mem_region *mem_regions)
{
        int ret;

        for (int j = 0; j < num_mem_regions; j++) {
                mem_regions[j].region_num = j;

                ret = _get_region_mba(cpu_agent, class_id, &mem_regions[j]);
                if (ret != PQOS_RETVAL_OK)
                        return ret;
        }

        return PQOS_RETVAL_OK;
}

static const struct pqos_cpu_agent_info *
get_mmio_cpu_agent_by_domain(const struct pqos_erdt_info *erdt,
                             uint16_t domain_id)
{
        int idx = get_cpu_agent_idx_by_domain_id(erdt, domain_id);

        return idx >= 0 ? &erdt->cpu_agents[idx] : NULL;
}

static const struct pqos_device_agent_info *
get_mmio_dev_agent_by_domain(const struct pqos_erdt_info *erdt,
                             uint16_t domain_id)
{
        int idx = get_dev_agent_idx_by_domain_id(erdt, domain_id);

        return idx >= 0 ? &erdt->dev_agents[idx] : NULL;
}

/**
 * @brief Returns value of L3 Non-Contiguous CBM(Cache Bit Mask) support
 *
 * @param [in] erdt ERDT information
 * @param [in]  domain_id Resource allocation domain's ID
 *
 * @return Operation status
 * @retval struct pqos_erdt_card member non_contiguous_cbm on success
 */
static int
cap_get_mmio_l3ca_non_contiguous(const struct pqos_erdt_info *erdt,
                                 uint16_t domain_id)
{
        const struct pqos_device_agent_info *dev_agent =
            get_mmio_dev_agent_by_domain(erdt, domain_id);

        if (dev_agent == NULL) {
                LOG_ERROR("domain_id is wrong\n");
                return !ERDT_CAT_NON_CONTIGUOUS_CBM_SUPPORT;
        }

        return dev_agent->card.non_contiguous_cbm;
}

/**
 * @brief Returns value of L3 Zero-length CBM(Cache Bit Mask) support
 *
 * @param [in] erdt ERDT information
 * @param [in]  domain_id Resource allocation domain's ID
 *
 * @return Operation status
 * @retval struct pqos_erdt_card member zero_length_bitmask on success
 */
static int
cap_get_mmio_l3ca_zero_length(const struct pqos_erdt_info *erdt,
                              uint16_t domain_id)
{

        const struct pqos_device_agent_info *dev_agent =
            get_mmio_dev_agent_by_domain(erdt, domain_id);

        if (dev_agent == NULL) {
                LOG_ERROR("domain_id is wrong\n");
                return !ERDT_CAT_ZERO_LENGTH_CBM_SUPPORT;
        }

        return dev_agent->card.zero_length_bitmask;
}

/**
 * @brief Whether a CPU agent carries MBA registers
 *
 * The Intel RDT architecture specification requires a MARC sub-structure for
 * every RDT domain that supports Memory Bandwidth Allocation, so a domain
 * without one does not support MBA. A CPU RMDD is valid that way:
 * erdt_populate_rmdd_cpu_agent() requires only CACD, so an agent enumerated
 * without MARC leaves the block zeroed, the way a device agent enumerated for
 * monitoring alone leaves CARD zeroed. The registers MBA programs are in that
 * block, and the three bandwidth control types each have a base address of
 * their own, so all three are needed before any of them can be reached.
 *
 * @param [in] cpu_agent CPU agent to examine
 *
 * @return whether the agent's MARC block was populated
 */
static int
cpu_agent_has_mba(const struct pqos_cpu_agent_info *cpu_agent)
{
        return cpu_agent->marc.reg_block_size != 0 &&
               cpu_agent->marc.opt_bw_reg_block_base_addr != 0 &&
               cpu_agent->marc.min_bw_reg_block_base_addr != 0 &&
               cpu_agent->marc.max_bw_reg_block_base_addr != 0;
}

/**
 * @brief Report a domain that carries no MBA registers
 *
 * On the MMIO interface the MBA registers live in the MARC block of a CPU
 * agent, so a device domain, a domain the platform does not have, and a CPU
 * agent enumerated without MARC cannot carry them. A CPU domain ID and a
 * device domain ID look alike on a command line, which is why the message
 * names the domains that do carry MBA.
 *
 * @param [in] erdt ERDT information
 * @param [in] domain_id domain that was asked for
 */
static void
log_domain_without_mba(const struct pqos_erdt_info *erdt, uint16_t domain_id)
{
        char domains[DOMAIN_LIST_SIZE] = {0};
        size_t used = 0;
        unsigned i;

        for (i = 0; i < erdt->num_cpu_agents; i++) {
                if (!cpu_agent_has_mba(&erdt->cpu_agents[i]))
                        continue;

                if (!append_domain(domains, &used,
                                   erdt->cpu_agents[i].rmdd.domain_id))
                        break;
        }

        LOG_ERROR("Domain ID 0x%x carries no MBA registers. On the MMIO "
                  "interface MBA applies to the CPU domains, %s. Use "
                  "--print-topology to list them\n",
                  domain_id,
                  used > 0 ? domains
                           : "of which this platform "
                             "reports none");
}

int
mmio_alloc_reset_mba(void)
{
        int ret;
        unsigned num_mem_regions;
        const struct pqos_erdt_info *erdt = _pqos_get_erdt();

        ASSERT(erdt != NULL);

        ret = mmio_get_num_mem_regions(&num_mem_regions);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        for (unsigned domain = 0; domain < erdt->num_cpu_agents; domain++) {
                /* An agent enumerated without a MARC block holds no MBA
                 * register to reset. Writing one anyway would map its zeroed
                 * register base, which fails, and take the whole allocation
                 * reset down with it, leaving every other domain unreset.
                 */
                if (!cpu_agent_has_mba(&erdt->cpu_agents[domain]))
                        continue;

                for (unsigned i = 0; i < erdt->max_clos; i++) {
                        for (int j = 0; j < (int)num_mem_regions; j++) {
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

/**
 * @brief Validates one MBA request
 *
 * The MARC bandwidth control fields are 9 bits wide. A wider value would be
 * silently cut down by the register write, so reject it instead.
 *
 * Memory regions are validated against the number of regions the platform
 * advertises through MRRM, not against PQOS_MAX_MEM_REGIONS, so a region the
 * platform does not implement is rejected before any register is written.
 *
 * @param [in] mba requested MBA configuration
 * @param [in] erdt ERDT information
 * @param [in] num_mem_regions number of memory regions supported by the
 *             platform
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 * @retval PQOS_RETVAL_PARAM on an invalid or out of range value
 */
static int
mmio_mba_check_request(const struct pqos_mba *mba,
                       const struct pqos_erdt_info *erdt,
                       const unsigned num_mem_regions)
{
        const struct pqos_cpu_agent_info *cpu_agent =
            get_mmio_cpu_agent_by_domain(erdt, mba->domain_id);

        /* the registers are named before the values are, so that a domain that
         * cannot be programmed at all is reported as such rather than through
         * the failure of a write to a zeroed register base
         */
        if (cpu_agent == NULL || !cpu_agent_has_mba(cpu_agent)) {
                log_domain_without_mba(erdt, mba->domain_id);
                return PQOS_RETVAL_PARAM;
        }

        if (mba->class_id >= erdt->max_clos || mba->num_mem_regions < 0 ||
            (unsigned)mba->num_mem_regions > num_mem_regions)
                return PQOS_RETVAL_PARAM;

        for (int j = 0; j < mba->num_mem_regions; j++) {
                const int region_num = mba->mem_regions[j].region_num;

                /* -1 marks a memory region that is not requested */
                if (region_num == -1)
                        continue;

                if (region_num < 0 || (unsigned)region_num >= num_mem_regions) {
                        LOG_ERROR("MBA CLOS%u region number %d is out of range "
                                  "(0-%u)!\n",
                                  mba->class_id, region_num,
                                  num_mem_regions - 1);
                        return PQOS_RETVAL_PARAM;
                }

                for (int type = 0; type < PQOS_BW_CTRL_TYPE_COUNT; type++) {
                        const int bw = mba->mem_regions[j].bw_ctrl_val[type];

                        /* -1 marks a bandwidth limit that is not requested */
                        if (bw == -1)
                                continue;

                        if (bw < 0) {
                                LOG_ERROR("MBA CLOS%u bandwidth value %d is "
                                          "not a valid bandwidth limit!\n",
                                          mba->class_id, bw);
                                return PQOS_RETVAL_PARAM;
                        }

                        if (bw > MBA_MAX_BW) {
                                LOG_ERROR("MBA CLOS%u bandwidth value %#x is "
                                          "out of range (0x0-%#x)!\n",
                                          mba->class_id, (unsigned)bw,
                                          MBA_MAX_BW);
                                return PQOS_RETVAL_PARAM;
                        }
                }
        }

        return PQOS_RETVAL_OK;
}

int
mmio_mba_set(const unsigned mba_id,
             const unsigned num_clos,
             const struct pqos_mba *requested,
             struct pqos_mba *actual)
{
        int ret;
        int current_bw;
        unsigned num_mem_regions;
        const struct pqos_erdt_info *erdt = _pqos_get_erdt();

        ASSERT(num_clos != 0);
        ASSERT(erdt != NULL);
        UNUSED_PARAM(mba_id);

        ret = mmio_get_num_mem_regions(&num_mem_regions);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        /**
         * Validate every request before touching the registers so that an
         * invalid value does not leave a partially applied configuration
         * behind.
         */
        for (unsigned i = 0; i < num_clos; i++) {
                ret = mmio_mba_check_request(&requested[i], erdt,
                                             num_mem_regions);
                if (ret != PQOS_RETVAL_OK)
                        return ret;
        }

        for (unsigned i = 0; i < num_clos; i++) {
                const struct pqos_cpu_agent_info *cpu_agent =
                    get_mmio_cpu_agent_by_domain(erdt, requested[i].domain_id);

                for (int j = 0; j < requested[i].num_mem_regions; j++) {
                        if (requested[i].mem_regions[j].region_num == -1)
                                continue;

                        current_bw =
                            requested[i]
                                .mem_regions[j]
                                .bw_ctrl_val[PQOS_BW_CTRL_TYPE_OPT_IDX];
                        if (current_bw != -1) {
                                ret = set_mba_optimal_bw_region_clos_v1(
                                    &cpu_agent->marc,
                                    requested[i].mem_regions[j].region_num,
                                    requested[i].class_id, current_bw);

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
                                    &cpu_agent->marc,
                                    requested[i].mem_regions[j].region_num,
                                    requested[i].class_id, current_bw);

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
                                    &cpu_agent->marc,
                                    requested[i].mem_regions[j].region_num,
                                    requested[i].class_id, current_bw);

                                if (ret != PQOS_RETVAL_OK)
                                        return ret;
                        }
                }

                if (actual == NULL)
                        continue;

                actual[i] = requested[i];
                for (int j = 0; j < actual[i].num_mem_regions; j++) {
                        if (actual[i].mem_regions[j].region_num == -1)
                                continue;

                        ret = _get_region_mba(cpu_agent, actual[i].class_id,
                                              &actual[i].mem_regions[j]);
                        if (ret != PQOS_RETVAL_OK)
                                return ret;
                }
        }

        return PQOS_RETVAL_OK;
}

int
mmio_mba_get(const unsigned mba_id,
             const unsigned max_num_clos,
             unsigned *num_clos,
             struct pqos_mba *mba_tab)
{
        const struct pqos_erdt_info *erdt = _pqos_get_erdt();
        const struct pqos_cpu_agent_info *cpu_agent;
        int num_mem_regions;
        unsigned supported;
        int ret = PQOS_RETVAL_OK;

        ASSERT(num_clos != NULL);
        ASSERT(mba_tab != NULL);
        ASSERT(max_num_clos != 0);
        ASSERT(erdt != NULL);
        UNUSED_PARAM(mba_id);

        ret = mmio_get_num_mem_regions(&supported);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        num_mem_regions = mba_tab[0].num_mem_regions;
        if (num_mem_regions == 0)
                num_mem_regions = (int)supported;

        if (erdt->max_clos > max_num_clos)
                return PQOS_RETVAL_ERROR;

        cpu_agent = get_mmio_cpu_agent_by_domain(erdt, mba_tab[0].domain_id);
        if (cpu_agent == NULL || !cpu_agent_has_mba(cpu_agent)) {
                log_domain_without_mba(erdt, mba_tab[0].domain_id);
                return PQOS_RETVAL_PARAM;
        }

        if (num_mem_regions < 0 || (unsigned)num_mem_regions > supported)
                return PQOS_RETVAL_PARAM;

        for (unsigned i = 0; i < erdt->max_clos; i++) {
                mba_tab[i].ctrl = 0;
                mba_tab[i].class_id = i;
                mba_tab[i].mb_max = 0;
                mba_tab[i].domain_id = mba_tab[0].domain_id;
                mba_tab[i].num_mem_regions = num_mem_regions;

                ret = _get_regions_mba(cpu_agent, mba_tab[i].class_id,
                                       num_mem_regions, mba_tab[i].mem_regions);
                if (ret != PQOS_RETVAL_OK)
                        return ret;
        }

        *num_clos = erdt->max_clos;

        return PQOS_RETVAL_OK;
}

/**
 * =======================================
 * I/O L3 cache allocation
 * =======================================
 */
/**
 * @brief Whether a device agent carries L3 CAT registers
 *
 * A device RMDD is valid without a CARD sub-structure: erdt_populate_rmdd_
 * device_agent() requires only DACD, so a device agent that is enumerated for
 * monitoring alone leaves the CARD block zeroed. The registers L3 CAT programs
 * are in that block, so such a domain cannot be allocated in, and its zeroed
 * register base and block size are what tell it apart.
 *
 * @param [in] dev_agent device agent to examine
 *
 * @return whether the agent's CARD block was populated
 */
static int
dev_agent_has_l3ca(const struct pqos_device_agent_info *dev_agent)
{
        return dev_agent->card.reg_base_addr != 0 &&
               dev_agent->card.reg_block_size != 0;
}

/**
 * @brief Report a domain that carries no L3 CAT registers
 *
 * On the MMIO interface the L3 CAT registers live in the CARD block of an I/O
 * device agent, so a CPU domain, a domain the platform does not have, and a
 * device agent enumerated for monitoring alone cannot carry them. A CPU domain
 * ID and a device domain ID look alike on a command line, which is why the
 * message names the domains that do carry L3 CAT.
 *
 * @param [in] erdt ERDT information
 * @param [in] domain_id domain that was asked for
 */
static void
log_domain_without_l3ca(const struct pqos_erdt_info *erdt, uint16_t domain_id)
{
        char domains[DOMAIN_LIST_SIZE] = {0};
        size_t used = 0;
        unsigned i;

        for (i = 0; i < erdt->num_dev_agents; i++) {
                if (!dev_agent_has_l3ca(&erdt->dev_agents[i]))
                        continue;

                if (!append_domain(domains, &used,
                                   erdt->dev_agents[i].rmdd.domain_id))
                        break;
        }

        LOG_ERROR("Domain ID 0x%x carries no L3 CAT registers. On the MMIO "
                  "interface L3 CAT applies to the I/O device domains, %s. "
                  "Use --print-io-devs to list them\n",
                  domain_id,
                  used > 0 ? domains
                           : "of which this platform "
                             "reports none");
}

int
mmio_l3ca_set(const unsigned l3cat_id,
              const unsigned num_ca,
              const struct pqos_l3ca *ca)
{
        int ret = PQOS_RETVAL_OK;
        unsigned i = 0;
        const struct pqos_erdt_info *erdt = _pqos_get_erdt();
        uint64_t io_l3_ways_mask = 0;

        ASSERT(ca != NULL);
        ASSERT(num_ca != 0);
        ASSERT(erdt != NULL);
        UNUSED_PARAM(l3cat_id);

        if (num_ca > erdt->max_clos)
                return PQOS_RETVAL_ERROR;

        // Check if all domains are valid
        for (unsigned i = 0; i < num_ca; i++) {
                const struct pqos_device_agent_info *dev_agent =
                    get_mmio_dev_agent_by_domain(erdt, ca[i].domain_id);

                if (dev_agent == NULL || !dev_agent_has_l3ca(dev_agent)) {
                        log_domain_without_l3ca(erdt, ca[i].domain_id);
                        return PQOS_RETVAL_PARAM;
                }

                io_l3_ways_mask =
                    (1ULL << get_mmio_dev_agent_by_domain(erdt, ca[i].domain_id)
                                 ->rmdd.num_io_l3_ways) -
                    1ULL;
                if (ca[i].u.ways_mask > io_l3_ways_mask) {
                        LOG_ERROR("L3 CAT CLOS%u Requested Cache Ways "
                                  "%#" PRIx64 ". But available Cache Ways "
                                  "%#" PRIx64 ".\n",
                                  ca[i].class_id, ca[i].u.ways_mask,
                                  io_l3_ways_mask);
                        return PQOS_RETVAL_PARAM;
                }
        }

        for (i = 0; i < num_ca; i++) {
                /* Check L3 CBM is non-contiguous */
                if (!cap_get_mmio_l3ca_non_contiguous(erdt, ca[i].domain_id)) {
                        /* Check all CLOS CBM are contiguous */
                        if (!IS_CONTIGNOUS(ca[i])) {
                                LOG_ERROR("L3 CAT CLOS%u bit mask is not "
                                          "contiguous!\n",
                                          ca[i].class_id);
                                return PQOS_RETVAL_PARAM;
                        }
                }

                /* Check L3 CBM is zero-length bitmask */
                if (ca[i].u.ways_mask == 0 &&
                    !cap_get_mmio_l3ca_zero_length(erdt, ca[i].domain_id)) {
                        LOG_ERROR("L3 CAT CLOS%u bit mask is 0 and Zero-length "
                                  "bitmask is not supported in Domain id %d.\n",
                                  ca[i].class_id, ca[i].domain_id);
                        return PQOS_RETVAL_PARAM;
                }

                ret = set_iol3_cbm_clos_v1(
                    &get_mmio_dev_agent_by_domain(erdt, ca[i].domain_id)->card,
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
        for (unsigned i = 0; i < erdt->max_clos; i++) {
                const struct pqos_device_agent_info *dev_agent =
                    get_mmio_dev_agent_by_domain(erdt, ca[i].domain_id);

                if (dev_agent == NULL || !dev_agent_has_l3ca(dev_agent)) {
                        log_domain_without_l3ca(erdt, ca[i].domain_id);
                        return PQOS_RETVAL_PARAM;
                }
        }

        for (i = 0; i < erdt->max_clos; i++) {
                ret = get_iol3_cbm_clos_v1(
                    &get_mmio_dev_agent_by_domain(erdt, ca[i].domain_id)->card,
                    i, REG_BLOCK_SIZE_ZERO, &value);

                if (ret != PQOS_RETVAL_OK)
                        return ret;

                ca[i].cdp = 0;
                ca[i].class_id = i;
                ca[i].u.ways_mask = value;
        }
        *num_ca = erdt->max_clos;

        return ret;
}

int
mmio_alloc_reset_cat(void)
{
        int ret;
        const struct pqos_erdt_info *erdt = _pqos_get_erdt();

        ASSERT(erdt != NULL);

        for (unsigned domain = 0; domain < erdt->num_dev_agents; domain++) {
                /* An agent enumerated for monitoring alone has no CARD block,
                 * so it holds no L3 CAT register to reset. Writing one anyway
                 * would map its zeroed register base, which fails, and take the
                 * whole allocation reset down with it.
                 */
                if (!dev_agent_has_l3ca(&erdt->dev_agents[domain]))
                        continue;

                for (unsigned i = 0; i < erdt->max_clos; i++) {
                        /* Reset I/ORDT L3 CAT */
                        ret = set_iol3_cbm_clos_v1(
                            (const struct pqos_erdt_card *)&erdt
                                ->dev_agents[domain]
                                .card,
                            i,
                            (1ULL
                             << erdt->dev_agents[domain].rmdd.num_io_l3_ways) -
                                1ULL);

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
