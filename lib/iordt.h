/*
 * BSD LICENSE
 *
 * Copyright(c) 2023-2026 Intel Corporation. All rights reserved.
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
 * @brief Internal header file for IORDT module
 */

#ifndef __PQOS_IORDT_H__
#define __PQOS_IORDT_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "pqos.h"
#include "types.h"

/**
 * @brief Initializes IORDT module
 *
 * Initialize I/O RDT module
 * Detect ACPI devices
 * Initialize pqos_devinfo structure
 * Print logs about detected ACPI configuration
 *
 * @param [in] cap capabilities structure
 * @param [out] devinfo pqos_devinfo structure to be initialized
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK success
 */
PQOS_LOCAL int iordt_init(const struct pqos_cap *cap,
                          struct pqos_devinfo **devinfo);

/**
 * @brief Shuts down IORDT module
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK success
 */
PQOS_LOCAL int iordt_fini(void);

/**
 * @brief Check if I/O RDT is supported
 *
 * @param [in] platform QoS capabilities structure
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK is I/O RDT is supported
 */
PQOS_LOCAL int iordt_check_support(const struct pqos_cap *cap);

/**
 * @brief Obtains numa node for the channel
 *
 * @param [in] channel MMIO channel
 * @param [out] numa Socket id
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK success
 */
PQOS_LOCAL int iordt_get_numa(const struct pqos_devinfo *devinfo,
                              pqos_channel_t channel_id,
                              unsigned *numa);

/**
 * @brief Writes RMID association
 *
 * @param channel channel to be associated with RMID
 * @param rmid RMID to associate channel with
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK success
 */
PQOS_LOCAL int iordt_mon_assoc_write(pqos_channel_t channel, pqos_rmid_t rmid);

/**
 * @brief Reads RMID association
 *
 * @param channel channel to read association of
 * @param rmid associated RMID
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK success
 */
PQOS_LOCAL int iordt_mon_assoc_read(pqos_channel_t channel, pqos_rmid_t *rmid);

/**
 * @brief reset I/O RDT channel assoc
 *
 * @param[in] dev I/O RDT device information
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK success
 */
PQOS_LOCAL int iordt_mon_assoc_reset(const struct pqos_devinfo *dev);

/**
 * @brief Writes CLOS association
 *
 * @param channel channel to be associated with CLOS
 * @param class_id CLOS to associate channel with
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK success
 */
PQOS_LOCAL int iordt_assoc_write(pqos_channel_t channel, unsigned class_id);

/**
 * @brief Reads CLOS association
 *
 * @param channel channel to read association of
 * @param class_id associated CLOS
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK success
 */
PQOS_LOCAL int iordt_assoc_read(pqos_channel_t channel, unsigned *class_id);

/**
 * @brief reset CLOS assoc
 *
 * @param[in] dev I/O RDT device information
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK success
 */
PQOS_LOCAL int iordt_assoc_reset(const struct pqos_devinfo *dev);

#ifdef __cplusplus
}
#endif

#endif /* __PQOS_IORDT_H__ */
