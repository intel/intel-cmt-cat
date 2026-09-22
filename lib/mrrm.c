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
 * @brief Parses MRRM ACPI table to extract MRE information
 *
 * @param p_pqos_mre Structure to be updated with MRE information
 * @param p_acpi_mre Table to be parsed for MRE information
 * @param flags Region assignment type
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK success
 */
static int
mrrm_populate_mre(struct pqos_mre_info *p_pqos_mre,
                  const struct mrrm_mre_list *p_acpi_mre,
                  uint8_t flags)
{
        size_t regs_length;
        size_t offset;
        uint64_t reg;

        if (p_acpi_mre->type != ACPI_MRRM_MRE_TYPE) {
                LOG_ERROR("Incorrect MRE structure type 0x%x\n",
                          (unsigned)p_acpi_mre->type);
                return PQOS_RETVAL_ERROR;
        }
        if (p_acpi_mre->length < sizeof(*p_acpi_mre)) {
                LOG_ERROR("Invalid MRE length %u\n",
                          (unsigned)p_acpi_mre->length);
                return PQOS_RETVAL_ERROR;
        }

        if (p_acpi_mre->region_id_flags & ~REGION_ID_FLAGS_MASK) {
                LOG_ERROR("Invalid MRE Region-ID flags\n");
                return PQOS_RETVAL_ERROR;
        }

        p_pqos_mre->base_address_low = p_acpi_mre->base_address_low;
        p_pqos_mre->base_address_high = p_acpi_mre->base_address_high;
        p_pqos_mre->length_low = p_acpi_mre->length_low;
        p_pqos_mre->length_high = p_acpi_mre->length_high;
        p_pqos_mre->region_id_flags = p_acpi_mre->region_id_flags;
        p_pqos_mre->local_region_id = p_acpi_mre->local_region_id;
        p_pqos_mre->remote_region_id = p_acpi_mre->remote_region_id;

        if (!(flags & REGION_ASSIGNMENT_TYPE_BIT)) {
                if (p_acpi_mre->length != sizeof(*p_acpi_mre)) {
                        LOG_ERROR("Static MRE contains programming "
                                  "registers\n");
                        return PQOS_RETVAL_ERROR;
                }
                return PQOS_RETVAL_OK;
        }

        regs_length = p_acpi_mre->length - sizeof(*p_acpi_mre);
        if (regs_length == 0 ||
            regs_length % ACPI_MRRM_PROGRAMMING_REGISTER_SIZE != 0) {
                LOG_ERROR("Invalid MRE programming register list length\n");
                return PQOS_RETVAL_ERROR;
        }

        for (offset = 0; offset < regs_length;
             offset += ACPI_MRRM_PROGRAMMING_REGISTER_SIZE) {
                memcpy(&reg,
                       p_acpi_mre->region_id_programming_registers + offset,
                       sizeof(reg));
                if (reg % ACPI_MRRM_PROGRAMMING_REGISTER_SIZE != 0) {
                        LOG_ERROR("Unaligned MRE programming register\n");
                        return PQOS_RETVAL_ERROR;
                }
        }

        p_pqos_mre->programming_regs = malloc(regs_length);
        if (p_pqos_mre->programming_regs == NULL) {
                LOG_ERROR("Can't allocate memory for MRE registers\n");
                return PQOS_RETVAL_ERROR;
        }

        p_pqos_mre->regs_length = regs_length;
        memcpy(p_pqos_mre->programming_regs,
               p_acpi_mre->region_id_programming_registers, regs_length);

        return PQOS_RETVAL_OK;
}

/**
 * @brief Parses MRRM ACPI table to extract MRRM information
 *
 * @param mrrm_info Structure to be updated with MRRM information
 * @param p_acpi_mrrm Table to be parsed for MRRM information
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK success
 */
