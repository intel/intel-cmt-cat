/*
 *   BSD LICENSE
 *
 *   Copyright(c) 2016 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _RDT_H
#define _RDT_H

#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes libpqos and configures CAT
 *
 * @return status
 * @retval 0 on success
 * @retval negative on error (-errno)
 */
int cat_init(void);

/**
 * @brief deinitializes libpqos
 */
void cat_fini(void);

/**
 * @brief Reverts CAT configuration and deinitializes libpqos
 */
void cat_exit(void);

/**
 * @brief Parses -r/--reset params
 *
 * @param [in] cpu params string
 *
 * @return parse status
 * @retval 0 on success
 * @retval negative on error (-errno)
 */
int parse_reset(const char *cpu);

/**
 * @brief Parses -t/--rdt params and stores configuration in g_cfg
 *
 * @note The format pattern:
 *       --rdt='<[l3(cbm)|l2(cbm)]@(cpus)>[;<l3(ccbm,dcbm)@(cpus)>...]'
 *       Capacity bit mask (cbm) could be a single mask
 *       or for a L3 CDP enabled system, a group of two masks
 *       ("code cbm" and "data cbm")
 *       cpus could be a single digit/range or a group.
 *
 *       e.g. 'l3(0x00F00)@(1,3);l3(0x0FF00)@(4-6);l3(0xF0000)@7'
 *       - CPUs 1 and 3 share its 4 ways of L3 with CPUs 4, 5 and 6;
 *       - CPUs 4,5 and 6 share half (4 out of 8 ways) of its L3 with 1 and 3;
 *       - CPUs 4,5 and 6 have exclusive access to 4 out of 8 L3 ways;
 *       - CPU 7 has exclusive access to all of its 4 L3 ways;
 *
 *       e.g. 'l3(0x00C00,0x00300)@(1,3)' for a CDP enabled system
 *       - cpus 1 and 3 have access to 2 L3 ways for code
 *       and 2 L3 ways for data, code and data ways are not overlapping.;
 *
 * @param [in] rdtstr params string
 *
 * @return parse status
 * @retval 0 on success
 * @retval negative on error (-errno)
 */
int parse_rdt(char *rdtstr);

/*
 * @brief Checks if it's possible to fulfill requested CAT configuration
 *        and then configures system.
 *
 * @return status
 * @retval 0 on success
 * @retval negative on error (-errno)
 */
int cat_set(void);

/*
 * @brief Resets COS association (assign COS#0) on listed CPUs
 *
 * @return status
 * @retval 0 on success
 * @retval negative on error (-errno)
 */
int cat_reset(void);

/**
 * @brief This function dumps internal config structures
 *        (updated by parse_rdt())
 */
void print_cmd_line_rdt_config(void);

#ifdef __cplusplus
}
#endif

#endif /* _RDT_H */
