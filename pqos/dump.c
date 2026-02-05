/*
 * BSD LICENSE
 *
 * Copyright(c) 2025-2026 Intel Corporation. All rights reserved.
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
 * @brief Dump module
 *
 */

#include "dump.h"

#include "common.h"
#include "log.h"
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

#define BIT_8  8
#define BIT_64 64

#define DOMAIN_TYPE_CPU    "CPU"
#define DOMAIN_TYPE_DEVICE "DEVICE"

/**
 * Dump configuration structure
 */
static struct sel_dump_info {
        uint32_t num_sockets;                /**< Number of sockets */
        uint64_t sockets[MAX_DOMAIN_IDS];    /**< List of sockets */
        uint32_t num_domain_ids;             /**< Number of domain ids */
        uint64_t domain_ids[MAX_DOMAIN_IDS]; /**< List of domain ids */
        enum pqos_mmio_dump_space space;     /**< ERDT Sub-structure types */
        enum pqos_mmio_dump_width width;     /**< Width of MMIO access */
        unsigned int le_flag;                /**< Little endian flag.
                                                default: 0, means big endian */
        unsigned int bin_flag;               /**< Binary flag.
                                                default: 0, means hexadecimal */
        uint64_t offset;                     /**< Offset MMIO registers address
                                                space. Default 0, means
                                                beginning of the MMIO space */
        uint64_t length;                     /**< Length of MMIO registers.
                                                Default 0, means from the
                                                offset to end of
                                                the MMIO space */
} sel_dump;

/* Map between ACPI structures and appropriate MMIO spaces */
static const struct pqos_mmio_dump_space_map_entry sel_mmio_dump_space_map[] = {
    {MMIO_DUMP_SPACE_CMRC, DM_TYPE_CPU, "CMRC",
     OFFSET_IN_STRUCT(struct pqos_cpu_agent_info, cmrc),
     OFFSET_IN_STRUCT(struct pqos_erdt_cmrc, block_base_addr),
     OFFSET_IN_STRUCT(struct pqos_erdt_cmrc, block_size), PAGE_SIZE},
    {MMIO_DUMP_SPACE_MMRC, DM_TYPE_CPU, "MMRC",
     OFFSET_IN_STRUCT(struct pqos_cpu_agent_info, mmrc),
     OFFSET_IN_STRUCT(struct pqos_erdt_mmrc, reg_block_base_addr),
     OFFSET_IN_STRUCT(struct pqos_erdt_mmrc, reg_block_size), PAGE_SIZE},
    {MMIO_DUMP_SPACE_MARC_OPT, DM_TYPE_CPU, "MARC(OPT)",
     OFFSET_IN_STRUCT(struct pqos_cpu_agent_info, marc),
     OFFSET_IN_STRUCT(struct pqos_erdt_marc, opt_bw_reg_block_base_addr),
     OFFSET_IN_STRUCT(struct pqos_erdt_marc, reg_block_size), PAGE_SIZE},
    {MMIO_DUMP_SPACE_MARC_MIN, DM_TYPE_CPU, "MARC(MIN)",
     OFFSET_IN_STRUCT(struct pqos_cpu_agent_info, marc),
     OFFSET_IN_STRUCT(struct pqos_erdt_marc, min_bw_reg_block_base_addr),
     OFFSET_IN_STRUCT(struct pqos_erdt_marc, reg_block_size), PAGE_SIZE},
    {MMIO_DUMP_SPACE_MARC_MAX, DM_TYPE_CPU, "MARC(MAX)",
     OFFSET_IN_STRUCT(struct pqos_cpu_agent_info, marc),
     OFFSET_IN_STRUCT(struct pqos_erdt_marc, max_bw_reg_block_base_addr),
     OFFSET_IN_STRUCT(struct pqos_erdt_marc, reg_block_size), PAGE_SIZE},
    {MMIO_DUMP_SPACE_CMRD, DM_TYPE_DEVICE, "CMRD",
     OFFSET_IN_STRUCT(struct pqos_device_agent_info, cmrd),
     OFFSET_IN_STRUCT(struct pqos_erdt_cmrd, reg_base_addr),
     OFFSET_IN_STRUCT(struct pqos_erdt_cmrd, reg_block_size), PAGE_SIZE},
    {MMIO_DUMP_SPACE_IBRD, DM_TYPE_DEVICE, "IBRD",
     OFFSET_IN_STRUCT(struct pqos_device_agent_info, ibrd),
     OFFSET_IN_STRUCT(struct pqos_erdt_ibrd, reg_base_addr),
     OFFSET_IN_STRUCT(struct pqos_erdt_ibrd, reg_block_size), PAGE_SIZE},
    {MMIO_DUMP_SPACE_CARD, DM_TYPE_DEVICE, "CARD",
     OFFSET_IN_STRUCT(struct pqos_device_agent_info, card),
     OFFSET_IN_STRUCT(struct pqos_erdt_card, reg_base_addr),
     OFFSET_IN_STRUCT(struct pqos_erdt_card, reg_block_size), PAGE_SIZE}};

