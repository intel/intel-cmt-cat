/*
 * BSD LICENSE
 *
 * Copyright(c) 2017-2023 Intel Corporation. All rights reserved.
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
 * @brief Platform QoS utility - capability module
 *
 */
#include "cap.h"

#include "common.h"
#include "main.h"
#include "pqos.h"

#include <inttypes.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __linux__
#include <sys/utsname.h>
#endif

#define BUFFER_SIZE            1024
#define NON_VERBOSE            0
#define VALID_LOCAL_REGION_ID  1
#define VALID_REMOTE_REGION_ID 2

#define UNAVAILABLE_BIT_SUPPORT 1
#define OVERFLOW_BIT_SUPPORT    2

#define MBA_OPTIMAL_CONTROL_WINDOW 1
#define MBA_MINIMUM_CONTROL_WINDOW 2
#define MBA_MAXIMUM_CONTROL_WINDOW 4

#define MSR_STR  "msr"
#define MMIO_STR "mmio"

/**
 * @brief Print line with indentation
 *
 * @param [in] indent indentation level
 * @param [in] format output format to produce output according to,
 *                    variable number of arguments
 */
static void
printf_indent(const unsigned indent, const char *format, ...)
{
        printf("%*s", indent, "");

        va_list args;

        va_start(args, format);
        vprintf(format, args);
        va_end(args);
}

/**
 * @brief Print cache information
 *
 * @param [in] indent indentation level
 * @param [in] cache CPU cache information structure
 */
static void
cap_print_cacheinfo(const unsigned indent, const struct pqos_cacheinfo *cache)
{
        ASSERT(cache != NULL);

        printf_indent(indent, "Num ways: %u\n", cache->num_ways);
        printf_indent(indent, "Way size: %u bytes\n", cache->way_size);
        printf_indent(indent, "Num sets: %u\n", cache->num_sets);
        printf_indent(indent, "Line size: %u bytes\n", cache->line_size);
        printf_indent(indent, "Total size: %u bytes\n", cache->total_size);
}

/**
 * @brief Get event name string
 *
 * @param [in] event mon event type
 *
 * @return Mon event name string
 */
static const char *
get_mon_event_name(int event)
{
        switch (event) {
        case PQOS_MON_EVENT_L3_OCCUP:
                return "LLC Occupancy (LLC)";
        case PQOS_MON_EVENT_LMEM_BW:
                return "Local Memory Bandwidth (LMEM)";
        case PQOS_MON_EVENT_TMEM_BW:
                return "Total Memory Bandwidth (TMEM)";
        case PQOS_MON_EVENT_RMEM_BW:
                return "Remote Memory Bandwidth (RMEM) (calculated)";
        case PQOS_PERF_EVENT_LLC_MISS:
                return "LLC misses";
        case PQOS_PERF_EVENT_LLC_REF:
                return "LLC references";
        case PQOS_PERF_EVENT_IPC:
                return "Instructions/Clock (IPC)";
        case PQOS_PERF_EVENT_LLC_MISS_PCIE_READ:
                return "LLC misses - pcie read";
        case PQOS_PERF_EVENT_LLC_MISS_PCIE_WRITE:
                return "LLC misses - pcie write";
        case PQOS_PERF_EVENT_LLC_REF_PCIE_READ:
                return "LLC references - pcie read";
        case PQOS_PERF_EVENT_LLC_REF_PCIE_WRITE:
                return "LLC references - pcie write";
        default:
                return "unknown";
        }
}

/**
 * @brief Print Monitoring capabilities
 *
 * @param [in] indent indentation level
 * @param [in] mon monitoring capability structure
 * @param [in] verbose enable verbose mode
 */
