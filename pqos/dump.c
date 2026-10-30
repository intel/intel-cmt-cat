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
#include "utils.h"

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
        enum pqos_mmio_dump_space space;     /**< ERDT Sub-structure type.
                                                MMIO_DUMP_SPACE_ERROR means
                                                --space was not given */
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
} sel_dump = {.space = MMIO_DUMP_SPACE_ERROR};

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
        sel_dump.num_domain_ids =
            selfn_domain_id_list(arg, sel_dump.domain_ids, MAX_DOMAIN_IDS);
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

/**
 * @brief Finds the socket of a CPU domain, and how many cores it has there
 *
 * A CPU agent sits on one socket, so the cores the core to domain mapping
 * gives it are all on that socket.
 *
 * @param [in] sys system configuration
 * @param [in] domain_id CPU domain to place
 * @param [out] socket socket of the domain's cores, when it has one
 * @param [out] other another socket one of its cores is on
 * @param [out] num_cores number of topology cores the mapping gives the domain
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK when the domain's cores, if any, are on one socket
 * @retval PQOS_RETVAL_ERROR when its cores are on more than one socket
 */
static int
get_cpu_domain_socket(const struct pqos_sysconfig *sys,
                      const uint16_t domain_id,
                      unsigned *socket,
                      unsigned *other,
                      unsigned *num_cores)
{
        const struct pqos_cores_domains *cores_domains = sys->cores_domains;

        *num_cores = 0;

        for (unsigned i = 0; i < sys->cpu->num_cores; i++) {
                const struct pqos_coreinfo *core = &sys->cpu->cores[i];

                if (core->lcore >= cores_domains->num_cores ||
                    cores_domains->domains[core->lcore] != domain_id)
                        continue;

                if (*num_cores == 0) {
                        *socket = core->socket;
                } else if (core->socket != *socket) {
                        *other = core->socket;
                        return PQOS_RETVAL_ERROR;
                }
                (*num_cores)++;
        }

        return PQOS_RETVAL_OK;
}

/**
 * @brief Finds an enumeration ID that two CACDs both list
 *
 * The core to domain mapping gives a core that two CACDs list to the domain
 * written last, so where CACDs overlap the mapping no longer says which cores
 * a domain's CACD enumerates.
 *
 * @param [in] erdt ERDT information
 * @param [out] first domain of the first CACD that lists the ID
 * @param [out] second domain of the second CACD that lists it
 * @param [out] id the enumeration ID both list
 *
 * @return 1 if two CACDs list the same enumeration ID, 0 otherwise
 */
static int
find_shared_enumeration_id(const struct pqos_erdt_info *erdt,
                           uint16_t *first,
                           uint16_t *second,
                           uint32_t *id)
{
        for (uint32_t a = 0; a < erdt->num_cpu_agents; a++) {
                const struct pqos_erdt_cacd *cacd_a = &erdt->cpu_agents[a].cacd;

                for (uint32_t b = a + 1; b < erdt->num_cpu_agents; b++) {
                        const struct pqos_erdt_cacd *cacd_b =
                            &erdt->cpu_agents[b].cacd;

                        for (uint32_t i = 0; i < cacd_a->enum_ids_length; i++)
                                for (uint32_t j = 0;
                                     j < cacd_b->enum_ids_length; j++) {
                                        if (cacd_a->enumeration_ids[i] !=
                                            cacd_b->enumeration_ids[j])
                                                continue;

                                        *first =
                                            erdt->cpu_agents[a].rmdd.domain_id;
                                        *second =
                                            erdt->cpu_agents[b].rmdd.domain_id;
                                        *id = cacd_a->enumeration_ids[i];
                                        return 1;
                                }
                }
        }

        return 0;
}

