/*
 * BSD LICENSE
 *
 * Copyright(c) 2026 Intel Corporation. All rights reserved.
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

#ifndef __PQOS_HYBRID_H__
#define __PQOS_HYBRID_H__

#include "pqos.h"
#include "types.h"

/**
 * @brief CPUID reader callback
 *
 * @param [in] leaf CPUID leaf
 * @param [in] subleaf CPUID subleaf
 * @param [out] out CPUID register values
 * @param [in] context Callback context
 *
 * @return Operation status
 * @retval 0 Success
 */
typedef int (*hybrid_cpuid_fn)(unsigned leaf,
                               unsigned subleaf,
                               struct pqos_hybrid_cpuid_out *out,
                               void *context);

/**
 * @brief Reads hybrid capabilities from CPUID
 *
 * @param [in] cpuid CPUID reader callback
 * @param [in] context CPUID callback context
 * @param [out] cap Logical processor hybrid capabilities
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK Hybrid capabilities read successfully
 * @retval PQOS_RETVAL_PARAM Invalid parameter
 * @retval PQOS_RETVAL_RESOURCE Processor is not hybrid
 * @retval PQOS_RETVAL_ERROR CPUID read failed
 */
PQOS_LOCAL int hybrid_cap_read(hybrid_cpuid_fn cpuid,
                               void *context,
                               struct pqos_hybrid_core_capability *cap);

/**
 * @brief Compares regular and asymmetric capability enumeration
 *
 * @param [in,out] cap Logical processor capabilities and differences
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK Capabilities compared successfully
 * @retval PQOS_RETVAL_PARAM Invalid parameter
 */
PQOS_LOCAL int hybrid_cap_compare(struct pqos_hybrid_core_capability *cap);

/**
 * @brief Checks whether Resource Priority is enumerated
 *
 * @param [in] cap Platform hybrid capabilities
 *
 * @return 1 if CPUID leaf 28H sub-leaf 6 is enumerated, 0 otherwise
 */
PQOS_LOCAL int
hybrid_cap_rp_supported(const struct pqos_hybrid_capabilities *cap);

/**
 * @brief Discovers hybrid capabilities for all logical processors
 *
 * The returned structure is allocated by this function and must be freed by
 * the caller.
 *
 * @param [out] cap Platform hybrid capabilities
 * @param [in] cpu CPU topology
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK Discovery completed successfully
 * @retval PQOS_RETVAL_PARAM Invalid parameter
 * @retval PQOS_RETVAL_RESOURCE Allocation failed
 * @retval PQOS_RETVAL_ERROR Affinity restoration or CPUID enumeration failed,
 *         or the processors disagree on whether the platform is hybrid
 * @retval PQOS_RETVAL_UNAVAILABLE No topology CPUs are accessible
 */
PQOS_LOCAL int hybrid_cap_discover(struct pqos_hybrid_capabilities **cap,
                                   const struct pqos_cpuinfo *cpu);

#endif /* __PQOS_HYBRID_H__ */
