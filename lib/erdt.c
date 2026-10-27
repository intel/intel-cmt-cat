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
 *
 */

#include "erdt.h"

#include "acpi.h"
#include "cap.h"
#include "common.h"
#include "cpuinfo.h"
#include "log.h"
#include "pci.h"
#include "utils.h"

#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define RMDD_L3_DOMAIN    1
#define RMDD_IO_L3_DOMAIN 2
#define RMDD_DOMAIN_MASK  (RMDD_L3_DOMAIN | RMDD_IO_L3_DOMAIN)

#define PATH_PAIR_LENGTH 2

/**
 * ERDT ACPI table information.
 * This pointer is allocated and initialized in this module.
 */
static struct pqos_erdt_info *p_erdt_info = NULL;

static struct pqos_channels_domains *p_channels_domains = NULL;

struct __attribute__((__packed__)) erdt_structure_header {
        uint16_t type;
        uint16_t length;
};

/**
 * @brief Gets and validates the next ERDT sub-structure
 *
 * @param p_cursor Current parsing position
 * @param p_remaining Number of bytes remaining in the containing structure
 * @param p_structure Validated sub-structure
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK success
 */
static int
erdt_get_structure(const uint8_t **p_cursor,
                   size_t *p_remaining,
                   const void **p_structure)
{
        const struct erdt_structure_header *p_header;

        if (*p_remaining < sizeof(*p_header)) {
                LOG_ERROR("Truncated ERDT sub-structure\n");
                return PQOS_RETVAL_ERROR;
        }

        p_header =
            (const struct erdt_structure_header *)(const void *)*p_cursor;
        if (p_header->length < sizeof(*p_header) ||
            p_header->length > *p_remaining) {
                LOG_ERROR("Invalid ERDT sub-structure type %u length %u\n",
                          (unsigned)p_header->type, (unsigned)p_header->length);
                return PQOS_RETVAL_ERROR;
        }

        *p_structure = *p_cursor;
        *p_cursor += p_header->length;
        *p_remaining -= p_header->length;

        return PQOS_RETVAL_OK;
}

/**
 * @brief Copies a correction-factor list from an ACPI table
 *
 * @param p_pqos_correction_factor Destination correction-factor list
 * @param p_acpi_correction_factor Source correction-factor list
 * @param count Number of correction factors
 * @param max_rmids Maximum number of RMIDs supported
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK success
 */
static int
copy_correction_factor(uint32_t **p_pqos_correction_factor,
                       const void *p_acpi_correction_factor,
                       size_t count,
                       uint32_t max_rmids)
{
        size_t bytes;

        if (count != NO_CORRECTION_FACTOR &&
            count != SINGLE_CORRECTION_FACTOR &&
            (max_rmids == UINT32_MAX || count != (size_t)max_rmids + 1)) {
                LOG_ERROR("Invalid correction-factor count %zu\n", count);
                return PQOS_RETVAL_ERROR;
        }

        *p_pqos_correction_factor = NULL;
        if (count == NO_CORRECTION_FACTOR)
                return PQOS_RETVAL_OK;

        if (count > SIZE_MAX / sizeof(**p_pqos_correction_factor)) {
                LOG_ERROR("Correction-factor list is too large\n");
                return PQOS_RETVAL_ERROR;
        }
        bytes = count * sizeof(**p_pqos_correction_factor);

        *p_pqos_correction_factor = malloc(bytes);
        if (*p_pqos_correction_factor == NULL) {
                LOG_ERROR("Can't allocate memory for correction factors\n");
                return PQOS_RETVAL_ERROR;
        }

        memcpy(*p_pqos_correction_factor, p_acpi_correction_factor, bytes);

        return PQOS_RETVAL_OK;
}

/**
 * @brief Populates CACD information from an ERDT sub-structure
 *
 * @param p_cacd CACD information to populate
 * @param p_acpi_cacd ACPI CACD sub-structure
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK success
 */
static int
erdt_populate_cacd(struct pqos_erdt_cacd *p_cacd,
                   const struct acpi_table_erdt_cacd *p_acpi_cacd)
{
        size_t enum_ids_size = p_acpi_cacd->length - sizeof(*p_acpi_cacd);

        if (enum_ids_size % sizeof(*p_cacd->enumeration_ids) != 0) {
                LOG_ERROR("Invalid CACD enumeration ID list length %zu\n",
                          enum_ids_size);
                return PQOS_RETVAL_ERROR;
        }

        p_cacd->rmdd_domain_id = p_acpi_cacd->rmdd_domain_id;
        p_cacd->enum_ids_length =
            enum_ids_size / sizeof(*p_cacd->enumeration_ids);
        if (enum_ids_size == 0)
                return PQOS_RETVAL_OK;

        p_cacd->enumeration_ids = malloc(enum_ids_size);
        if (p_cacd->enumeration_ids == NULL) {
                LOG_ERROR("Can't allocate memory for enumeration IDs\n");
                return PQOS_RETVAL_ERROR;
        }
        memcpy(p_cacd->enumeration_ids, p_acpi_cacd->enumeration_ids,
               enum_ids_size);

        return PQOS_RETVAL_OK;
}

/**
 * @brief Populates CMRC information from an ERDT sub-structure
 *
 * @param p_cmrc CMRC information to populate
 * @param p_acpi_cmrc ACPI CMRC sub-structure
 */