/**
 * @brief Checks that the CPU topology lists exactly the CPUs the mapping covers
 *
 * The core to domain mapping covers the logical CPUs from 0 to its num_cores
 * minus 1. A CPU it covers that the topology does not list, as a cpuset can
 * leave one out, may be one a CACD enumerates, and a core that no CACD lists
 * can then stand in for it in the count of its domain's cores. A CPU the
 * topology lists beyond the mapping has no domain to read.
 *
 * @param [in] sys system configuration
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK when the topology lists every CPU the mapping covers,
 *         and no other
 * @retval PQOS_RETVAL_ERROR when it does not
 * @retval PQOS_RETVAL_RESOURCE when the memory to check it cannot be had
 */
static int
check_topology_coverage(const struct pqos_sysconfig *sys)
{
        const unsigned num_cores = sys->cores_domains->num_cores;
        unsigned char *listed = calloc(num_cores > 0 ? num_cores : 1, 1);
        int ret = PQOS_RETVAL_OK;

        if (listed == NULL) {
                printf("Error with memory allocation for the CPU topology "
                       "check!\n");
                return PQOS_RETVAL_RESOURCE;
        }

        for (unsigned i = 0; i < sys->cpu->num_cores; i++) {
                const unsigned lcore = sys->cpu->cores[i].lcore;

                if (lcore >= num_cores) {
                        printf("CPU %u of the CPU topology is outside the core "
                               "to domain mapping, so the sockets of the CPU "
                               "domains are unknown. Use --dump-domain-id\n",
                               lcore);
                        ret = PQOS_RETVAL_ERROR;
                        goto check_topology_coverage_exit;
                }
                listed[lcore] = 1;
        }

        for (unsigned lcore = 0; lcore < num_cores; lcore++)
                if (!listed[lcore]) {
                        printf("CPU %u of the core to domain mapping is not in "
                               "the CPU topology, so the sockets of the CPU "
                               "domains are unknown. Use --dump-domain-id\n",
                               lcore);
                        ret = PQOS_RETVAL_ERROR;
                        break;
                }

check_topology_coverage_exit:
        free(listed);

        return ret;
}

/**
 * @brief Collects the domains of one socket
 *
 * A CPU agent belongs to the socket of the cores its CACD enumerates, not to a
 * position in ERDT: a socket may populate fewer CBBs than
 * CPU_AGENTS_PER_SOCKET, with domain IDs that neither start at 0 nor follow
 * one another, two CBBs with domains 1 and 3 for one. A device agent has no
 * cores to place it by, so device domains are still taken
 * DEVICE_AGENTS_PER_SOCKET at a time in ERDT order.
 *
 * The mapping is zero-filled, and a core no CACD enumerates reads as domain 0,
 * so the mapping is taken only where it agrees with ERDT. The topology has to
 * list exactly the CPUs the mapping covers, so that every core a CACD lists is
 * in it, and no two CACDs may list the same core, since the mapping keeps only
 * one of them for it. Then the mapping gives each domain the cores its CACD
 * lists and, for domain 0, the cores no CACD lists, and a CPU domain is placed
 * by its cores only when there are as many as its CACD enumerates, all on one
 * socket. Any other domain refuses the selection, which would otherwise leave
 * it out of a socket's dump or put it on a socket by a stray core. The counts
 * are checked before the IDs are compared, so that the IDs left to compare
 * are no more than the cores.
 *
 * @param [in] socket socket whose domains are collected
 * @param [out] domain_ids table the domains are written to
 * @param [out] num_domain_ids number of domains written
 * @param [in] sys system configuration
 * @param [in] dm_type type of the domains to collect
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 * @retval PQOS_RETVAL_PARAM when the socket has no domain of the type
 * @retval PQOS_RETVAL_RESOURCE when the core to domain mapping is missing
 * @retval PQOS_RETVAL_ERROR when a CPU domain cannot be placed
 */
