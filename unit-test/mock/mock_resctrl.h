/*
 * BSD LICENSE
 *
 * Copyright(c) 2020-2023 Intel Corporation. All rights reserved.
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

#ifndef MOCK_RESCTRL_H_
#define MOCK_RESCTRL_H_

#include "pqos.h"
#include "resctrl.h"
#include "resctrl_schemata.h"

int __wrap_resctrl_cpumask_write(FILE *fd, const struct resctrl_cpumask *mask);
int __wrap_resctrl_cpumask_read(FILE *fd, struct resctrl_cpumask *mask);
void __wrap_resctrl_cpumask_set(const unsigned lcore,
                                struct resctrl_cpumask *mask);
void __wrap_resctrl_cpumask_unset(const unsigned lcore,
                                  struct resctrl_cpumask *mask);
int __wrap_resctrl_cpumask_get(const unsigned lcore,
                               const struct resctrl_cpumask *mask);
int __wrap_resctrl_mount(const enum pqos_cdp_config l3_cdp_cfg,
                         const enum pqos_cdp_config l2_cdp_cfg,
                         const enum pqos_mba_config mba_cfg);
int __wrap_resctrl_umount(void);
int __wrap_resctrl_lock_shared(void);
int __wrap_resctrl_lock_exclusive(void);
int __wrap_resctrl_lock_release(void);
int __wrap_resctrl_is_supported(void);

#endif /* MOCK_RESCTRL_H_ */