static void
cap_print_features_mon(const unsigned indent,
                       const struct pqos_cap_mon *mon,
                       const int verbose)
{
        unsigned i;
        char buffer_cache[BUFFER_SIZE] = "\0";
        char buffer_memory[BUFFER_SIZE] = "\0";
        char buffer_other[BUFFER_SIZE] = "\0";

        ASSERT(mon != NULL);

        /**
         * Iterate through all supported monitoring events
         * and generate capability detail string for each of them
         */
        for (i = 0; i < mon->num_events; i++) {
                const struct pqos_monitor *monitor = &(mon->events[i]);
                int iordt = 0;
                char *buffer = NULL;

                switch (monitor->type) {
                case PQOS_MON_EVENT_L3_OCCUP:
                        buffer = buffer_cache;
                        iordt = 1;
                        break;

                case PQOS_MON_EVENT_LMEM_BW:
                case PQOS_MON_EVENT_TMEM_BW:
                case PQOS_MON_EVENT_RMEM_BW:
                        buffer = buffer_memory;
                        iordt = 1;
                        break;

                case PQOS_PERF_EVENT_LLC_MISS:
                case PQOS_PERF_EVENT_LLC_REF:
                case PQOS_PERF_EVENT_IPC:
                case PQOS_PERF_EVENT_LLC_REF_PCIE_READ:
                case PQOS_PERF_EVENT_LLC_MISS_PCIE_READ:
                case PQOS_PERF_EVENT_LLC_REF_PCIE_WRITE:
                case PQOS_PERF_EVENT_LLC_MISS_PCIE_WRITE:
                        buffer = buffer_other;
                        break;

                default:
                        break;
                }

                if (buffer == NULL)
                        continue;

                snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                         "%*s%s\n", indent + 8, "",
                         get_mon_event_name(monitor->type));

                if (iordt)
                        snprintf(buffer + strlen(buffer),
                                 BUFFER_SIZE - strlen(buffer),
                                 "%*s I/O RDT: %s\n", indent + 12, "",
                                 monitor->iordt
                                     ? (mon->iordt_on ? "enabled" : "disabled")
                                     : "unsupported");

                if (verbose) {
                        if (monitor->scale_factor != 0)
                                snprintf(buffer + strlen(buffer),
                                         BUFFER_SIZE - strlen(buffer),
                                         "%*s scale factor: %u\n", indent + 12,
                                         "", monitor->scale_factor);
                        if (monitor->max_rmid != 0)
                                snprintf(buffer + strlen(buffer),
                                         BUFFER_SIZE - strlen(buffer),
                                         "%*s max rmid: %u\n", indent + 12, "",
                                         monitor->max_rmid);
                        if (monitor->counter_length != 0)
                                snprintf(buffer + strlen(buffer),
                                         BUFFER_SIZE - strlen(buffer),
                                         "%*s counter length: %ub\n",
                                         indent + 12, "",
                                         monitor->counter_length);
                }
        }

        printf_indent(indent, "Monitoring\n");

        if (mon->snc_num > 1) {
                const char *snc_state = "";

                switch (mon->snc_mode) {
                case PQOS_SNC_LOCAL:
                        snc_state = "local";
                        break;
                case PQOS_SNC_TOTAL:
                        snc_state = "total";
                        break;
                };
                printf_indent(indent + 4, "Sub-NUMA Clustering: %s\n",
                              snc_state);
        }

        if (strlen(buffer_cache) > 0) {
                printf_indent(indent + 4,
                              "Cache Monitoring Technology (CMT) events:\n");
                printf("%s", buffer_cache);
        }

        if (strlen(buffer_memory) > 0) {
                printf_indent(indent + 4,
                              "Memory Bandwidth Monitoring (MBM) events:\n");
                printf("%s", buffer_memory);
        }

        if (strlen(buffer_other) > 0) {
                printf_indent(indent + 4, "PMU events:\n");
                printf("%s", buffer_other);
        }
}

/**
 * @brief Print L3 CAT capabilities
 *
 * @param [in] indent indentation level
 * @param [in] l3ca L3 CAT capability structure
 * @param [in] verbose enable verbose mode
 */
static void
cap_print_features_l3ca(const unsigned indent,
                        const struct pqos_cap_l3ca *l3ca,
                        const int verbose)
{
        unsigned min_cbm_bits;

        ASSERT(l3ca != NULL);

        printf_indent(indent, "L3 CAT\n");
        printf_indent(indent + 4, "CDP: %s\n",
                      l3ca->cdp ? (l3ca->cdp_on ? "enabled" : "disabled")
                                : "unsupported");
        printf_indent(indent + 4, "Non-Contiguous CBM: %s\n",
                      l3ca->non_contiguous_cbm ? "supported" : "unsupported");
        printf_indent(indent + 4, "I/O RDT: %s\n",
                      l3ca->iordt ? (l3ca->iordt_on ? "enabled" : "disabled")
                                  : "unsupported");
        printf_indent(indent + 4, "Num COS: %u\n", l3ca->num_classes);

        if (!verbose)
                return;

        printf_indent(indent + 4, "Way size: %u bytes\n", l3ca->way_size);
        printf_indent(indent + 4, "Ways contention bit-mask: 0x%lx\n",
                      l3ca->way_contention);
        if (pqos_l3ca_get_min_cbm_bits(&min_cbm_bits) != PQOS_RETVAL_OK)
                printf_indent(indent + 4, "Min CBM bits: unavailable\n");
        else
                printf_indent(indent + 4, "Min CBM bits: %u\n", min_cbm_bits);
        printf_indent(indent + 4, "Max CBM bits: %u\n", l3ca->num_ways);
}

/**
 * @brief Print L2 CAT capabilities
 *
 * @param [in] indent indentation level
 * @param [in] l2ca L2 CAT capability structure
 * @param [in] verbose enable verbose mode
 */
static void
cap_print_features_l2ca(const unsigned indent,
                        const struct pqos_cap_l2ca *l2ca,

                        const int verbose)
{
        unsigned min_cbm_bits;

        ASSERT(l2ca != NULL);

        printf_indent(indent, "L2 CAT\n");
        printf_indent(indent + 4, "CDP: %s\n",
                      l2ca->cdp ? (l2ca->cdp_on ? "enabled" : "disabled")
                                : "unsupported");
        printf_indent(indent + 4, "Non-Contiguous CBM: %s\n",
                      l2ca->non_contiguous_cbm ? "supported" : "unsupported");
        printf_indent(indent + 4, "Num COS: %u\n", l2ca->num_classes);

        if (!verbose)
                return;

        printf_indent(indent + 4, "Way size: %u bytes\n", l2ca->way_size);
        printf_indent(indent + 4, "Ways contention bit-mask: 0x%lx\n",
                      l2ca->way_contention);
        if (pqos_l2ca_get_min_cbm_bits(&min_cbm_bits) != PQOS_RETVAL_OK)
                printf_indent(indent + 4, "Min CBM bits: unavailable\n");
        else
                printf_indent(indent + 4, "Min CBM bits: %u\n", min_cbm_bits);
        printf_indent(indent + 4, "Max CBM bits: %u\n", l2ca->num_ways);
}

