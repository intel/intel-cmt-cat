/*
 * BSD LICENSE
 *
 * Copyright(c) 2025 Intel Corporation. All rights reserved.
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

#include <inttypes.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#define RMDD_L3_DOMAIN    1
#define RMDD_IO_L3_DOMAIN 2

#define PATH_PAIR_LENGTH 2

/**
 * ERDT ACPI table information.
 * This pointer is allocated and initialized in this module.
 */
static struct pqos_erdt_info *p_erdt_info = NULL;

static struct pqos_channels_domains *p_channels_domains = NULL;

/**
 * @brief Copies correction factor list from ACPI table to internal structure
 *
 * @param p_pqos_correction_factor Pointer to store the correction factor list
 * @param p_acpi_correction_factor Pointer to the correction factor list in ACPI
 * @param length Length of the correction factor list
 * @param max_rmids Maximum number of RMIDs supported
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK success
 */
static int
copy_correction_factor(void *p_pqos_correction_factor,
                       const void *p_acpi_correction_factor,
                       uint32_t length,
                       uint32_t max_rmids)
{
        /* Check correction factor list length */
        if (length != NO_CORRECTION_FACTOR &&
            length != SINGLE_CORRECTION_FACTOR && length != (max_rmids + 1))
                return PQOS_RETVAL_ERROR;

        /* Copy correction factor list length */
        if (length == NO_CORRECTION_FACTOR) {
                p_pqos_correction_factor = NULL;
        } else {
                p_pqos_correction_factor = malloc(length);
                if (p_pqos_correction_factor == NULL) {
                        printf("Can't allocate memory for "
                               "correction factors\n");
                        return PQOS_RETVAL_ERROR;
                }
                memcpy(p_pqos_correction_factor, p_acpi_correction_factor,
                       length);
        }

        return PQOS_RETVAL_OK;
}

/**
 * @brief Parses ERDT table to extract ERDT Sub-structure CACD info
 *
 * @param p_cacd struct to be updated with CACD info
 * @param p_acpi_cacd table to be parsed for CACD info
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK success
 */
static int
erdt_populate_cacd(struct pqos_erdt_cacd *p_cacd,
                   struct acpi_table_erdt_cacd *p_acpi_cacd)
{
        uint32_t enum_ids_length = 0;

        if (p_acpi_cacd->type != ACPI_ERDT_STRUCT_CACD_TYPE) {
                printf("Incorrect CACD structure type 0x%x\n",
                       p_acpi_cacd->type);
                return PQOS_RETVAL_ERROR;
        }

        p_cacd->rmdd_domain_id = p_acpi_cacd->rmdd_domain_id;
        enum_ids_length =
            p_acpi_cacd->length - sizeof(struct acpi_table_erdt_cacd);

        if (enum_ids_length != 0) {
                p_cacd->enumeration_ids = (uint32_t *)malloc(enum_ids_length);
                if (p_cacd->enumeration_ids == NULL) {
                        printf("Can't allocate memory for enumeration_ids\n");
                        return PQOS_RETVAL_ERROR;
                }
                memcpy(p_cacd->enumeration_ids, p_acpi_cacd->enumeration_ids,
                       enum_ids_length);
        }

        p_cacd->enum_ids_length = enum_ids_length / sizeof(uint32_t);

        return PQOS_RETVAL_OK;
}

/**
 * @brief Parses ERDT table to extract ERDT Sub-structure CMRC info
 *
 * @param p_cmrc struct to be updated with CMRC info
 * @param p_acpi_cmrc table to be parsed for CMRC info
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK success
 */
static int
erdt_populate_cmrc(struct pqos_erdt_cmrc *p_cmrc,
                   struct acpi_table_erdt_cmrc *p_acpi_cmrc)
{
        if (p_acpi_cmrc->type != ACPI_ERDT_STRUCT_CMRC_TYPE) {
                printf("Incorrect CMRC structure type 0x%x\n",
                       p_acpi_cmrc->type);
                return PQOS_RETVAL_ERROR;
        }

        p_cmrc->flags = p_acpi_cmrc->flags;
        p_cmrc->reg_index_func_ver =
            p_acpi_cmrc->register_indexing_function_version;
        p_cmrc->block_base_addr =
            p_acpi_cmrc->cmt_register_block_base_address_for_cpu;
        p_cmrc->block_size = p_acpi_cmrc->cmt_register_block_size_for_cpu;
        p_cmrc->clump_size = p_acpi_cmrc->cmt_register_clump_size_for_cpu;
        p_cmrc->clump_stride = p_acpi_cmrc->cmt_register_clump_stride_for_cpu;
        p_cmrc->upscaling_factor = p_acpi_cmrc->cmt_counter_upscaling_factor;

