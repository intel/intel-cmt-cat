/*
 * BSD LICENSE
 *
 * Copyright(c) 2025-2026 Intel Corporation. All rights reserved.
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

#include "mrrm.h"

#include "common.h"
#include "log.h"
#include "utils.h"

#include <inttypes.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

/**
 * MRRM ACPI table information.
 * This pointer is allocated and initialized in this module.
 */
static struct pqos_mrrm_info *p_mrrm_info = NULL;

/**
 * @brief Parses MRRM acpi table to extract MRE information
 *
 * @param p_pqos_mre struct to be updated with MRE info
 * @param p_acpi_mre table to be parsed for MREs info
 * @param flags region assignment type
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK success
 */
static int
mrrm_populate_mre(struct pqos_mre_info *p_pqos_mre,
                  struct mrrm_mre_list *p_acpi_mre,
                  uint8_t flags)
{
        int regs_length = 0;

        if (p_acpi_mre->type != ACPI_MRRM_MRE_TYPE) {
                printf("Incorrect MRE structure type 0x%x\n", p_acpi_mre->type);
                return PQOS_RETVAL_ERROR;
        }

        p_pqos_mre->base_address_low = p_acpi_mre->base_address_low;
        p_pqos_mre->base_address_high = p_acpi_mre->base_address_high;
        p_pqos_mre->length_low = p_acpi_mre->length_low;
        p_pqos_mre->length_high = p_acpi_mre->length_high;
        p_pqos_mre->region_id_flags = p_acpi_mre->region_id_flags;
        p_pqos_mre->local_region_id = p_acpi_mre->local_region_id;
        p_pqos_mre->remote_region_id = p_acpi_mre->remote_region_id;

        if (flags == REGION_ASSIGNMENT_TYPE_SET) {
                regs_length = p_acpi_mre->length - ACPI_MRRM_MRE_SIZE;

                if (regs_length == 0)
                        return PQOS_RETVAL_ERROR;
                /* Copy Region-ID Programming Registers */
                p_pqos_mre->programming_regs = (uint8_t *)malloc(regs_length);
                if (p_pqos_mre->programming_regs == NULL)
                        return PQOS_RETVAL_ERROR;

                p_pqos_mre->regs_length = regs_length;
                memcpy(p_pqos_mre->programming_regs,
                       p_acpi_mre->region_id_programming_registers,
                       regs_length);
        }

        return PQOS_RETVAL_OK;
}

/**
 * @brief Parses MRRM acpi table to extract MRRM information
 *
 * @param mrrm_info struct to be updated with MRRM info
 * @param p_acpi_mrrm table to be parsed for MRRM info
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK success
 */
static int
mrrm_populate(struct pqos_mrrm_info **mrrm_info,
              struct acpi_table_mrrm *p_acpi_mrrm)
{
        int ret = PQOS_RETVAL_OK;
        int idx = 0;
        int mre_idx = 0;
        int mre_count = 0;
        int mres_size = 0;
        struct acpi_table_header *mrrm_acpi_header =
            (struct acpi_table_header *)p_acpi_mrrm;
        struct mrrm_mre_list *p_acpi_mre = NULL;

        mres_size = mrrm_acpi_header->length - sizeof(struct mrrm_header);
        p_acpi_mre = p_acpi_mrrm->mre;

        /* Find number of mre structures */
        while (mres_size > 0) {
                mres_size -= p_acpi_mrrm->mre[idx].length;
                mre_count++;
        }

        /* check invalid length */
        if (mres_size < 0) {
                printf("Invalid MRE length in MRRM %d\n",
                       mrrm_acpi_header->length);
                return PQOS_RETVAL_ERROR;
        }

        p_mrrm_info =
            (struct pqos_mrrm_info *)calloc(1, sizeof(struct pqos_mrrm_info));
        p_mrrm_info->max_memory_regions_supported =
            p_acpi_mrrm->header.max_memory_regions_supported;
        p_mrrm_info->flags = p_acpi_mrrm->header.flags;
        p_mrrm_info->num_mres = mre_count;
        p_mrrm_info->mre = (struct pqos_mre_info *)calloc(
            mre_count, sizeof(struct pqos_mre_info));

        mre_idx = 0;
        while (mre_idx < mre_count) {
                ret = mrrm_populate_mre(&p_mrrm_info->mre[mre_idx], p_acpi_mre,
                                        p_acpi_mrrm->header.flags);
                mre_idx++;
                if (ret != PQOS_RETVAL_OK)
                        break;
                p_acpi_mre = (struct mrrm_mre_list *)((char *)p_acpi_mre +
                                                      p_acpi_mre->length);
        }

        if (ret == PQOS_RETVAL_OK)
                *mrrm_info = p_mrrm_info;
        else
                mrrm_fini();

        return ret;
}

int
mrrm_init(const struct pqos_cap *cap, struct pqos_mrrm_info **mrrm_info)
{
        int ret;

        if (cap == NULL || mrrm_info == NULL)
                return PQOS_RETVAL_PARAM;

        ret = acpi_init();
        if (ret != PQOS_RETVAL_OK) {
                LOG_WARN("Could not initialize ACPI!\n");
                return ret;
        }

        struct acpi_table *table = acpi_get_sig(ACPI_TABLE_SIG_MRRM);

        if (table == NULL) {
                LOG_WARN("Could not obtain %s table\n", ACPI_TABLE_SIG_MRRM);
                return PQOS_RETVAL_RESOURCE;
        }

        acpi_print(table);
        ret = mrrm_populate(mrrm_info, table->mrrm);
        acpi_free(table);

        return ret;
}

void
mrrm_fini(void)
{
        uint32_t idx = 0;

        if (p_mrrm_info != NULL) {
                idx = 0;
                while (idx < p_mrrm_info->num_mres && p_mrrm_info->mre) {
                        if (p_mrrm_info->mre[idx].programming_regs)
                                free(p_mrrm_info->mre[idx].programming_regs);
                        idx++;
                }

                free(p_mrrm_info->mre);
                free(p_mrrm_info);
        }
}