static void
erdt_populate_cmrc(struct pqos_erdt_cmrc *p_cmrc,
                   const struct acpi_table_erdt_cmrc *p_acpi_cmrc)
{
        p_cmrc->flags = p_acpi_cmrc->flags;
        p_cmrc->reg_index_func_ver =
            p_acpi_cmrc->register_indexing_function_version;
        p_cmrc->block_base_addr =
            p_acpi_cmrc->cmt_register_block_base_address_for_cpu;
        p_cmrc->block_size = p_acpi_cmrc->cmt_register_block_size_for_cpu;
        p_cmrc->clump_size = p_acpi_cmrc->cmt_register_clump_size_for_cpu;
        p_cmrc->clump_stride = p_acpi_cmrc->cmt_register_clump_stride_for_cpu;
        p_cmrc->upscaling_factor = p_acpi_cmrc->cmt_counter_upscaling_factor;
}

/**
 * @brief Populates MMRC information from an ERDT sub-structure
 *
 * @param p_mmrc MMRC information to populate
 * @param p_acpi_mmrc ACPI MMRC sub-structure
 * @param max_rmids Maximum number of RMIDs supported
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK success
 */
static int
erdt_populate_mmrc(struct pqos_erdt_mmrc *p_mmrc,
                   const struct acpi_table_erdt_mmrc *p_acpi_mmrc,
                   uint32_t max_rmids)
{
        const size_t available = p_acpi_mmrc->length - sizeof(*p_acpi_mmrc);
        const size_t factor_size = sizeof(*p_acpi_mmrc->mbm_correction_factor);

        if (available % factor_size != 0 ||
            p_acpi_mmrc->mbm_correction_factor_list_length !=
                available / factor_size) {
                LOG_ERROR("MMRC correction-factor list exceeds structure\n");
                return PQOS_RETVAL_ERROR;
        }

        p_mmrc->flags = p_acpi_mmrc->flags;
        p_mmrc->reg_index_func_ver =
            p_acpi_mmrc->register_indexing_function_version;
        p_mmrc->reg_block_base_addr =
            p_acpi_mmrc->mbm_register_block_base_address;
        p_mmrc->reg_block_size = p_acpi_mmrc->mbm_register_block_size;
        p_mmrc->counter_width = p_acpi_mmrc->mbm_counter_width;
        p_mmrc->upscaling_factor = p_acpi_mmrc->mbm_counter_upscaling_factor;
        p_mmrc->correction_factor_length =
            p_acpi_mmrc->mbm_correction_factor_list_length;

        return copy_correction_factor(
            &p_mmrc->correction_factor, p_acpi_mmrc->mbm_correction_factor,
            p_mmrc->correction_factor_length, max_rmids);
}

/**
 * @brief Populates MARC information from an ERDT sub-structure
 *
 * @param p_marc MARC information to populate
 * @param p_acpi_marc ACPI MARC sub-structure
 */
static void
erdt_populate_marc(struct pqos_erdt_marc *p_marc,
                   const struct acpi_table_erdt_marc *p_acpi_marc)
{
        p_marc->flags = p_acpi_marc->mba_flags;
        p_marc->reg_index_func_ver =
            p_acpi_marc->register_indexing_function_version;
        p_marc->opt_bw_reg_block_base_addr =
            p_acpi_marc->mba_optimal_bw_register_block_base_address;
        p_marc->min_bw_reg_block_base_addr =
            p_acpi_marc->mba_minimum_bw_register_block_base_address;
        p_marc->max_bw_reg_block_base_addr =
            p_acpi_marc->mba_maximum_bw_register_block_base_address;
        p_marc->reg_block_size = p_acpi_marc->mba_register_block_size;
        p_marc->control_window_range = p_acpi_marc->mba_bw_control_window_range;
}

/**
 * @brief Calculates the number of DASE structures in a DACD structure
 *
 * @param length Length of the DASE data
 * @param p_acpi_dase First ACPI DASE sub-structure
 * @param num_dases Number of DASE structures found
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK success
 */
static int
erdt_calculate_num_dases(size_t length,
                         const struct acpi_table_erdt_dase *p_acpi_dase,
                         uint32_t *num_dases)
{
        const struct acpi_table_erdt_dase *p_dase = p_acpi_dase;
        uint32_t count = 0;

        while (length > 0) {
                if (length < ACPI_ERDT_STRUCT_DASE_HEADER_LENGTH ||
                    p_dase->length < ACPI_ERDT_STRUCT_DASE_HEADER_LENGTH +
                                         PATH_PAIR_LENGTH ||
                    p_dase->length > length) {
                        LOG_ERROR("Invalid DASE structure length\n");
                        return PQOS_RETVAL_ERROR;
                }
                if ((p_dase->length - ACPI_ERDT_STRUCT_DASE_HEADER_LENGTH) %
                        PATH_PAIR_LENGTH !=
                    0) {
                        LOG_ERROR(
                            "Invalid DASE path length %u\n",
                            (unsigned)(p_dase->length -
                                       ACPI_ERDT_STRUCT_DASE_HEADER_LENGTH));
                        return PQOS_RETVAL_ERROR;
                }
                if (p_dase->type != 1 && p_dase->type != 2) {
                        LOG_ERROR("Unsupported DASE type %u\n",
                                  (unsigned)p_dase->type);
                        return PQOS_RETVAL_ERROR;
                }

                length -= p_dase->length;
                p_dase = (const struct acpi_table_erdt_dase
                              *)(const void *)((const uint8_t *)p_dase +
                                               p_dase->length);
                count++;
        }

        *num_dases = count;
        return PQOS_RETVAL_OK;
}

/**
 * @brief Frees DACD device agent scope entries
 *
 * @param p_dacd DACD information to free
 */