/**
 * @brief Print MBA capabilities
 *
 * @param [in] indent indentation level
 * @param [in] mba MBA capability structure
 * @param [in] verbose enable verbose mode
 */
static void
cap_print_features_mba(const unsigned indent,
                       const struct pqos_cap_mba *mba,
                       const int verbose)
{
        const char *mba40_status = "unsupported";

        ASSERT(mba != NULL);

        printf_indent(indent, "Memory Bandwidth Allocation (MBA)\n");
        printf_indent(indent + 4, "Num COS: %u\n", mba->num_classes);

        if (mba->ctrl != -1) {
                const char *ctrl_status = NULL;

                if (!mba->ctrl)
                        ctrl_status = "unsupported";
                else if (!mba->ctrl_on)
                        ctrl_status = "disabled";
                else if (mba->ctrl_on == 1)
                        ctrl_status = "enabled";

                if (ctrl_status)
                        printf_indent(indent + 4, "CTRL: %s\n", ctrl_status);
        }

        if (!verbose)
                return;

        printf_indent(indent + 4, "Granularity: %u\n", mba->throttle_step);
        printf_indent(indent + 4, "Min B/W: %u\n", 100 - mba->throttle_max);
        printf_indent(indent + 4, "Type: %s\n",
                      mba->is_linear ? "linear" : "nonlinear");

        if (mba->mba40) {
                if (mba->mba40_on)
                        mba40_status = "enabled";
                else
                        mba40_status = "disabled";
        }

        printf_indent(indent + 4, "MBA 4.0 extensions: %s\n", mba40_status);
}

/**
 * @brief Print IO RDT devices
 *
 * @param [in] indent indentation level
 * @param [in] devinfo IO RDT topology structure
 */
static void
cap_print_devinfo_channel(const unsigned indent,
                          const struct pqos_devinfo *devinfo)
{
        size_t i;

        for (i = 0; i < devinfo->num_channels; i++) {
                const struct pqos_channel *chan = &devinfo->channels[i];

                printf_indent(indent, "Channel 0x%" PRIx64 "\n",
                              chan->channel_id);

                if (chan->rmid_tagging)
                        printf_indent(indent + 4,
                                      "RMID tagging is supported\n");
                else
                        printf_indent(indent + 4,
                                      "RMID tagging is not supported\n");

                if (chan->clos_tagging)
                        printf_indent(indent + 4,
                                      "CLOS tagging is supported\n");
                else
                        printf_indent(indent + 4,
                                      "CLOS tagging is not supported\n");
        }
}

/**
 * @brief Print IO RDT channels
 *
 * @param [in] indent indentation level
 * @param [in] devinfo IO RDT topology structure
 */
static void
cap_print_devinfo_device(const unsigned indent,
                         const struct pqos_devinfo *devinfo)
{
        size_t i, j;

        for (i = 0; i < devinfo->num_devs; i++) {
                const struct pqos_dev *dev = &devinfo->devs[i];
                uint8_t pci_bus = dev->bdf >> 8;
                uint8_t pci_dev = (dev->bdf & 0xF8) >> 3;
                uint8_t pci_fun = dev->bdf & 0x7;

                printf_indent(indent, "Device %.4X:%.4X:%.2X.%X\n",
                              dev->segment, pci_bus, pci_dev, pci_fun);

                for (j = 0; j < PQOS_DEV_MAX_CHANNELS; j++) {
                        if (!dev->channel[j])
                                continue;
                        printf_indent(indent + 4, "Channel 0x%" PRIx64 "\n",
                                      dev->channel[j]);
                }
        }
}

/**
 * @brief Print SMBA capabilities
 *
 * @param [in] indent indentation level
 * @param [in] smba SMBA capability structure
 * @param [in] verbose verbose mode
 */
static void
cap_print_features_smba(const unsigned indent,
                        const struct pqos_cap_mba *smba,
                        const int verbose)
{
        ASSERT(smba != NULL);

        printf_indent(indent, "Slow Memory Bandwidth Allocation (SMBA)\n");
        printf_indent(indent + 4, "Num COS: %u\n", smba->num_classes);

        if (smba->ctrl != -1) {
                const char *ctrl_status = NULL;

                if (!smba->ctrl)
                        ctrl_status = "unsupported";
                else if (!smba->ctrl_on)
                        ctrl_status = "disabled";
                else if (smba->ctrl_on == 1)
                        ctrl_status = "enabled";

                if (ctrl_status)
                        printf_indent(indent + 4, "CTRL: %s\n", ctrl_status);
        }

        if (!verbose)
                return;

        printf_indent(indent + 4, "Granularity: %u\n", smba->throttle_step);
        printf_indent(indent + 4, "Min B/W: %u\n", 100 - smba->throttle_max);
        printf_indent(indent + 4, "Type: %s\n",
                      smba->is_linear ? "linear" : "nonlinear");
}

