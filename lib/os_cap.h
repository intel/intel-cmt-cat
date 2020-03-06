/*
 * BSD LICENSE
 *
 * Copyright(c) 2019-2020 Intel Corporation. All rights reserved.
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
 */

#ifndef __PQOS_OS_CAP_H__
#define __PQOS_OS_CAP_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Retrieves MBA controller configuration status
 *
 * @param [in] cap platform QoS capabilities structure
 *                 returned by \a pqos_cap_get
 * @param [in] cpu CPU information structure from \a pqos_cap_get
 * @param [out] supported place to store MBA controller support status
 * @param [out] enabled place to store MBA controller enable status
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 */
int os_cap_get_mba_ctrl(const struct pqos_cap *cap,
                        const struct pqos_cpuinfo *cpu,
                        int *supported,
                        int *enabled);

/**
 * @brief Initializes os capabilities
 *
 * @param inter selected pqos interface
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK success
 */
int os_cap_init(const enum pqos_interface inter);

/**
 * @brief Discovers OS monitoring support
 *
 * @param r_cap place to store monitoring capabilities structure
 * @param cpu detected cpu topology
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK success
 */
int os_cap_mon_discover(struct pqos_cap_mon **r_cap,
                        const struct pqos_cpuinfo *cpu);

/**
 * @brief Discovers OS support of L3 CAT
 *
 * @param cap place to store CAT capabilities
 * @param cpu detected cpu topology
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK success
 */
int os_cap_l3ca_discover(struct pqos_cap_l3ca *cap,
                         const struct pqos_cpuinfo *cpu);

/**
 * @brief Discovers OS support of L2 CAT
 *
 * @param cap place to store CAT capabilities
 * @param cpu detected cpu topology
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK success
 */
int os_cap_l2ca_discover(struct pqos_cap_l2ca *cap,
                         const struct pqos_cpuinfo *cpu);

/**
 * @brief Discovers OS support of MBA
 *
 * @param cap place to store MBA capabilities
 * @param cpu detected cpu topology
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK success
 */
int os_cap_mba_discover(struct pqos_cap_mba *cap,
                        const struct pqos_cpuinfo *cpu);

#ifdef __cplusplus
}
#endif

#endif /* __PQOS_HOSTCAP_H__ */
