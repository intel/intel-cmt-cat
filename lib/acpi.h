/*
 * BSD LICENSE
 *
 * Copyright(c) 2023 Intel Corporation. All rights reserved.
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
 * @brief Internal header file for ACPI initialization
 */

#ifndef __PQOS_ACPI_H__
#define __PQOS_ACPI_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "acpi_table.h"
#include "pqos.h"
#include "types.h"

#if defined(__x86_64__)
typedef uint64_t acpi_address;
typedef uint64_t acpi_size;
#elif defined(__i386__)
typedef uint32_t acpi_address;
typedef uint32_t acpi_size;
#else
#error "Unsupported architecture"
#endif

/**
 * ACPI Table
 */
struct acpi_table {
        union {
                struct acpi_table_header *header;
                struct acpi_table_rsdp *rsdp;
                struct acpi_table_rsdt *rsdt;
                struct acpi_table_xsdt *xsdt;
                struct acpi_table_irdt *irdt;
                struct acpi_table_erdt *erdt;
                struct acpi_table_mrrm *mrrm;
                char *signature;
                uint8_t *generic;
        };
};

/**
 * @brief Initializes ACPI module
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK success
 */
PQOS_LOCAL int acpi_init(void);

/**
 * @brief Finalizes ACPI module
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK success
 */
PQOS_LOCAL int acpi_fini(void);

/**
 * @brief mmaps ACPI table
 *
 * @param addr ACPI Table Address (physical)
 *
 * @return Pointer to struct with mmapped ACPI table
 * @retval NULL on error
 */
PQOS_LOCAL struct acpi_table *acpi_get_addr(acpi_address addr);

/**
 * @brief Gets ACPI Table with matching signature
 *
 * @param sig Signature of ACPI Table to get
 *
 * @return Pointer to struct with mmapped ACPI table
 * @retval NULL on error
 */
PQOS_LOCAL struct acpi_table *acpi_get_sig(const char *sig);

/**
 * @brief Gets ACPI Root System Description Pointer table (RSDP)
 *
 * @return Pointer to struct with mmapped ACPI table
 * @retval NULL on error
 */
PQOS_LOCAL struct acpi_table *acpi_get_rsdp(void);

/**
 * @brief Gets ACPI Extended System Description Table (XSDT)
 *
 * @return Pointer to struct with mmapped ACPI table
 * @retval NULL on error
 */
PQOS_LOCAL struct acpi_table *acpi_get_xsdt(void);

/**
 * @brief Unmmaps ACPI table and frees helper struct
 *
 * @param table Pointer to struct with mmapped ACPI table
 */
PQOS_LOCAL void acpi_free(struct acpi_table *table);

/**
 * @brief Get RMUDs from IRDT table
 *
 * @param irdt IRDT table
 * @param num number of extracted RMUDs
 *
 * @return array of extracted RMUDs
 */
PQOS_LOCAL struct acpi_table_irdt_rmud **
acpi_get_irdt_rmud(struct acpi_table_irdt *irdt, size_t *num);

/**
 * @brief Get DEVICEs from RMUD table
 *
 * @param rmud RMUD table
 * @param num number of extracted DEVICEs
 *
 * @return array of extracted DEVICEs (DSS and/or RCS)
 */
PQOS_LOCAL struct acpi_table_irdt_device **
acpi_get_irdt_dev(struct acpi_table_irdt_rmud *rmud, size_t *num);

/**
 * @brief Get CHMSs from DEV/DSS table
 *
 * @param dev DEV/DSS table
 * @param chms array of extracted CHMSs
 * @param num number of extracted CHMSs
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK success
 */
PQOS_LOCAL int acpi_get_irdt_chms(struct acpi_table_irdt_device *dev,
                                  struct acpi_table_irdt_chms **chms[],
                                  size_t *num);
/**
 * @brief Prints out ACPI table in human readable form
 *
 * @param table ACPI table to be printed out
 */
PQOS_LOCAL void acpi_print(struct acpi_table *table);

#ifdef __cplusplus
}
#endif

#endif /* __PQOS_ACPI_H__ */