/**
 *  @brief Print MMIO map table header
 **/
static void
print_mmio_mm_header(void)
{
        printf("%-12s %-12s %-12s %-18s   %s\n", "Domain ID", "Type", "Space",
               "Base Address", "Size (bytes)");
        printf("---------------------------------------------------------------"
               "---------\n");
}

/**
 *  @brief Print MMIO map table row
 *
 *  @param [in] domain_str MMIO domain string
 *  @param [in] space MMIO space string
 *  @param [in] base MMIO base address
 *  @param [in] size MMIO space size in bytes
 **/
static void
print_mmio_mm_row(uint16_t domain_id,
                  const char *type,
                  const char *space,
                  uint64_t base,
                  uint32_t size)
{
        printf("0x%02x         %-12s %-12s 0x%016lx   0x%08x\n", domain_id,
               type, space, (unsigned long)base, size);
}

int
pqos_print_dump_info(const struct pqos_sysconfig *sys)
{
        int printed = 0;

        if (!sys || !sys->erdt) {
                printf("ERDT info not available!\n");
                return PQOS_RETVAL_PARAM;
        }

        printf("\n");
        print_mmio_mm_header();

        /* CPU agents */
        for (uint32_t i = 0; i < sys->erdt->num_cpu_agents; ++i) {
                const struct pqos_cpu_agent_info *agent =
                    &sys->erdt->cpu_agents[i];

                for (size_t m = 0; m < sizeof(sel_mmio_dump_space_map) /
                                           sizeof(sel_mmio_dump_space_map[0]);
                     ++m) {
                        if (sel_mmio_dump_space_map[m].domain_type !=
                            DM_TYPE_CPU)
                                continue;

                        /* Get the structure pointer for the space */
                        uint64_t base;
                        uint32_t size;
                        const char *agent_base = (const char *)agent;
                        const void *space_struct =
                            agent_base +
                            sel_mmio_dump_space_map[m].struct_offset;

                        base = *(const uint64_t *)((const char *)space_struct +
                                                   sel_mmio_dump_space_map[m]
                                                       .base_offset);
                        size = *(const uint32_t *)((const char *)space_struct +
                                                   sel_mmio_dump_space_map[m]
                                                       .size_offset) *
                               sel_mmio_dump_space_map[m].size_adjustment;

                        /* Only print if base and size are nonzero */
                        if (base && size) {
                                print_mmio_mm_row(
                                    agent->rmdd.domain_id, DOMAIN_TYPE_CPU,
                                    sel_mmio_dump_space_map[m].space_name, base,
                                    size);
                                printed++;
                        }
                }
                printf("\n");
        }

        /* Device agents */
        for (uint32_t i = 0; i < sys->erdt->num_dev_agents; ++i) {
                const struct pqos_device_agent_info *agent =
                    &sys->erdt->dev_agents[i];

                for (size_t m = 0; m < sizeof(sel_mmio_dump_space_map) /
                                           sizeof(sel_mmio_dump_space_map[0]);
                     ++m) {
                        if (sel_mmio_dump_space_map[m].domain_type !=
                            DM_TYPE_DEVICE)
                                continue;
                        const char *agent_base = (const char *)agent;
                        const void *space_struct =
                            agent_base +
                            sel_mmio_dump_space_map[m].struct_offset;
                        uint64_t base =
                            *(const uint64_t *)((const char *)space_struct +
                                                sel_mmio_dump_space_map[m]
                                                    .base_offset);
                        uint32_t size =
                            *(const uint32_t *)((const char *)space_struct +
                                                sel_mmio_dump_space_map[m]
                                                    .size_offset) *
                            sel_mmio_dump_space_map[m].size_adjustment;
                        if (base && size) {
                                print_mmio_mm_row(
                                    agent->rmdd.domain_id, DOMAIN_TYPE_DEVICE,
                                    sel_mmio_dump_space_map[m].space_name, base,
                                    size);
                                printed++;
                        }
                }
                printf("\n");
        }

        printf("\n");
        /* If nothing printed, return error */
        if (!printed) {
                printf("No MMIO spaces found.\n");
                return PQOS_RETVAL_UNAVAILABLE;
        }

        return PQOS_RETVAL_OK;
}