static int
mrrm_populate(struct pqos_mrrm_info **mrrm_info,
              const struct acpi_table_mrrm *p_acpi_mrrm)
{
        const struct acpi_table_header *header = &p_acpi_mrrm->header.header;
        const struct mrrm_mre_list *mre;
        size_t remaining;
        unsigned mre_count = 0;
        unsigned i;
        int ret;

        if (header->revision != ACPI_MRRM_REVISION) {
                LOG_ERROR("Unsupported MRRM revision %u\n",
                          (unsigned)header->revision);
                return PQOS_RETVAL_ERROR;
        }
        if (header->length < sizeof(struct mrrm_header)) {
                LOG_ERROR("Invalid MRRM length %u\n", header->length);
                return PQOS_RETVAL_ERROR;
        }
        if (p_acpi_mrrm->header.flags & ~REGION_ASSIGNMENT_TYPE_BIT) {
                LOG_ERROR("Invalid MRRM flags 0x%x\n",
                          (unsigned)p_acpi_mrrm->header.flags);
                return PQOS_RETVAL_ERROR;
        }
        if (p_acpi_mrrm->header.max_memory_regions_supported == 0) {
                LOG_ERROR("MRRM supports no memory regions\n");
                return PQOS_RETVAL_ERROR;
        }

        remaining = header->length - sizeof(struct mrrm_header);
        mre = p_acpi_mrrm->mre;
        while (remaining > 0) {
                if (remaining < sizeof(*mre) || mre->length < sizeof(*mre) ||
                    mre->length > remaining) {
                        LOG_ERROR("Invalid MRE length in MRRM table\n");
                        return PQOS_RETVAL_ERROR;
                }

                remaining -= mre->length;
                mre = (const struct mrrm_mre_list *)((const uint8_t *)mre +
                                                     mre->length);
                mre_count++;
        }

        /* A table with no MRE structures is not an error. It still says how
         * many memory regions the platform supports and how their IDs are
         * assigned, which is what the header carries, and the MMIO interface
         * addresses regions by ID rather than by address - so the only thing
         * missing is the mapping from addresses to regions, and the only caller
         * that wants it is the report, which says it has none. Refusing here
         * failed pqos_init() outright and took the two answers the table does
         * give down with it.
         */
        if (mre_count == 0)
                LOG_INFO("MRRM table maps no memory ranges\n");

        p_mrrm_info = calloc(1, sizeof(*p_mrrm_info));
        if (p_mrrm_info == NULL) {
                LOG_ERROR("Can't allocate memory for MRRM information\n");
                return PQOS_RETVAL_ERROR;
        }

        /**
         * The value reported by the table is kept as it is, because it is
         * platform information. Limiting it to the number of regions the
         * interfaces can address is done by their accessors.
         */
        p_mrrm_info->max_memory_regions_supported =
            p_acpi_mrrm->header.max_memory_regions_supported;
        if (p_mrrm_info->max_memory_regions_supported > PQOS_MAX_MEM_REGIONS)
                LOG_WARN("MRRM reports %u memory regions, only %u of them can "
                         "be used\n",
                         (unsigned)p_mrrm_info->max_memory_regions_supported,
                         (unsigned)PQOS_MAX_MEM_REGIONS);
        p_mrrm_info->flags = p_acpi_mrrm->header.flags;
        p_mrrm_info->num_mres = mre_count;

        if (mre_count > 0) {
                p_mrrm_info->mre = calloc(mre_count, sizeof(*p_mrrm_info->mre));
                if (p_mrrm_info->mre == NULL) {
                        LOG_ERROR("Can't allocate memory for MREs\n");
                        mrrm_fini();
                        return PQOS_RETVAL_ERROR;
                }
        }

        mre = p_acpi_mrrm->mre;
        for (i = 0; i < mre_count; i++) {
                ret = mrrm_populate_mre(&p_mrrm_info->mre[i], mre,
                                        p_acpi_mrrm->header.flags);
                if (ret != PQOS_RETVAL_OK) {
                        mrrm_fini();
                        return ret;
                }
                mre = (const struct mrrm_mre_list *)((const uint8_t *)mre +
                                                     mre->length);
        }

        LOG_DEBUG("Parsed %u MRE structures\n", mre_count);
        *mrrm_info = p_mrrm_info;

        return PQOS_RETVAL_OK;
}

int
mrrm_init(const struct pqos_cap *cap, struct pqos_mrrm_info **mrrm_info)
{
        struct acpi_table *table;
        int ret;

        if (cap == NULL || mrrm_info == NULL)
                return PQOS_RETVAL_PARAM;
        *mrrm_info = NULL;

        ret = acpi_init();
        if (ret != PQOS_RETVAL_OK) {
                LOG_WARN("Could not initialize ACPI!\n");
                return ret;
        }

        table = acpi_get_sig(ACPI_TABLE_SIG_MRRM);
        if (table == NULL) {
                LOG_WARN("Could not obtain %s table\n", ACPI_TABLE_SIG_MRRM);
                (void)acpi_fini();
                return PQOS_RETVAL_RESOURCE;
        }

        acpi_print(table);
        ret = mrrm_populate(mrrm_info, table->mrrm);
        acpi_free(table);
        if (ret != PQOS_RETVAL_OK)
                (void)acpi_fini();

        return ret;
}

void
mrrm_fini(void)
{
        uint32_t idx;

        if (p_mrrm_info == NULL)
                return;

        if (p_mrrm_info->mre != NULL)
                for (idx = 0; idx < p_mrrm_info->num_mres; idx++)
                        free(p_mrrm_info->mre[idx].programming_regs);

        free(p_mrrm_info->mre);
        free(p_mrrm_info);
        p_mrrm_info = NULL;
}
