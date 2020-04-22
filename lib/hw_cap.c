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
 * @brief Adds new event type to \a mon monitoring structure
 *
 * @param mon Monitoring structure which is to be updated with the new
 *        event type
 * @param res_id resource id
 * @param event_type event type
 * @param max_rmid max RMID for the event
 * @param scale_factor event specific scale factor
 * @param counter_length counter bit length for the event
 * @param max_num_events maximum number of events that \a mon can accommodate
 */
static void
add_monitoring_event(struct pqos_cap_mon *mon,
                     const unsigned res_id,
                     const int event_type,
                     const unsigned max_rmid,
                     const uint32_t scale_factor,
                     const unsigned counter_length,
                     const unsigned max_num_events)
{
        if (mon->num_events >= max_num_events) {
                LOG_WARN("%s() no space for event type %d (resource id %u)!\n",
                         __func__, event_type, res_id);
                return;
        }

        LOG_DEBUG("Adding monitoring event: resource ID %u, "
                  "type %d to table index %u\n",
                  res_id, event_type, mon->num_events);

        mon->events[mon->num_events].type = (enum pqos_mon_event)event_type;
        mon->events[mon->num_events].max_rmid = max_rmid;
        mon->events[mon->num_events].scale_factor = scale_factor;
        mon->events[mon->num_events].counter_length = counter_length;
        mon->num_events++;
}

/**
 * @brief Discovers monitoring capabilities
 *
 * Runs series of CPUID instructions to discover system CMT
 * capabilities.
 * Allocates memory for monitoring structure and
 * returns it through \a r_mon to the caller.
 *
 * @param r_mon place to store created monitoring structure
 * @param cpu CPU topology structure
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK success
 * @retval PQOS_RETVAL_RESOURCE monitoring not supported
 * @retval PQOS_RETVAL_ERROR enumeration error
 */