        return PQOS_RETVAL_OK;
}

/**
 * @brief Parses ERDT table to extract ERDT Sub-structure MMRC info
 *
 * @param p_mmrc struct to be updated with MMRC info
 * @param p_acpi_mmrc table to be parsed for MMRC info
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK success
 */
static int
erdt_populate_mmrc(struct pqos_erdt_mmrc *p_mmrc,
                   struct acpi_table_erdt_mmrc *p_acpi_mmrc,
                   uint32_t max_rmids)
{
        int ret = PQOS_RETVAL_OK;

        if (p_acpi_mmrc->type != ACPI_ERDT_STRUCT_MMRC_TYPE) {
                printf("Incorrect MMRC structure type 0x%x\n",
                       p_acpi_mmrc->type);
                return PQOS_RETVAL_ERROR;
        }

        p_mmrc->flags = p_acpi_mmrc->flags;
        p_mmrc->reg_index_func_ver =
            p_acpi_mmrc->register_indexing_function_version;
        p_mmrc->reg_block_base_addr =
            p_acpi_mmrc->mbm_register_block_base_address;
        p_mmrc->reg_block_size = p_acpi_mmrc->mbm_register_blockSize;
        p_mmrc->counter_width = p_acpi_mmrc->mbm_counter_width;
        p_mmrc->upscaling_factor = p_acpi_mmrc->mbm_counter_upscaling_factor;
        p_mmrc->correction_factor_length =
            p_acpi_mmrc->mbm_correction_factor_list_length;

        ret = copy_correction_factor(
            (void *)p_mmrc->correction_factor,
            (const void *)p_acpi_mmrc->mbm_correction_factor,
            p_mmrc->correction_factor_length, max_rmids);
        if (ret == PQOS_RETVAL_ERROR) {
                printf("Wrong MBM correction factor list length in "
                       "MMRC structure\n");
                return ret;
        }

        return ret;
}

/**
 * @brief Parses ERDT table to extract ERDT Sub-structure MARC info
 *
 * @param p_marc struct to be updated with MARC info
 * @param p_acpi_marc table to be parsed for MARC info
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK success
 */
static int
erdt_populate_marc(struct pqos_erdt_marc *p_marc,
                   struct acpi_table_erdt_marc *p_acpi_marc)
{
        if (p_acpi_marc->type != ACPI_ERDT_STRUCT_MARC_TYPE) {
                printf("Incorrect MARC structure type 0x%x\n",
                       p_acpi_marc->type);
                return PQOS_RETVAL_ERROR;
        }

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

        return PQOS_RETVAL_OK;
}

/**
 * @brief Calculates number of DASE structures in DACD structure
 *
 * @param length Length of the DACD structure excluding the header
 * @param p_acpi_dase Pointer to the first DASE structure in the DACD from ACPI
 * @param num_dases Pointer to store the number of DASEs found
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK success
 */
static int
erdt_calculate_num_dases(uint32_t length,
                         struct acpi_table_erdt_dase *p_acpi_dase,
                         uint32_t *num_dases)
{
        uint32_t dase_length = 0;
        uint32_t count = 0;
        struct acpi_table_erdt_dase *p_dase = p_acpi_dase;

        if (p_acpi_dase == NULL) {
                printf("Invalid DASE pointer\n");
                return PQOS_RETVAL_ERROR;
        }

        if (length < ACPI_ERDT_STRUCT_DASE_HEADER_LENGTH) {
                printf("Invalid DASE length %u\n", length);
                return PQOS_RETVAL_ERROR;
        }

        dase_length = p_acpi_dase->length;
        if (dase_length < ACPI_ERDT_STRUCT_DASE_HEADER_LENGTH) {
                printf("Invalid DASE length %u\n", dase_length);
                return PQOS_RETVAL_ERROR;
        }

        if (dase_length > length) {
                printf("Invalid DASE length %u, expected less than %u\n",
                       dase_length, length);
                return PQOS_RETVAL_ERROR;
        }

        /* Calculate number of DASEs in the DACD structure */
        while (length > 0) {
                dase_length = p_dase->length;
                if (dase_length > length) {
                        printf("Invalid DASE length %u, "
                               "exceeds remaining length %u\n",
                               dase_length, length);
                        return PQOS_RETVAL_ERROR;
                }
                length = length - dase_length;
                p_dase =
                    (struct acpi_table_erdt_dase *)((unsigned char *)p_dase +
                                                    dase_length);
                count++;
        }

        *num_dases = count;

        return PQOS_RETVAL_OK;
}