static void
erdt_free_dacd(struct pqos_erdt_dacd *p_dacd)
{
        uint32_t i;

        if (p_dacd->dase != NULL)
                for (i = 0; i < p_dacd->num_dases; i++)
                        free(p_dacd->dase[i].path);

        free(p_dacd->dase);
        memset(p_dacd, 0, sizeof(*p_dacd));
}

/**
 * @brief Populates DACD information from an ERDT sub-structure
 *
 * @param p_dacd DACD information to populate
 * @param p_acpi_dacd ACPI DACD sub-structure
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK success
 */
static int
erdt_populate_dacd(struct pqos_erdt_dacd *p_dacd,
                   const struct acpi_table_erdt_dacd *p_acpi_dacd)
{
        const struct acpi_table_erdt_dase *p_acpi_dase;
        size_t dase_size = p_acpi_dacd->length - sizeof(*p_acpi_dacd);
        uint32_t num_dases;
        uint32_t i;
        int ret;

        p_dacd->rmdd_domain_id = p_acpi_dacd->rmdd_domain_id;
        if (dase_size == 0) {
                LOG_ERROR("DACD contains no DASE structures\n");
                return PQOS_RETVAL_ERROR;
        }

        ret =
            erdt_calculate_num_dases(dase_size, p_acpi_dacd->dase, &num_dases);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        p_dacd->dase = calloc(num_dases, sizeof(*p_dacd->dase));
        if (p_dacd->dase == NULL) {
                LOG_ERROR("Can't allocate memory for DASE structures\n");
                return PQOS_RETVAL_ERROR;
        }
        p_dacd->num_dases = num_dases;

        p_acpi_dase = p_acpi_dacd->dase;
        for (i = 0; i < num_dases; i++) {
                struct pqos_erdt_dase *p_dase = &p_dacd->dase[i];

                p_dase->type = p_acpi_dase->type;
                p_dase->segment_number = p_acpi_dase->segment_number;
                p_dase->start_bus_number = p_acpi_dase->start_bus_number;
                p_dase->path_length =
                    p_acpi_dase->length - ACPI_ERDT_STRUCT_DASE_HEADER_LENGTH;
                if (p_dase->path_length > 0) {
                        p_dase->path = malloc(p_dase->path_length);
                        if (p_dase->path == NULL) {
                                LOG_ERROR(
                                    "Can't allocate memory for DASE path\n");
                                erdt_free_dacd(p_dacd);
                                return PQOS_RETVAL_ERROR;
                        }
                        memcpy(p_dase->path, p_acpi_dase->path,
                               p_dase->path_length);
                }

                p_acpi_dase =
                    (const struct acpi_table_erdt_dase
                         *)(const void *)((const uint8_t *)p_acpi_dase +
                                          p_acpi_dase->length);
        }

        return PQOS_RETVAL_OK;
}

/**
 * @brief Populates CMRD information from an ERDT sub-structure
 *
 * @param p_cmrd CMRD information to populate
 * @param p_acpi_cmrd ACPI CMRD sub-structure
 */
static void
erdt_populate_cmrd(struct pqos_erdt_cmrd *p_cmrd,
                   const struct acpi_table_erdt_cmrd *p_acpi_cmrd)
{
        p_cmrd->flags = p_acpi_cmrd->flags;
        p_cmrd->reg_index_func_ver =
            p_acpi_cmrd->register_indexing_function_version;
        p_cmrd->reg_base_addr = p_acpi_cmrd->register_base_address;
        p_cmrd->reg_block_size = p_acpi_cmrd->register_block_size;
        p_cmrd->offset = p_acpi_cmrd->cmt_register_offset_for_io;
        p_cmrd->clump_size = p_acpi_cmrd->cmt_register_clump_size_for_io;
        p_cmrd->upscaling_factor = p_acpi_cmrd->cmt_counter_upscaling_factor;
}

/**
 * @brief Populates IBRD information from an ERDT sub-structure
 *
 * @param p_ibrd IBRD information to populate
 * @param p_acpi_ibrd ACPI IBRD sub-structure
 * @param max_rmids Maximum number of RMIDs supported
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK success
 */
static int
erdt_populate_ibrd(struct pqos_erdt_ibrd *p_ibrd,
                   const struct acpi_table_erdt_ibrd *p_acpi_ibrd,
                   uint32_t max_rmids)
{
        const size_t available = p_acpi_ibrd->length - sizeof(*p_acpi_ibrd);
        const size_t factor_size =
            sizeof(*p_acpi_ibrd->io_bw_counter_correction_factor);

        if (available % factor_size != 0 ||
            p_acpi_ibrd->io_bw_counter_correction_factor_list_length !=
                available / factor_size) {
                LOG_ERROR("IBRD correction-factor list exceeds structure\n");
                return PQOS_RETVAL_ERROR;
        }

        p_ibrd->flags = p_acpi_ibrd->flags;
        p_ibrd->reg_index_func_ver =
            p_acpi_ibrd->register_indexing_function_version;
        p_ibrd->reg_base_addr = p_acpi_ibrd->register_base_address;
        p_ibrd->reg_block_size = p_acpi_ibrd->register_block_size;
        p_ibrd->bw_reg_offset = p_acpi_ibrd->total_io_bw_register_offset;
        p_ibrd->miss_bw_reg_offset = p_acpi_ibrd->io_miss_bw_register_offset;
        p_ibrd->bw_reg_clump_size =
            p_acpi_ibrd->total_io_bw_register_clump_size;
        p_ibrd->miss_reg_clump_size = p_acpi_ibrd->io_miss_register_clump_size;
        p_ibrd->counter_width = p_acpi_ibrd->io_bw_counter_width;
        p_ibrd->upscaling_factor = p_acpi_ibrd->io_bw_counter_upscaling_factor;
        p_ibrd->correction_factor_length =
            p_acpi_ibrd->io_bw_counter_correction_factor_list_length;

        return copy_correction_factor(
            &p_ibrd->correction_factor,
            p_acpi_ibrd->io_bw_counter_correction_factor,
            p_ibrd->correction_factor_length, max_rmids);
}

