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

#ifndef __HYBRID_H__
#define __HYBRID_H__

#include "pqos.h"

/**
 * @brief Parses a logical processor list
 *
 * The returned array is sorted, contains no duplicates and must be freed by
 * the caller.
 *
 * @param [in] text Comma-separated processor numbers and ranges
 * @param [out] cores Parsed logical processor identifiers
 * @param [out] count Number of parsed logical processors
 *
 * @return Operation status
 * @retval 0 Success
 * @retval -1 Invalid list or allocation failure
 */
int hybrid_parse_core_list(const char *text, unsigned **cores, unsigned *count);

/**
 * @brief Prints the platform hybrid processor status
 *
 * @param [in] sys PQoS system configuration returned by pqos_sysconfig_get()
 */
void hybrid_print_status(const struct pqos_sysconfig *sys);

/**
 * @brief Prints hybrid capabilities for selected logical processors
 *
 * If selection is NULL, capabilities for all logical processors represented
 * by the hybrid capability are printed.
 *
 * @param [in] sys PQoS system configuration returned by pqos_sysconfig_get()
 * @param [in] selection Optional logical processor list
 *
 * @return Operation status
 * @retval 0 Success
 * @retval -1 Invalid selection or hybrid capability unavailable
 */
int hybrid_enum_cores(const struct pqos_sysconfig *sys, const char *selection);

#endif /* __HYBRID_H__ */
