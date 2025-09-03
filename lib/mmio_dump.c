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
 *
 */

#include "mmio_dump.h"

#include "common.h"
#include "log.h"
#include "pqos.h"

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#define DUMP_LINE_LEN        512
#define BYTE_ELEMS_PER_LINE  16
#define QWORD_ELEMS_PER_LINE 2

/**
 * brief Hex dump output helpers
 **/

/**
 *  @brief Print a hex dump
 *
 *  @param [in] buf buffer to dump
 *  @param [in] length number of elements to dump
 *  @param [in] width_bytes element width in bytes (1 or 8)
 *  @param [in] le 1 for little-endian, 0 for big-endian
 *  @param [in] binary 1 for binary output, 0 for hexadecimal output
 **/
static void
print_hex_dump(const uint8_t *buf,
               size_t length,
               unsigned width_bytes,
               int le,
               int binary)
{
        size_t i, j, k;
        uint8_t current_byte;
        char line[DUMP_LINE_LEN];
        size_t elems_per_line =
            (width_bytes == 8) ? QWORD_ELEMS_PER_LINE : BYTE_ELEMS_PER_LINE;
        size_t line_num = length / elems_per_line;

        LOG_DEBUG("Dumping %zu elements as %s with width %u bytes\n", length,
                  le ? "little-endian" : "big-endian", width_bytes);
        for (i = 0; i < line_num; i++) {
                size_t offset = i * width_bytes * elems_per_line;

                snprintf(line, sizeof(line), "%06zx ", offset);
                for (j = 0; j < elems_per_line; j++) {
                        LOG_DEBUG("elem_idx: %u\n", j);
                        for (k = 0; k < width_bytes; k++) {
                                LOG_DEBUG("elem_byte_offset: %02x\n",
                                          (le) ? (offset + j * width_bytes + k)
                                               : (offset +
                                                  (j + 1) * width_bytes - k -
                                                  1));
                                current_byte =
                                    (le) ? buf[offset + j * width_bytes + k]
                                         : buf[offset + (j + 1) * width_bytes -
                                               k - 1];
                                if (binary) {
                                        for (int b = 7; b >= 0; --b)
                                                snprintf(
                                                    line + strlen(line),
                                                    sizeof(line) - strlen(line),
                                                    "%c",
                                                    (current_byte & (1 << b))
                                                        ? '1'
                                                        : '0');
                                } else {
                                        snprintf(line + strlen(line),
                                                 sizeof(line) - strlen(line),
                                                 "%02x", current_byte);
                                }
                        }
                        snprintf(line + strlen(line),
                                 sizeof(line) - strlen(line), " ");
                }
                printf("%s\n", line);
        }
}

/**
 * brief Dump for a single address range
 *
 * @param [in] base address
 * @param [in] size size in bytes
 * @param [in] offset offset in elements from base
 * @param [in] length length in elements
 * @param [in] width_bytes element width in bytes (1 or 8)
 * @param [in] le 1 for little-endian, 0 for big-endian
 * @param [in] binary 1 for binary output, 0 for hexadecimal output
 *
 * @return Operation status
 **/
static int
dump_mmio_range(uint64_t base,
                uint32_t size,
                unsigned long offset,
                unsigned long length,
                unsigned width_bytes,
                int le,
                int binary)
{
        unsigned int offset_bytes = offset * width_bytes;
        unsigned int length_bytes = length * width_bytes;

        if ((offset + length_bytes) > size) {
                LOG_ERROR("View port out of range\n");
                return PQOS_RETVAL_PARAM;
        }
        void *map = pqos_mmap_write(base + offset_bytes, length_bytes);

        if (map == NULL)
                return PQOS_RETVAL_ERROR;

        const uint8_t *buf = (const uint8_t *)map;

        print_hex_dump(buf, length, width_bytes, le, binary);

        pqos_munmap(map, length_bytes);

        return PQOS_RETVAL_OK;
}

/**
 * brief MMIO dump for a single MMIO space structure
 *
 * @param [in] space_struct pointer to the MMIO space structure
 * @param [in] space_type type of the MMIO space
 * @param [in] dump dump configuration
 *
 * @return Operation status
 **/
