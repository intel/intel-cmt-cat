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

/**
 * @brief HW implementation of PQoS API / capabilities.
 */
#include <stdlib.h>
#include <string.h>

#include "cpu_registers.h"
#include "hw_cap.h"
#include "log.h"
#include "machine.h"
#include "types.h"

/**
 * @brief Retrieves cache size and number of ways
 *
 * Retrieves information about cache from \a cache_info structure.
 *
 * @param cache_info cache information structure
 * @param num_ways place to store number of cache ways
 * @param size place to store cache size in bytes
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK success
 * @retval PQOS_RETVAL_PARAM incorrect parameters
 * @retval PQOS_RETVAL_RESOURCE cache not detected
 */
static int
get_cache_info(const struct pqos_cacheinfo *cache_info,
               unsigned *num_ways,
               unsigned *size)
{
        if (num_ways == NULL && size == NULL)
                return PQOS_RETVAL_PARAM;
        if (cache_info == NULL)
                return PQOS_RETVAL_PARAM;
        if (!cache_info->detected)
                return PQOS_RETVAL_RESOURCE;
        if (num_ways != NULL)
                *num_ways = cache_info->num_ways;
        if (size != NULL)
                *size = cache_info->total_size;
        return PQOS_RETVAL_OK;
}

/**
 * @brief Checks L3 CDP enable status across all CPU sockets
 *
 * It also validates if L3 CDP enabling is consistent across
 * CPU sockets.
 * At the moment, such scenario is considered as error
 * that requires CAT reset.
 *
 * @param cpu detected CPU topology
 * @param enabled place to store L3 CDP enabling status
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 */
static int
l3cdp_is_enabled(const struct pqos_cpuinfo *cpu, int *enabled)
{
        unsigned *l3cat_ids = NULL;
        unsigned l3cat_id_num = 0, j = 0;
        unsigned enabled_num = 0, disabled_num = 0;
        int ret = PQOS_RETVAL_OK;

        ASSERT(enabled != NULL && cpu != NULL);
        if (enabled == NULL || cpu == NULL)
                return PQOS_RETVAL_PARAM;

        *enabled = 0;

        /**
         * Get list of l3cat id's
         */
        l3cat_ids = pqos_cpu_get_l3cat_ids(cpu, &l3cat_id_num);
        if (l3cat_ids == NULL)
                return PQOS_RETVAL_RESOURCE;

        for (j = 0; j < l3cat_id_num; j++) {
                uint64_t reg = 0;
                unsigned core = 0;

                ret = pqos_cpu_get_one_by_l3cat_id(cpu, l3cat_ids[j], &core);
                if (ret != PQOS_RETVAL_OK)
                        goto l3cdp_is_enabled_exit;

                if (msr_read(core, PQOS_MSR_L3_QOS_CFG, &reg) !=
                    MACHINE_RETVAL_OK) {
                        ret = PQOS_RETVAL_ERROR;
                        goto l3cdp_is_enabled_exit;
                }

                if (reg & PQOS_MSR_L3_QOS_CFG_CDP_EN)
                        enabled_num++;
                else
                        disabled_num++;
        }

        if (disabled_num > 0 && enabled_num > 0) {
                LOG_ERROR("Inconsistent L3 CDP settings across l3cat_ids."
                          "Please reset CAT or reboot your system!\n");
                ret = PQOS_RETVAL_ERROR;
                goto l3cdp_is_enabled_exit;
        }

        if (enabled_num > 0)
                *enabled = 1;

        LOG_INFO("L3 CDP is %s\n", (*enabled) ? "enabled" : "disabled");

l3cdp_is_enabled_exit:
        if (l3cat_ids != NULL)
                free(l3cat_ids);

        return ret;
}

/**
 * @brief Detects presence of L3 CAT based on register probing.
 *
 * This method of detecting CAT does the following steps.
 * - probe COS registers one by one and exit on first error
 * - if procedure fails on COS0 then CAT is not supported
 * - use CPUID.0x4.0x3 to get number of cache ways
 *
 * @param cap CAT structure to be initialized
 * @param cpu CPU topology structure
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK success
 * @retval PQOS_RETVAL_PARAM invalid input configuration/parameters
 * @retval PQOS_RETVAL_RESOURCE technology not supported
 */