/**
 * @brief Parses ERDT table to extract ERDT Sub-structure DACD info
 *
 * @param p_dacd struct to be updated with DACD info
 * @param p_acpi_dacd table to be parsed for DACD info
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK success
 */
static int
erdt_populate_dacd(struct pqos_erdt_dacd *p_dacd,
                   struct acpi_table_erdt_dacd *p_acpi_dacd)
{
        uint32_t num_dases = 0;
        uint32_t idx = 0;
        uint32_t length = 0;
        int ret = PQOS_RETVAL_OK;
        struct acpi_table_erdt_dase *p_acpi_dase = NULL;

        if (p_acpi_dacd->type != ACPI_ERDT_STRUCT_DACD_TYPE) {
                printf("Incorrect DACD structure type 0x%x\n",
                       p_acpi_dacd->type);
                return PQOS_RETVAL_ERROR;
        }

        if (p_acpi_dacd->length < ACPI_ERDT_STRUCT_DACD_HEADER_LENGTH) {
                printf("Invalid DACD length %u\n", p_acpi_dacd->length);
                return PQOS_RETVAL_ERROR;
        }

        p_dacd->rmdd_domain_id = p_acpi_dacd->rmdd_domain_id;

        length = p_acpi_dacd->length - ACPI_ERDT_STRUCT_DACD_HEADER_LENGTH;
        if (length == 0) {
                p_dacd->num_dases = length;
                p_dacd->dase = NULL;
                return PQOS_RETVAL_OK;
        }
        ret =
            erdt_calculate_num_dases(length, &p_acpi_dacd->dase[0], &num_dases);
        if (ret == PQOS_RETVAL_ERROR) {
                printf("Error calculating number of DASEs\n");
                return PQOS_RETVAL_ERROR;
        }

        p_dacd->dase = (struct pqos_erdt_dase *)calloc(
            1, sizeof(struct pqos_erdt_dase) * num_dases);
        if (p_dacd->dase == NULL) {
                printf("Can't allocate memory for DASEs\n");
                return PQOS_RETVAL_ERROR;
        }
        p_dacd->num_dases = num_dases;

        p_acpi_dase = &p_acpi_dacd->dase[0];
        for (idx = 0; idx < num_dases; idx++) {
                p_dacd->dase[idx].type = p_acpi_dase->type;
                p_dacd->dase[idx].segment_number = p_acpi_dase->segment_number;
                p_dacd->dase[idx].start_bus_number =
                    p_acpi_dase->start_bus_number;
                p_dacd->dase[idx].path_length =
                    p_acpi_dase->length - ACPI_ERDT_STRUCT_DASE_HEADER_LENGTH;

                if (p_dacd->dase[idx].path_length == 0) {
                        p_dacd->dase[idx].path = NULL;
                        p_acpi_dase = (struct acpi_table_erdt_dase
                                           *)((uint8_t *)p_acpi_dase +
                                              p_acpi_dase->length);
                        continue;
                }

                p_dacd->dase[idx].path =
                    (uint8_t *)calloc(1, p_dacd->dase[idx].path_length);
                if (p_dacd->dase[idx].path == NULL) {
                        printf("Can't allocate memory for DASE path\n");
                        for (uint32_t cleanup_idx = 0; cleanup_idx < idx;
                             cleanup_idx++)
                                free(p_dacd->dase[cleanup_idx].path);
                        free(p_dacd->dase);
                        return PQOS_RETVAL_ERROR;
                }

                memcpy(p_dacd->dase[idx].path, p_acpi_dase->path,
                       p_dacd->dase[idx].path_length);

                p_acpi_dase =
                    (struct acpi_table_erdt_dase *)((uint8_t *)p_acpi_dase +
                                                    p_acpi_dase->length);
        }

        return PQOS_RETVAL_OK;
}

/**
 * @brief Parses ERDT table to extract ERDT Sub-structure CMRD info
 *
 * @param p_cmrd struct to be updated with CMRD info
 * @param p_acpi_cmrd table to be parsed for CMRD info
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK success
 */
static int
erdt_populate_cmrd(struct pqos_erdt_cmrd *p_cmrd,
                   struct acpi_table_erdt_cmrd *p_acpi_cmrd)
{
        if (p_acpi_cmrd->type != ACPI_ERDT_STRUCT_CMRD_TYPE) {
                printf("Incorrect CMRD structure type 0x%x\n",
                       p_acpi_cmrd->type);
                return PQOS_RETVAL_ERROR;
        }

