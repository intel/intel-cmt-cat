/*
 * BSD LICENSE
 *
 * Copyright(c) 2017-2026 Intel Corporation. All rights reserved.
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

#define BUFFER_SIZE 1024
#define NON_VERBOSE 0

#define UNAVAILABLE_BIT_SUPPORT 1
#define OVERFLOW_BIT_SUPPORT    2

#define MBA_OPTIMAL_CONTROL_WINDOW 1
#define MBA_MINIMUM_CONTROL_WINDOW 2
#define MBA_MAXIMUM_CONTROL_WINDOW 4

/* the suggested allocation example shows one mask next to another, so it needs
 * two classes and enough cache ways to give each of them a mask the hardware
 * accepts
 */
#define EXAMPLE_NUM_CLASSES 2

struct pci_dev {
        uint16_t segment; /**< PCI segment */
        uint16_t bdf;     /**< Bus Device Function */
};

static struct pci_dev *sel_pci_dev = NULL;
static unsigned int sel_pci_dev_count = 0;

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
        case PQOS_MON_EVENT_CORE_ENERGY:
                return "Core Energy";
        case PQOS_MON_EVENT_ACTIVITY:
                return "Activity Counter";
        case PQOS_MON_EVENT_POWER:
                return "Power (synthetic)";
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
        char buffer_telemetry[BUFFER_SIZE] = "\0";

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

                case PQOS_MON_EVENT_CORE_ENERGY:
                case PQOS_MON_EVENT_ACTIVITY:
                case PQOS_MON_EVENT_POWER:
                        buffer = buffer_telemetry;
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

        if (strlen(buffer_telemetry) > 0) {
                printf_indent(indent + 4, "AET telemetry events:\n");
                printf("%s", buffer_telemetry);
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
        printf_indent(indent + 4, "Num CLOS: %u\n", l3ca->num_classes);

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
        printf_indent(indent + 4, "Num CLOS: %u\n", l2ca->num_classes);

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
        printf_indent(indent + 4, "Num CLOS: %u\n", mba->num_classes);

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
        printf_indent(indent + 4, "Num CLOS: %u\n", smba->num_classes);

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

/**
 * @brief The last address of a range, where the range has one
 *
 * A nonzero length is not enough: MRRM can declare a base and length whose sum
 * leaves the address space, and adding them would print an end address below
 * the start. Such a range is reported as having no valid end rather than a
 * fabricated one.
 *
 * The library applies the same rule to the same table in range_last(), and the
 * two are separate on purpose: this is the utility, which reads the library's
 * public structures and does not share its internals, and exporting six lines
 * of arithmetic as API to save repeating them would be the worse trade.
 *
 * @param [in] base start of the range
 * @param [in] length its length
 * @param [out] last its last address
 *
 * @retval 1 the range has a representable last address
 * @retval 0 it does not
 */
static int
cap_range_last(const uint64_t base, const uint64_t length, uint64_t *last)
{
        if (length == 0 || base > UINT64_MAX - (length - 1))
                return 0;

        *last = base + (length - 1);

        return 1;
}

/**
 * @brief Prints one memory range entry
 *
 * Shared by the description the library builds and the fallback below it, so
 * that a range reads the same either way.
 *
 * @param [in] index which entry it is, in table order
 * @param [in] base start of the range
 * @param [in] length its length
 * @param [in] local_valid the platform marked the local region ID valid
 * @param [in] local_id that ID
 * @param [in] remote_valid the platform marked the remote region ID valid
 * @param [in] remote_id that ID
 */
static void
cap_print_range(const unsigned index,
                const uint64_t base,
                const uint64_t length,
                const int local_valid,
                const uint8_t local_id,
                const int remote_valid,
                const uint8_t remote_id)
{
        uint64_t last;

        printf("  [%u]\n", index);
        printf("    Base Address     : 0x%016" PRIx64 "\n", base);
        printf("    Length           : 0x%016" PRIx64 "\n", length);
        if (cap_range_last(base, length, &last))
                printf("    End Address      : 0x%016" PRIx64 "\n", last);
        else
                printf("    End Address      : Not Valid\n");

        if (local_valid)
                printf("    Local Region ID  : 0x%x\n", local_id);
        else
                printf("    Local Region ID  : Not Valid\n");

        if (remote_valid)
                printf("    Remote Region ID : 0x%x\n", remote_id);
        else
                printf("    Remote Region ID : Not Valid\n");
}

/**
 * @brief Prints the ranges MRRM reported, in the order the table lists them
 *
 * The indices printed here are what the per-region listing refers to, so a
 * reader can follow a range from the raw table into the region carrying it.
 *
 * @param [in] regions the memory regions
 */
static void
cap_print_raw_ranges(const struct pqos_mem_regions *regions)
{
        unsigned idx;

        printf("Raw Memory Range Entries:\n");

        for (idx = 0; idx < regions->num_range_entries; idx++) {
                const struct pqos_mem_range *r = &regions->range[idx];

                cap_print_range(idx, r->base_address, r->length,
                                r->local_region_id_valid, r->local_region_id,
                                r->remote_region_id_valid, r->remote_region_id);

                printf("\n");
        }
}

/**
 * @brief Prints what the ACPI tables say about one region
 *
 * A field no table described is named as such rather than printed as a zero.
 * SRAT is three-valued for that reason: whether it describes a CXL window is up
 * to the platform, so no entry there is not the same answer as "no".
 *
 * @param [in] regions the description the region came from, for what the
 *             platform's tables were able to say at all
 * @param [in] region the region
 */
static void
cap_print_region_acpi(const struct pqos_mem_regions *regions,
                      const struct pqos_mem_region *region)
{
        printf("  ACPI:\n");
        /* three answers per table, not two. A table the platform does not have,
         * or that could not be read, has said nothing about this region - which
         * is not the same as having been read and not describing it, and a
         * reader cannot tell those apart from a "No"
         */
        printf("    SRAT Match        : ");
        if (!regions->srat_available)
                printf("Not Available\n");
        else if (region->srat_match)
                printf("Yes\n");
        else if (region->type == PQOS_MEM_REGION_CXL)
                printf("Optional / Platform dependent\n");
        else
                printf("No\n");

        /* HMAT is asked about the target domain SRAT gives the region, so
         * where SRAT gives none the table was never consulted and a "No" here
         * would be the same unchecked absence as one about a table that is not
         * there. The reason line below says which of the two it was
         */
        printf("    HMAT Match        : %s\n",
               (!regions->hmat_available || !region->srat_match)
                   ? "Not Available"
                   : (region->hmat_match ? "Yes" : "No"));
        printf("    CEDT Match        : %s\n",
               !regions->cedt_available ? "Not Available"
                                        : (region->cedt_match ? "Yes" : "No"));

        printf("\n  Proximity:\n");
        if (region->proximity_valid) {
                printf("    Initiator Domain  : %u\n",
                       region->initiator_domain);
                printf("    Target Domain     : %u\n", region->target_domain);
                printf("    Mapping           : Initiator%u -> Target%u\n",
                       region->initiator_domain, region->target_domain);
        } else if (region->srat_match) {
                /* SRAT placed this memory in a domain, but no HMAT entry pairs
                 * an initiator with that target, so half the mapping is unknown
                 */
                printf("    Initiator Domain  : Not Available\n");
                printf("    Target Domain     : %u\n", region->target_domain);
                printf("    Mapping           : Not Available\n");
        } else {
                printf("    Initiator Domain  : Not Available\n");
                printf("    Target Domain     : Not Available\n");
                printf("    Mapping           : Not Available\n");
        }

        if (region->cedt_match) {
                printf("\n  CXL Windows:\n");
                printf("    CFMWS Match       : %s\n",
                       region->cfmws_match ? "Yes" : "No");
                printf("    CXL Range Match   : %s\n",
                       region->cxl_range_match ? "Yes" : "No");
                /* Three counts, one line each, and all three wherever this
                 * block appears - which is wherever CEDT describes the region.
                 * A platform with no CXL window has no such block at all,
                 * and printing three zeroes under a heading about windows that
                 * do not exist would be a statement about nothing: the counts
                 * are counts of ranges in windows.
                 *
                 * What the three of them are is memory range entries, labelled
                 * as such: these tables enumerate no devices, so a count of
                 * ranges cannot be called a count of devices without claiming
                 * what ACPI does not say.
                 *
                 * What separates them is who said what. SRAT enabling the
                 * memory is firmware saying it is there; SRAT describing it and
                 * leaving it disabled is firmware reserving the window; SRAT
                 * saying nothing is neither, and folding that into either of
                 * the others would attribute to firmware a statement it never
                 * made.
                 *
                 * None of the three is qualified in parentheses, and none is
                 * omitted for being zero, because the functional tests parse
                 * these lines: within a block that is there, a field that comes
                 * and goes is a field a test has to guess at.
                 */
                printf("    Active MREs       : %u\n", region->active_mres);
                printf("    Reserved MREs     : %u\n", region->reserved_mres);
                printf("    Unclassified MREs : %u\n",
                       region->unclassified_mres);
        }
}

/**
 * @brief Prints one of the four locality numbers
 *
 * A figure HMAT qualifies by a minimum transfer size is not an unconditional
 * one, so the line says so where the platform said so. The minimum is printed
 * as the byte the table carries rather than converted to a size: what that byte
 * counts is the table's business, and naming a unit for it here would state
 * more than the platform did.
 *
 * @param [in] loc the locality the number came from
 * @param [in] label the name of the number, padded by the caller
 * @param [in] unit what the number is counted in
 * @param [in] valid whether it is known
 * @param [in] value the number
 * @param [in] sized whether it holds only above a minimum transfer size
 * @param [in] min_transfer that minimum, as ACPI encodes it
 * @param [in] non_sequential whether it describes non-sequential transfers
 */
static void
cap_print_locality_value(const struct pqos_mem_locality *loc,
                         const char *label,
                         const char *unit,
                         const int valid,
                         const uint64_t value,
                         const int sized,
                         const uint8_t min_transfer,
                         const int non_sequential)
{
        printf("    %-18s: ", label);

        if (!valid) {
                printf("Not Available\n");
                return;
        }

        printf("Initiator-Target[%u-%u]: %" PRIu64 " %s", loc->initiator_domain,
               loc->target_domain, value, unit);

        if (sized)
                printf(", for transfers of encoded size %u and above",
                       min_transfer);
        if (non_sequential)
                printf(", for non-sequential transfers");

        printf("\n");
}

/**
 * @brief Prints the HMAT numbers of one region
 *
 * @param [in] regions the description the region came from, for what the
 *             platform's tables were able to say at all
 * @param [in] region the region
 */
static void
cap_print_region_locality(const struct pqos_mem_regions *regions,
                          const struct pqos_mem_region *region)
{
        const struct pqos_mem_locality *loc = &region->locality;

        printf("\n  HMAT Locality:\n");

        /* each number says whether it was found, because HMAT describes the
         * four independently: a platform can carry read latency and no write
         * bandwidth, and printing the one it does not carry as zero would read
         * as a measurement
         */
        cap_print_locality_value(
            loc, "Read  Latency", "nsec", loc->read_latency_valid,
            loc->read_latency_ns, loc->read_latency_min_transfer_qualified,
            loc->read_latency_min_transfer, loc->read_latency_non_sequential);
        cap_print_locality_value(
            loc, "Write Latency", "nsec", loc->write_latency_valid,
            loc->write_latency_ns, loc->write_latency_min_transfer_qualified,
            loc->write_latency_min_transfer, loc->write_latency_non_sequential);
        cap_print_locality_value(
            loc, "Read  Bandwidth", "MB/s", loc->read_bandwidth_valid,
            loc->read_bandwidth_mbs, loc->read_bandwidth_min_transfer_qualified,
            loc->read_bandwidth_min_transfer,
            loc->read_bandwidth_non_sequential);
        cap_print_locality_value(loc, "Write Bandwidth", "MB/s",
                                 loc->write_bandwidth_valid,
                                 loc->write_bandwidth_mbs,
                                 loc->write_bandwidth_min_transfer_qualified,
                                 loc->write_bandwidth_min_transfer,
                                 loc->write_bandwidth_non_sequential);

        /* the reason belongs with an answer that is entirely missing; where
         * some of the numbers are there, the missing ones say so themselves
         */
        if (!loc->valid) {
                printf("    Reason            : ");
                /* Five ways to have no number, told apart, and none of them
                 * mentions a device - whether one is present is not something
                 * these tables answer.
                 *
                 * The order is the order the lookup happens in, so that each
                 * line reports the step that actually stopped. A table the
                 * platform does not have, or that could not be read, comes
                 * first of all: saying "no entry" about a table nothing looked
                 * in would assert an absence that was never checked, and so
                 * would saying it about HMAT when SRAT never produced a target
                 * domain to look up. With a target and no initiator paired with
                 * it, the pair does not exist to be looked up. And where the
                 * pair is described, a value marked unavailable is the platform
                 * saying it has no number, a value that will not scale is a
                 * number this report cannot state, and neither is the same as
                 * no matrix carrying the pair.
                 */
                if (!regions->srat_available)
                        printf("SRAT is not available, so these ranges have no "
                               "proximity domain to look up\n");
                else if (!region->srat_match)
                        printf("SRAT gives these ranges no proximity domain to "
                               "look up\n");
                else if (!regions->hmat_available)
                        printf("HMAT is not available, so the target domain "
                               "has no locality to look up\n");
                else if (!region->hmat_match)
                        printf("No HMAT entry pairs an initiator with target "
                               "domain %u\n",
                               region->target_domain);
                else if (loc->values_unrepresentable)
                        printf("HMAT value for this pair does not fit once "
                               "scaled by its base unit\n");
                else if (loc->values_unavailable)
                        printf("HMAT marks the values for this pair "
                               "unavailable\n");
                else
                        printf("HMAT describes this pair but no locality "
                               "matrix carries it\n");
        }
}

/**
 * @brief Prints one region: its ranges, and what the tables say about them
 *
 * @param [in] regions all the regions, for the range array
 * @param [in] index which region to print
 */
static void
cap_print_region(const struct pqos_mem_regions *regions, const unsigned index)
{
        const struct pqos_mem_region *region = &regions->region[index];
        unsigned i;

        printf("\nREGION %u:\n", index);
        printf("  Type               : ");
        switch (region->type) {
        case PQOS_MEM_REGION_CXL:
                printf("CXL Reserved\n");
                break;
        case PQOS_MEM_REGION_LOCAL:
                printf("DDR / Local Memory\n");
                break;
        default:
                /* no table described these ranges well enough to say. Printing
                 * one of the two above would be a guess dressed as a finding
                 */
                printf("Unknown - not described by SRAT or CEDT\n");
                break;
        }
        printf("  Local Region ID    : 0x%x\n", region->local_region_id);
        printf("  Range Count        : %u\n", region->num_ranges);
        if (region->total_size_valid)
                printf("  Total Size         : 0x%016" PRIx64 "\n",
                       region->total_size);
        else
                printf("  Total Size         : Not Valid\n");

        printf("\n  Ranges:\n");
        for (i = 0; i < region->num_ranges; i++) {
                const unsigned idx = region->range_index[i];
                const struct pqos_mem_range *r = &regions->range[idx];
                uint64_t last;

                if (cap_range_last(r->base_address, r->length, &last))
                        printf("    [%u] 0x%016" PRIx64 " - 0x%016" PRIx64
                               ", Size: 0x%016" PRIx64 "\n",
                               idx, r->base_address, last, r->length);
                else
                        printf("    [%u] 0x%016" PRIx64
                               " - Not Valid, Size: 0x%016" PRIx64 "\n",
                               idx, r->base_address, r->length);
        }

        printf("\n");
        cap_print_region_acpi(regions, region);
        cap_print_region_locality(regions, region);
        printf("\n");
}

/**
 * @brief Prints the memory region report
 *
 * @param [in] regions the memory regions
 */
static void
cap_print_mem_region_info(const struct pqos_mem_regions *regions)
{
        unsigned idx;

        printf("\nMemory Region Discovery\n");
        printf("-----------------------\n");
        printf("Region ID Type       : %s\n",
               regions->dynamic_region_ids ? "Dynamic" : "Static");
        /* what the platform says it supports, and what its table describes.
         * Both, because they are different questions and a report that answered
         * only the second would drop the MRRM header's own figure
         */
        printf("Regions Supported    : %u\n", regions->max_regions_supported);
        printf("Total Local Regions  : %u\n", regions->num_regions);
        printf("Total Range Entries  : %u\n", regions->num_range_entries);
        printf("\n");

        cap_print_raw_ranges(regions);

        for (idx = 0; idx < regions->num_regions; idx++)
                cap_print_region(regions, idx);
}

/**
 * @brief Prints the ranges MRRM reported, where nothing else could be built
 *
 * The description the library builds from MRRM can fail to be built at all -
 * there is memory to allocate for it - and MRRM itself is parsed by then, so
 * its ranges are known. Printing them is better than printing nothing: the
 * header's two answers and every range entry are exactly as available as they
 * were, and what is missing - the grouping into regions, and everything the
 * other tables would have said - is marked unavailable in the same words the
 * described report uses, so the two have the same fields either way.
 *
 * @param [in] mrrm the ranges, as MRRM reported them
 */
static void
cap_print_mrrm_ranges(const struct pqos_mrrm_info *mrrm)
{
        unsigned idx;

        printf("\nMemory Region Discovery\n");
        printf("-----------------------\n");
        printf("Region ID Type       : %s\n",
               mrrm->flags != 0 ? "Dynamic" : "Static");
        printf("Regions Supported    : %u\n",
               (unsigned)mrrm->max_memory_regions_supported);
        printf("Total Local Regions  : Not Available\n");
        printf("Total Range Entries  : %u\n", (unsigned)mrrm->num_mres);
        printf("\n");

        printf("Raw Memory Range Entries:\n");

        for (idx = 0; idx < mrrm->num_mres; idx++) {
                const struct pqos_mre_info *mre = &mrrm->mre[idx];
                const uint64_t base = ((uint64_t)mre->base_address_high << 32) |
                                      mre->base_address_low;
                const uint64_t length =
                    ((uint64_t)mre->length_high << 32) | mre->length_low;

                cap_print_range(idx, base, length,
                                (mre->region_id_flags &
                                 PQOS_MRE_VALID_LOCAL_REGION_ID) != 0,
                                mre->local_region_id,
                                (mre->region_id_flags &
                                 PQOS_MRE_VALID_REMOTE_REGION_ID) != 0,
                                mre->remote_region_id);

                printf("\n");
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

        /* the regions, with what the ACPI tables say about them. Where those
         * tables could not be read the ranges MRRM reported are still listed,
         * with the fields they would have filled marked unavailable - and where
         * even the description could not be allocated, the ranges are listed
         * from MRRM directly, since a report of nothing at all would hide facts
         * the library holds
         */
        if (sys->mem_regions != NULL)
                cap_print_mem_region_info(sys->mem_regions);
        else
                cap_print_mrrm_ranges(sys->mrrm);
}

static void
cap_print_cpu_agents_info(const struct pqos_cpu_agent_info *cpu_agent)
{
        uint32_t idx = 0;

        printf("    CACD Info:\n");
        printf("        Domain ID:                                    %d\n",
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
        printf("        CMT Register Offset:                           0x%x\n",
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

        printf("Num CLOS:       %d\n", erdt->max_clos);
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

/**
 * @brief Release the device selection of the --print-io-dev option
 *
 * parse_io_dev() grows the selection once per option, so it is freed and the
 * count reset on every path out of cap_print_io_dev(), which is the only
 * consumer. Leaving it behind would leak it and would show the devices of the
 * previous call again if the function ran twice in one process.
 */
static void
release_io_dev_selection(void)
{
        free(sel_pci_dev);
        sel_pci_dev = NULL;
        sel_pci_dev_count = 0;
}

/**
 * @brief Print an allocation example the platform can run
 *
 * The classes and the ways are taken from the counts printed above the example,
 * so the command can be run as it stands. The example this replaced was fixed
 * at CLOS 14 and 10 with the masks 0x000f and 0x0ff0, which needs twelve cache
 * ways: on a device that has eight the second allocation failed and left the
 * first one applied.
 *
 * @param [in] num_classes number of classes of service the domain reports
 * @param [in] num_ways number of L3 cache ways the domain reports
 * @param [in] min_cbm_bits fewest bits a mask may have, or 0 where no minimum
 *             could be established, in which case nothing is suggested
 * @param [in] domain_id device domain of the example, or -1 for an interface
 *             whose allocation command does not name one
 */
static void
print_alloc_ways_example(unsigned num_classes,
                         unsigned num_ways,
                         unsigned min_cbm_bits,
                         int domain_id)
{
        unsigned first_ways;
        unsigned second_ways;
        unsigned first_clos;
        unsigned second_clos;
        unsigned long long first_mask;
        unsigned long long second_mask;

        /* nothing is suggested where the narrowest usable mask is unknown,
         * since a suggestion built on a guess is what this replaced
         */
        if (min_cbm_bits == 0)
                return;

        /* a class for each half, and enough ways that the smaller half still
         * holds the fewest bits a mask may have
         */
        if (num_classes < EXAMPLE_NUM_CLASSES ||
            num_ways < EXAMPLE_NUM_CLASSES * min_cbm_bits)
                return;

        first_ways = num_ways / EXAMPLE_NUM_CLASSES;
        second_ways = num_ways - first_ways;

        /* the highest classes, to leave CLOS 0 alone where there are more
         * than two of them
         */
        first_clos = num_classes - 1;
        second_clos = num_classes - 2;
        first_mask = (1ULL << first_ways) - 1;
        second_mask = ((1ULL << second_ways) - 1) << first_ways;

        printf("\tFor example, set CLOS %u to the first %u L3 cache ways and "
               "CLOS %u to the next %u L3 cache ways",
               first_clos, first_ways, second_clos, second_ways);
        if (domain_id >= 0)
                printf(" in Device Domain 0x%x", (unsigned)domain_id);
        printf("\n");

        printf("\tpqos ");
        if (domain_id >= 0)
                printf("--alloc-domain-id=0x%x ", (unsigned)domain_id);
        printf("-e \"llc:%u=%#llx;llc:%u=%#llx;\"\n", first_clos, first_mask,
               second_clos, second_mask);
}

/**
 * @brief Determine the fewest bits an L3 CAT mask may have
 *
 * On the MSR interface pqos_l3ca_get_min_cbm_bits() is not a passive query: it
 * finds a free class of service, writes masks of growing width, reads them back
 * and restores the original. It is therefore resolved once per command rather
 * than once per device, so that a report over every device does not repeat
 * those transient allocation changes. A failure of that probe leaves the
 * minimum unknown, which is not the same as there being none, so the two are
 * told apart by the interface: only MMIO is known to have no minimum, and both
 * cases otherwise come back as PQOS_RETVAL_RESOURCE.
 *
 * @param [in] interface the interface in use
 *
 * @return fewest bits a mask may have, or 0 where no minimum could be
 *         established, in which case nothing should be suggested
 */
static unsigned
get_min_cbm_bits(enum pqos_interface interface)
{
        unsigned min_cbm_bits;

        /* the CARD block of a device agent has no such minimum, and the MMIO
         * interface installs no operation to ask for one, so one way per mask
         * is acceptable there
         */
        if (interface == PQOS_INTER_MMIO)
                return 1;

        /* everywhere else this is an active probe, and a failure means the
         * minimum is unknown rather than absent
         */
        if (pqos_l3ca_get_min_cbm_bits(&min_cbm_bits) != PQOS_RETVAL_OK)
                return 0;

        return min_cbm_bits;
}

/**
 * @brief Print information about I/O device from ERDT & IRDT ACPI tables
 *
 * @param [in] segment PCI Device's segment
 * @param [in] bdf PCI Device's Bus, Device and Function
 * @param [in] min_cbm_bits fewest bits an L3 CAT mask may have, as returned by
 *             get_min_cbm_bits(), so 0 means it could not be established rather
 *             than that there is none. Passed in rather than queried here,
 *             because on the MSR interface the query writes and restores a mask
 *             and this runs once per device
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 * @retval PQOS_RETVAL_RESOURCE if the device information cannot be read, e.g.
 *         when no such PCI device is present, and when the device is present
 *         but is not an I/O RDT device
 */
static int
print_io_dev(const struct pqos_sysconfig *sys,
             const struct pqos_capability *cap_l3ca,
             enum pqos_interface interface,
             uint16_t segment,
             uint16_t bdf,
             unsigned min_cbm_bits)
{
        int ret;
        unsigned int j;
        struct pqos_pci_info pci_info;

        memset(&pci_info, 0, sizeof(pci_info));

        printf("\n%.4x:%.2x:%.2x.%x ", segment, BDF_BUS(bdf), BDF_DEV(bdf),
               BDF_FUNC(bdf));

        ret = pqos_io_devs_get(&pci_info, segment, bdf);
        if (ret != PQOS_RETVAL_OK) {
                printf("Unable to get I/O device %.4x:%.2x:%.2x.%x PCI "
                       "information\n",
                       segment, BDF_BUS(bdf), BDF_DEV(bdf), BDF_FUNC(bdf));
                return ret;
        }

        printf("%s:", pci_info.subclass_name[0] ? pci_info.subclass_name
                                                : "PCI device");
        if (pci_info.vendor_name[0])
                printf(" %s", pci_info.vendor_name);
        if (pci_info.device_name[0])
                printf(" %s", pci_info.device_name);

        if (pci_info.revision != 0)
                printf(" (rev %02x)", pci_info.revision);
        printf("\n");

        /* An I/O RDT device is one the platform reports a channel for, since a
         * channel is what monitoring counts and what allocation assigns. With
         * none of them the fields, the commands and the class of service and
         * cache way counts below would describe operations that cannot be run
         * on this device, so it is reported the way an absent device is, with a
         * status the caller turns into a non-zero exit code. Which of the two
         * reasons it is gets named, because the commands printed above resolve
         * one of them and cannot do anything about the other
         */
        if (pci_info.num_channels == 0) {
                if (sys->dev->num_devs == 0)
                        printf("\tNo I/O RDT channels. The platform reports no "
                               "I/O RDT device, so I/O RDT is either not "
                               "supported or not enabled\n");
                else
                        printf("\tNo I/O RDT channels, so this is not an I/O "
                               "RDT device\n");

                return PQOS_RETVAL_RESOURCE;
        }

        if (pci_info.is_pcie)
                printf("\tPCIe                 : %s\n", pci_info.pcie_type);
        else
                printf("\tConventional PCI\n");
        if (pci_info.numa >= 0)
                printf("\tNUMA                 : %d\n", pci_info.numa);
        if (pci_info.kernel_driver[0])
                printf("\tKernel driver in use : %s\n", pci_info.kernel_driver);

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

        printf("\tCXL Devices Presence : ");
        if (pci_info.cxld == 1)
                printf("Yes\n");
        else
                printf("No\n");

        printf("\n\tMonitoring Commands:\n");
        for (j = 0; j < pci_info.num_channels; j++) {
                if (pci_info.channels[j] > 0) {
                        printf("\t\tpqos --mon-dev=all:"
                               "%.4x:%.2x:%.2x.%x@%d\n",
                               segment, BDF_BUS(bdf), BDF_DEV(bdf),
                               BDF_FUNC(bdf), j);
                        printf("\t\tpqos --mon-channel=all:0x%lx\n",
                               pci_info.channels[j]);
                }

                if (((j + 1) < pci_info.num_channels) &&
                    pci_info.channels[j + 1] > 0)
                        printf("\n");
        }

        if (interface == PQOS_INTER_MSR && cap_l3ca != NULL)
                printf("\n\tAvailable CLOS     : 0 to %d\n",
                       cap_l3ca->u.l3ca->num_classes - 1);
        else if (interface == PQOS_INTER_MMIO && sys->erdt->max_clos > 0)
                printf("\n\tAvailable CLOS     : 0 to %d\n",
                       sys->erdt->max_clos - 1);

        printf("\tAllocation Commands:\n");
        for (j = 0; j < pci_info.num_channels; j++) {
                if (pci_info.channels[j] > 0) {
                        printf("\t\tpqos -a dev:<CLOS>="
                               "%.4x:%.2x:%.2x.%x@%d\n",
                               segment, BDF_BUS(bdf), BDF_DEV(bdf),
                               BDF_FUNC(bdf), j);
                        printf("\t\tpqos -a channel:<CLOS>=0x%lx\n",
                               pci_info.channels[j]);
                }

                if (((j + 1) < pci_info.num_channels) &&
                    pci_info.channels[j + 1] > 0)
                        printf("\n");
        }
        printf("\n\tAfter/Before allocation commands, assign required "
               "Cache Ways to CLOS\n");
        if (interface == PQOS_INTER_MSR && cap_l3ca != NULL) {
                printf("\tAvailable Cache Ways: %d\n",
                       cap_l3ca->u.l3ca->num_ways);
                print_alloc_ways_example(cap_l3ca->u.l3ca->num_classes,
                                         cap_l3ca->u.l3ca->num_ways,
                                         min_cbm_bits, -1);
        } else if (interface == PQOS_INTER_MMIO) {
                for (j = 0; j < sys->erdt->num_dev_agents; j++) {
                        if (pci_info.domain_id !=
                            sys->erdt->dev_agents[j].rmdd.domain_id)
                                continue;

                        printf("\tAvailable Cache Ways: %d\n",
                               sys->erdt->dev_agents[j].rmdd.num_io_l3_ways);
                        print_alloc_ways_example(
                            sys->erdt->max_clos,
                            sys->erdt->dev_agents[j].rmdd.num_io_l3_ways,
                            min_cbm_bits, pci_info.domain_id);
                }
        }

        return PQOS_RETVAL_OK;
}

void
parse_io_dev(char *str)
{
        struct pci_dev *sel_pci_dev_tmp = NULL;
        uint16_t segment = 0;
        uint16_t bdf = 0;
        char *vc = NULL;

        if (pqos_parse_pci_id(str, 0, &segment, &bdf, &vc) != 0) {
                free(sel_pci_dev);
                parse_error(str, "Invalid PCI ID");
        }

        sel_pci_dev_tmp = realloc(
            sel_pci_dev, (sizeof(struct pci_dev) * (sel_pci_dev_count + 1)));

        if (sel_pci_dev_tmp == NULL) {
                free(sel_pci_dev);
                parse_error(str, "Memory reallocation failed.");
        }

        sel_pci_dev = sel_pci_dev_tmp;
        sel_pci_dev[sel_pci_dev_count].segment = segment;
        sel_pci_dev[sel_pci_dev_count].bdf = bdf;
        sel_pci_dev_count++;
}

int
cap_print_io_dev(const struct pqos_sysconfig *sys)
{
        int ret;
        int result = PQOS_RETVAL_OK;
        uint32_t idx;
        unsigned min_cbm_bits;
        enum pqos_interface interface;
        const struct pqos_capability *cap_l3ca = NULL;

        /* None of the reasons below leaves anything to report, so each of them
         * returns a status rather than PQOS_RETVAL_OK: the caller turns it into
         * a non-zero exit code, which is what tells a script that the report it
         * asked for was not produced
         */
        if (sel_pci_dev_count == 0) {
                printf("Segment and BDF information are missing. "
                       "--print-io-dev=<segment>:<bus>:<device>.<function>\n");
                release_io_dev_selection();
                return PQOS_RETVAL_PARAM;
        }

        if (!sys || !sys->dev) {
                printf("IRDT info not available!\n");
                release_io_dev_selection();
                return PQOS_RETVAL_RESOURCE;
        }

        ret = pqos_inter_get(&interface);
        if (ret != PQOS_RETVAL_OK) {
                printf("unable to get interface\n");
                release_io_dev_selection();
                return ret;
        }

        if (interface == PQOS_INTER_MMIO) {
                if (!sys->erdt) {
                        printf("ERDT info not available!\n");
                        release_io_dev_selection();
                        return PQOS_RETVAL_RESOURCE;
                }
        } else if (interface != PQOS_INTER_MSR) {
                printf("--print-io-dev command is supported in msr and mmio "
                       "interfaces only\n");
                release_io_dev_selection();
                return PQOS_RETVAL_PARAM;
        }

        for (idx = 0; sys->cap && (idx < sys->cap->num_cap); idx++)
                switch (sys->cap->capabilities[idx].type) {
                case PQOS_CAP_TYPE_L3CA:
                        cap_l3ca = &(sys->cap->capabilities[idx]);
                        break;
                default:
                        break;
                }

        printf("\nEnable I/O RDT Allocation  : pqos -R l3iordt-on -d\n");
        printf("Enable I/O RDT Monitoring  : pqos -r l3iordt-on -d\n\n");
        printf("Disable I/O RDT Allocation : pqos -R l3iordt-off -d\n");
        printf("Disable I/O RDT Monitoring : pqos -r l3iordt-off -d\n\n");
        printf("Reset I/O RDT Allocation   : pqos -R -d\n");
        printf("Reset I/O RDT Monitoring   : pqos -r -d\n");

        min_cbm_bits = get_min_cbm_bits(interface);

        /*
         * every requested device is reported, and the first failure is
         * returned, so a list is not cut short by one absent device
         */
        for (idx = 0; idx < sel_pci_dev_count; idx++) {
                ret = print_io_dev(sys, cap_l3ca, interface,
                                   sel_pci_dev[idx].segment,
                                   sel_pci_dev[idx].bdf, min_cbm_bits);
                if (ret != PQOS_RETVAL_OK && result == PQOS_RETVAL_OK)
                        result = ret;
        }

        printf("\n");
        release_io_dev_selection();

        return result;
}

int
cap_print_io_devs(const struct pqos_sysconfig *sys)
{
        int ret;
        int result = PQOS_RETVAL_OK;
        uint32_t i;
        unsigned min_cbm_bits;
        enum pqos_interface interface;
        struct pqos_devinfo *dev = NULL;
        const struct pqos_capability *cap_l3ca = NULL;

        if (!sys || !sys->dev) {
                printf("IRDT info not available!\n");
                return PQOS_RETVAL_OK;
        }

        ret = pqos_inter_get(&interface);
        if (ret != PQOS_RETVAL_OK) {
                printf("unable to get interface\n");
                return PQOS_RETVAL_OK;
        }

        if (interface == PQOS_INTER_MMIO) {
                if (!sys->erdt) {
                        printf("ERDT info not available!\n");
                        return PQOS_RETVAL_OK;
                }
        } else if (interface != PQOS_INTER_MSR) {
                printf("--print-io-devs command is supported in msr and mmio "
                       "interfaces only\n");
                return PQOS_RETVAL_OK;
        }

        for (i = 0; sys->cap && (i < sys->cap->num_cap); i++)
                switch (sys->cap->capabilities[i].type) {
                case PQOS_CAP_TYPE_L3CA:
                        cap_l3ca = &(sys->cap->capabilities[i]);
                        break;
                default:
                        break;
                }

        printf("\nEnable I/O RDT Allocation  : pqos -R l3iordt-on -d\n");
        printf("Enable I/O RDT Monitoring  : pqos -r l3iordt-on -d\n\n");
        printf("Disable I/O RDT Allocation : pqos -R l3iordt-off -d\n");
        printf("Disable I/O RDT Monitoring : pqos -r l3iordt-off -d\n\n");
        printf("Reset I/O RDT Allocation   : pqos -R -d\n");
        printf("Reset I/O RDT Monitoring   : pqos -r -d\n");

        min_cbm_bits = get_min_cbm_bits(interface);

        dev = sys->dev;
        for (i = 0; i < dev->num_devs; i++) {
                ret =
                    print_io_dev(sys, cap_l3ca, interface, dev->devs[i].segment,
                                 dev->devs[i].bdf, min_cbm_bits);
                if (ret != PQOS_RETVAL_OK && result == PQOS_RETVAL_OK)
                        result = ret;
        }

        printf("\n");

        return result;
}