static int
get_socket_domain_ids(uint32_t socket,
                      uint16_t *domain_ids,
                      uint32_t *num_domain_ids,
                      const struct pqos_sysconfig *sys,
                      enum pqos_domain_type dm_type)
{
        const struct pqos_erdt_info *erdt = sys->erdt;
        uint32_t idx = 0;
        uint32_t dm_idx = 0;
        uint16_t first = 0;
        uint16_t second = 0;
        uint32_t shared = 0;
        int ret;

        if (!domain_ids) {
                printf("domain_ids not available!\n");
                return PQOS_RETVAL_PARAM;
        }
        if (!erdt) {
                printf("ERDT info not available!\n");
                return PQOS_RETVAL_PARAM;
        }

        if (dm_type == DM_TYPE_CPU) {
                if (sys->cpu == NULL || sys->cores_domains == NULL ||
                    sys->cores_domains->domains == NULL) {
                        printf("Core to domain mapping not available!\n");
                        return PQOS_RETVAL_RESOURCE;
                }

                ret = check_topology_coverage(sys);
                if (ret != PQOS_RETVAL_OK)
                        return ret;

                for (idx = 0; idx < erdt->num_cpu_agents; idx++) {
                        const struct pqos_cpu_agent_info *agent =
                            &erdt->cpu_agents[idx];
                        const uint16_t domain_id = agent->rmdd.domain_id;
                        const uint32_t listed = agent->cacd.enum_ids_length;
                        unsigned dm_socket = 0;
                        unsigned other = 0;
                        unsigned num_cores = 0;

                        if (listed == 0) {
                                printf("Domain ID %u has no core in its CACD, "
                                       "so its socket is unknown. Use "
                                       "--dump-domain-id\n",
                                       domain_id);
                                return PQOS_RETVAL_ERROR;
                        }

                        if (get_cpu_domain_socket(sys, domain_id, &dm_socket,
                                                  &other, &num_cores) !=
                            PQOS_RETVAL_OK) {
                                printf("Domain ID %u has cores on sockets %u "
                                       "and %u, so its socket is unknown. "
                                       "Use --dump-domain-id\n",
                                       domain_id, dm_socket, other);
                                return PQOS_RETVAL_ERROR;
                        }

                        if (num_cores != listed) {
                                printf("Domain ID %u has %u cores in the CPU "
                                       "topology where its CACD lists %u, so "
                                       "its socket is unknown. Use "
                                       "--dump-domain-id\n",
                                       domain_id, num_cores, listed);
                                return PQOS_RETVAL_ERROR;
                        }

                        if (dm_socket == socket)
                                domain_ids[dm_idx++] = domain_id;
                }

                /* every CACD now lists as many IDs as it has cores */
                if (find_shared_enumeration_id(erdt, &first, &second,
                                               &shared)) {
                        printf("Domain IDs %u and %u both list enumeration "
                               "ID 0x%x in their CACDs, so the socket of their "
                               "cores is unknown. Use --dump-domain-id\n",
                               first, second, shared);
                        return PQOS_RETVAL_ERROR;
                }

                if (dm_idx == 0) {
                        printf("Socket %u holds none of the %u CPU Agents\n",
                               socket, erdt->num_cpu_agents);
                        return PQOS_RETVAL_PARAM;
                }

                *num_domain_ids = dm_idx;

                return PQOS_RETVAL_OK;
        }

        dm_idx = socket * DEVICE_AGENTS_PER_SOCKET;

        if ((dm_idx + DEVICE_AGENTS_PER_SOCKET) > erdt->num_dev_agents) {
                printf("Mismatch between socket and Device Agents. "
                       "Available Device Agents %d. "
                       "The requested socket %d\n",
                       erdt->num_dev_agents, socket);
                return PQOS_RETVAL_PARAM;
        }

        for (idx = 0; idx < DEVICE_AGENTS_PER_SOCKET; idx++, dm_idx++)
                domain_ids[idx] = erdt->dev_agents[dm_idx].rmdd.domain_id;

        *num_domain_ids = DEVICE_AGENTS_PER_SOCKET;

        return PQOS_RETVAL_OK;
}