        p_cmrd->flags = p_acpi_cmrd->flags;
        p_cmrd->reg_index_func_ver =
            p_acpi_cmrd->register_indexing_function_version;
        p_cmrd->reg_base_addr = p_acpi_cmrd->register_base_address;
        p_cmrd->reg_block_size = p_acpi_cmrd->register_block_size;
        p_cmrd->offset = p_acpi_cmrd->cmt_register_offset_for_io;
        p_cmrd->clump_size = p_acpi_cmrd->cmt_register_clump_size_ror_io;
        p_cmrd->upscaling_factor = p_acpi_cmrd->cmt_counter_upscaling_factor;

        return PQOS_RETVAL_OK;
}

/**
 * @brief Parses ERDT table to extract ERDT Sub-structure IBRD info
 *
 * @param p_ibrd struct to be updated with IBRD info
 * @param p_acpi_ibrd table to be parsed for IBRD info
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK success
 */
static int
erdt_populate_ibrd(struct pqos_erdt_ibrd *p_ibrd,
                   struct acpi_table_erdt_ibrd *p_acpi_ibrd,
                   int max_rmids)
{
        int ret = PQOS_RETVAL_OK;

        if (p_acpi_ibrd->type != ACPI_ERDT_STRUCT_IBRD_TYPE) {
                printf("Incorrect IBRD structure type 0x%x\n",
                       p_acpi_ibrd->type);
                return PQOS_RETVAL_ERROR;
        }

        p_ibrd->flags = p_acpi_ibrd->flags;
        p_ibrd->reg_index_func_ver =
            p_acpi_ibrd->register_indexing_function_version;
        p_ibrd->reg_base_addr = p_acpi_ibrd->register_base_address;
        p_ibrd->reg_block_size = p_acpi_ibrd->register_blockSize;
        p_ibrd->bw_reg_offset = p_acpi_ibrd->total_io_bw_registerOffset;
        p_ibrd->miss_bw_reg_offset = p_acpi_ibrd->io_miss_bw_registerOffset;
        p_ibrd->bw_reg_clump_size =
            p_acpi_ibrd->total_io_bwr_register_clumpSize;
        p_ibrd->miss_reg_clump_size = p_acpi_ibrd->io_miss_register_clumpSize;
        p_ibrd->counter_width = p_acpi_ibrd->io_bw_counter_width;
        p_ibrd->upscaling_factor = p_acpi_ibrd->io_bw_counter_upscaling_factor;
        p_ibrd->correction_factor_length =
            p_acpi_ibrd->io_bw_counter_correction_factor_list_length;

        ret = copy_correction_factor(
            (void *)p_ibrd->correction_factor,
            (const void *)p_acpi_ibrd->io_bw_counter_correction_factor,
            p_ibrd->correction_factor_length, max_rmids);
        if (ret == PQOS_RETVAL_ERROR) {
                printf("Wrong I/O BW counter correction factor list length in "
                       "IBRD structure\n");
                return ret;
        }

        return ret;
}

/**
 * @brief Parses ERDT table to extract ERDT Sub-structure CARD info
 *
 * @param p_card struct to be updated with CARD info
 * @param p_acpi_card table to be parsed for CARD info
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK success
 */
static int
erdt_populate_card(struct pqos_erdt_card *p_card,
                   struct acpi_table_erdt_card *p_acpi_card)
{
        if (p_acpi_card->type != ACPI_ERDT_STRUCT_CARD_TYPE) {
                printf("Incorrect CARD structure type 0x%x\n",
                       p_acpi_card->type);
                return PQOS_RETVAL_ERROR;
        }

        if (p_acpi_card->flags & CARD_CONTENTION_BITMASKS_VALID_BIT)
                p_card->contention_bitmask_valid = 1;
        else
                p_card->contention_bitmask_valid = 0;

        if (p_acpi_card->flags & CARD_NON_CONTIGUOUS_BITMASKS_SUPPORTED_BIT)
                p_card->non_contiguous_cbm = 1;
        else
                p_card->non_contiguous_cbm = 0;

        if (p_acpi_card->flags & CARD_ZERO_LENGTH_BITMASKS_BIT)
                p_card->zero_length_bitmask = 1;
        else
                p_card->zero_length_bitmask = 0;

        p_card->contention_bitmask = p_acpi_card->contention_bitmask;
        p_card->reg_index_func_ver =
            p_acpi_card->register_indexing_function_version;
        p_card->reg_base_addr = p_acpi_card->register_base_address;
        p_card->reg_block_size = p_acpi_card->register_block_size;
        p_card->cat_reg_offset =
            p_acpi_card->cache_allocation_rgister_ofsets_for_io;
        p_card->cat_reg_block_size =
            p_acpi_card->cache_allocation_register_block_size;