int
hw_cap_mon_discover(struct pqos_cap_mon **r_mon, const struct pqos_cpuinfo *cpu)
{
        struct cpuid_out res, cpuid_0xa;
        struct cpuid_out cpuid_0xf_1;
        int ret = PQOS_RETVAL_OK;
        unsigned sz = 0, max_rmid = 0, l3_size = 0, num_events = 0;
        struct pqos_cap_mon *mon = NULL;

        ASSERT(r_mon != NULL && cpu != NULL);

        /**
         * Run CPUID.0x7.0 to check
         * for quality monitoring capability (bit 12 of ebx)
         */
        lcpuid(0x7, 0x0, &res);
        if (!(res.ebx & (1 << 12))) {
                LOG_WARN("CPUID.0x7.0: Monitoring capability not supported!\n");
                return PQOS_RETVAL_RESOURCE;
        }

        /**
         * We can go to CPUID.0xf.0 for further
         * exploration of monitoring capabilities
         */
        lcpuid(0xf, 0x0, &res);
        if (!(res.edx & (1 << 1))) {
                LOG_WARN("CPUID.0xf.0: Monitoring capability not supported!\n");
                return PQOS_RETVAL_RESOURCE;
        }

        /**
         * MAX_RMID for the socket
         */
        max_rmid = (unsigned)res.ebx + 1;
        ret = get_cache_info(&cpu->l3, NULL, &l3_size); /**< L3 cache size */
        if (ret != PQOS_RETVAL_OK) {
                LOG_ERROR("Error reading L3 information!\n");
                return PQOS_RETVAL_ERROR;
        }

        /**
         * Check number of monitoring events to allocate memory for
         * Sub-leaf 1 provides information on monitoring.
         */
        lcpuid(0xf, 1, &cpuid_0xf_1); /**< query resource monitoring */

        if (cpuid_0xf_1.edx & 1)
                num_events++; /**< LLC occupancy */
        if (cpuid_0xf_1.edx & 2)
                num_events++; /**< total memory bandwidth event */
        if (cpuid_0xf_1.edx & 4)
                num_events++; /**< local memory bandwidth event */
        if ((cpuid_0xf_1.edx & 2) && (cpuid_0xf_1.edx & 4))
                num_events++; /**< remote memory bandwidth virtual event */

        if (!num_events)
                return PQOS_RETVAL_ERROR;

        /**
         * Check if IPC can be calculated & supported
         */
        lcpuid(0xa, 0x0, &cpuid_0xa);
        if (((cpuid_0xa.ebx & 3) == 0) && ((cpuid_0xa.edx & 31) > 1))
                num_events++;

        /**
         * This means we can program LLC misses too
         */
        if (((cpuid_0xa.eax >> 8) & 0xff) > 1)
                num_events++;

        /**
         * Allocate memory for detected events and
         * fill the events in.
         */
        sz = (num_events * sizeof(struct pqos_monitor)) + sizeof(*mon);
        mon = (struct pqos_cap_mon *)malloc(sz);
        if (mon == NULL)
                return PQOS_RETVAL_RESOURCE;

        memset(mon, 0, sz);
        mon->mem_size = sz;
        mon->max_rmid = max_rmid;
        mon->l3_size = l3_size;

        {
                const unsigned max_rmid = cpuid_0xf_1.ecx + 1;
                const uint32_t scale_factor = cpuid_0xf_1.ebx;
                const unsigned counter_length = (cpuid_0xf_1.eax & 0x7f) + 24;

                if (cpuid_0xf_1.edx & 1)
                        add_monitoring_event(mon, 1, PQOS_MON_EVENT_L3_OCCUP,
                                             max_rmid, scale_factor,
                                             counter_length, num_events);
                if (cpuid_0xf_1.edx & 2)
                        add_monitoring_event(mon, 1, PQOS_MON_EVENT_TMEM_BW,
                                             max_rmid, scale_factor,
                                             counter_length, num_events);
                if (cpuid_0xf_1.edx & 4)
                        add_monitoring_event(mon, 1, PQOS_MON_EVENT_LMEM_BW,
                                             max_rmid, scale_factor,
                                             counter_length, num_events);

                if ((cpuid_0xf_1.edx & 2) && (cpuid_0xf_1.edx & 4))
                        add_monitoring_event(mon, 1, PQOS_MON_EVENT_RMEM_BW,
                                             max_rmid, scale_factor,
                                             counter_length, num_events);
        }

        if (((cpuid_0xa.ebx & 3) == 0) && ((cpuid_0xa.edx & 31) > 1))
                add_monitoring_event(mon, 0, PQOS_PERF_EVENT_IPC, 0, 0, 0,
                                     num_events);

        if (((cpuid_0xa.eax >> 8) & 0xff) > 1)
                add_monitoring_event(mon, 0, PQOS_PERF_EVENT_LLC_MISS, 0, 0, 0,
                                     num_events);

        (*r_mon) = mon;
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
#define CPUID_LEAF_BRAND_END   0x80000004UL

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
 * @param cap place to store CAT capabilities structure
 * @param cpu detected cpu topology
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK success
 */
int
hw_cap_l3ca_discover(struct pqos_cap_l3ca *cap, const struct pqos_cpuinfo *cpu)
{
        struct cpuid_out res;
        unsigned l3_size = 0;
        int ret = PQOS_RETVAL_OK;

        ASSERT(cap != NULL);
        ASSERT(cpu != NULL);

        memset(cap, 0, sizeof(*cap));
        cap->mem_size = sizeof(*cap);

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

int
hw_cap_l2ca_discover(struct pqos_cap_l2ca *cap, const struct pqos_cpuinfo *cpu)
{
        struct cpuid_out res;
        unsigned l2_size = 0;
        int ret = PQOS_RETVAL_OK;

        ASSERT(cpu != NULL);
        ASSERT(cap != NULL);

        /**
         * Run CPUID.0x7.0 to check
         * for allocation capability (bit 15 of ebx)
         */
        lcpuid(0x7, 0x0, &res);
        if (!(res.ebx & (1 << 15))) {
                LOG_INFO("CPUID.0x7.0: L2 CAT not supported\n");
                return PQOS_RETVAL_RESOURCE;
        }

        /**
         * We can go to CPUID.0x10.0 to obtain more info
         */
        lcpuid(0x10, 0x0, &res);
        if (!(res.ebx & (1 << PQOS_RES_ID_L2_ALLOCATION))) {
                LOG_INFO("CPUID 0x10.0: L2 CAT not supported!\n");
                return PQOS_RETVAL_RESOURCE;
        }

        lcpuid(0x10, PQOS_RES_ID_L2_ALLOCATION, &res);

        memset(cap, 0, sizeof(*cap));
        cap->mem_size = sizeof(*cap);
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
                        return ret;
                }
                cap->cdp_on = cdp_on;
                if (cdp_on)
                        cap->num_classes = cap->num_classes / 2;
        }

        ret = get_cache_info(&cpu->l2, NULL, &l2_size);
        if (ret != PQOS_RETVAL_OK) {
                LOG_ERROR("Error reading L2 info!\n");
                return PQOS_RETVAL_ERROR;
        }
        if (cap->num_ways > 0)
                cap->way_size = l2_size / cap->num_ways;

        return ret;
}