static int
hw_cap_l3ca_probe(struct pqos_cap_l3ca *cap, const struct pqos_cpuinfo *cpu)
{
        unsigned i = 0, lcore;
        const unsigned max_classes =
            PQOS_MSR_L3CA_MASK_END - PQOS_MSR_L3CA_MASK_START + 1;

        ASSERT(cap != NULL && cpu != NULL);

        /**
         * Pick a valid core and run series of MSR reads on it
         */
        lcore = cpu->cores[0].lcore;
        for (i = 0; i < max_classes; i++) {
                int msr_ret;
                uint64_t value;

                msr_ret = msr_read(lcore, PQOS_MSR_L3CA_MASK_START + i, &value);
                if (msr_ret != MACHINE_RETVAL_OK)
                        break;
        }

        if (i == 0) {
                LOG_WARN("Error probing COS0 on core %u\n", lcore);
                return PQOS_RETVAL_RESOURCE;
        }

        /**
         * Number of ways and CBM is detected with CPUID.0x4.0x3 later on
         */
        cap->num_classes = i;
        return PQOS_RETVAL_OK;
}

/**
 * @brief Detects presence of L3 CAT based on brand string.
 *
 * If CPUID.0x7.0 doesn't report CAT feature
 * platform may still support it:
 * - we need to check brand string vs known ones
 * - use CPUID.0x4.0x3 to get number of cache ways
 *
 * @param cap CAT structure to be initialized
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK success
 * @retval PQOS_RETVAL_RESOURCE technology not supported
 */
static int
hw_cap_l3ca_brandstr(struct pqos_cap_l3ca *cap)
{
#define CPUID_LEAF_BRAND_START 0x80000002UL
#define CPUID_LEAF_BRAND_END 0x80000004UL
#define CPUID_LEAF_BRAND_NUM (CPUID_LEAF_BRAND_END - CPUID_LEAF_BRAND_START + 1)
#define MAX_BRAND_STRING_LEN (CPUID_LEAF_BRAND_NUM * 4 * sizeof(uint32_t))
        static const char *const supported_brands[] = {
            "E5-2658 v3",  "E5-2648L v3", "E5-2628L v3", "E5-2618L v3",
            "E5-2608L v3", "E5-2658A v3", "E3-1258L v4", "E3-1278L v4"};
        struct cpuid_out res;
        int ret = PQOS_RETVAL_OK;
        int match_found = 0;
        uint32_t brand[MAX_BRAND_STRING_LEN / 4 + 1];
        char *brand_str = (char *)brand;
        uint32_t *brand_u32 = brand;
        unsigned i = 0;

        /**
         * Assume \a cap is not NULL at this stage.
         * Adequate check has to be done in the caller.
         */
        ASSERT(cap != NULL);

        lcpuid(0x80000000, 0, &res);
        if (res.eax < CPUID_LEAF_BRAND_END) {
                LOG_ERROR("Brand string CPU-ID extended functions "
                          "not supported\n");
                return PQOS_RETVAL_ERROR;
        }

        memset(brand, 0, sizeof(brand));

        for (i = 0; i < CPUID_LEAF_BRAND_NUM; i++) {
                lcpuid((unsigned)CPUID_LEAF_BRAND_START + i, 0, &res);
                *brand_u32++ = res.eax;
                *brand_u32++ = res.ebx;
                *brand_u32++ = res.ecx;
                *brand_u32++ = res.edx;
        }

        LOG_DEBUG("CPU brand string '%s'\n", brand_str);

        /**
         * match brand against supported ones
         */
        for (i = 0; i < DIM(supported_brands); i++)
                if (strstr(brand_str, supported_brands[i]) != NULL) {
                        LOG_INFO("Cache allocation detected for model name "
                                 "'%s'\n",
                                 brand_str);
                        match_found = 1;
                        break;
                }

        if (!match_found) {
                LOG_WARN("Cache allocation not supported on model name '%s'!\n",
                         brand_str);
                return PQOS_RETVAL_RESOURCE;
        }

        /**
         * Figure out number of ways and CBM (1:1)
         * using CPUID.0x4.0x3
         */
        cap->num_classes = 4;

        return ret;
}

/**
 * @brief Detects presence of L3 CAT based on CPUID
 *
 * @param cap CAT structure to be initialized
 * @param cpu CPU topology structure
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK success
 * @retval PQOS_RETVAL_RESOURCE technology not supported
 */