/**
 * @brief Populates CARD information from an ERDT sub-structure
 *
 * @param p_card CARD information to populate
 * @param p_acpi_card ACPI CARD sub-structure
 */
static void
erdt_populate_card(struct pqos_erdt_card *p_card,
                   const struct acpi_table_erdt_card *p_acpi_card)
{
        p_card->contention_bitmask_valid =
            !!(p_acpi_card->flags & CARD_CONTENTION_BITMASKS_VALID_BIT);
        p_card->non_contiguous_cbm =
            !!(p_acpi_card->flags & CARD_NON_CONTIGUOUS_BITMASKS_SUPPORTED_BIT);
        p_card->zero_length_bitmask =
            !!(p_acpi_card->flags & CARD_ZERO_LENGTH_BITMASKS_BIT);
        p_card->contention_bitmask = p_acpi_card->contention_bitmask;
        p_card->reg_index_func_ver =
            p_acpi_card->register_indexing_function_version;
        p_card->reg_base_addr = p_acpi_card->register_base_address;
        p_card->reg_block_size = p_acpi_card->register_block_size;
        p_card->cat_reg_offset =
            p_acpi_card->cache_allocation_register_offsets_for_io;
        p_card->cat_reg_block_size =
            p_acpi_card->cache_allocation_register_block_size;
}

/**
 * @brief Populates common RMDD information from an ERDT sub-structure
 *
 * @param p_rmdd RMDD information to populate
 * @param p_acpi_rmdd ACPI RMDD sub-structure
 */
static void
erdt_populate_rmdd(struct pqos_erdt_rmdd *p_rmdd,
                   const struct acpi_table_erdt_rmdd *p_acpi_rmdd)
{
        p_rmdd->flags = p_acpi_rmdd->flags;
        p_rmdd->num_io_l3_slices = p_acpi_rmdd->number_of_io_l3_slices;
        p_rmdd->num_io_l3_sets = p_acpi_rmdd->number_of_io_l3_sets;
        p_rmdd->num_io_l3_ways = p_acpi_rmdd->number_of_io_l3_ways;
        p_rmdd->domain_id = p_acpi_rmdd->domain_id;
        p_rmdd->max_rmids = p_acpi_rmdd->max_rmids;
        p_rmdd->control_reg_base_addr =
            p_acpi_rmdd->control_register_base_address;
        p_rmdd->control_reg_size = p_acpi_rmdd->control_register_size;
}

/**
 * @brief Populates ERDT information for a CPU agent
 *
 * @param p_cpu_agent_info CPU agent information to populate
 * @param p_acpi_rmdd ACPI RMDD structure for the CPU agent
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK success
 */
static int
erdt_populate_rmdd_cpu_agent(struct pqos_cpu_agent_info *p_cpu_agent_info,
                             const struct acpi_table_erdt_rmdd *p_acpi_rmdd)
{
        const uint8_t *p_cursor =
            (const uint8_t *)p_acpi_rmdd + sizeof(*p_acpi_rmdd);
        size_t remaining = p_acpi_rmdd->length - sizeof(*p_acpi_rmdd);
        const struct erdt_structure_header *p_header;
        const void *p_structure;
        unsigned seen = 0;
        int ret;

        erdt_populate_rmdd(&p_cpu_agent_info->rmdd, p_acpi_rmdd);

        while (remaining > 0) {
                ret = erdt_get_structure(&p_cursor, &remaining, &p_structure);
                if (ret != PQOS_RETVAL_OK)
                        return ret;

                p_header = p_structure;
                switch (p_header->type) {
                case ACPI_ERDT_STRUCT_CACD_TYPE: {
                        const struct acpi_table_erdt_cacd *p_cacd = p_structure;

                        if (seen & (1U << ACPI_ERDT_STRUCT_CACD_TYPE) ||
                            p_header->length < sizeof(*p_cacd) ||
                            p_cacd->rmdd_domain_id != p_acpi_rmdd->domain_id) {
                                LOG_ERROR("Invalid CACD structure\n");
                                return PQOS_RETVAL_ERROR;
                        }
                        seen |= 1U << ACPI_ERDT_STRUCT_CACD_TYPE;
                        ret =
                            erdt_populate_cacd(&p_cpu_agent_info->cacd, p_cacd);
                        if (ret != PQOS_RETVAL_OK)
                                return ret;
                        break;
                }
                case ACPI_ERDT_STRUCT_CMRC_TYPE:
                        if (seen & (1U << ACPI_ERDT_STRUCT_CMRC_TYPE) ||
                            p_header->length !=
                                sizeof(struct acpi_table_erdt_cmrc)) {
                                LOG_ERROR("Invalid CMRC structure\n");
                                return PQOS_RETVAL_ERROR;
                        }
                        seen |= 1U << ACPI_ERDT_STRUCT_CMRC_TYPE;
                        erdt_populate_cmrc(&p_cpu_agent_info->cmrc,
                                           p_structure);
                        break;
                case ACPI_ERDT_STRUCT_MMRC_TYPE:
                        if (seen & (1U << ACPI_ERDT_STRUCT_MMRC_TYPE) ||
                            p_header->length <
                                sizeof(struct acpi_table_erdt_mmrc)) {
                                LOG_ERROR("Invalid MMRC structure\n");
                                return PQOS_RETVAL_ERROR;
                        }
                        seen |= 1U << ACPI_ERDT_STRUCT_MMRC_TYPE;
                        ret = erdt_populate_mmrc(
                            &p_cpu_agent_info->mmrc, p_structure,
                            p_cpu_agent_info->rmdd.max_rmids);
                        if (ret != PQOS_RETVAL_OK)
                                return ret;
                        break;
                case ACPI_ERDT_STRUCT_MARC_TYPE:
                        if (seen & (1U << ACPI_ERDT_STRUCT_MARC_TYPE) ||
                            p_header->length !=
                                sizeof(struct acpi_table_erdt_marc)) {
                                LOG_ERROR("Invalid MARC structure\n");
                                return PQOS_RETVAL_ERROR;
                        }
                        seen |= 1U << ACPI_ERDT_STRUCT_MARC_TYPE;
                        erdt_populate_marc(&p_cpu_agent_info->marc,
                                           p_structure);
                        break;
                default:
                        LOG_DEBUG("Skipping ERDT sub-structure type %u\n",
                                  (unsigned)p_header->type);
                        break;
                }
        }

        if (!(seen & (1U << ACPI_ERDT_STRUCT_CACD_TYPE))) {
                LOG_ERROR("CPU RMDD contains no CACD structure\n");
                return PQOS_RETVAL_ERROR;
        }

        return PQOS_RETVAL_OK;
}

