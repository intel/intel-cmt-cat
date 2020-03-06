/*
 * BSD LICENSE
 *
 * Copyright(c) 2020 Intel Corporation. All rights reserved.
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

#ifndef __PQOS_HW_CAP_H__
#define __PQOS_HW_CAP_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "pqos.h"

/**
 * @brief Discovers HW monitoring support
 *
 * @param r_cap place to store monitoring capabilities structure
 * @param cpu detected cpu topology
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK success
 */
int hw_cap_mon_discover(struct pqos_cap_mon **r_cap,
                        const struct pqos_cpuinfo *cpu);

/**
 * @brief Discovers HW support of L3 CAT
 *
 * @param cap place to store CAT capabilities
 * @param cpu detected cpu topology
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK success
 */
int hw_cap_l3ca_discover(struct pqos_cap_l3ca *cap,
                         const struct pqos_cpuinfo *cpu);

/**
 * @brief Discovers HW support of L2 CAT
 *
 * @param cap place to store L2 CAT capabilities
 * @param cpu CPU topology structure
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK success
 */
int hw_cap_l2ca_discover(struct pqos_cap_l2ca *cap,
                         const struct pqos_cpuinfo *cpu);

/**
 * @brief Discovers HW support of MBA
 *
 * @param cap place to store MBA capabilities
 * @param cpu detected cpu topology
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK success
 */
int hw_cap_mba_discover(struct pqos_cap_mba *cap,
                        const struct pqos_cpuinfo *cpu);

/**
 * @brief Discovers MBA support for AMD
 *
 * @param cap place to store MBA capabilities
 * @param cpu detected cpu topology
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK success
 * @retval PQOS_RETVAL_RESOURCE if not supported
 */
int amd_cap_mba_discover(struct pqos_cap_mba *cap,
                         const struct pqos_cpuinfo *cpu);

#ifdef __cplusplus
}
#endif

#endif /* __PQOS_HW_CAP_H__ */