int
dump_mmio_regs(const struct pqos_sysconfig *sys)
{
        struct pqos_mmio_dump dump;
        int ret = PQOS_RETVAL_OK;
        uint32_t idx = 0;
        uint32_t dm_idx = 0;
        enum pqos_domain_type dm_type = 0;

        /* each refusal returns rather than exits: this runs after
         * pqos_init(), and main() reaches pqos_fini() only by a return
         */
        if (!sys || !sys->erdt) {
                printf("ERDT info not available!\n");
                return PQOS_RETVAL_RESOURCE;
        }

        if (sel_dump.num_sockets == 0 && sel_dump.num_domain_ids == 0) {
                printf("Provide either --socket or --dump-domain-id\n");
                return PQOS_RETVAL_PARAM;
        }

        if (sel_dump.space == MMIO_DUMP_SPACE_ERROR) {
                printf("Provide --space\n");
                printf("Available inputs in --space=: cmrc mmrc marc-opt "
                       "marc-min marc-max cmrd ibrd card\n");
                return PQOS_RETVAL_PARAM;
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
                        return PQOS_RETVAL_RESOURCE;
                }

                for (unsigned int idx = 0; idx < sel_dump.num_domain_ids; idx++)
                        dump.topology.domain_ids[idx] =
                            sel_dump.domain_ids[idx];
        } else if (sel_dump.num_sockets) {
                uint32_t max_domain_ids = 0;

                /* Check requested MMIO Register space in Domain ID type CPU */
                if (sel_dump.space == MMIO_DUMP_SPACE_CMRC ||
                    sel_dump.space == MMIO_DUMP_SPACE_MMRC ||
                    sel_dump.space == MMIO_DUMP_SPACE_MARC_OPT ||
                    sel_dump.space == MMIO_DUMP_SPACE_MARC_MIN ||
                    sel_dump.space == MMIO_DUMP_SPACE_MARC_MAX) {
                        /* a socket holds at most every CPU agent */
                        max_domain_ids =
                            sel_dump.num_sockets * sys->erdt->num_cpu_agents;
                        dm_type = DM_TYPE_CPU;
                }

                /* Check requested MMIO Register space in Domain ID
                   type Device */
                if (sel_dump.space == MMIO_DUMP_SPACE_CMRD ||
                    sel_dump.space == MMIO_DUMP_SPACE_IBRD ||
                    sel_dump.space == MMIO_DUMP_SPACE_CARD) {
                        max_domain_ids =
                            sel_dump.num_sockets * DEVICE_AGENTS_PER_SOCKET;
                        dm_type = DM_TYPE_DEVICE;
                }

                if (max_domain_ids == 0) {
                        printf("ERDT lists no CPU Agents\n");
                        return PQOS_RETVAL_PARAM;
                }

                dump.topology.domain_ids =
                    (uint16_t *)malloc(sizeof(uint16_t) * max_domain_ids);
                if (dump.topology.domain_ids == NULL) {
                        printf("Error with memory allocation for "
                               "domain_ids!\n");
                        return PQOS_RETVAL_RESOURCE;
                }

                for (idx = 0, dm_idx = 0; idx < sel_dump.num_sockets; idx++) {
                        uint32_t num = 0;

                        ret = get_socket_domain_ids(
                            sel_dump.sockets[idx],
                            &dump.topology.domain_ids[dm_idx], &num, sys,
                            dm_type);
                        if (ret != PQOS_RETVAL_OK) {
                                printf("Unable to get domain ids for socket "
                                       "%ld\n",
                                       sel_dump.sockets[idx]);
                                free(dump.topology.domain_ids);
                                return ret;
                        }

                        dm_idx += num;
                }

                dump.topology.num_domain_ids = dm_idx;
        }

        ret = pqos_dump(&dump);
        if (ret != PQOS_RETVAL_OK)
                printf("MMIO Registers Dump is failed!\n");

        free(dump.topology.domain_ids);

        return ret;
}