        return PQOS_RETVAL_OK;
}

/**
 * @brief Parses ERDT table to extract ERDT Sub-structure RMDD info
 *
 * @param p_rmdd struct to be updated with RMDD info
 * @param p_acpi_rmdds table to be parsed for RMDD info
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK success
 */
static int
erdt_populate_rmdd(struct pqos_erdt_rmdd *p_rmdd,
                   struct acpi_table_erdt_rmdd *p_acpi_rmdds)
{
        if (p_acpi_rmdds->type != ACPI_ERDT_STRUCT_RMDD_TYPE) {
                printf("Incorrect RMDD structure type 0x%x\n",
                       p_acpi_rmdds->type);
                return PQOS_RETVAL_ERROR;
        }

        p_rmdd->flags = p_acpi_rmdds->flags;
        p_rmdd->num_io_l3_slices = p_acpi_rmdds->number_of_io_l3Slices;
        p_rmdd->num_io_l3_sets = p_acpi_rmdds->number_of_io_l3_sets;
        p_rmdd->num_io_l3_ways = p_acpi_rmdds->number_of_io_l3_ways;
        p_rmdd->domain_id = p_acpi_rmdds->domainId;
        p_rmdd->max_rmids = p_acpi_rmdds->max_rmids;
        p_rmdd->control_reg_base_addr =
            p_acpi_rmdds->control_register_base_address;
        p_rmdd->control_reg_size = p_acpi_rmdds->control_register_size;

        return 0;
}

/**
 * @brief Parses ERDT table to extract ERDT Sub-structures info for CPU agent
 *
 * @param p_cpu_agent_info struct to be updated with ERDT Sub-structures info
 *                         for CPU agent
 * @param p_acpi_rmdds table to be parsed for ERDT Sub-structures info
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK success
 */
static int
erdt_populate_rmdd_cpu_agents(struct pqos_cpu_agent_info *p_cpu_agent_info,
                              struct acpi_table_erdt_rmdd *p_acpi_rmdds)
{
        int ret = PQOS_RETVAL_OK;
        uint32_t length = 0;

        /* Copy CPU agent's CMRD structure */
        ret = erdt_populate_rmdd(&p_cpu_agent_info->rmdd, p_acpi_rmdds);
        length = sizeof(struct acpi_table_erdt_rmdd);
        p_acpi_rmdds =
            (struct acpi_table_erdt_rmdd *)((unsigned char *)p_acpi_rmdds +
                                            length);

        /* Copy CPU agent's CACD structure */
        ret |= erdt_populate_cacd(&p_cpu_agent_info->cacd,
                                  (struct acpi_table_erdt_cacd *)p_acpi_rmdds);
        length = ((struct acpi_table_erdt_cacd *)p_acpi_rmdds)->length;
        p_acpi_rmdds =
            (struct acpi_table_erdt_rmdd *)((unsigned char *)p_acpi_rmdds +
                                            length);

        /* Copy CPU agent's CMRC structure */
        ret |= erdt_populate_cmrc(&p_cpu_agent_info->cmrc,
                                  (struct acpi_table_erdt_cmrc *)p_acpi_rmdds);
        length = ((struct acpi_table_erdt_cmrc *)p_acpi_rmdds)->length;
        p_acpi_rmdds =
            (struct acpi_table_erdt_rmdd *)((unsigned char *)p_acpi_rmdds +
                                            length);

        /* Copy CPU agent's MMRC structure */
        ret |= erdt_populate_mmrc(&p_cpu_agent_info->mmrc,
                                  (struct acpi_table_erdt_mmrc *)p_acpi_rmdds,
                                  p_acpi_rmdds->max_rmids);
        length = ((struct acpi_table_erdt_mmrc *)p_acpi_rmdds)->length;
        p_acpi_rmdds =
            (struct acpi_table_erdt_rmdd *)((unsigned char *)p_acpi_rmdds +
                                            length);

        /* Copy CPU agent's MARC structure */
        ret |= erdt_populate_marc(&p_cpu_agent_info->marc,
                                  (struct acpi_table_erdt_marc *)p_acpi_rmdds);

        return ret;
}

/**
 * @brief Parses ERDT table to extract ERDT Sub-structures info for Device
 *        agent
 *
 * @param p_dev_agent_info struct to be updated with ERDT Sub-structures info
 *                         for Device agent
 * @param p_acpi_rmdds table to be parsed for ERDT Sub-structures info
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK success
 */
static int
erdt_populate_rmdd_device_agents(
    struct pqos_device_agent_info *p_dev_agent_info,
    struct acpi_table_erdt_rmdd *p_acpi_rmdds)
{
        int ret = PQOS_RETVAL_OK;
        uint32_t length = 0;

