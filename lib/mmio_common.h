/*
 * BSD LICENSE
 *
 * Copyright(c) 2014-2025 Intel Corporation. All rights reserved.
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
 * @brief Internal header file for MBM/MBA region aware init/shutdown
 */

#ifndef __PQOS_MMIO_COMMON_H__
#define __PQOS_MMIO_COMMON_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "pqos.h"
#include "types.h"

/**
 * @brief  Total/Region-aware MBM and MBA mode for the whole system
 *
 * @param[in] mode Total/Region-Aware mode to set
 * @param[in] erdt ERDT Top-Level ACPI Structure
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK success
 */
PQOS_LOCAL int mmio_set_mbm_mba_mode(enum pqos_mbm_mba_modes mode,
                                     const struct pqos_erdt_info *erdt);

/**
 * @brief Scale MBM RMID value to bytes
 *
 * @param [in] mmrc MMRC table containing scaling factors
 * @param [in] rmid RMID value to be scaled
 * @param [in] val value to be scaled
 *
 * @return scaled value in bytes
 */
PQOS_LOCAL uint64_t scale_mbm_value(const struct pqos_erdt_mmrc *mmrc,
                                    const pqos_rmid_t rmid,
                                    const uint64_t val);

/**
 * @brief Scale LLC RMID value to bytes
 *
 * @param [in] cmrc CMRC table containing scaling factors
 * @param [in] val value to be scaled
 *
 * @return scaled value in bytes
 */
PQOS_LOCAL uint64_t scale_llc_value(const struct pqos_erdt_cmrc *cmrc,
                                    const uint64_t val);

/**
 * @brief Scale IO LLC RMID value to bytes
 *
 * @param [in] cmrd CMRD table containing scaling factors
 * @param [in] val value to be scaled
 *
 * @return scaled value in bytes
 */
PQOS_LOCAL uint64_t scale_io_llc_value(const struct pqos_erdt_cmrd *cmrd,
                                       const uint64_t val);

/**
 * @brief Scale IO MBM RMID value to bytes
 *
 * @param [in] ibrd IBRD table containing scaling factors
 * @param [in] rmid RMID value to be scaled
 * @param [in] val value to be scaled
 *
 * @return scaled value in bytes
 */
PQOS_LOCAL uint64_t scale_io_mbm_value(const struct pqos_erdt_ibrd *ibrd,
                                       const pqos_rmid_t rmid,
                                       const uint64_t val);
#ifdef __cplusplus
}
#endif

#endif /* __PQOS_MMIO_COMMON_H__ */
