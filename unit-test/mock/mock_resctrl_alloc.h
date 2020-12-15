/*
 * BSD LICENSE
 *
 * Copyright(c) 2020 Intel Corporation. All rights reserved.
 * All rights reserved.
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

#ifndef MOCK_RESCTRL_ALLOC_H_
#define MOCK_RESCTRL_ALLOC_H_

#include "resctrl_alloc.h"

int __wrap_resctrl_alloc_init(const struct pqos_cpuinfo *cpu,
                              const struct pqos_cap *cap);
int __wrap_resctrl_alloc_fini(void);
int __wrap_resctrl_alloc_assoc_set(const unsigned lcore,
                                   const unsigned class_id);
int __wrap_resctrl_alloc_assoc_get(const unsigned lcore, unsigned *class_id);
int __wrap_resctrl_alloc_assoc_set_pid(const pid_t task,
                                       const unsigned class_id);
int __wrap_resctrl_alloc_assoc_get_pid(const pid_t task, unsigned *class_id);
int __wrap_resctrl_alloc_get_unused_group(const unsigned grps_num,
                                          unsigned *group_id);
int __wrap_resctrl_alloc_cpumask_write(const unsigned class_id,
                                       const struct resctrl_cpumask *mask);
int __wrap_resctrl_alloc_cpumask_read(const unsigned class_id,
                                      struct resctrl_cpumask *mask);
int __wrap_resctrl_alloc_schemata_read(const unsigned class_id,
                                       struct resctrl_schemata *schemata);
int
__wrap_resctrl_alloc_schemata_write(const unsigned class_id,
                                    const unsigned technology,
                                    const struct resctrl_schemata *schemata);
int __wrap_resctrl_alloc_task_write(const unsigned class_id, const pid_t task);
int __wrap_resctrl_alloc_get_num_closids(unsigned *num_closids);

#endif /* MOCK_RESCTRL_ALLOC_H_ */
