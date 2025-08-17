/*
 * BSD LICENSE
 *
 * Copyright(c) 2020-2023 Intel Corporation. All rights reserved.
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

#include "hw_cap.h"

#include "cap.h"
#include "cpu_registers.h"
#include "cpuinfo.h"
#include "hw_monitoring.h"
#include "log.h"
#include "machine.h"
#include "uncore_monitoring.h"

#include <stdlib.h>
#include <string.h>

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
 * @brief Checks L3 feature enable status across all CPU sockets
 *
 * It also validates if L3 feature enabling is consistent across CPU sockets.
 * At the moment, such scenario is considered as error that requires CAT reset.
 *
 * @param [in] cpu detected CPU topology
 * @param [in] feature_name printable feature name
 * @param [in] reg configuration register address
 * @param [in] bit configuration bit
 * @param [out] enabled place to store L3 feature enabling status
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 */
static int
hw_cap_l3_feature(const struct pqos_cpuinfo *cpu,
                  const char *feature_name,
                  uint32_t reg,
                  uint64_t bit,
                  int *enabled)
{
        unsigned *l3cat_ids = NULL;
        unsigned l3cat_id_num = 0;
        unsigned enabled_num = 0, disabled_num = 0;
        unsigned j;
        int ret = PQOS_RETVAL_OK;

        if (enabled == NULL || cpu == NULL)
                return PQOS_RETVAL_PARAM;

        /**
         * Get list of l3cat id's
         */
        l3cat_ids = pqos_cpu_get_l3cat_ids(cpu, &l3cat_id_num);
        if (l3cat_ids == NULL)
                return PQOS_RETVAL_RESOURCE;

        for (j = 0; j < l3cat_id_num; j++) {
                uint64_t value = 0;
                unsigned core = 0;

                ret = pqos_cpu_get_one_by_l3cat_id(cpu, l3cat_ids[j], &core);
                if (ret != PQOS_RETVAL_OK)
                        goto hw_cap_l3_feature_exit;

                if (msr_read(core, reg, &value) != MACHINE_RETVAL_OK) {
                        ret = PQOS_RETVAL_ERROR;
                        goto hw_cap_l3_feature_exit;
                }

                if (value & bit)
                        enabled_num++;
                else
                        disabled_num++;
        }

        if (disabled_num > 0 && enabled_num > 0) {
                LOG_ERROR("Inconsistent %s settings across l3cat_ids."
                          "Please reset CAT or reboot your system!\n",
                          feature_name);
                ret = PQOS_RETVAL_ERROR;

        } else if (enabled_num > 0)
                *enabled = 1;
        else
                *enabled = 0;

        if (ret == PQOS_RETVAL_OK)
                LOG_INFO("%s is %s\n", feature_name,
                         (*enabled) ? "enabled" : "disabled");

hw_cap_l3_feature_exit:
        if (l3cat_ids != NULL)
                free(l3cat_ids);

        return ret;
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
 * @param iordt I/O RDT support
 */
static void
add_monitoring_event(struct pqos_cap_mon *mon,
                     const unsigned res_id,
                     const int event_type,
                     const unsigned max_rmid,
                     const uint32_t scale_factor,
                     const unsigned counter_length,
                     const unsigned max_num_events,
                     const int iordt)
{
        if (mon->num_events >= max_num_events) {
                LOG_WARN("%s() no space for event type %d (resource id %u)!\n",
                         __func__, event_type, res_id);
                return;
        }

        LOG_DEBUG("Adding monitoring event: resource ID %u, "
                  "type 0x%x, iordt %d\n",
                  res_id, event_type, iordt);

        mon->events[mon->num_events].type = (enum pqos_mon_event)event_type;
        mon->events[mon->num_events].max_rmid = max_rmid;
        mon->events[mon->num_events].scale_factor = scale_factor;
        mon->events[mon->num_events].counter_length = counter_length;
        mon->events[mon->num_events].iordt = iordt;
        mon->num_events++;

        if (iordt)
                mon->iordt = 1;
}

int
hw_cap_mon_iordt(const struct pqos_cpuinfo *cpu, int *enabled)
{
        return hw_cap_l3_feature(cpu, "L3 I/O RDT monitoring",
                                 PQOS_MSR_L3_IO_QOS_CFG,
                                 PQOS_MSR_L3_IO_QOS_MON_EN, enabled);
}

/**
 * @brief Discovers HW monitoring SNC support
 *
 * @param [in] cpu detected CPU topology
 * @param [out] snc_num Number of monitoring clusters
 * @param [out] snc_mode SNC monitoring mode
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK success
 */
PQOS_STATIC int
hw_cap_mon_snc_state(const struct pqos_cpuinfo *cpu,
                     unsigned *snc_num,
                     enum pqos_snc_mode *snc_mode)
{
        int numa_num = cpuinfo_get_numa_num(cpu);
        int socket_num = cpuinfo_get_socket_num(cpu);
        unsigned i, sock_count, *sockets = NULL;
        int ret = PQOS_RETVAL_OK;
        enum pqos_snc_mode st;
        enum pqos_snc_mode mode = (enum pqos_snc_mode)(-1);

        if (cpu == NULL || snc_num == NULL || snc_mode == NULL)
                return PQOS_RETVAL_PARAM;

        if (numa_num < 0 || socket_num < 0)
                return PQOS_RETVAL_ERROR;
        if (numa_num == 0 || socket_num == 0 || numa_num == socket_num) {
                *snc_num = 1;
                *snc_mode = PQOS_SNC_LOCAL;
                return PQOS_RETVAL_OK;
        }

        sockets = pqos_cpu_get_sockets(cpu, &sock_count);
        if (sockets == NULL || sock_count == 0) {
                printf("Error retrieving information for Sockets\n");
                ret = PQOS_RETVAL_ERROR;
                goto hw_cap_mon_snc_state_exit;
        }

        for (i = 0; i < sock_count; i++) {
                unsigned lcore;
                uint64_t val = 0;
                uint32_t reg = PQOS_MSR_SNC_CFG;

                ret = pqos_cpu_get_one_core(cpu, sockets[i], &lcore);
                if (ret != PQOS_RETVAL_OK) {
                        printf("Error retrieving lcore for socket %u\n",
                               sockets[i]);
                        ret = PQOS_RETVAL_ERROR;
                        break;
                };

                if (msr_read(lcore, reg, &val) != MACHINE_RETVAL_OK) {
                        ret = PQOS_RETVAL_ERROR;
                        break;
                }

                st = (val & 1) ? PQOS_SNC_LOCAL : PQOS_SNC_TOTAL;
                if (mode == (enum pqos_snc_mode)(-1))
                        mode = st;
                else if (mode != st) {
                        printf("Error inconsistent SNC mode\n");
                        ret = PQOS_RETVAL_ERROR;
                        break;
                };
        };

        if (ret == PQOS_RETVAL_OK) {
                *snc_num = numa_num / socket_num;
                *snc_mode = mode;
        }

hw_cap_mon_snc_state_exit:
        if (sockets)
                free(sockets);
        return ret;
}

/**
 * @brief Detects if the platform has hybrid architecture
 *
 * Runs CPUID instruction and return bit shows the platorm
 * hybrid status. For example whether the platform has both
 * P-cores and E-cores on board.
 *
 * @return 1 if the platform is a hybrid one, 0 - otherwise
 */
int
hw_detect_hybrid(void)
{
        struct cpuid_out res;

        lcpuid(0x7, 0x0, &res);
        return res.edx & (1 << 15);
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
hw_cap_mon_discover(struct pqos_cap_mon **r_mon,
                    const struct pqos_cpuinfo *cpu,
                    enum pqos_interface interface)
{
        struct cpuid_out res, cpuid_0xa;
        struct cpuid_out cpuid_0xf_1;
        int ret = PQOS_RETVAL_OK;
        unsigned sz = 0, max_rmid = 0, l3_size = 0, num_events = 0;
        struct pqos_cap_mon *mon = NULL;
        enum pqos_mon_event uncore_events = (enum pqos_mon_event)0;

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

        /*
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

        if (cpuid_0xf_1.edx & PQOS_CPUID_MON_L3_OCCUP_BIT)
                num_events++; /**< LLC occupancy */
        if (cpuid_0xf_1.edx & PQOS_CPUID_MON_TMEM_BW_BIT)
                num_events++; /**< total memory bandwidth event */

        if (interface == PQOS_INTER_MMIO) {
                num_events++; /**< local memory bandwidth event */
                num_events++; /**< IO LLC memory bandwidth event */
                num_events++; /**< IO TOTAL memory bandwidth event */
                num_events++; /**< IO MISS memory bandwidth event */
        } else {
                if (cpuid_0xf_1.edx & PQOS_CPUID_MON_LMEM_BW_BIT)
                        num_events++; /**< local memory bandwidth event */
        }

        if ((cpuid_0xf_1.edx & PQOS_CPUID_MON_TMEM_BW_BIT) &&
            (cpuid_0xf_1.edx & PQOS_CPUID_MON_LMEM_BW_BIT))
                num_events++; /**< remote memory bandwidth virtual event */

        ret = uncore_mon_discover(&uncore_events);
        if (ret == PQOS_RETVAL_RESOURCE)
                LOG_DEBUG("Uncore monitoring not detected\n");
        else if (ret == PQOS_RETVAL_OK) {
                if (uncore_events & PQOS_PERF_EVENT_LLC_MISS_PCIE_READ)
                        num_events++;
                if (uncore_events & PQOS_PERF_EVENT_LLC_MISS_PCIE_WRITE)
                        num_events++;
                if (uncore_events & PQOS_PERF_EVENT_LLC_REF_PCIE_READ)
                        num_events++;
                if (uncore_events & PQOS_PERF_EVENT_LLC_REF_PCIE_WRITE)
                        num_events++;
        } else
                return ret;

        if (!num_events)
                return PQOS_RETVAL_ERROR;

        /**
         * Check if IPC can be calculated & supported
         */
        lcpuid(0xa, 0x0, &cpuid_0xa);
        if (((cpuid_0xa.ebx & 3) == 0) && ((cpuid_0xa.edx & 31) > 1))
                num_events++;

        /**
         * This means we can program LLC misses and references too
         */
        if (((cpuid_0xa.eax >> 8) & 0xff) > 1)
                num_events += 2;

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

        /* Detect SNC state */
        ret = hw_cap_mon_snc_state(cpu, &mon->snc_num, &mon->snc_mode);
        if (ret != PQOS_RETVAL_OK) {
                LOG_ERROR("Error reading SNC information!\n");
                free(mon);
                return PQOS_RETVAL_ERROR;
        }

        {
                const unsigned max_rmid = cpuid_0xf_1.ecx + 1;
                const uint32_t scale_factor = cpuid_0xf_1.ebx;
                const unsigned counter_length = (cpuid_0xf_1.eax & 0x7f) + 24;

                if (cpuid_0xf_1.edx & PQOS_CPUID_MON_L3_OCCUP_BIT) {
                        int iordt =
                            cpuid_0xf_1.eax & PQOS_CPUID_MON_IO_OCCUP_BIT ? 1
                                                                          : 0;

                        add_monitoring_event(mon, 1, PQOS_MON_EVENT_L3_OCCUP,
                                             max_rmid, scale_factor,
                                             counter_length, num_events, iordt);
                }
                if (cpuid_0xf_1.edx & PQOS_CPUID_MON_TMEM_BW_BIT) {

                        int iordt =
                            cpuid_0xf_1.eax & PQOS_CPUID_MON_IO_MEM_BW ? 1 : 0;

                        add_monitoring_event(mon, 1, PQOS_MON_EVENT_TMEM_BW,
                                             max_rmid, scale_factor,
                                             counter_length, num_events, iordt);
                }
                if (cpuid_0xf_1.edx & PQOS_CPUID_MON_LMEM_BW_BIT) {
                        int iordt =
                            cpuid_0xf_1.eax & PQOS_CPUID_MON_IO_MEM_BW ? 1 : 0;

                        add_monitoring_event(mon, 1, PQOS_MON_EVENT_LMEM_BW,
                                             max_rmid, scale_factor,
                                             counter_length, num_events, iordt);
                }

                if (interface == PQOS_INTER_MMIO) {
                        int iordt =
                            cpuid_0xf_1.eax & PQOS_CPUID_MON_IO_MEM_BW ? 1 : 0;

                        add_monitoring_event(mon, 1, PQOS_MON_EVENT_LMEM_BW,
                                             max_rmid, scale_factor,
                                             counter_length, num_events, iordt);
                        add_monitoring_event(mon, 1, PQOS_MON_EVENT_IO_L3_OCCUP,
                                             max_rmid, scale_factor,
                                             counter_length, num_events, iordt);
                        add_monitoring_event(
                            mon, 1, PQOS_MON_EVENT_IO_TOTAL_MEM_BW, max_rmid,
                            scale_factor, counter_length, num_events, iordt);
                        add_monitoring_event(
                            mon, 1, PQOS_MON_EVENT_IO_MISS_MEM_BW, max_rmid,
                            scale_factor, counter_length, num_events, iordt);
                }

                if ((cpuid_0xf_1.edx & PQOS_CPUID_MON_TMEM_BW_BIT) &&
                    (cpuid_0xf_1.edx & PQOS_CPUID_MON_LMEM_BW_BIT)) {
                        int iordt =
                            cpuid_0xf_1.eax & PQOS_CPUID_MON_IO_MEM_BW ? 1 : 0;

                        add_monitoring_event(mon, 1, PQOS_MON_EVENT_RMEM_BW,
                                             max_rmid, scale_factor,
                                             counter_length, num_events, iordt);
                }
        }

        if (((cpuid_0xa.ebx & 3) == 0) && ((cpuid_0xa.edx & 31) > 1))
                add_monitoring_event(mon, 0, PQOS_PERF_EVENT_IPC, 0, 0, 0,
                                     num_events, 0);

        if (((cpuid_0xa.eax >> 8) & 0xff) > 1) {
                add_monitoring_event(mon, 0, PQOS_PERF_EVENT_LLC_MISS, 0, 0, 0,
                                     num_events, 0);
                add_monitoring_event(mon, 0, PQOS_PERF_EVENT_LLC_REF, 0, 0, 0,
                                     num_events, 0);
        }

        if (mon->iordt) {
                ret = hw_cap_mon_iordt(cpu, &mon->iordt_on);
                if (ret != PQOS_RETVAL_OK) {
                        free(mon);
                        return ret;
                }
        }

        if (uncore_events & PQOS_PERF_EVENT_LLC_MISS_PCIE_READ)
                add_monitoring_event(mon, 0, PQOS_PERF_EVENT_LLC_MISS_PCIE_READ,
                                     0, 0, 0, num_events, 0);
        if (uncore_events & PQOS_PERF_EVENT_LLC_MISS_PCIE_WRITE)
                add_monitoring_event(mon, 0,
                                     PQOS_PERF_EVENT_LLC_MISS_PCIE_WRITE, 0, 0,
                                     0, num_events, 0);
        if (uncore_events & PQOS_PERF_EVENT_LLC_REF_PCIE_READ)
                add_monitoring_event(mon, 0, PQOS_PERF_EVENT_LLC_REF_PCIE_READ,
                                     0, 0, 0, num_events, 0);
        if (uncore_events & PQOS_PERF_EVENT_LLC_REF_PCIE_WRITE)
                add_monitoring_event(mon, 0, PQOS_PERF_EVENT_LLC_REF_PCIE_WRITE,
                                     0, 0, 0, num_events, 0);

        (*r_mon) = mon;
        return PQOS_RETVAL_OK;
}

int
hw_cap_l3ca_cdp(const struct pqos_cpuinfo *cpu, int *enabled)
{
        return hw_cap_l3_feature(cpu, "L3 CDP", PQOS_MSR_L3_QOS_CFG,
                                 PQOS_MSR_L3_QOS_CFG_CDP_EN, enabled);
}

int
hw_cap_l3ca_iordt(const struct pqos_cpuinfo *cpu, int *enabled)
{
        return hw_cap_l3_feature(cpu, "L3 I/O RDT allocation",
                                 PQOS_MSR_L3_IO_QOS_CFG,
                                 PQOS_MSR_L3_IO_QOS_CA_EN, enabled);
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
 * @brief Detects presence of L3 CAT based on model & family ID.
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK success
 * @retval PQOS_RETVAL_RESOURCE technology not supported
 */
static int
hw_cap_l3ca_model(void)
{
        const int cpu_model = cpuinfo_get_cpu_model();
        const int cpu_family = cpuinfo_get_cpu_family();

        const struct {
                int model;
                int family;
        } supported_cpus[] = {
            {.family = CPU_FAMILY_06, .model = CPU_MODEL_HSX}};

        for (unsigned i = 0; i < DIM(supported_cpus); i++) {
                if (supported_cpus[i].model == cpu_model &&
                    supported_cpus[i].family == cpu_family)
                        return PQOS_RETVAL_OK;
        };

        return PQOS_RETVAL_RESOURCE;
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
        cap->non_contiguous_cbm =
            (res.ecx >> PQOS_CPUID_CAT_NON_CONTIGUOUS_CBM_SUPPORT_BIT) & 1;
        cap->iordt = (res.ecx >> PQOS_CPUID_CAT_IORDT_BIT) & 1;
        cap->iordt_on = 0;

        if (cap->cdp) {
                /**
                 * CDP is supported but is it on?
                 */
                int cdp_on = 0;

                ret = hw_cap_l3ca_cdp(cpu, &cdp_on);
                if (ret != PQOS_RETVAL_OK) {
                        LOG_ERROR("L3 CDP detection error!\n");
                        return ret;
                }
                cap->cdp_on = cdp_on;
                if (cdp_on)
                        cap->num_classes = cap->num_classes / 2;
        }

        /* Detect if I/O RDT is enabled */
        if (cap->iordt) {
                ret = hw_cap_l3ca_iordt(cpu, &cap->iordt_on);
                if (ret != PQOS_RETVAL_OK) {
                        LOG_ERROR("L3 I/O RDT detection error!\n");
                        return ret;
                }
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
        unsigned check_brand_str = 0;
        unsigned check_cpu_model = 0;
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
                ret = hw_cap_l3ca_cpuid(cap, cpu);
                if (ret == PQOS_RETVAL_RESOURCE) {
                        LOG_INFO("CPUID.0x10.0: L3 CAT not detected. "
                                 "Checking brand string...\n");
                        check_brand_str = 1;
                } else if (ret == PQOS_RETVAL_OK)
                        ret = get_cache_info(&cpu->l3, NULL, &l3_size);
        } else {
                LOG_INFO("CPUID.0x7.0: L3 CAT not detected. "
                         "Checking brand string...\n");
                check_brand_str = 1;
                check_cpu_model = 1;
        }
        if (check_brand_str) {
                /**
                 * Use brand string matching method 1st.
                 * If it fails then check the model and family ID.
                 */
                ret = hw_cap_l3ca_brandstr(cap);
                if (ret != PQOS_RETVAL_OK && check_cpu_model) {
                        LOG_INFO("Checking model and family ID...\n");
                        ret = hw_cap_l3ca_model();
                }
                if (ret != PQOS_RETVAL_OK && getenv("RDT_PROBE_MSR") != NULL) {
                        LOG_INFO("Probing msr....\n");
                        ret = hw_cap_l3ca_probe(cap, cpu);
                }
                if (ret == PQOS_RETVAL_OK) {
                        ret =
                            get_cache_info(&cpu->l3, &cap->num_ways, &l3_size);

                        LOG_WARN("Detected model specific non-architectural "
                                 "features (L3 "
                                 "CAT).\n      Intel recommends validating "
                                 "that the feature "
                                 "provides the\n      performance necessary "
                                 "for your "
                                 "use-case. Non-architectural\n      features "
                                 "may not "
                                 "behave as expected in all scenarios.\n");
                }
        }

        if (cap->num_ways > 0)
                cap->way_size = l3_size / cap->num_ways;

        return ret;
}

int
hw_cap_l2ca_cdp(const struct pqos_cpuinfo *cpu, int *enabled)
{
        unsigned *l2ids = NULL;
        unsigned l2id_num = 0;
        unsigned enabled_num = 0, disabled_num = 0;
        unsigned i;
        int ret = PQOS_RETVAL_OK;

        if (enabled == NULL || cpu == NULL)
                return PQOS_RETVAL_PARAM;

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
        else
                *enabled = 0;

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
        cap->non_contiguous_cbm =
            (res.ecx >> PQOS_CPUID_CAT_NON_CONTIGUOUS_CBM_SUPPORT_BIT) & 1;

        if (cap->cdp) {
                int cdp_on = 0;

                /*
                 * Check if L2 CDP is enabled
                 */
                ret = hw_cap_l2ca_cdp(cpu, &cdp_on);
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

static int
is_mba40_supported(unsigned core, unsigned *supported)
{
        int ret;
        uint64_t reg = 0;

        *supported = 0;

        ret = msr_read(core, PQOS_MSR_CORE_CAPABILITIES, &reg);
        if (ret != MACHINE_RETVAL_OK)
                return PQOS_RETVAL_ERROR;

        if (reg & PQOS_MSR_CORE_CAPABILITIES_MBA40_EN)
                *supported = 1;

        return PQOS_RETVAL_OK;
}

static int
detect_mba40(const struct pqos_cpuinfo *cpu, unsigned *supported)
{
        int ret = PQOS_RETVAL_OK;
        unsigned *mba_ids;
        unsigned mba_id_num;
        unsigned i;

        *supported = 0;

        /* MBA IDs */
        mba_ids = pqos_cpu_get_mba_ids(cpu, &mba_id_num);
        if (mba_ids == NULL)
                return PQOS_RETVAL_RESOURCE;

        /*
         * Detect MBA 4.0 extensions support
         * Checking all cores to log a debug message
         */
        for (i = 0; i < mba_id_num; i++) {
                unsigned core_supported = 0;
                unsigned num_cores = 0;
                unsigned *cores = NULL;
                unsigned j;

                cores = pqos_cpu_get_cores(cpu, mba_ids[i], &num_cores);
                if (cores == NULL) {
                        *supported = 0;
                        ret = PQOS_RETVAL_ERROR;
                        break;
                }

                for (j = 0; j < num_cores; j++) {
                        ret = is_mba40_supported(cores[j], &core_supported);
                        if (ret == PQOS_RETVAL_OK && core_supported) {
                                *supported = 1;
                                LOG_DEBUG("MBA 4.0 available for core %u\n",
                                          cores[j]);
                        }
                }

                free(cores);
        }

        free(mba_ids);

        return ret;
}

int
hw_cap_mba_discover(struct pqos_cap_mba *cap, const struct pqos_cpuinfo *cpu)
{
        struct cpuid_out res;
        int ret = PQOS_RETVAL_OK;
        unsigned version;
        unsigned mba_thread_ctrl = 0;
        unsigned msr_core_caps_available = 0;
        unsigned mba40_supported = 0;

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

        if (res.edx & (1 << 30)) {
                msr_core_caps_available = 1;
                LOG_INFO("CPUID.0x7.0: CORE_CAPABILITIES MSR (0x%X) "
                         "available\n",
                         PQOS_MSR_CORE_CAPABILITIES);
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
         *  - MBA4.0 extensions
         *  - MBA3.0 introduces per-thread MBA controls
         *  - MBA2.0 increases number of MBA COS to 15
         */
        if (msr_core_caps_available)
                detect_mba40(cpu, &mba40_supported);

        if (mba40_supported) {
                version = 4;
                cap->mba40 = 1;
                mba_thread_ctrl = 1;
        } else if (res.ecx & 0x1) {
                version = 3;
                mba_thread_ctrl = 1;
        } else if (cap->num_classes > 8)
                version = 2;
        else
                version = 1;

        LOG_INFO("Detected MBA version %u.0\n", version);
        LOG_INFO("Detected Per-%s MBA controls\n",
                 mba_thread_ctrl ? "Thread" : "Core");

        if ((version == 2) || (version >= 4)) {
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

                        if ((version == 2) && (reg & 0x2))
                                LOG_INFO(
                                    "MBA Legacy Mode enabled on socket %u\n",
                                    mba_ids[i]);

                        if (version >= 4) {
                                if (reg & PQOS_MSR_MBA_CFG_MBA40_EN)
                                        cap->mba40_on = 1;
                                else
                                        cap->mba40_on = 0;

                                LOG_INFO(
                                    "MBA 4.0 extensions %s for socket %u\n",
                                    cap->mba40_on ? "enabled" : "disabled",
                                    mba_ids[i]);
                        }

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

int
amd_cap_smba_discover(struct pqos_cap_mba *cap, const struct pqos_cpuinfo *cpu)
{
        struct cpuid_out res;

        UNUSED_PARAM(cpu);
        ASSERT(cap != NULL);

        memset(cap, 0, sizeof(*cap));
        cap->mem_size = sizeof(*cap);
        cap->ctrl = -1;
        cap->ctrl_on = 0;

        /**
         * Run CPUID.0x80000020.0 to check
         * for SMBA allocation capability (bit 2 of ebx)
         */
        lcpuid(0x80000020, 0x0, &res);
        if (!(res.ebx & (1 << 2))) {
                LOG_INFO("CPUID.0x80000008.0: SMBA not supported\n");
                free(cap);
                return PQOS_RETVAL_RESOURCE;
        }

        lcpuid(0x80000020, 1, &res);

        cap->num_classes = (res.edx & 0xffff) + 1;

        /* AMD does not support throttle_max and is_linear. Set it to 0 */
        cap->throttle_max = 0;
        cap->is_linear = 0;

        return PQOS_RETVAL_OK;
}