        /* Copy device agent's RMDD structure */
        ret = erdt_populate_rmdd(&p_dev_agent_info->rmdd, p_acpi_rmdds);
        length = sizeof(struct acpi_table_erdt_rmdd);
        p_acpi_rmdds =
            (struct acpi_table_erdt_rmdd *)((unsigned char *)p_acpi_rmdds +
                                            length);

        /* Copy device agent's DACD structure */
        ret |= erdt_populate_dacd(&p_dev_agent_info->dacd,
                                  (struct acpi_table_erdt_dacd *)p_acpi_rmdds);
        length = ((struct acpi_table_erdt_dacd *)p_acpi_rmdds)->length;
        p_acpi_rmdds =
            (struct acpi_table_erdt_rmdd *)((unsigned char *)p_acpi_rmdds +
                                            length);

        /* Copy device agent's CMRD structure */
        ret |= erdt_populate_cmrd(&p_dev_agent_info->cmrd,
                                  (struct acpi_table_erdt_cmrd *)p_acpi_rmdds);
        length = ((struct acpi_table_erdt_cmrd *)p_acpi_rmdds)->length;
        p_acpi_rmdds =
            (struct acpi_table_erdt_rmdd *)((unsigned char *)p_acpi_rmdds +
                                            length);

        /* Copy device agent's IBRD structure */
        ret |= erdt_populate_ibrd(&p_dev_agent_info->ibrd,
                                  (struct acpi_table_erdt_ibrd *)p_acpi_rmdds,
                                  p_acpi_rmdds->max_rmids);
        length = ((struct acpi_table_erdt_ibrd *)p_acpi_rmdds)->length;
        p_acpi_rmdds =
            (struct acpi_table_erdt_rmdd *)((unsigned char *)p_acpi_rmdds +
                                            length);

        /* Copy device agent's CARD structure */
        ret |= erdt_populate_card(&p_dev_agent_info->card,
                                  (struct acpi_table_erdt_card *)p_acpi_rmdds);

        return ret;
}

/**
 * @brief Parses ERDT table to extract RMDD Sub-structures info for CPU and
 *        Device agents
 *
 * @param erdt_info Pointer to store the ERDT information
 * @param p_acpi_erdt ACPI ERDT table to be parsed
 * @param socket_num Number of sockets in the system
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK success
 */