/**
 * @brief Print capabilities
 *
 * @param [in] cap system capability structure
 * @param [in] cpu CPU topology structure
 * @param [in] dev IO RDT topology structure
 * @param [in] verbose enable verbose mode
 */
void
cap_print_features(const struct pqos_sysconfig *sys, const int verbose)
{
        unsigned i;
        const struct pqos_capability *cap_mon = NULL;
        const struct pqos_capability *cap_l3ca = NULL;
        const struct pqos_capability *cap_l2ca = NULL;
        const struct pqos_capability *cap_mba = NULL;
        const struct pqos_capability *cap_smba = NULL;
        enum pqos_interface interface;
        int ret;

        if (!sys || !sys->cap || !sys->cpu)
                return;

        for (i = 0; i < sys->cap->num_cap; i++)
                switch (sys->cap->capabilities[i].type) {
                case PQOS_CAP_TYPE_MON:
                        cap_mon = &(sys->cap->capabilities[i]);
                        break;
                case PQOS_CAP_TYPE_L3CA:
                        cap_l3ca = &(sys->cap->capabilities[i]);
                        break;
                case PQOS_CAP_TYPE_L2CA:
                        cap_l2ca = &(sys->cap->capabilities[i]);
                        break;
                case PQOS_CAP_TYPE_MBA:
                        cap_mba = &(sys->cap->capabilities[i]);
                        break;
                case PQOS_CAP_TYPE_SMBA:
                        cap_smba = &(sys->cap->capabilities[i]);
                        break;
                default:
                        break;
                }

        if (cap_mon == NULL && cap_l3ca == NULL && cap_l2ca == NULL &&
            cap_mba == NULL && cap_smba == NULL)
                return;

        ret = pqos_inter_get(&interface);
        if (ret != PQOS_RETVAL_OK)
                return;

        if (interface == PQOS_INTER_MSR || interface == PQOS_INTER_MMIO)
                printf("Hardware capabilities\n");

#ifdef __linux__
        else {
                struct utsname name;

                printf("OS capabilities");
                if (uname(&name) >= 0)
                        printf(" (%s kernel %s)", name.sysname, name.release);
                printf("\n");
        }
#endif

        /**
         * Monitoring capabilities
         */
        if (cap_mon != NULL)
                cap_print_features_mon(4, cap_mon->u.mon, verbose);

        if (cap_l3ca != NULL || cap_l2ca != NULL || cap_mba != NULL)
                printf_indent(4, "Allocation\n");

        /**
         * Cache Allocation capabilities
         */
        if (cap_l3ca != NULL || cap_l2ca != NULL)
                printf_indent(8, "Cache Allocation Technology (CAT)\n");

        if (cap_l3ca != NULL)
                cap_print_features_l3ca(12, cap_l3ca->u.l3ca, verbose);

        if (cap_l2ca != NULL)
                cap_print_features_l2ca(12, cap_l2ca->u.l2ca, verbose);

        /**
         * Memory Bandwidth Allocation capabilities
         */
        if (cap_mba != NULL)
                cap_print_features_mba(8, cap_mba->u.mba, verbose);

        /**
         * Slow Memory Bandwidth Allocation capabilities
         */
        if (cap_smba != NULL)
                cap_print_features_smba(8, cap_smba->u.smba, verbose);

        if (!verbose)
                return;

        printf("Cache information\n");

        if (sys->cpu->l3.detected) {
                printf_indent(4, "L3 Cache\n");
                cap_print_cacheinfo(8, &(sys->cpu->l3));
        }

        if (sys->cpu->l2.detected) {
                printf_indent(4, "L2 Cache\n");
                cap_print_cacheinfo(8, &(sys->cpu->l2));
        }

        if (sys->dev) {
                if (sys->dev->num_channels > 0) {
                        printf("Control channel information\n");
                        cap_print_devinfo_channel(4, sys->dev);
                }

                if (sys->dev->num_devs > 0) {
                        printf("Device information\n");
                        cap_print_devinfo_device(4, sys->dev);
                }
        }
}

static void
cap_print_mrrm_info_regions(const struct pqos_mrrm_info *mrrm)
{
        uint32_t idx = 0;
        uint32_t low = 0;
        uint32_t high = 0;
        uint64_t base_addr = 0;
        uint64_t length = 0;

        printf("Total Memory Regions:  %d\n",
               mrrm->max_memory_regions_supported);
        if (mrrm->flags == 0)
                printf("Region ID Type:        Static\n");
        else
                printf("Region ID Type:        Dynamic\n");

        printf("\n\n");
        for (idx = 0; idx < mrrm->num_mres; idx++) {

                /* Log Region's Base Address */
                low = mrrm->mre[idx].base_address_low;
                high = mrrm->mre[idx].base_address_high;
                base_addr =
                    ((uint64_t)high << (sizeof(uint32_t) * CHAR_BIT)) | low;
                printf("\n\n\nBase Address     : 0x%lx\n", base_addr);

                /* Log Region's  Length */
                low = mrrm->mre[idx].length_low;
                high = mrrm->mre[idx].length_high;
                length =
                    ((uint64_t)high << (sizeof(uint32_t) * CHAR_BIT)) | low;
                printf("Length           : 0x%lx\n", length);

                /* Local Region ID Info */
                if (mrrm->mre[idx].region_id_flags & VALID_LOCAL_REGION_ID) {
                        printf("Local Region ID  : 0x%x\n",
                               mrrm->mre[idx].local_region_id);
                } else
                        printf("Local Region ID  : Not Valid\n");

                /* Remote Region ID Info */
                if (mrrm->mre[idx].region_id_flags & VALID_REMOTE_REGION_ID)
                        printf("Remote Region ID: 0x%x\n",
                               mrrm->mre[idx].remote_region_id);
                else
                        printf("Remote Region ID : Not Valid\n");
        }

        printf("\n");
}