void
selfn_dump_socket(const char *arg)
{
        char *str = NULL;
        unsigned int i = 0;
        unsigned int j = 0;
        unsigned int n = 0;

        if (arg == NULL)
                parse_error(arg, "NULL pointer!");

        if (*arg == '\0')
                parse_error(arg, "Empty string!");

        selfn_strdup(&str, arg);

        n = strlisttotab(str, sel_dump.sockets, MAX_DOMAIN_IDS);
        if (n == 0) {
                printf("No Socket specified: %s\n", str);
                exit(EXIT_FAILURE);
        }

        sel_dump.num_sockets = n;

        /* Check for invalid socket */
        for (i = 0; i < n; i++) {
                if (sel_dump.sockets[i] >= MAX_DOMAINS) {
                        printf("Socket out of range: %s\n", str);
                        exit(EXIT_FAILURE);
                }
        }

        /* Check duplicate Socket entry */
        for (i = 0; i < n; i++) {
                for (j = i + 1; j < n; j++) {
                        if (sel_dump.sockets[i] == sel_dump.sockets[j]) {
                                parse_error(str, "Duplicate Socket selection");
                                printf("The socket %ld is entered twice\n",
                                       sel_dump.sockets[i]);
                                exit(EXIT_FAILURE);
                        }
                }
        }

        free(str);
}

void
selfn_dump_domain_id(const char *arg)
{
        char *str = NULL;
        unsigned int i = 0;
        unsigned int j = 0;
        unsigned int n = 0;

        if (arg == NULL)
                parse_error(arg, "NULL pointer!");

        if (*arg == '\0')
                parse_error(arg, "Empty string!");

        selfn_strdup(&str, arg);

        n = strlisttotab(str, sel_dump.domain_ids, MAX_DOMAIN_IDS);
        if (n == 0) {
                printf("No Domain ID specified: %s\n", str);
                exit(EXIT_FAILURE);
        }

        sel_dump.num_domain_ids = n;

        /* Check for invalid Domain ID */
        for (i = 0; i < n; i++) {
                if (sel_dump.domain_ids[i] >= MAX_DOMAINS) {
                        printf("Domain ID out of range: %s\n", str);
                        exit(EXIT_FAILURE);
                }
        }

        /* Check duplicate Domain ID entry */
        for (i = 0; i < n; i++) {
                for (j = i + 1; j < n; j++) {
                        if (sel_dump.domain_ids[i] == sel_dump.domain_ids[j]) {
                                parse_error(str,
                                            "Duplicate Domain ID selection");
                                printf("The Domain ID %ld is entered twice\n",
                                       sel_dump.domain_ids[i]);
                                exit(EXIT_FAILURE);
                        }
                }
        }

        free(str);
}

void
selfn_dump_space(const char *arg)
{
        if (arg == NULL)
                parse_error(arg, "NULL pointer!");

        /**
         * Determine selected type (L3/L2/MBA/MBA CTRL)
         */
        if (strcasecmp(arg, "cmrc") == 0)
                sel_dump.space = MMIO_DUMP_SPACE_CMRC;
        else if (strcasecmp(arg, "mmrc") == 0)
                sel_dump.space = MMIO_DUMP_SPACE_MMRC;
        else if (strcasecmp(arg, "marc-opt") == 0)
                sel_dump.space = MMIO_DUMP_SPACE_MARC_OPT;
        else if (strcasecmp(arg, "marc-min") == 0)
                sel_dump.space = MMIO_DUMP_SPACE_MARC_MIN;
        else if (strcasecmp(arg, "marc-max") == 0)
                sel_dump.space = MMIO_DUMP_SPACE_MARC_MAX;
        else if (strcasecmp(arg, "cmrd") == 0)
                sel_dump.space = MMIO_DUMP_SPACE_CMRD;
        else if (strcasecmp(arg, "ibrd") == 0)
                sel_dump.space = MMIO_DUMP_SPACE_IBRD;
        else if (strcasecmp(arg, "card") == 0)
                sel_dump.space = MMIO_DUMP_SPACE_CARD;
        else {
                if (*arg == '\0')
                        printf("Missing input in --space\n");
                else
                        printf("Wrong input in --space=%s.\n", arg);
                printf("Available inputs in --space=: cmrc mmrc marc-opt "
                       "marc-min marc-max cmrd ibrd card\n");
                exit(EXIT_FAILURE);
        }
}

