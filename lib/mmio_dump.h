/*
 * BSD LICENSE
 *
 * Copyright(c) 2025-2026 Intel Corporation. All rights reserved.
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

#ifndef PQOS_MMIO_DUMP_H_
#define PQOS_MMIO_DUMP_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "pqos.h"
#include "types.h"
/**
 * @brief Print MMIO spaces according to the requested configuration
 *
 * @param [in] dump_cfg dump configuration
 *
 * @retval PQOS_RETVAL_OK on success
 * @retval PQOS_RETVAL_PARAM if the configuration is invalid
 * @retval PQOS_RETVAL_ERROR if the MMIO space cannot be dumped
 */
PQOS_LOCAL int mmio_dump(const struct pqos_mmio_dump *dump_cfg);

/**
 * @brief Dump a single MMIO address range
 *
 * @param [in] base base address
 * @param [in] size MMIO space size in bytes
 * @param [in] offset offset in elements from the base
 * @param [in] length number of elements, or zero for all remaining elements
 * @param [in] width_bytes element width in bytes
 * @param [in] le little-endian output flag
 * @param [in] binary binary output flag
 *
 * @return Operation status
 */
PQOS_LOCAL int dump_mmio_range(uint64_t base,
                               uint64_t size,
                               unsigned long offset,
                               unsigned long length,
                               unsigned width_bytes,
                               int le,
                               int binary);

#ifdef __cplusplus
}
#endif

#endif /* PQOS_MMIO_DUMP_H_ */
