/*
 * BSD LICENSE
 *
 * Copyright(c) 2014-2023 Intel Corporation. All rights reserved.
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
 * @brief Internal header file to PQoS MBA allocation initialization
 */

#ifndef __PQOS_MMIO_ALLOC_H__
#define __PQOS_MMIO_ALLOC_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "pqos.h"
#include "types.h"

/**
 * @brief Writes range of MBA/CAT COS MSR's with \a msr_val value
 *
 * Used as part of CAT/MBA reset process.
 *
 * @param [in] mba_ids_num Number of items in \a mba_ids array
 * @param [in] mba_ids Array with MBA IDs
 * @param [in] enable MBA 4.0 enable flag, 1 - enable, 0 - disable
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 * @retval PQOS_RETVAL_ERROR on failure, MSR read/write error
 */
PQOS_LOCAL int mmio_alloc_reset_mba(const unsigned mba_ids_num,
                                    const unsigned *mba_ids,
                                    const int enable);

/**
 * @brief Hardware interface to set classes of service
 *        defined by \a MBA  on \a mba id
 *
 * @param [in]  mba_id MBA resource id. Ignored. Preserved for code
 *              compatibility.
 * @param [in]  num_cos number of classes of service at \a ca
 * @param [in]  requested table with class of service definitions
 * @param [out] actual table with class of service definitions
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 */
PQOS_LOCAL int mmio_mba_set(const unsigned mba_id,
                            const unsigned num_cos,
                            const struct pqos_mba *requested,
                            struct pqos_mba *actual);

/**
 * @brief Hardware interface to read MBA from domains
 *
 * @param [in]  mba_id MBA resource id. Ignored. Preserved for code
 *              compatibility.
 * @param [in]  max_num_cos maximum number of classes of service
 *              that can be accommodated at \a mba_tab
 * @param [out] num_cos number of classes of service read into \a mba_tab
 * @param [out] mba_tab table with read classes of service.
 *              mba_tab.domain_id must be set
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 */
PQOS_LOCAL int mmio_mba_get(const unsigned mba_id,
                            const unsigned max_num_cos,
                            unsigned *num_cos,
                            struct pqos_mba *mba_tab);

/**
 * @brief Hardware interface to set classes of service
 *        defined by a ca in Resource Allocation Domain
 *
 * @param [in] l3cat_id L3 CAT resource id
 * @param [in] num_ca number of classes of service at \a ca
 * @param [in] ca table with class of service definitions
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 */
PQOS_LOCAL int mmio_l3ca_set(const unsigned l3cat_id,
                             const unsigned num_ca,
                             const struct pqos_l3ca *ca);

/**
 * @brief Hardware interface to get classes of service
 *        defined by a ca in Resource Allocation Domain
 *
 * @param [in] l3cat_id L3 CAT resource id
 * @param [in] max_num_ca maximum number of classes of service
 *             that can be accommodated at \a ca
 * @param [in] num_ca number of classes of service at \a ca
 * @param [in] ca table with class of service definitions
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 */
PQOS_LOCAL int mmio_l3ca_get(const unsigned l3cat_id,
                             const unsigned max_num_ca,
                             unsigned *num_ca,
                             struct pqos_l3ca *ca);

#ifdef __cplusplus
}
#endif

#endif /* __PQOS_MMIO_ALLOC_H__ */