/**
 * @brief Populates ERDT information for a device agent
 *
 * @param p_dev_agent_info Device agent information to populate
 * @param p_acpi_rmdd ACPI RMDD structure for the device agent
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK success
 */
static int
erdt_populate_rmdd_device_agent(struct pqos_device_agent_info *p_dev_agent_info,
                                const struct acpi_table_erdt_rmdd *p_acpi_rmdd)
{
        const uint8_t *p_cursor =
            (const uint8_t *)p_acpi_rmdd + sizeof(*p_acpi_rmdd);
        size_t remaining = p_acpi_rmdd->length - sizeof(*p_acpi_rmdd);
        const struct erdt_structure_header *p_header;
        const void *p_structure;
        unsigned seen = 0;
        int ret;

        erdt_populate_rmdd(&p_dev_agent_info->rmdd, p_acpi_rmdd);

        while (remaining > 0) {
                ret = erdt_get_structure(&p_cursor, &remaining, &p_structure);
                if (ret != PQOS_RETVAL_OK)
                        return ret;

                p_header = p_structure;
                switch (p_header->type) {
                case ACPI_ERDT_STRUCT_DACD_TYPE: {
                        const struct acpi_table_erdt_dacd *p_dacd = p_structure;

                        if (seen & (1U << ACPI_ERDT_STRUCT_DACD_TYPE) ||
                            p_header->length < sizeof(*p_dacd) ||
                            p_dacd->rmdd_domain_id != p_acpi_rmdd->domain_id) {
                                LOG_ERROR("Invalid DACD structure\n");
                                return PQOS_RETVAL_ERROR;
                        }
                        seen |= 1U << ACPI_ERDT_STRUCT_DACD_TYPE;
                        ret =
                            erdt_populate_dacd(&p_dev_agent_info->dacd, p_dacd);
                        if (ret != PQOS_RETVAL_OK)
                                return ret;
                        break;
                }
                case ACPI_ERDT_STRUCT_CMRD_TYPE:
                        if (seen & (1U << ACPI_ERDT_STRUCT_CMRD_TYPE) ||
                            p_header->length !=
                                sizeof(struct acpi_table_erdt_cmrd)) {
                                LOG_ERROR("Invalid CMRD structure\n");
                                return PQOS_RETVAL_ERROR;
                        }
                        seen |= 1U << ACPI_ERDT_STRUCT_CMRD_TYPE;
                        erdt_populate_cmrd(&p_dev_agent_info->cmrd,
                                           p_structure);
                        break;
                case ACPI_ERDT_STRUCT_IBRD_TYPE:
                        if (seen & (1U << ACPI_ERDT_STRUCT_IBRD_TYPE) ||
                            p_header->length <
                                sizeof(struct acpi_table_erdt_ibrd)) {
                                LOG_ERROR("Invalid IBRD structure\n");
                                return PQOS_RETVAL_ERROR;
                        }
                        seen |= 1U << ACPI_ERDT_STRUCT_IBRD_TYPE;
                        ret = erdt_populate_ibrd(
                            &p_dev_agent_info->ibrd, p_structure,
                            p_dev_agent_info->rmdd.max_rmids);
                        if (ret != PQOS_RETVAL_OK)
                                return ret;
                        break;
                case ACPI_ERDT_STRUCT_CARD_TYPE:
                        if (seen & (1U << ACPI_ERDT_STRUCT_CARD_TYPE) ||
                            p_header->length !=
                                sizeof(struct acpi_table_erdt_card)) {
                                LOG_ERROR("Invalid CARD structure\n");
                                return PQOS_RETVAL_ERROR;
                        }
                        seen |= 1U << ACPI_ERDT_STRUCT_CARD_TYPE;
                        erdt_populate_card(&p_dev_agent_info->card,
                                           p_structure);
                        break;
                default:
                        LOG_DEBUG("Skipping ERDT sub-structure type %u\n",
                                  (unsigned)p_header->type);
                        break;
                }
        }

        if (!(seen & (1U << ACPI_ERDT_STRUCT_DACD_TYPE))) {
                LOG_ERROR("Device RMDD contains no DACD structure\n");
                return PQOS_RETVAL_ERROR;
        }

        return PQOS_RETVAL_OK;
}