void
selfn_dump_width(const char *arg)
{
        uint64_t value = 0;

        if (arg == NULL)
                parse_error(arg, "NULL pointer!");

        if (*arg == '\0') {
                printf("Missing input in --width=%s\n", arg);
                printf("Available input in --width=: 8 or 64. By default the "
                       "--width is 64, if the --width is not provided.\n");
                exit(EXIT_FAILURE);
        }

        value = strtouint64(arg);
        switch (value) {
        case BIT_8:
                sel_dump.width = MMIO_DUMP_WIDTH_8BIT;
                break;
        case BIT_64:
                sel_dump.width = MMIO_DUMP_WIDTH_64BIT;
                break;
        default:
                printf("Wrong input in --width=%s\n", arg);
                printf("Available input in --width=: 8 or 64. By default the "
                       "--width is 64, if the --width is not provided.\n");
                exit(EXIT_FAILURE);
                break;
        }
}

void
selfn_dump_le(const char *arg)
{
        UNUSED_ARG(arg);
        sel_dump.le_flag = 1;
}

void
selfn_dump_binary(const char *arg)
{
        UNUSED_ARG(arg);
        sel_dump.bin_flag = 1;
}

void
selfn_dump_offset(const char *arg)
{
        if (arg == NULL)
                parse_error(arg, "NULL pointer!");

        sel_dump.offset = strtouint64(arg);
}

void
selfn_dump_length(const char *arg)
{
        if (arg == NULL)
                parse_error(arg, "NULL pointer!");

        sel_dump.length = strtouint64(arg);
}

static int
get_socket_domain_ids(uint32_t socket,
                      uint16_t *domain_ids,
                      struct pqos_erdt_info *erdt,
                      enum pqos_domain_type dm_type,
                      uint32_t dm_per_socket)
{
        uint32_t idx = 0;
        uint32_t dm_idx = 0;

        if (!domain_ids) {
                printf("domain_ids not available!\n");
                return PQOS_RETVAL_PARAM;
        }
        if (!erdt) {
                printf("ERDT info not available!\n");
                return PQOS_RETVAL_PARAM;
        }

        dm_idx = socket * dm_per_socket;

        if (dm_type == DM_TYPE_CPU &&
            ((dm_idx + dm_per_socket) > erdt->num_cpu_agents)) {
                printf("Mismatch between socket and CPU Agents. "
                       "Available CPU Agents %d. "
                       "The requested socket %d\n",
                       erdt->num_cpu_agents, socket);
                return PQOS_RETVAL_PARAM;
        }

        if (dm_type == DM_TYPE_DEVICE &&
            ((dm_idx + dm_per_socket) > erdt->num_dev_agents)) {
                printf("Mismatch between socket and Device Agents. "
                       "Available Device Agents %d. "
                       "The requested socket %d\n",
                       erdt->num_dev_agents, socket);
                return PQOS_RETVAL_PARAM;
        }

        for (idx = 0; idx < dm_per_socket; idx++, dm_idx++) {
                if (dm_type == DM_TYPE_CPU)
                        domain_ids[idx] =
                            erdt->cpu_agents[dm_idx].rmdd.domain_id;
                if (dm_type == DM_TYPE_DEVICE)
                        domain_ids[idx] =
                            erdt->dev_agents[dm_idx].rmdd.domain_id;
        }

        return PQOS_RETVAL_OK;
}

