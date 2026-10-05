/*
 * BSD LICENSE
 *
 * Copyright(c) 2014-2026 Intel Corporation. All rights reserved.
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

#ifndef __PQOS_ALLOC_COMMON_H__
#define __PQOS_ALLOC_COMMON_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "pqos.h"
#include "types.h"

/**
 * @brief Hardware interface to reset configuration
 *        of allocation technologies
 *
 * Reverts CAT/MBA state to the one after reset:
 * - all cores associated with CLOS0
 * - all CLOS are set to give access to entire resource
 * - all device channels associated with CLOS0
 *
 * As part of allocation reset CDP, MBA, I/O RDT reconfiguration
 * can be performed. This can be requested via \a cfg.
 *
 * @param [in] cfg requested configuration
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 */
PQOS_LOCAL int alloc_reset(const struct pqos_alloc_config *cfg);

/**
 * @brief Refuses a class whose capacity bitmask selects nothing
 *
 * A class with a zero mask has no cache to allocate, and every interface that
 * cannot say otherwise refuses it. The MMIO interface is the one that can: a
 * domain's CARD structure says whether it supports a zero-length bitmask, so
 * mmio_allocation.c asks the domain before refusing and keeps a check of its
 * own rather than calling this.
 *
 * With CDP enabled a class carries two masks and needs both, because code and
 * data are allocated separately: a class whose code mask selects no way, or
 * whose data mask selects none, is as unusable as one where neither does.
 *
 * @param [in] ca the classes to check
 * @param [in] num_ca how many there are
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK every class selects something
 * @retval PQOS_RETVAL_PARAM one of them does not, and it was reported
 */
PQOS_LOCAL int alloc_l3ca_check_bitmasks(const struct pqos_l3ca *ca,
                                         const unsigned num_ca);

/**
 * @brief Refuses an L2 class whose capacity bitmask selects nothing
 *
 * The L2 counterpart of alloc_l3ca_check_bitmasks(), on the same terms.
 *
 * @param [in] ca the classes to check
 * @param [in] num_ca how many there are
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK every class selects something
 * @retval PQOS_RETVAL_PARAM one of them does not, and it was reported
 */
PQOS_LOCAL int alloc_l2ca_check_bitmasks(const struct pqos_l2ca *ca,
                                         const unsigned num_ca);

#ifdef __cplusplus
}
#endif

#endif /* __PQOS_ALLOC_COMMON_H__ */