static int
hw_cap_l3ca_cpuid(struct pqos_cap_l3ca *cap, const struct pqos_cpuinfo *cpu)
{
        struct cpuid_out res;
        int ret = PQOS_RETVAL_OK;

        /**
         * We can go to CPUID.0x10.0 to explore
         * allocation capabilities
         */
        lcpuid(0x10, 0x0, &res);
        if (!(res.ebx & (1 << PQOS_RES_ID_L3_ALLOCATION))) {
                LOG_INFO("CPUID.0x10.0: L3 CAT not detected.\n");
                return PQOS_RETVAL_RESOURCE;
        }

        /**
         * L3 CAT detected
         * - get more info about it
         */
        lcpuid(0x10, PQOS_RES_ID_L3_ALLOCATION, &res);
        cap->num_classes = res.edx + 1;
        cap->num_ways = res.eax + 1;
        cap->cdp = (res.ecx >> PQOS_CPUID_CAT_CDP_BIT) & 1;
        cap->cdp_on = 0;
        cap->way_contention = (uint64_t)res.ebx;

        if (cap->cdp) {
                /**
                 * CDP is supported but is it on?
                 */
                int cdp_on = 0;

                ret = l3cdp_is_enabled(cpu, &cdp_on);
                if (ret != PQOS_RETVAL_OK) {
                        LOG_ERROR("L3 CDP detection error!\n");
                        return ret;
                }
                cap->cdp_on = cdp_on;
                if (cdp_on)
                        cap->num_classes = cap->num_classes / 2;
        }

        return ret;
}

/**
 * @brief Discovers L3 CAT
 *
 * First it tries to detects CAT through CPUID.0x7.0
 * if this fails then falls into brand string check.
 *
 * Function allocates memory for CAT capabilities
 * and returns it to the caller through \a r_cap.
 *
 * \a cpu is only needed to detect CDP status.
 *
 * @param r_cap place to store CAT capabilities structure
 * @param cpu detected cpu topology
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK success
 */
int
hw_cap_l3ca_discover(struct pqos_cap_l3ca **r_cap,
                     const struct pqos_cpuinfo *cpu)
{
        struct cpuid_out res;
        struct pqos_cap_l3ca *cap = NULL;
        const unsigned sz = sizeof(*cap);
        unsigned l3_size = 0;
        int ret = PQOS_RETVAL_OK;

        cap = (struct pqos_cap_l3ca *)malloc(sz);
        if (cap == NULL)
                return PQOS_RETVAL_RESOURCE;

        ASSERT(cap != NULL);

        memset(cap, 0, sz);
        cap->mem_size = sz;

        /**
         * Run CPUID.0x7.0 to check
         * for allocation capability (bit 15 of ebx)
         */
        lcpuid(0x7, 0x0, &res);

        if (res.ebx & (1 << 15)) {
                /**
                 * Use CPUID method
                 */
                LOG_INFO("CPUID.0x7.0: L3 CAT supported\n");
                ret = hw_cap_l3ca_cpuid(cap, cpu);
                if (ret == PQOS_RETVAL_OK)
                        ret = get_cache_info(&cpu->l3, NULL, &l3_size);
        } else {
                /**
                 * Use brand string matching method 1st.
                 * If it fails then try register probing.
                 */
                LOG_INFO("CPUID.0x7.0: L3 CAT not detected. "
                         "Checking brand string...\n");
                ret = hw_cap_l3ca_brandstr(cap);
                if (ret != PQOS_RETVAL_OK)
                        ret = hw_cap_l3ca_probe(cap, cpu);
                if (ret == PQOS_RETVAL_OK)
                        ret =
                            get_cache_info(&cpu->l3, &cap->num_ways, &l3_size);
        }

        if (cap->num_ways > 0)
                cap->way_size = l3_size / cap->num_ways;

        if (ret == PQOS_RETVAL_OK)
                (*r_cap) = cap;
        else
                free(cap);

        return ret;
}