static int
erdt_populate_rmdds(struct pqos_erdt_info **erdt_info,
                    struct acpi_table_erdt *p_acpi_erdt,
                    int socket_num)
{
        int ret = PQOS_RETVAL_OK;
        uint32_t cpu_idx = 0;
        uint32_t dev_idx = 0;
        int rmdd_idx = 0;
        int total_rmdds =
            socket_num * (CPU_AGENTS_PER_SOCKET + DEVICE_AGENTS_PER_SOCKET);
        void *p_acpi_rmdds = (void *)&p_acpi_erdt->erdt_sub_structures[0];
        struct acpi_table_erdt_rmdd *p_acpi_rmdd = NULL;
        uint32_t length = 0;
        uint32_t max_cpu_agents = 0;
        uint32_t max_dev_agents = 0;

        if (p_acpi_erdt->header.header.length <
            sizeof(struct acpi_table_erdt_header)) {
                printf("Invalid ACPI ERDT header length: %u\n",
                       p_acpi_erdt->header.header.length);
                return PQOS_RETVAL_ERROR;
        }
        length = p_acpi_erdt->header.header.length -
                 sizeof(struct acpi_table_erdt_header);

        p_erdt_info =
            (struct pqos_erdt_info *)calloc(1, sizeof(struct pqos_erdt_info));

        if (p_erdt_info == NULL) {
                printf("Can't allocate memory for pqos_erdt_info\n");
                return PQOS_RETVAL_ERROR;
        }

        p_erdt_info->max_clos = p_acpi_erdt->header.max_clos;
        max_cpu_agents = CPU_AGENTS_PER_SOCKET * socket_num;
        max_dev_agents = DEVICE_AGENTS_PER_SOCKET * socket_num;

        p_erdt_info->cpu_agents = (struct pqos_cpu_agent_info *)calloc(
            max_cpu_agents, sizeof(struct pqos_cpu_agent_info));
        if (p_erdt_info->cpu_agents == NULL) {
                printf("Can't allocate memory for CPU agents\n");
                free(p_erdt_info);
                return PQOS_RETVAL_ERROR;
        }

        p_erdt_info->dev_agents = (struct pqos_device_agent_info *)calloc(
            max_dev_agents, sizeof(struct pqos_device_agent_info));
        if (p_erdt_info->dev_agents == NULL) {
                printf("Can't allocate memory for Device agents\n");
                free(p_erdt_info->cpu_agents);
                free(p_erdt_info);
                return PQOS_RETVAL_ERROR;
        }

        cpu_idx = 0;
        dev_idx = 0;
        for (rmdd_idx = 0; rmdd_idx < total_rmdds && length > 0; rmdd_idx++) {

                p_acpi_rmdd = (struct acpi_table_erdt_rmdd *)p_acpi_rmdds;

                switch (p_acpi_rmdd->flags) {
                case RMDD_L3_DOMAIN:
                        if (cpu_idx >= max_cpu_agents) {
                                printf("ERDT table has more CPU Domain RMDD "
                                       "structures than available CPU domains "
                                       "in the machine\n");
                                erdt_fini();
                                return PQOS_RETVAL_ERROR;
                        }
                        ret = erdt_populate_rmdd_cpu_agents(
                            &p_erdt_info->cpu_agents[cpu_idx],
                            (struct acpi_table_erdt_rmdd *)p_acpi_rmdds);
                        cpu_idx++;
                        p_erdt_info->num_cpu_agents++;
                        break;
                case RMDD_IO_L3_DOMAIN:
                        if (dev_idx >= max_dev_agents) {
                                printf("ERDT table has more I/O Device Domain "
                                       "RMDD structures than available I/O "
                                       "Device domains in the machine\n");
                                erdt_fini();
                                return PQOS_RETVAL_ERROR;
                        }
                        ret = erdt_populate_rmdd_device_agents(
                            &p_erdt_info->dev_agents[dev_idx],
                            (struct acpi_table_erdt_rmdd *)p_acpi_rmdds);
                        dev_idx++;
                        p_erdt_info->num_dev_agents++;
                        break;
                default:
                        break;
                }
                if (ret != PQOS_RETVAL_OK)
                        break;

                if (length < p_acpi_rmdd->length) {
                        printf("Invalid length in ERDT table\n");
                        erdt_fini();
                        return PQOS_RETVAL_ERROR;
                }

                length -= p_acpi_rmdd->length;

                p_acpi_rmdds = (void *)((unsigned char *)p_acpi_rmdds +
                                        p_acpi_rmdd->length);
        }

        if (ret == PQOS_RETVAL_OK)
                *erdt_info = p_erdt_info;
        else
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
 *        coressponding domain_ids and indexes in channels_domains structure.
 *
 * @param dacd Pointer of ERDT Sub-structure DACD
 * @param devinfo Pointer of IORDT Device information
 * @param channels_domains Pointer of structure to be populated
 * @param dev_agent_idx Index of device agent in erdt_info->dev_agents[]
 *        The dev_agent_idx is tored in channels_domains->domain_id_idx[]
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK success
 */
static int
erdt_dev_populate_chans(const struct pqos_erdt_dacd *dacd,
                        const struct pqos_devinfo *devinfo,
                        struct pqos_channels_domains *channels_domains,
                        unsigned dev_agent_idx)
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
                                LOG_DEBUG("Failed to get channels for "
                                          "Segment: 0x%x BDF: 0x%x\n",
                                          dacd->dase[i].segment_number, bdf);
                                continue;
                        }

                        idx = channels_domains->num_channel_ids;
                        for (ch_idx = 0; ch_idx < num_channels; ch_idx++) {
                                ret = check_channel_id_exist(channels_domains,
                                                             channels[ch_idx]);
                                if (ret == PQOS_RETVAL_ERROR)
                                        continue;

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
        int ret;

        ASSERT(num_channels > 0);
        ASSERT(erdt != NULL);
        ASSERT(channels_domains != NULL);

        p_channels_domains = (struct pqos_channels_domains *)calloc(
            1, sizeof(struct pqos_channels_domains));

        if (p_channels_domains == NULL) {
                LOG_ERROR("Can't allocate memory for pqos_channels_domains\n");
                return PQOS_RETVAL_ERROR;
        }

        p_channels_domains->num_channel_ids = 0;

        p_channels_domains->channel_ids =
            (pqos_channel_t *)malloc(num_channels * sizeof(pqos_channel_t));
        if (p_channels_domains->channel_ids == NULL) {
                LOG_ERROR("Can't allocate memory for pqos_channel_t\n");
                free(p_channels_domains);
                return PQOS_RETVAL_ERROR;
        }
        memset(p_channels_domains->channel_ids, 0,
               num_channels * sizeof(pqos_channel_t));

        p_channels_domains->domain_ids =
            (uint16_t *)malloc(num_channels * sizeof(uint16_t));
        if (p_channels_domains->domain_ids == NULL) {
                LOG_ERROR("Can't allocate memory for domains in "
                          "pqos_channels_domains\n");
                free(p_channels_domains->channel_ids);
                free(p_channels_domains);
                return PQOS_RETVAL_ERROR;
        }
        memset(p_channels_domains->domain_ids, 0,
               num_channels * sizeof(uint16_t));

        p_channels_domains->domain_id_idxs =
            (uint16_t *)malloc(num_channels * sizeof(uint16_t));
        if (p_channels_domains->domain_id_idxs == NULL) {
                LOG_ERROR("Can't allocate memory for domain_id_idxs in "
                          "pqos_channels_domains\n");
                free(p_channels_domains->domain_ids);
                free(p_channels_domains->channel_ids);
                free(p_channels_domains);
                return PQOS_RETVAL_ERROR;
        }
        memset(p_channels_domains->domain_id_idxs, 0,
               num_channels * sizeof(uint16_t));

        for (unsigned i = 0; i < erdt->num_dev_agents; i++) {
                ret = erdt_dev_populate_chans(&erdt->dev_agents[i].dacd,
                                              devinfo, p_channels_domains, i);
                if (ret != PQOS_RETVAL_OK) {
                        free(p_channels_domains->domain_id_idxs);
                        free(p_channels_domains->domain_ids);
                        free(p_channels_domains->channel_ids);
                        free(p_channels_domains);
                        return ret;
                }
        }

        *channels_domains = p_channels_domains;

        return PQOS_RETVAL_OK;
}

