/*
 * BSD LICENSE
 *
 * Copyright(c) 2025 Intel Corporation. All rights reserved.
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
 * Dump module
 */

#ifndef __DUMP_H__
#define __DUMP_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "pqos.h"

/**
 *  @brief Prints all domains' MMIO Registers base address and size
 *
 * @param [in] sys PQoS system configuration struct
 *                 returned by a pqos_sysconfig_get
 */
int
pqos_print_dump_info(const struct pqos_sysconfig *sys);

/**
 * @brief Selects domain id option in dump command
 *
 * @param [in] arg string passed from --dump-domain-id command line option
 */
void
selfn_dump_domain_id(const char *arg);

/**
 * @brief Selects socket option in dump command
 *
 * @param [in] arg string passed from --socket command line option
 */
void
selfn_dump_socket(const char *arg);

/**
 * @brief Selects space option in dump command
 *
 * @param [in] arg string passed from --space command line option
 */
void
selfn_dump_space(const char *arg);

/**
 * @brief Selects width option in dump command
 *
 * @param [in] arg string passed from --width command line option
 */
void
selfn_dump_width(const char *arg);

/**
 * @brief Selects little-endian option in dump command
 *
 * @param arg not used
 */
void
selfn_dump_le(const char *arg);


/**
 * @brief Selects binary option in dump command
 *
 * @param arg not used
 */
void
selfn_dump_binary(const char *arg);

/**
 * @brief Selects offset option in dump command
 *
 * @param [in] arg string passed from --offset command line option
 */
void
selfn_dump_offset(const char *arg);

/**
 * @brief Selects length option in dump command
 *
 * @param [in] arg string passed from --length command line option
 */
void
selfn_dump_length(const char *arg);

/**
 *  @brief Prints the values of MMIO registers provided in --dump command
 *
 * @param [in] sys PQoS system configuration struct
 *                 returned by a pqos_sysconfig_get
 */
void
dump_mmio_regs(const struct pqos_sysconfig *sys);

#ifdef __cplusplus
}
#endif

#endif /* __DUMP_H__ */