/**
 * @brief Print capabilities
 *
 * @param [in] cap system capability structure
 * @param [in] cpu CPU topology structure
 * @param [in] dev IO RDT topology structure
 * @param [in] verbose enable verbose mode
 */
void
cap_print_mem_regions(const struct pqos_sysconfig *sys)
{
        enum pqos_interface interface;
        int ret;

        if (!sys || !sys->mrrm)
                return;

        ret = pqos_inter_get(&interface);
        if (ret != PQOS_RETVAL_OK)
                return;

        if (interface != PQOS_INTER_MMIO) {
                printf("MMIO interface provides Memory Regions\n");
                return;
        }

        if (sys->mrrm)
                cap_print_mrrm_info_regions(sys->mrrm);
}

static void
cap_print_cpu_agents_info(const struct pqos_cpu_agent_info *cpu_agent)
{
        uint32_t idx = 0;

        printf("    CACD Info:\n");
        printf("        Domanin ID:                                    %d\n",
               cpu_agent->cacd.rmdd_domain_id);
        printf("        Enumeration IDs:                               ");
        for (idx = 0; idx < cpu_agent->cacd.enum_ids_length; idx++)
                printf("0x%x ", cpu_agent->cacd.enumeration_ids[idx]);
        printf("\n\n");

        printf("    CMRC Info:\n");
        printf("        Unavailable Bit Support:                       ");
        if (cpu_agent->cmrc.flags == 1)
                printf("Yes\n");
        else
                printf("No\n");
        printf("        Indexing Function Version:                     %d\n",
               cpu_agent->cmrc.reg_index_func_ver);
        printf("        CMT Register Block Base Address:               0x%lx\n",
               cpu_agent->cmrc.block_base_addr);
        printf("        CMT Register Block Size:                       0x%x\n",
               cpu_agent->cmrc.block_size);
        printf("        CMT Register Clump Size:                       0x%x\n",
               cpu_agent->cmrc.clump_size);
        printf("        CMT Register Clump Stride:                     0x%x\n",
               cpu_agent->cmrc.clump_stride);
        printf("        CMT Counter Upscaling Factor:                  0x%lx\n",
               cpu_agent->cmrc.upscaling_factor);

        printf("\n\n    MMRC Info:\n");
        printf("        Unavailable Bit Support:                       ");
        if (cpu_agent->mmrc.flags & UNAVAILABLE_BIT_SUPPORT)
                printf("Yes\n");
        else
                printf("No\n");

        printf("        Overflow Bit Support:                          ");
        if (cpu_agent->mmrc.flags & OVERFLOW_BIT_SUPPORT)
                printf("Yes\n");
        else
                printf("No\n");

        printf("        Indexing Function Version:                     %d\n",
               cpu_agent->mmrc.reg_index_func_ver);
        printf("        MBM Register Block Base Address:               0x%lx\n",
               cpu_agent->mmrc.reg_block_base_addr);
        printf("        MBM Register Block Size:                       0x%x\n",
               cpu_agent->mmrc.reg_block_size);
        printf("        MBM Counter Width:                             0x%x\n",
               cpu_agent->mmrc.counter_width);
        printf("        MBM Counter Upscaling Factor:                  0x%lx\n",
               cpu_agent->mmrc.upscaling_factor);
        printf("        MBM Correction Factor List Length:             %d\n",
               cpu_agent->mmrc.correction_factor_length);

        if (cpu_agent->mmrc.correction_factor_length != 0) {
                printf("        MBM Correction Factor:                 ");
                idx = 0;
                while (idx < cpu_agent->mmrc.correction_factor_length) {
                        printf("0x%x ", cpu_agent->mmrc.correction_factor[idx]);
                        idx++;
                }
        }

        printf("\n\n");

        printf("\n\n    MARC Info:\n");
        printf("        MBA Optimal Control Window:                    ");
        if (cpu_agent->marc.flags & MBA_OPTIMAL_CONTROL_WINDOW)
                printf("Yes\n");
        else
                printf("No\n");
        printf("        MBA Minimum Control Window:                    ");
        if (cpu_agent->marc.flags & MBA_MINIMUM_CONTROL_WINDOW)
                printf("Yes\n");
        else
                printf("No\n");

        printf("        MBA Maximum Control Window:                    ");
        if (cpu_agent->marc.flags & MBA_MAXIMUM_CONTROL_WINDOW)
                printf("Yes\n");
        else
                printf("No\n");

        printf("        Indexing Function Version:                     %d\n",
               cpu_agent->marc.reg_index_func_ver);
        printf("        MBA Optimal BW Register Block Base Address:    0x%lx\n",
               cpu_agent->marc.opt_bw_reg_block_base_addr);
        printf("        MBA Minimum BW Register Block Base Address:    0x%lx\n",
               cpu_agent->marc.min_bw_reg_block_base_addr);
        printf("        MBA Maximum BW Register Block Base Address:    0x%lx\n",
               cpu_agent->marc.max_bw_reg_block_base_addr);
        printf("        MBA Register Block Size:                       0x%x\n",
               cpu_agent->marc.reg_block_size);
        printf("        MBA BW Control Window Range:                   %d\n",
               cpu_agent->marc.control_window_range);

        printf("\n\n");
}

