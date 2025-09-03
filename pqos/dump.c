/*
 * BSD LICENSE
 *
 * Copyright(c) 2025 Intel Corporation. All rights reserved.
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

#include "log.h"
#include "pqos.h"

#define DOMAIN_STR_LEN 32

/**
 *  @brief Print MMIO map table header
 **/
static void
print_mmio_mm_header(void)
{
        printf("%-12s %-12s %-18s %-12s\n", "Domain", "Space", "Base Address",
               "Size (bytes)");
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
print_mmio_mm_row(const char *domain_str,
                  const char *space,
                  uint64_t base,
                  uint32_t size)
{
        printf("%-10s %-12s 0x%016lx 0x%08x\n", domain_str, space,
               (unsigned long)base, size);
}

/**
 *  @brief Print table header
 *
 *  @param [in] buf buffer to dump
 *  @param [in] length number of elements to dump
 *  @param [in] width_bytes element width in bytes (1 or 8)
 *  @param [in] le 1 for little-endian, 0 for big-endian
 *  @param [in] binary 1 for binary output, 0 for hexadecimal output
 **/
int
pqos_print_dump_info(const struct pqos_erdt_info *erdt)
{
        int printed = 0;

        if (!erdt) {
                printf("ERDT info not available!\n");
                return PQOS_RETVAL_PARAM;
        }

        print_mmio_mm_header();

        /* CPU agents */
        for (uint32_t i = 0; i < erdt->num_cpu_agents; ++i) {
                const struct pqos_cpu_agent_info *agent = &erdt->cpu_agents[i];
                char domain_str[DOMAIN_STR_LEN];

                snprintf(domain_str, sizeof(domain_str), "CPU_AGENT: %u", i);

                for (size_t m = 0;
                     m < sizeof(dump_space_map) / sizeof(dump_space_map[0]);
                     ++m) {
                        if (dump_space_map[m].domain_type != DM_TYPE_CPU)
                                continue;

                        /* Get the structure pointer for the space */
                        uint64_t base;
                        uint32_t size;
                        const char *agent_base = (const char *)agent;
                        const void *space_struct =
                            agent_base + dump_space_map[m].struct_offset;

                        base = *(const uint64_t
                                     *)((const char *)space_struct +
                                        dump_space_map[m].base_offset);
                        size = *(const uint32_t
                                     *)((const char *)space_struct +
                                        dump_space_map[m].size_offset) *
                               dump_space_map[m].size_adjustment;

                        /* Only print if base and size are nonzero */
                        if (base && size) {
                                print_mmio_mm_row(domain_str,
                                                  dump_space_map[m].space_name,
                                                  base, size);
                                printed++;
                        }
                }
        }

        /* Device agents */
        for (uint32_t i = 0; i < erdt->num_dev_agents; ++i) {
                const struct pqos_device_agent_info *agent =
                    &erdt->dev_agents[i];
                char domain_str[DOMAIN_STR_LEN];

                snprintf(domain_str, sizeof(domain_str), "DEV_AGENT: %u", i);

                for (size_t m = 0;
                     m < sizeof(dump_space_map) / sizeof(dump_space_map[0]);
                     ++m) {
                        if (dump_space_map[m].domain_type != DM_TYPE_DEVICE)
                                continue;
                        const char *agent_base = (const char *)agent;
                        const void *space_struct =
                            agent_base + dump_space_map[m].struct_offset;
                        uint64_t base =
                            *(const uint64_t *)((const char *)space_struct +
                                                dump_space_map[m].base_offset);
                        uint32_t size =
                            *(const uint32_t *)((const char *)space_struct +
                                                dump_space_map[m].size_offset) *
                            dump_space_map[m].size_adjustment;
                        if (base && size) {
                                print_mmio_mm_row(domain_str,
                                                  dump_space_map[m].space_name,
                                                  base, size);
                                printed++;
                        }
                }
        }

        /* If nothing printed, return error */
        if (!printed) {
                printf("No MMIO spaces found.\n");
                return PQOS_RETVAL_UNAVAILABLE;
        }

        return PQOS_RETVAL_OK;
}
