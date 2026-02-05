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

/**
 * @brief Internal header file for MRRM module
 */

#ifndef __PQOS_MRRM_H__
#define __PQOS_MRRM_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "acpi.h"

#define ACPI_MRRM_REVISION 1
#ifndef ACPI_TABLE_SIG_MRRM
#define ACPI_TABLE_SIG_MRRM "MRRM"
#endif

#define REGION_ASSIGNMENT_TYPE_SET 1
/* MRE structure size without Region-ID Programming Registers[] */
#define ACPI_MRRM_MRE_SIZE 32
#define ACPI_MRRM_MRE_TYPE 0

/**
 * Memory Range and Region Mapping (MRRM) Structure
 */
struct __attribute__((__packed__)) mrrm_header {
        struct acpi_table_header header;
        uint8_t max_memory_regions_supported;
        uint8_t flags;
        uint8_t reserved[26];
};

/**
 * Memory Range Entry (MRE) Structure
 */
struct __attribute__((__packed__)) mrrm_mre_list {
        uint16_t type;
        uint16_t length;
        uint8_t reserved[4];
        uint32_t base_address_low;
        uint32_t base_address_high;
        uint32_t length_low;
        uint32_t length_high;
        uint16_t region_id_flags;
        uint8_t local_region_id;
        uint8_t remote_region_id;
        uint8_t reserved2[4];
        uint8_t region_id_programming_registers[0];
};

/**
 * MRRM Top-Level ACPI Structure
 */
struct __attribute__((__packed__)) acpi_table_mrrm {
        struct mrrm_header header;
        struct mrrm_mre_list mre[0];
};

/**
 * @brief Initializes MRRM module
 *
 * Initialize MRRM module
 * Detect ACPI MRRM tables
 * Initialize mmio data
 * Print logs about detected ACPI configuration
 *
 * @param [in] cap capabilities structure
 * @param [out] pqos_mrrm pqos_mrrm_info structure to be initialized
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK success
 */
PQOS_LOCAL int mrrm_init(const struct pqos_cap *cap,
                         struct pqos_mrrm_info **pqos_mrrm);

/**
 * @brief Shuts down MRRM module
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK success
 */
PQOS_LOCAL void mrrm_fini(void);

#ifdef __cplusplus
}
#endif

#endif /* __PQOS_MRRM_H_ */