static void
cap_print_device_agents_info(const struct pqos_device_agent_info *dev_agent)
{
        uint32_t idx = 0;

        printf("\n\n    DACD Info:\n");
        printf("        Domain ID:                                    %d\n",
               dev_agent->dacd.rmdd_domain_id);
        printf("        Number of DASEs:                              %d\n",
               dev_agent->dacd.num_dases);
        for (idx = 0; idx < dev_agent->dacd.num_dases; idx++) {
                printf("\n        DASE %d:\n", idx);
                printf("             Type:             %x\n",
                       dev_agent->dacd.dase[idx].type);
                printf("             Segment Number:   %x\n",
                       dev_agent->dacd.dase[idx].segment_number);
                printf("             Start Bus Number: %x\n",
                       dev_agent->dacd.dase[idx].start_bus_number);
                printf("             Path:             ");
                for (int i = 0; i < dev_agent->dacd.dase[idx].path_length; i++)
                        printf("0x%02x ", dev_agent->dacd.dase[idx].path[i]);
                printf("\n");
        }

        printf("\n\n    CMRD Info:\n");
        printf("        Unavailable Bit Support:                       ");
        if (dev_agent->cmrd.flags == 1)
                printf("Yes\n");
        else
                printf("No\n");
        printf("        Indexing Function Version:                     %d\n",
               dev_agent->cmrd.reg_index_func_ver);
        printf("        Register Base Address:                         "
               "0x%lx\n",
               dev_agent->cmrd.reg_base_addr);
        printf("        Register Block Size:                           0x%x\n",
               dev_agent->cmrd.reg_block_size);
        printf("        CMT Register Clump Size:                       0x%x\n",
               dev_agent->cmrd.offset);
        printf("        CMT Register Clump Size:                       0x%x\n",
               dev_agent->cmrd.clump_size);
        printf("        CMT Counter Upscaling Factor:                  "
               "0x%lx\n",
               dev_agent->cmrd.upscaling_factor);

        printf("\n\n    IBRD Info:\n");
        printf("        Unavailable Bit Support:                       ");
        if (dev_agent->ibrd.flags & UNAVAILABLE_BIT_SUPPORT)
                printf("Yes\n");
        else
                printf("No\n");

        printf("        Overflow Bit Support:                          ");
        if (dev_agent->ibrd.flags & OVERFLOW_BIT_SUPPORT)
                printf("Yes\n");
        else
                printf("No\n");

        printf("        Indexing Function Version:                     %d\n",
               dev_agent->ibrd.reg_index_func_ver);
        printf("        Register Base Address:                         0x%lx\n",
               dev_agent->ibrd.reg_base_addr);
        printf("        Register Block Size:                           0x%x\n",
               dev_agent->ibrd.reg_block_size);
        printf("        Total I/O BW Register Offset:                  0x%x\n",
               dev_agent->ibrd.bw_reg_offset);
        printf("        I/O Miss BW Register Offset:                   0x%x\n",
               dev_agent->ibrd.miss_bw_reg_offset);
        printf("        Total I/O BW  Register Clump Size:             0x%x\n",
               dev_agent->ibrd.bw_reg_clump_size);
        printf("        I/O Miss Register Clump Size:                  0x%x\n",
               dev_agent->ibrd.miss_reg_clump_size);
        printf("        I/O BW Counter Width:                          0x%x\n",
               dev_agent->ibrd.counter_width);
        printf("        I/O BW Counter Upscaling Factor:               0x%lx\n",
               dev_agent->ibrd.upscaling_factor);
        printf("        I/O BW Counter Correction Factor List Length:  %d\n",
               dev_agent->ibrd.correction_factor_length);

        if (dev_agent->ibrd.correction_factor_length != 0) {
                printf("        I/O BW Counter Correction Factor:      ");
                idx = 0;
                while (idx < dev_agent->ibrd.correction_factor_length) {
                        printf("0x%x ", dev_agent->ibrd.correction_factor[idx]);
                        idx++;
                }
        }

        printf("\n\n    CARD Info:\n");
        printf("        Contention Bitmask Valid:                      ");
        if (dev_agent->card.contention_bitmask_valid)
                printf("Yes\n");
        else
                printf("No\n");

        printf("        Non-Contiguous Bitmasks Supported:             ");
        if (dev_agent->card.non_contiguous_cbm)
                printf("Yes\n");
        else
                printf("No\n");

        printf("        Zero-length Bitmask:                           ");
        if (dev_agent->card.zero_length_bitmask)
                printf("Yes\n");
        else
                printf("No\n");

        printf("        Contention Bitmask:                            0x%x\n",
               dev_agent->card.contention_bitmask);
        printf("        Indexing Function Version:                     %d\n",
               dev_agent->card.reg_index_func_ver);
        printf("        Register Base Address:                         0x%lx\n",
               dev_agent->card.reg_base_addr);
        printf("        Register Block Size:                           0x%x\n",
               dev_agent->card.reg_block_size);
        printf("        CAT Register Offset:                           0x%x\n",
               dev_agent->card.cat_reg_offset);
        printf("        CAT Register Block Size:                       0x%x\n",
               dev_agent->card.cat_reg_block_size);

        printf("\n\n");
}

