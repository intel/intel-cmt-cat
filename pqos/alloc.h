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
 */

/**
 * Allocation module
 */

#ifndef __ALLOCATION_H__
#define __ALLOCATION_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "pqos.h"

#include <stdint.h>
#include <stdio.h>

/**
 * @brief Defines allocation class of service
 *
 * @param [in] arg string passed to -e command line option
 */
void selfn_allocation_class(const char *arg);

/**
 * @brief Associates cores with selected class of service
 *
 * @param [in] arg string passed to -a command line option
 */
void selfn_allocation_assoc(const char *arg);

/**
 * @brief Selects memory regions for allocation
 *
 * @param [in] arg the list given to --alloc-mem-regions: region numbers in any
 *             form strlisttotab() reads - commas, ranges, decimal or
 *             hexadecimal, with duplicates dropped
 */
void selfn_alloc_mem_regions(const char *arg);

/**
 * @brief Selects optimal bandwidth in memory regions for allocation
 *
 * @param arg not used
 */
void selfn_alloc_opt_bw(const char *arg);

/**
 * @brief Selects minimum bandwidth in memory regions for allocation
 *
 * @param arg not used
 */
void selfn_alloc_min_bw(const char *arg);

/**
 * @brief Selects maximum bandwidth in memory regions for allocation
 *
 * @param arg not used
 */
void selfn_alloc_max_bw(const char *arg);

/**
 * @brief Selects domain id for allocation
 *
 * @param [in] arg the list given to --alloc-domain-id: domain identifiers in
 *             any form strlisttotab() reads - commas, ranges, decimal or
 *             hexadecimal, with duplicates dropped
 */
void selfn_alloc_domain_id(const char *arg);

/**
 * @brief Prints information about cache allocation settings in the system
 *
 * @param [in] cap_mon monitoring capability structure
 * @param [in] cap_l3ca L3 CAT capability structures
 * @param [in] cap_l2ca L2 CAT capability structures
 * @param [in] cap_mba MBA capability structures
 * @param [in] cpu_info cpu information structure
 * @param [in] dev_info IO RDT device information structure
 * @param [in] verbose enable verbose mode
 */
void alloc_print_config(const struct pqos_capability *cap_mon,
                        const struct pqos_capability *cap_l3ca,
                        const struct pqos_capability *cap_l2ca,
                        const struct pqos_capability *cap_mba,
                        const struct pqos_capability *cap_smba,
                        const struct pqos_sysconfig *sys,
                        const int verbose);

/**
 * @brief Prints information about each domain's cache allocation settings
 *        in the system
 *
 * @param [in] cap_mon monitoring capability structure
 * @param [in] cap_l3ca L3 CAT capability structures
 * @param [in] cap_l2ca L2 CAT capability structures
 * @param [in] cap_mba MBA capability structures
 * @param [in] sys PQoS system configuration structure
 * @param [in] verbose enable verbose mode
 */
void print_domain_alloc_config(const struct pqos_capability *cap_mon,
                               const struct pqos_capability *cap_l3ca,
                               const struct pqos_capability *cap_l2ca,
                               const struct pqos_capability *cap_mba,
                               const struct pqos_sysconfig *sys);

/**
 * @brief Applies allocation settings previously selected via
 *        selfn_xxxx() functions
 *
 * @param [in] cap_l3ca CAT capability structures
 * @param [in] cap_l2ca CAT capability structures
 * @param [in] cap_mba MBA capability structures
 * @param [in] cap_smba SMBA capability structures
 * @param [in] cpu CPU information structure
 * @param [in] dev Device information structure
 *
 * @return Operation status
 * @retval 0 there was no new config to apply
 * @retavl 1 there was new config to apply and it went smoothly
 * @retval -1 an error occurred when applying new config
 */
int alloc_apply(const struct pqos_capability *cap_l3ca,
                const struct pqos_capability *cap_l2ca,
                const struct pqos_capability *cap_mba,
                const struct pqos_capability *cap_smba,
                const struct pqos_cpuinfo *cpu,
                const struct pqos_devinfo *dev);

/**
 * @brief Checks the allocation options selected via selfn_xxxx() functions
 *
 * The --alloc-domain-id, --alloc-mem-regions, --alloc-opt-bw, --alloc-min-bw
 * and --alloc-max-bw options only say where and how a class definition given
 * with -e applies. Without -e nothing reads them, and the utility fell through
 * to monitoring, so a forgotten or mistyped -e turned an allocation command
 * into a monitoring session that runs until it is interrupted.
 *
 * Called once the command line is known and before the utility does anything
 * with it: nothing is reset, printed or applied, and the library is not yet up,
 * so every such command line is rejected without a side effect. Only the option
 * conflicts that resolve_interface() reports can be printed before this, and
 * those exit where they are found. alloc_apply(), where the options are read,
 * is reached far later: -R has reset the configuration by then, and -s, -d,
 * --print-mem-regions and the dump options have printed and exited without ever
 * getting there.
 *
 * @param [in] class_pending whether a class definition is still to be added,
 *             which -c/--profile does through profile_l3ca_apply() after this
 *             check has run
 *
 * @return Operation status
 * @retval 0 the selected options are usable
 * @retval -1 allocation options were selected without a class
 */
int alloc_check_options(const int class_pending);

/**
 * @brief Names an option through which the command line asks for an allocation
 *
 * Asked before anything is printed or applied, so that a command mode which
 * prints and exits can be refused the allocation it would drop. The name is
 * returned rather than a flag, because the report is about the command line and
 * naming the option that was given is what makes it actionable.
 *
 * -c/--profile-set is not one of them: the profile is turned into class
 * definitions by profile_l3ca_apply(), which runs later, so main.c holds that
 * one and asks about it separately.
 *
 * @return the option as it is written on the command line, or NULL when no
 *         allocation was asked for
 */
const char *alloc_requested_option(void);

#ifdef __cplusplus
}
#endif

#endif /* __ALLOCATION_H__ */