void
dump_mmio_regs(const struct pqos_sysconfig *sys)
{
        struct pqos_mmio_dump dump;
        int ret = PQOS_RETVAL_OK;
        uint32_t idx = 0;
        uint32_t dm_idx = 0;
        uint32_t dm_per_socket = 0;
        enum pqos_domain_type dm_type = 0;

        if (!sys || !sys->erdt) {
                printf("ERDT info not available!\n");
                exit(EXIT_FAILURE);
        }

        if (sel_dump.num_sockets == 0 && sel_dump.num_domain_ids == 0) {
                printf("Provide either --socket or --dump-domain-id\n");
                exit(EXIT_FAILURE);
        }

        memset(&dump, 0, sizeof(struct pqos_mmio_dump));

        dump.topology.space = sel_dump.space;
        dump.fmt.width = sel_dump.width;
        dump.fmt.le = sel_dump.le_flag;
        dump.fmt.bin = sel_dump.bin_flag;
        dump.view.offset = sel_dump.offset;
        dump.view.length = sel_dump.length;

        if (sel_dump.num_domain_ids && sel_dump.num_sockets)
                printf("Dumping MMIO Registers for --dump-domain-id options. "
                       "--socket is ignored.\n");

        if (sel_dump.num_domain_ids) {
                dump.topology.num_domain_ids = sel_dump.num_domain_ids;

                dump.topology.domain_ids = (uint16_t *)malloc(
                    sizeof(uint16_t) * dump.topology.num_domain_ids);
                if (dump.topology.domain_ids == NULL) {
                        printf("Error with memory allocation for "
                               "domain_ids!\n");
                        exit(EXIT_FAILURE);
                }

                for (unsigned int idx = 0; idx < sel_dump.num_domain_ids; idx++)
                        dump.topology.domain_ids[idx] =
                            sel_dump.domain_ids[idx];
        } else if (sel_dump.num_sockets) {
                /* Check requested MMIO Register space in Domain ID type CPU */
                if (sel_dump.space == MMIO_DUMP_SPACE_CMRC ||
                    sel_dump.space == MMIO_DUMP_SPACE_MMRC ||
                    sel_dump.space == MMIO_DUMP_SPACE_MARC_OPT ||
                    sel_dump.space == MMIO_DUMP_SPACE_MARC_MIN ||
                    sel_dump.space == MMIO_DUMP_SPACE_MARC_MAX) {
                        dump.topology.num_domain_ids =
                            sel_dump.num_sockets * CPU_AGENTS_PER_SOCKET;
                        dm_type = DM_TYPE_CPU;
                        dm_per_socket = CPU_AGENTS_PER_SOCKET;
                }

                /* Check requested MMIO Register space in Domain ID
                   type Device */
                if (sel_dump.space == MMIO_DUMP_SPACE_CMRD ||
                    sel_dump.space == MMIO_DUMP_SPACE_IBRD ||
                    sel_dump.space == MMIO_DUMP_SPACE_CARD) {
                        dump.topology.num_domain_ids =
                            sel_dump.num_sockets * DEVICE_AGENTS_PER_SOCKET;
                        dm_type = DM_TYPE_DEVICE;
                        dm_per_socket = DEVICE_AGENTS_PER_SOCKET;
                }

                dump.topology.domain_ids = (uint16_t *)malloc(
                    sizeof(uint16_t) * dump.topology.num_domain_ids);
                if (dump.topology.domain_ids == NULL) {
                        printf("Error with memory allocation for "
                               "domain_ids!\n");
                        exit(EXIT_FAILURE);
                }

                for (idx = 0, dm_idx = 0; idx < sel_dump.num_sockets; idx++) {
                        ret = get_socket_domain_ids(
                            sel_dump.sockets[idx],
                            &dump.topology.domain_ids[dm_idx], sys->erdt,
                            dm_type, dm_per_socket);
                        if (ret != PQOS_RETVAL_OK) {
                                printf("Unable to get domain ids for socket "
                                       "%ld\n",
                                       sel_dump.sockets[idx]);
                                free(dump.topology.domain_ids);
                                exit(EXIT_FAILURE);
                        }

                        dm_idx += dm_per_socket;
                }
        }

        ret = pqos_dump(&dump);
        if (ret != PQOS_RETVAL_OK)
                printf("MMIO Registers Dump is failed!\n");

        free(dump.topology.domain_ids);
}