static void
cap_print_erdt_info_topology(const struct pqos_erdt_info *erdt)
{
        uint32_t idx = 0;

        printf("CLOS:           %d\n", erdt->max_clos);
        printf("CPU Agents:     %d\n", erdt->num_cpu_agents);
        printf("Device Agents:  %d\n", erdt->num_dev_agents);

        printf("\n\n\n");
        for (idx = 0; idx < erdt->num_cpu_agents; idx++) {
                printf("\n\nDomain ID %d\n",
                       erdt->cpu_agents[idx].cacd.rmdd_domain_id);
                printf("\n    Type: CPU\n");
                cap_print_cpu_agents_info(
                    (const struct pqos_cpu_agent_info *)&erdt->cpu_agents[idx]);
        }

        printf("\n\n\n");
        for (idx = 0; idx < erdt->num_dev_agents; idx++) {
                printf("\n\nDomain ID %d\n",
                       erdt->dev_agents[idx].dacd.rmdd_domain_id);
                printf("\n    Type: Device\n");
                cap_print_device_agents_info(
                    (const struct pqos_device_agent_info *)&erdt
                        ->dev_agents[idx]);
        }
}

/**
 * @brief Print capabilities
 *
 * @param [in] cap system capability structure
 * @param [in] cpu CPU topology structure
 * @param [in] dev IO RDT topology structure
 * @param [in] verbose enable verbose mode
 */
void
cap_print_topology(const struct pqos_sysconfig *sys)
{
        enum pqos_interface interface;
        int ret;

        if (!sys || !sys->erdt)
                return;

        ret = pqos_inter_get(&interface);
        if (ret != PQOS_RETVAL_OK)
                return;

        if (interface != PQOS_INTER_MMIO) {
                printf("MMIO interface provides Memory Regions\n");
                return;
        }

        if (sys->erdt)
                cap_print_erdt_info_topology(sys->erdt);
}