/**
 * @brief Counts CPU and device RMDD structures in an ERDT table
 *
 * @param p_acpi_erdt ACPI ERDT table to parse
 * @param p_num_cpu_agents Number of CPU RMDD structures
 * @param p_num_dev_agents Number of device RMDD structures
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK success
 */
static int
erdt_count_rmdds(const struct acpi_table_erdt *p_acpi_erdt,
                 unsigned *p_num_cpu_agents,
                 unsigned *p_num_dev_agents)
{
        const uint8_t *p_start =
            (const uint8_t *)p_acpi_erdt->erdt_sub_structures;
        const uint8_t *p_cursor = p_start;
        size_t remaining = p_acpi_erdt->header.header.length -
                           sizeof(struct acpi_table_erdt_header);
        const struct erdt_structure_header *p_header;
        const struct acpi_table_erdt_rmdd *p_rmdd;
        const uint8_t *p_prior;
        size_t prior_remaining;
        const void *p_structure;
        unsigned num_rmdds = 0;
        int ret;

        *p_num_cpu_agents = 0;
        *p_num_dev_agents = 0;

        while (remaining > 0) {
                ret = erdt_get_structure(&p_cursor, &remaining, &p_structure);
                if (ret != PQOS_RETVAL_OK)
                        return ret;

                p_header = p_structure;
                if (p_header->type != ACPI_ERDT_STRUCT_RMDD_TYPE) {
                        LOG_DEBUG("Skipping top-level ERDT sub-structure "
                                  "type %u\n",
                                  (unsigned)p_header->type);
                        continue;
                }

                p_rmdd = p_structure;
                if (p_header->length < sizeof(*p_rmdd) ||
                    !(p_rmdd->flags & RMDD_DOMAIN_MASK) ||
                    (p_rmdd->flags & ~RMDD_DOMAIN_MASK)) {
                        LOG_ERROR("Invalid RMDD structure\n");
                        return PQOS_RETVAL_ERROR;
                }

                p_prior = p_start;
                prior_remaining = (const uint8_t *)p_structure - p_start;
                while (prior_remaining > 0) {
                        const struct erdt_structure_header *p_prior_header =
                            (const struct erdt_structure_header *)(const void *)
                                p_prior;

                        if (p_prior_header->type ==
                                ACPI_ERDT_STRUCT_RMDD_TYPE &&
                            ((const struct acpi_table_erdt_rmdd *)(const void *)
                                 p_prior)
                                    ->domain_id == p_rmdd->domain_id) {
                                LOG_ERROR("Duplicate RMDD Domain ID %u\n",
                                          (unsigned)p_rmdd->domain_id);
                                return PQOS_RETVAL_ERROR;
                        }
                        p_prior += p_prior_header->length;
                        prior_remaining -= p_prior_header->length;
                }

                if ((p_rmdd->flags & RMDD_L3_DOMAIN) &&
                    *p_num_cpu_agents == UINT_MAX) {
                        LOG_ERROR("Too many CPU RMDD structures\n");
                        return PQOS_RETVAL_ERROR;
                }
                if ((p_rmdd->flags & RMDD_IO_L3_DOMAIN) &&
                    *p_num_dev_agents == UINT_MAX) {
                        LOG_ERROR("Too many device RMDD structures\n");
                        return PQOS_RETVAL_ERROR;
                }

                if (p_rmdd->flags & RMDD_L3_DOMAIN)
                        (*p_num_cpu_agents)++;
                if (p_rmdd->flags & RMDD_IO_L3_DOMAIN)
                        (*p_num_dev_agents)++;
                num_rmdds++;
        }

        if (num_rmdds == 0) {
                LOG_ERROR("ERDT table contains no RMDD structures\n");
                return PQOS_RETVAL_ERROR;
        }

        return PQOS_RETVAL_OK;
}

/**
 * @brief Parses CPU and device agent RMDD structures from an ERDT table
 *
 * @param erdt_info ERDT information populated by the function
 * @param p_acpi_erdt ACPI ERDT table to parse
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK success
 */
