/*
 * BSD LICENSE
 *
 * Copyright(c) 2017-2023 Intel Corporation. All rights reserved.
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
 * @brief Internal header file to PQoS utils initialization
 */

#ifndef __PQOS_UTILS_H__
#define __PQOS_UTILS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "pqos.h"
#include "types.h"

/**
 * @brief Initializes utils module
 *
 * @param interface option, MSR or OS
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK success
 */
PQOS_LOCAL int _pqos_utils_init(int interface);

/**
 * @brief Retrieves L3 MON I/O RDT status
 *
 * @param [in] cap platform QoS capabilities structure
 *                 returned by \a pqos_cap_get
 * @param [out] supported place to store I/O RDT monitoring support status
 * @param [out] enabled place to store I/O RDT monitoring enable status
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 */
int pqos_mon_iordt_enabled(const struct pqos_cap *cap,
                           int *supported,
                           int *enabled);

/**
 * @brief Retrieves channel information from dev info structure
 *
 * @param [in] dev Device information structure
 * @param [in] channel_id channel id
 *
 * @return Channel information structure
 * @retval NULL on error
 */
const struct pqos_channel *
pqos_devinfo_get_channel(const struct pqos_devinfo *dev,
                         const pqos_channel_t channel_id);

/**
 * @brief Checks if channel is shared between multiple devices
 *
 * @param [in] dev Device information structure
 * @param [in] channel_id channel id
 * @param [out] shared 1 if channel is shared
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 */
PQOS_LOCAL int pqos_devinfo_get_channel_shared(const struct pqos_devinfo *dev,
                                               const pqos_channel_t channel_id,
                                               int *shared);

/**
 * @brief Internal API to retrieve \a type of capability
 *
 * @param [in] type capability type to look for
 *
 * @return Pointer to selected capability
 */
PQOS_LOCAL const struct pqos_capability *
_pqos_cap_get_type(const enum pqos_cap_type type);

/**
 * @brief Returns CPU agent information for a given domain
 *
 * @param [in]  domain_id domain to extract CPU agent information for
 *
 * @return CPU agent information for a given domain or NULL if not found
 */
PQOS_LOCAL const struct pqos_cpu_agent_info *
get_cpu_agent_by_domain(uint16_t domain_id);

/**
 * @brief Returns DEV agent information for a given domain
 *
 * @param [in]  domain_id domain to extract DEV agent information for
 *
 * @return DEV agent information for a given domain or NULL if not found
 */
PQOS_LOCAL const struct pqos_device_agent_info *
get_dev_agent_by_domain(uint16_t domain_id);

/**
 * @brief Retrieves Domain ID of given PCI device
 *
 * @param [in] devinfo Device information structure
 * @param [in] segment PCI device's segment
 * @param [in] bdf PCI devices's bus,device and function information
 * @param [out] domain_id domain id of given PCI device
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 */
PQOS_LOCAL int pqos_devinfo_get_domain_id(const struct pqos_devinfo *devinfo,
                                          const uint16_t segment,
                                          const uint16_t bdf,
                                          uint16_t *domain_id);

#ifdef __cplusplus
}
#endif

#endif /* __PQOS_UTULS_H__ */