void
cap_print_io_devs(const struct pqos_sysconfig *sys)
{
        int ret;
        uint32_t i;
        uint16_t j;
        enum pqos_interface interface;
        struct pqos_pci_info pci_info;
        struct pqos_devinfo *dev = NULL;
        const char *interface_str = NULL;
        const struct pqos_capability *cap_l3ca = NULL;

        if (!sys || !sys->dev) {
                printf("IRDT info not available!\n");
                return;
        }

        ret = pqos_inter_get(&interface);
        if (ret != PQOS_RETVAL_OK) {
                printf("unable to get interface\n");
                return;
        }

        if (interface == PQOS_INTER_MSR)
                interface_str = MSR_STR;
        else if (interface == PQOS_INTER_MMIO) {
                if (!sys->erdt) {
                        printf("ERDT info not available!\n");
                        return;
                }
                interface_str = MMIO_STR;
	} else {
                printf("--print-io-devs command is supported in msr and mmio "
                       "interfaces only\n");
                return;
        }

        for (i = 0; sys->cap && (i < sys->cap->num_cap); i++)
                switch (sys->cap->capabilities[i].type) {
                case PQOS_CAP_TYPE_L3CA:
                        cap_l3ca = &(sys->cap->capabilities[i]);
                        break;
                default:
                        break;
                }

        printf("Enable I/O RDT            : pqos --iface=%s l3iordt-on\n",
               interface_str);
        printf("Disable I/O RDT           : pqos --iface=%s l3iordt-off\n",
               interface_str);
        printf("Reset I/O RDT Allocation  : pqos --iface=%s --alloc-reset or "
               "pqos --iface=%s -R\n",
               interface_str, interface_str);
        printf("Reset I/O RDT Monitoring  : pqos --iface=%s --mon-reset or "
               "pqos --iface=%s -r -d\n",
               interface_str, interface_str);

        dev = sys->dev;
        for (i = 0; i < dev->num_devs; i++) {
                printf("\n%.4x:%.2x:%.2x.%x: ", dev->devs[i].segment,
                       BDF_BUS(dev->devs[i].bdf), BDF_DEV(dev->devs[i].bdf),
                       BDF_FUNC(dev->devs[i].bdf));

                ret = pqos_io_devs_get(&pci_info, dev->devs[i].segment,
                                       dev->devs[i].bdf);
                if (ret != PQOS_RETVAL_OK)
                        printf("Unable to get I/O device %.4x:%.2x:%.2x.%x PCI "
                               "information\n",
                               dev->devs[i].segment, BDF_BUS(dev->devs[i].bdf),
                               BDF_DEV(dev->devs[i].bdf),
                               BDF_FUNC(dev->devs[i].bdf));

                printf("%s: %s %s",
                       pci_info.subclass_name[0] ? pci_info.subclass_name
                                                 : "PCI device",
                       pci_info.vendor_name[0] ? pci_info.vendor_name : "",
                       pci_info.device_name[0] ? pci_info.device_name : "");

                if (pci_info.revision != 0)
                        printf(" (rev %02x)", pci_info.revision);
                printf("\n");

                if (pci_info.is_pcie)
                        printf("\tPCIe                 : %s\n", pci_info.pcie_type);
                else
                        printf("\tConventional PCI\n");
                if (pci_info.numa >= 0)
                        printf("\tNUMA                 : %d\n", pci_info.numa);
                if (pci_info.kernel_driver[0])
                        printf("\tKernel driver in use : %s\n",
                               pci_info.kernel_driver);

                if (interface == PQOS_INTER_MMIO)
                        printf("\tDomain ID            : 0x%x\n", pci_info.domain_id);

                printf("\tAssociated Channels  : ");
                for (j = 0; j < pci_info.num_channels; j++)
                        if (pci_info.channels[j] > 0)
                                printf("0x%lx         ", pci_info.channels[j]);
                printf("\n");

                printf("\tMMIO Addresses       : ");
                for (j = 0; j < pci_info.num_channels; j++)
                        printf("0x%lx       ", pci_info.mmio_addr[j]);
                printf("\n");

                printf("\tMonitoring Commands:\n");
                for (j = 0; j < PQOS_DEV_MAX_CHANNELS; j++) {
                        if (dev->devs[i].channel[j] > 0) {
                                printf("\t\tpqos --iface=%s --mon-dev=all:"
                                       "%.4x:%.2x:%.2x.%x@%d\n",
                                       interface_str, dev->devs[i].segment,
                                       BDF_BUS(dev->devs[i].bdf),
                                       BDF_DEV(dev->devs[i].bdf),
                                       BDF_FUNC(dev->devs[i].bdf), j);
                                printf("\t\tpqos --iface=%s --mon-channel="
                                       "all:0x%lx\n",
                                       interface_str, dev->devs[i].channel[j]);
                        }

                        if (dev->devs[i].channel[j + 1] > 0 &&
                            ((j + 1) < PQOS_DEV_MAX_CHANNELS))
                                printf("\n");
                }

                if (interface == PQOS_INTER_MSR && cap_l3ca != NULL)
                        printf("\n\tAvailable CLOS: 0 to %d\n",
                               cap_l3ca->u.l3ca->num_classes - 1);
                else if (interface == PQOS_INTER_MMIO)
                        printf("\n\tAvailable CLOS: 0 to %d\n",
                               sys->erdt->max_clos - 1);

                printf("\tAllocation Commands:\n");
                for (j = 0; j < pci_info.num_channels; j++) {
                        if (pci_info.channels[j] > 0) {
                                printf("\t\tpqos --iface=%s -a channel:<CLOS>="
                                       "%.4x:%.2x:%.2x.%x@%d\n",
                                       interface_str, dev->devs[i].segment,
                                       BDF_BUS(dev->devs[i].bdf),
                                       BDF_DEV(dev->devs[i].bdf),
                                       BDF_FUNC(dev->devs[i].bdf), j);
                                printf("\t\tpqos --iface=%s -a channel:<CLOS>="
                                       "0x%lx\n",
                                       interface_str, pci_info.channels[j]);
                        }

                        if (pci_info.channels[j + 1] > 0 &&
                            ((j + 1) < PQOS_DEV_MAX_CHANNELS))
                                printf("\n");
                }
                printf("\n\tAfter/Before allocation commands, assign required "
                       "Cache Ways to CLOS\n");
                if (interface == PQOS_INTER_MSR && cap_l3ca != NULL)
                        printf("\tAvailable Cache Ways: %d\n",
                               cap_l3ca->u.l3ca->num_ways);
                if (interface == PQOS_INTER_MSR) {
                        printf("\tFor example, set COS 14 to the first 4 "
                               "L3 cache ways and COS 10 to the next 8 "
                               "L3 cache ways\n");
                        printf("\tpqos --iface=%s -e \"llc:14=0x000f;"
                               "llc:10=0x0ff0;\"\n",
                               interface_str);
                } else if (interface == PQOS_INTER_MMIO) {
                        for (j = 0; j < sys->erdt->num_dev_agents; j++) {
                                if (pci_info.domain_id ==
                                    sys->erdt->dev_agents[j].rmdd.domain_id)
                                        printf("\tAvailable Cache Ways: %d\n",
                                               sys->erdt->dev_agents[j]
                                                   .rmdd.num_io_l3_ways);
                        }

                        printf("\tFor example, set COS 14 to the first 4 "
                               "L3 cache ways and COS 10 to the next 8 "
                               "L3 cache ways in Device Domain 0x%x\n",
                               pci_info.domain_id);
                        printf("\tpqos --iface=%s --alloc-domain-id=0x%x -e "
                               "\"llc:14=0x000f;llc:10=0x0ff0;\"\n",
                               interface_str, pci_info.domain_id);
                }
        }
        printf("\n");
}