static int
erdt_populate_rmdds(struct pqos_erdt_info **erdt_info,
                    const struct acpi_table_erdt *p_acpi_erdt)
{
        const uint8_t *p_cursor;
        size_t remaining;
        const struct erdt_structure_header *p_header;
        const struct acpi_table_erdt_rmdd *p_acpi_rmdd;
        const void *p_structure;
        unsigned num_cpu_agents;
        unsigned num_dev_agents;
        int ret;

        if (p_acpi_erdt->header.header.revision != ACPI_ERDT_REVISION) {
                LOG_ERROR("Unsupported ERDT revision %u\n",
                          (unsigned)p_acpi_erdt->header.header.revision);
                return PQOS_RETVAL_ERROR;
        }
        if (p_acpi_erdt->header.header.length <
            sizeof(struct acpi_table_erdt_header)) {
                LOG_ERROR("Invalid ACPI ERDT header length: %u\n",
                          p_acpi_erdt->header.header.length);
                return PQOS_RETVAL_ERROR;
        }

        ret = erdt_count_rmdds(p_acpi_erdt, &num_cpu_agents, &num_dev_agents);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        p_erdt_info = calloc(1, sizeof(*p_erdt_info));
        if (p_erdt_info == NULL) {
                LOG_ERROR("Can't allocate memory for ERDT information\n");
                return PQOS_RETVAL_ERROR;
        }
        p_erdt_info->max_clos = p_acpi_erdt->header.max_clos;

        if (num_cpu_agents > 0) {
                p_erdt_info->cpu_agents =
                    calloc(num_cpu_agents, sizeof(*p_erdt_info->cpu_agents));
                if (p_erdt_info->cpu_agents == NULL) {
                        LOG_ERROR("Can't allocate memory for CPU agents\n");
                        erdt_fini();
                        return PQOS_RETVAL_ERROR;
                }
        }
        if (num_dev_agents > 0) {
                p_erdt_info->dev_agents =
                    calloc(num_dev_agents, sizeof(*p_erdt_info->dev_agents));
                if (p_erdt_info->dev_agents == NULL) {
                        LOG_ERROR("Can't allocate memory for device agents\n");
                        erdt_fini();
                        return PQOS_RETVAL_ERROR;
                }
        }

        p_cursor = (const uint8_t *)p_acpi_erdt->erdt_sub_structures;
        remaining = p_acpi_erdt->header.header.length -
                    sizeof(struct acpi_table_erdt_header);
        while (remaining > 0) {
                ret = erdt_get_structure(&p_cursor, &remaining, &p_structure);
                if (ret != PQOS_RETVAL_OK)
                        goto error;

                p_header = p_structure;
                if (p_header->type != ACPI_ERDT_STRUCT_RMDD_TYPE)
                        continue;

                p_acpi_rmdd = p_structure;
                if (p_acpi_rmdd->flags & RMDD_L3_DOMAIN) {
                        ret = erdt_populate_rmdd_cpu_agent(
                            &p_erdt_info
                                 ->cpu_agents[p_erdt_info->num_cpu_agents++],
                            p_acpi_rmdd);
                        if (ret != PQOS_RETVAL_OK)
                                goto error;
                }
                if (p_acpi_rmdd->flags & RMDD_IO_L3_DOMAIN) {
                        ret = erdt_populate_rmdd_device_agent(
                            &p_erdt_info
                                 ->dev_agents[p_erdt_info->num_dev_agents++],
                            p_acpi_rmdd);
                        if (ret != PQOS_RETVAL_OK)
                                goto error;
                }
        }

        LOG_DEBUG("Parsed %u CPU and %u device ERDT agents\n",
                  p_erdt_info->num_cpu_agents, p_erdt_info->num_dev_agents);
        *erdt_info = p_erdt_info;
        return PQOS_RETVAL_OK;

error:
        erdt_fini();
        return ret;
}

/**
 * @brief Checks if channel_id already exists in channels_domains structure
 *
 * @param channels_domains Pointer of structure to be checked
 * @param channel_id Channel ID to be checked
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK success
 */
static int
check_channel_id_exist(struct pqos_channels_domains *channels_domains,
                       pqos_channel_t channel_id)
{
        for (unsigned idx = 0; idx < channels_domains->num_channel_ids; idx++)
                if (channels_domains->channel_ids[idx] == channel_id)
                        return PQOS_RETVAL_ERROR;

        return PQOS_RETVAL_OK;
}

/**
 * @brief The BDF info is in DACD ERDT Sub-structure.
 *        The channel ids are in pqos_devinfo structure.
 *        The function maps BDF to channel ids using pqos_devinfo structure.
 *        And populates channel_ids count, channel_ids,
 *        corresponding domain_ids and indexes in channels_domains structure.
 *
 * @param dacd Pointer of ERDT Sub-structure DACD
 * @param devinfo Pointer of IORDT Device information
 * @param channels_domains Pointer of structure to be populated
 * @param dev_agent_idx Index of device agent in erdt_info->dev_agents[]
 *        The dev_agent_idx is stored in channels_domains->domain_id_idxs[]
 * @param max_channels Maximum number of channel mappings
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK success
 */
static int
erdt_dev_populate_chans(const struct pqos_erdt_dacd *dacd,
                        const struct pqos_devinfo *devinfo,
                        struct pqos_channels_domains *channels_domains,
                        unsigned dev_agent_idx,
                        unsigned max_channels)
{
        uint16_t bdf = 0;
        uint32_t i = 0;
        int j = 0;
        int idx = 0;
        unsigned ch_idx = 0;
        unsigned num_channels = 0;
        pqos_channel_t *channels = NULL;
        int ret;

        for (i = 0; i < dacd->num_dases; i++) {
                j = 0;
                while (j < dacd->dase[i].path_length) {
                        bdf = 0;
                        num_channels = 0;
                        /* PCI BDF */
                        bdf |= dacd->dase[i].start_bus_number << 8;
                        bdf |= (dacd->dase[i].path[j] & 0x1F) << 3;
                        bdf |= dacd->dase[i].path[j + 1] & 0x7;
                        j += PATH_PAIR_LENGTH;

                        channels = pqos_devinfo_get_channel_ids(
                            devinfo, dacd->dase[i].segment_number, bdf,
                            &num_channels);

                        if (channels == NULL) {
                                LOG_DEBUG(
                                    "Failed to get channels for "
                                    "Segment: 0x%x BDF: 0x%x\n",
                                    (unsigned)dacd->dase[i].segment_number,
                                    (unsigned)bdf);
                                continue;
                        }

                        idx = channels_domains->num_channel_ids;
                        for (ch_idx = 0; ch_idx < num_channels; ch_idx++) {
                                ret = check_channel_id_exist(channels_domains,
                                                             channels[ch_idx]);
                                if (ret == PQOS_RETVAL_ERROR)
                                        continue;

                                if (channels_domains->num_channel_ids >=
                                    max_channels) {
                                        LOG_ERROR("Too many channel-domain "
                                                  "mappings\n");
                                        free(channels);
                                        return PQOS_RETVAL_ERROR;
                                }

                                channels_domains->channel_ids[idx] =
                                    channels[ch_idx];
                                channels_domains->domain_ids[idx] =
                                    dacd->rmdd_domain_id;
                                channels_domains->domain_id_idxs[idx] =
                                    dev_agent_idx;
                                idx++;
                                channels_domains->num_channel_ids++;
                        }

                        free(channels);
                }
        }

        return PQOS_RETVAL_OK;
}