/**
 * @brief Checks L2 CDP enable status across all CPU clusters
 *
 * It also validates if L2 CDP enabling is consistent across
 * CPU clusters.
 * At the moment, such scenario is considered as error
 * that requires CAT reset.
 *
 * @param cpu detected CPU topology
 * @param enabled place to store L2 CDP enabling status
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 */
static int
l2cdp_is_enabled(const struct pqos_cpuinfo *cpu, int *enabled)
{
        unsigned *l2ids = NULL;
        unsigned l2id_num = 0;
        unsigned enabled_num = 0, disabled_num = 0;
        unsigned i;
        int ret = PQOS_RETVAL_OK;

        /**
         * Get list of L2 clusters id's
         */
        l2ids = pqos_cpu_get_l2ids(cpu, &l2id_num);
        if (l2ids == NULL || l2id_num == 0) {
                ret = PQOS_RETVAL_RESOURCE;
                goto l2cdp_is_enabled_exit;
        }

        for (i = 0; i < l2id_num; i++) {
                uint64_t reg = 0;
                unsigned core = 0;

                ret = pqos_cpu_get_one_by_l2id(cpu, l2ids[i], &core);
                if (ret != PQOS_RETVAL_OK)
                        goto l2cdp_is_enabled_exit;

                if (msr_read(core, PQOS_MSR_L2_QOS_CFG, &reg) !=
                    MACHINE_RETVAL_OK) {
                        ret = PQOS_RETVAL_ERROR;
                        goto l2cdp_is_enabled_exit;
                }

                if (reg & PQOS_MSR_L2_QOS_CFG_CDP_EN)
                        enabled_num++;
                else
                        disabled_num++;
        }

        if (disabled_num > 0 && enabled_num > 0) {
                LOG_ERROR("Inconsistent L2 CDP settings across clusters."
                          "Please reset CAT or reboot your system!\n");
                ret = PQOS_RETVAL_ERROR;
                goto l2cdp_is_enabled_exit;
        }

        if (enabled_num > 0)
                *enabled = 1;

        LOG_INFO("L2 CDP is %s\n", (*enabled) ? "enabled" : "disabled");

l2cdp_is_enabled_exit:
        if (l2ids != NULL)
                free(l2ids);

        return ret;
}

/**
 * @brief Discovers L2 CAT
 *
 * @param r_cap place to store L2 CAT capabilities structure
 * @param cpu CPU topology structure
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK success
 */
int
hw_cap_l2ca_discover(struct pqos_cap_l2ca **r_cap,
                     const struct pqos_cpuinfo *cpu)
{
        struct cpuid_out res;
        struct pqos_cap_l2ca *cap = NULL;
        const unsigned sz = sizeof(*cap);
        unsigned l2_size = 0;
        int ret = PQOS_RETVAL_OK;

        ASSERT(cpu != NULL);

        cap = (struct pqos_cap_l2ca *)malloc(sz);
        if (cap == NULL)
                return PQOS_RETVAL_RESOURCE;

        ASSERT(cap != NULL);

        memset(cap, 0, sz);
        cap->mem_size = sz;

        /**
         * Run CPUID.0x7.0 to check
         * for allocation capability (bit 15 of ebx)
         */
        lcpuid(0x7, 0x0, &res);
        if (!(res.ebx & (1 << 15))) {
                LOG_INFO("CPUID.0x7.0: L2 CAT not supported\n");
                free(cap);
                return PQOS_RETVAL_RESOURCE;
        }

        /**
         * We can go to CPUID.0x10.0 to obtain more info
         */
        lcpuid(0x10, 0x0, &res);
        if (!(res.ebx & (1 << PQOS_RES_ID_L2_ALLOCATION))) {
                LOG_INFO("CPUID 0x10.0: L2 CAT not supported!\n");
                free(cap);
                return PQOS_RETVAL_RESOURCE;
        }

        lcpuid(0x10, PQOS_RES_ID_L2_ALLOCATION, &res);

        cap->num_classes = res.edx + 1;
        cap->num_ways = res.eax + 1;
        cap->cdp = (res.ecx >> PQOS_CPUID_CAT_CDP_BIT) & 1;
        cap->cdp_on = 0;
        cap->way_contention = (uint64_t)res.ebx;

        if (cap->cdp) {
                int cdp_on = 0;

                /*
                 * Check if L2 CDP is enabled
                 */
                ret = l2cdp_is_enabled(cpu, &cdp_on);
                if (ret != PQOS_RETVAL_OK) {
                        LOG_ERROR("L2 CDP detection error!\n");
                        free(cap);
                        return ret;
                }
                cap->cdp_on = cdp_on;
                if (cdp_on)
                        cap->num_classes = cap->num_classes / 2;
        }

        ret = get_cache_info(&cpu->l2, NULL, &l2_size);
        if (ret != PQOS_RETVAL_OK) {
                LOG_ERROR("Error reading L2 info!\n");
                free(cap);
                return PQOS_RETVAL_ERROR;
        }
        if (cap->num_ways > 0)
                cap->way_size = l2_size / cap->num_ways;

        (*r_cap) = cap;
        return ret;
}