static int
dump_mmio_space(const void *space_struct,
                enum pqos_dump_space space_type,
                const struct pqos_dump *dump)
{
        uint64_t base = 0;
        uint32_t size = 0;

        switch (space_type) {
        case DUMP_SPACE_CMRC: {
                const struct pqos_erdt_cmrc *s =
                    (const struct pqos_erdt_cmrc *)space_struct;
                base = s->block_base_addr;
                size = s->block_size * PAGE_SIZE;
                break;
        }
        case DUMP_SPACE_MMRC: {
                const struct pqos_erdt_mmrc *s =
                    (const struct pqos_erdt_mmrc *)space_struct;
                base = s->reg_block_base_addr;
                size = s->reg_block_size * PAGE_SIZE;
                break;
        }
        case DUMP_SPACE_MARC_OPT: {
                const struct pqos_erdt_marc *s =
                    (const struct pqos_erdt_marc *)space_struct;
                base = s->opt_bw_reg_block_base_addr;
                size = s->reg_block_size * PAGE_SIZE;
                break;
        }
        case DUMP_SPACE_MARC_MIN: {
                const struct pqos_erdt_marc *s =
                    (const struct pqos_erdt_marc *)space_struct;
                base = s->min_bw_reg_block_base_addr;
                size = s->reg_block_size * PAGE_SIZE;
                break;
        }
        case DUMP_SPACE_MARC_MAX: {
                const struct pqos_erdt_marc *s =
                    (const struct pqos_erdt_marc *)space_struct;
                base = s->max_bw_reg_block_base_addr;
                size = s->reg_block_size * PAGE_SIZE;
                break;
        }
        case DUMP_SPACE_CMRD: {
                const struct pqos_erdt_cmrd *s =
                    (const struct pqos_erdt_cmrd *)space_struct;
                base = s->reg_base_addr;
                size = s->reg_block_size * PAGE_SIZE;
                break;
        }
        case DUMP_SPACE_IBRD_TOTAL_BW: {
                const struct pqos_erdt_ibrd *s =
                    (const struct pqos_erdt_ibrd *)space_struct;
                base = s->reg_base_addr;
                size = s->reg_block_size * PAGE_SIZE;
                break;
        }
        case DUMP_SPACE_IBRD_MISS_BW: {
                const struct pqos_erdt_ibrd *s =
                    (const struct pqos_erdt_ibrd *)space_struct;
                base = s->reg_base_addr;
                size = s->reg_block_size * PAGE_SIZE;
                break;
        }
        case DUMP_SPACE_CARD: {
                const struct pqos_erdt_card *s =
                    (const struct pqos_erdt_card *)space_struct;
                base = s->reg_base_addr;
                size = s->reg_block_size * PAGE_SIZE;
                break;
        }
        default:
                return PQOS_RETVAL_PARAM;
        }

        unsigned width_bytes =
            (dump->fmt.dump_width == DUMP_WIDTH_8BIT) ? 1 : 8;
        int le = dump->fmt.le;
        int binary = dump->fmt.bin;
        unsigned long offset = dump->view.offset;
        unsigned long length = dump->view.length;

        printf("MMIO space dump: base=0x%lx size=0x%x offset=%lu len=%lu "
               "width(bytes)=%u le=%d bin=%d\n",
               (unsigned long)base, size, offset, length, width_bytes, le,
               binary);

        return dump_mmio_range(base, size, offset, length, width_bytes, le,
                               binary);
}

/**
 * brief Main MMIO dump function
 *
 * @param [in] dump_cfg dump configuration
 *
 * @return Operation status
 **/
int
mmio_dump(const struct pqos_dump *dump_cfg)
{
        const struct pqos_erdt_info *erdt = _pqos_get_erdt();

        if (!dump_cfg || !erdt)
                return PQOS_RETVAL_PARAM;

        int dump_all = (dump_cfg->topology.space == DUMP_SPACE_ALL);
        int domains_all = (dump_cfg->topology.num_domain_ids == 0);
        int domain_num = dump_cfg->topology.num_domain_ids;
        uint16_t *curr_domain = dump_cfg->topology.domain_ids;

        /* CPU agents */
        for (uint32_t i = 0; i < erdt->num_cpu_agents; ++i) {
                const struct pqos_cpu_agent_info *agent = &erdt->cpu_agents[i];

                if (!domains_all && *curr_domain != agent->rmdd.domain_id)
                        continue;

                for (size_t m = 0;
                     m < sizeof(dump_space_map) / sizeof(dump_space_map[0]);
                     ++m) {
                        if (dump_space_map[m].domain_type != DM_TYPE_CPU)
                                continue;
                        if (!dump_all &&
                            dump_space_map[m].space != dump_cfg->topology.space)
                                continue;
                        const void *space_struct =
                            (const char *)agent +
                            dump_space_map[m].struct_offset;
                        dump_mmio_space(space_struct, dump_space_map[m].space,
                                        dump_cfg);
                }

                if (!domains_all) {
                        domain_num--;
                        if (domain_num == 0)
                                break;
                        curr_domain++;
                }
        }

        domain_num = dump_cfg->topology.num_domain_ids;
        curr_domain = dump_cfg->topology.domain_ids;

        /* Device agents */
        for (uint32_t i = 0; i < erdt->num_dev_agents; ++i) {
                const struct pqos_device_agent_info *agent =
                    &erdt->dev_agents[i];

                if (!domains_all && *curr_domain != agent->rmdd.domain_id)
                        continue;

                for (size_t m = 0;
                     m < sizeof(dump_space_map) / sizeof(dump_space_map[0]);
                     ++m) {
                        if (dump_space_map[m].domain_type != DM_TYPE_DEVICE)
                                continue;
                        if (!dump_all &&
                            dump_space_map[m].space != dump_cfg->topology.space)
                                continue;
                        const void *space_struct =
                            (const char *)agent +
                            dump_space_map[m].struct_offset;
                        dump_mmio_space(space_struct, dump_space_map[m].space,
                                        dump_cfg);
                }

                if (!domains_all) {
                        domain_num--;
                        if (domain_num == 0)
                                break;
                        curr_domain++;
                }
        }

        return PQOS_RETVAL_OK;
}