int
hw_cap_mba_discover(struct pqos_cap_mba *cap, const struct pqos_cpuinfo *cpu)
{
        struct cpuid_out res;
        int ret = PQOS_RETVAL_OK;
        unsigned version;
        unsigned mba_thread_ctrl = 0;

        ASSERT(cpu != NULL);
        ASSERT(cap != NULL);

        memset(cap, 0, sizeof(*cap));
        cap->mem_size = sizeof(*cap);
        cap->ctrl = -1;
        cap->ctrl_on = 0;

        /**
         * Run CPUID.0x7.0 to check
         * for allocation capability (bit 15 of ebx)
         */
        lcpuid(0x7, 0x0, &res);
        if (!(res.ebx & (1 << 15))) {
                LOG_INFO("CPUID.0x7.0: MBA not supported\n");
                return PQOS_RETVAL_RESOURCE;
        }

        /**
         * We can go to CPUID.0x10.0 to obtain more info
         */
        lcpuid(0x10, 0x0, &res);
        if (!(res.ebx & (1 << PQOS_RES_ID_MB_ALLOCATION))) {
                LOG_INFO("CPUID 0x10.0: MBA not supported!\n");
                return PQOS_RETVAL_RESOURCE;
        }

        lcpuid(0x10, PQOS_RES_ID_MB_ALLOCATION, &res);

        cap->num_classes = (res.edx & 0xffff) + 1;
        cap->throttle_max = (res.eax & 0xfff) + 1;
        cap->is_linear = (res.ecx >> 2) & 1;
        if (cap->is_linear)
                cap->throttle_step = 100 - cap->throttle_max;
        else {
                LOG_WARN("MBA non-linear mode not supported yet!\n");
                return PQOS_RETVAL_RESOURCE;
        }

        /*
         * Detect MBA version
         *  - MBA3.0 introduces per-thread MBA controls
         *  - MBA2.0 increases number of MBA COS to 15
         */
        if (res.ecx & 0x1) {
                version = 3;
                mba_thread_ctrl = 1;
        } else if (cap->num_classes > 8)
                version = 2;
        else
                version = 1;

        LOG_INFO("Detected MBA version %u.0\n", version);
        LOG_INFO("Detected Per-%s MBA controls\n",
                 mba_thread_ctrl ? "Thread" : "Core");

        if (version >= 2) {
                unsigned *mba_ids;
                unsigned mba_id_num;
                unsigned i;

                /* mba ids */
                mba_ids = pqos_cpu_get_mba_ids(cpu, &mba_id_num);
                if (mba_ids == NULL)
                        return PQOS_RETVAL_RESOURCE;

                /* Detect MBA configuration */
                for (i = 0; i < mba_id_num; i++) {
                        uint64_t reg = 0;
                        unsigned core = 0;

                        ret =
                            pqos_cpu_get_one_by_mba_id(cpu, mba_ids[i], &core);
                        if (ret != PQOS_RETVAL_OK)
                                break;

                        if (msr_read(core, PQOS_MSR_MBA_CFG, &reg) !=
                            MACHINE_RETVAL_OK) {
                                ret = PQOS_RETVAL_ERROR;
                                break;
                        }

                        if (reg & 0x2)
                                LOG_INFO(
                                    "MBA Legacy Mode enabled on socket %u\n",
                                    mba_ids[i]);
                        if (!mba_thread_ctrl)
                                LOG_INFO("%s MBA delay enabled on socket %u\n",
                                         (reg & 0x1) ? "Min" : "Max",
                                         mba_ids[i]);
                }

                free(mba_ids);
        }

        return ret;
}

int
amd_cap_mba_discover(struct pqos_cap_mba *cap, const struct pqos_cpuinfo *cpu)
{
        struct cpuid_out res;
        int ret = PQOS_RETVAL_OK;

        UNUSED_PARAM(cpu);
        ASSERT(cap != NULL);

        memset(cap, 0, sizeof(*cap));
        cap->mem_size = sizeof(*cap);
        cap->ctrl = -1;
        cap->ctrl_on = 0;

        /**
         * Run CPUID.0x80000008.0 to check
         * for MBA allocation capability (bit 6 of ebx)
         */
        lcpuid(0x80000008, 0x0, &res);
        if (!(res.ebx & (1 << 6))) {
                LOG_INFO("CPUID.0x80000008.0: MBA not supported\n");
                free(cap);
                return PQOS_RETVAL_RESOURCE;
        }

        /**
         * We can go to CPUID.0x10.0 to obtain more info
         */
        lcpuid(0x80000020, 0x0, &res);
        if (!(res.ebx & (1 << 1))) {
                LOG_INFO("CPUID 0x10.0: MBA not supported!\n");
                free(cap);
                return PQOS_RETVAL_RESOURCE;
        }

        lcpuid(0x80000020, 1, &res);

        cap->num_classes = (res.edx & 0xffff) + 1;

        /* AMD does not support throttle_max and is_linear. Set it to 0 */
        cap->throttle_max = 0;
        cap->is_linear = 0;

        return ret;
}