int
channels_domains_init(unsigned int num_channels,
                      const struct pqos_erdt_info *erdt,
                      const struct pqos_devinfo *devinfo,
                      struct pqos_channels_domains **channels_domains)
{
        unsigned i;
        int ret;

        if (num_channels == 0 || erdt == NULL || devinfo == NULL ||
            channels_domains == NULL || erdt->num_dev_agents > UINT16_MAX)
                return PQOS_RETVAL_PARAM;
        *channels_domains = NULL;

        p_channels_domains = calloc(1, sizeof(*p_channels_domains));
        if (p_channels_domains == NULL) {
                LOG_ERROR("Can't allocate memory for channel domains\n");
                return PQOS_RETVAL_ERROR;
        }

        p_channels_domains->channel_ids =
            calloc(num_channels, sizeof(*p_channels_domains->channel_ids));
        p_channels_domains->domain_ids =
            calloc(num_channels, sizeof(*p_channels_domains->domain_ids));
        p_channels_domains->domain_id_idxs =
            calloc(num_channels, sizeof(*p_channels_domains->domain_id_idxs));
        if (p_channels_domains->channel_ids == NULL ||
            p_channels_domains->domain_ids == NULL ||
            p_channels_domains->domain_id_idxs == NULL) {
                LOG_ERROR("Can't allocate channel-domain mappings\n");
                channels_domains_fini();
                return PQOS_RETVAL_ERROR;
        }

        for (i = 0; i < erdt->num_dev_agents; i++) {
                ret = erdt_dev_populate_chans(&erdt->dev_agents[i].dacd,
                                              devinfo, p_channels_domains, i,
                                              num_channels);
                if (ret != PQOS_RETVAL_OK) {
                        channels_domains_fini();
                        return ret;
                }
        }

        *channels_domains = p_channels_domains;
        return PQOS_RETVAL_OK;
}

void
channels_domains_fini(void)
{
        if (p_channels_domains == NULL)
                return;

        free(p_channels_domains->channel_ids);
        free(p_channels_domains->domain_ids);
        free(p_channels_domains->domain_id_idxs);
        free(p_channels_domains);
        p_channels_domains = NULL;
}

int
erdt_init(const struct pqos_cap *cap,
          struct pqos_cpuinfo *cpu,
          struct pqos_erdt_info **erdt_info)
{
        struct acpi_table *table;
        int ret;

        if (cap == NULL || cpu == NULL || erdt_info == NULL)
                return PQOS_RETVAL_PARAM;
        *erdt_info = NULL;

        ret = acpi_init();
        if (ret != PQOS_RETVAL_OK) {
                LOG_WARN("Could not initialize ACPI!\n");
                return ret;
        }

        /* no table is a platform without the feature, which pqos_init() says
         * at debug level: a warning here began the output of every command on
         * such a platform. Only the kernel's listing of the tables can say
         * there is none: without it the tables are looked for in memory, and
         * that scan says itself when it fails. A table the listing may hold
         * is said when it cannot be read, as by a user who is not root (the
         * kernel makes the files 0400) or one who may not look it up, and
         * when it cannot be used, below
         */
        table = acpi_get_sig(ACPI_TABLE_SIG_ERDT);
        if (table == NULL) {
                if (!pqos_dir_exists(ACPI_TABLE_FS_PATH))
                        LOG_DEBUG("%s table not found in ACPI memory\n",
                                  ACPI_TABLE_SIG_ERDT);
                else if (access(ACPI_TABLE_FS_PATH "/" ACPI_TABLE_SIG_ERDT,
                                F_OK) != 0 &&
                         (errno == ENOENT || errno == ENOTDIR))
                        LOG_DEBUG("No %s table\n", ACPI_TABLE_SIG_ERDT);
                else
                        LOG_WARN("Could not obtain %s table\n",
                                 ACPI_TABLE_SIG_ERDT);
                (void)acpi_fini();
                return PQOS_RETVAL_RESOURCE;
        }

        acpi_print(table);
        ret = erdt_populate_rmdds(erdt_info, table->erdt);
        acpi_free(table);
        if (ret != PQOS_RETVAL_OK)
                (void)acpi_fini();

        return ret;
}

void
erdt_fini(void)
{
        uint32_t idx;

        if (p_erdt_info == NULL)
                return;

        if (p_erdt_info->cpu_agents != NULL) {
                for (idx = 0; idx < p_erdt_info->num_cpu_agents; idx++) {
                        free(p_erdt_info->cpu_agents[idx].cacd.enumeration_ids);
                        free(p_erdt_info->cpu_agents[idx]
                                 .mmrc.correction_factor);
                }
                free(p_erdt_info->cpu_agents);
        }

        if (p_erdt_info->dev_agents != NULL) {
                for (idx = 0; idx < p_erdt_info->num_dev_agents; idx++) {
                        free(p_erdt_info->dev_agents[idx]
                                 .ibrd.correction_factor);
                        erdt_free_dacd(&p_erdt_info->dev_agents[idx].dacd);
                }
                free(p_erdt_info->dev_agents);
        }
        free(p_erdt_info);
        p_erdt_info = NULL;
}
