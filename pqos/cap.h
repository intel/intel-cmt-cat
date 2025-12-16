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
 */

/**
 * @brief Capability module
 */

#include "pqos.h"

#include <stdint.h>
#include <stdio.h>

#ifndef __CAP_H__
#define __CAP_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Print information about supported RDT features
 *
 * @param [in] sys PQoS system configuration struct
 *                 returned by a pqos_sysconfig_get
 * @param [in] verbose enable verbose mode
 */
void cap_print_features(const struct pqos_sysconfig *sys, const int verbose);

/**
 * @brief Print information about Memory Regions from MRRM ACPI table
 *
 * @param [in] sys PQoS system configuration struct
 *                 returned by a pqos_sysconfig_get
 */
void cap_print_mem_regions(const struct pqos_sysconfig *sys);

/**
 * @brief Print information about Processor topology from ERDT ACPI table
 *
 * @param [in] sys PQoS system configuration struct
 *                 returned by a pqos_sysconfig_get
 */
void cap_print_topology(const struct pqos_sysconfig *sys);

/**
 * @brief Verifies and translates PCI Device's Segment and BDF information
 *
 * @param str single dev string passed to --print-io-dev command line option
 */
void parse_io_dev(char *str);

/**
 * @brief Print information about I/O devices from ERDT & IRDT ACPI tables
 *
 * @param [in] sys PQoS system configuration struct
 *                 returned by a pqos_sysconfig_get
 */
void cap_print_io_devs(const struct pqos_sysconfig *sys);

/**
 * @brief Print information about I/O device from ERDT & IRDT ACPI tables
 *
 * @param [in] sys PQoS system configuration struct
 *                 returned by a pqos_sysconfig_get
 */
void cap_print_io_dev(const struct pqos_sysconfig *sys);

#ifdef __cplusplus
}
#endif

#endif /* __CAP_H__ */