void
channels_domains_fini(void)
{
        ASSERT(p_channels_domains != NULL);
        ASSERT(p_channels_domains->channel_ids != NULL);
        ASSERT(p_channels_domains->domain_ids != NULL);
        ASSERT(p_channels_domains->domain_id_idxs != NULL);

        free(p_channels_domains->channel_ids);
        free(p_channels_domains->domain_ids);
        free(p_channels_domains->domain_id_idxs);
        free(p_channels_domains);
}

int
erdt_init(const struct pqos_cap *cap,
          struct pqos_cpuinfo *cpu,
          struct pqos_erdt_info **erdt_info)
{
        int ret = PQOS_RETVAL_OK;
        int socket_num = 0;

        if (cap == NULL || cpu == NULL || erdt_info == NULL)
                return PQOS_RETVAL_PARAM;

        ret = acpi_init();
        if (ret != PQOS_RETVAL_OK) {
                LOG_WARN("Could not initialize ACPI!\n");
                return ret;
        }

        struct acpi_table *table = acpi_get_sig(ACPI_TABLE_SIG_ERDT);

        if (table == NULL) {
                LOG_WARN("Could not obtain %s table\n", ACPI_TABLE_SIG_ERDT);
                return PQOS_RETVAL_RESOURCE;
        }

        socket_num = cpuinfo_get_socket_num(cpu);
        if (socket_num == -1) {
                printf("Unable to get socket count\n");
                acpi_free(table);
                return PQOS_RETVAL_ERROR;
        }

        acpi_print(table);
        ret = erdt_populate_rmdds(erdt_info, table->erdt, socket_num);
        acpi_free(table);

        return ret;
}

void
erdt_fini(void)
{
        uint32_t idx = 0;
        uint32_t dase_idx = 0;

        if (p_erdt_info != NULL) {
                if (p_erdt_info->cpu_agents != NULL) {
                        idx = 0;
                        while (idx < p_erdt_info->num_cpu_agents) {
                                free(p_erdt_info->cpu_agents[idx]
                                         .cacd.enumeration_ids);
                                free(p_erdt_info->cpu_agents[idx]
                                         .mmrc.correction_factor);
                                idx++;
                        }

                        free(p_erdt_info->cpu_agents);
                }

                if (p_erdt_info->dev_agents != NULL) {
                        idx = 0;
                        while (idx < p_erdt_info->num_dev_agents) {
                                free(p_erdt_info->dev_agents[idx]
                                         .ibrd.correction_factor);
                                dase_idx = 0;
                                while (p_erdt_info->dev_agents[idx].dacd.dase &&
                                       dase_idx < p_erdt_info->dev_agents[idx]
                                                      .dacd.num_dases) {
                                        free(p_erdt_info->dev_agents[idx]
                                                 .dacd.dase[dase_idx]
                                                 .path);
                                        dase_idx++;
                                }
                                free(p_erdt_info->dev_agents[idx].dacd.dase);
                                idx++;
                        }

                        free(p_erdt_info->dev_agents);
                }

                free(p_erdt_info);
        }
}
