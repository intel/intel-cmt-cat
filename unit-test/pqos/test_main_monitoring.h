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

#ifndef __TEST_MAIN_MONITORING_H
#define __TEST_MAIN_MONITORING_H

#include "pqos.h"

/* What main() becomes when main.c is included by the test, declared before its
 * definition is read so that it does not look like one without a prototype
 */
int appmain(int argc, char **argv);

/* What main() asks of the library on its way to the monitoring, answered here
 * through --wrap: a system of two cores whose one capability is monitoring
 */
int __wrap_pqos_init(const struct pqos_config *config);
int __wrap_pqos_fini(void);
int __wrap_pqos_inter_get(enum pqos_interface *interface);
int __wrap_pqos_sysconfig_get(const struct pqos_sysconfig **sysconf);
int __wrap_pqos_cap_get_type(const struct pqos_cap *cap,
                             const enum pqos_cap_type type,
                             const struct pqos_capability **cap_item);

/* The allocation it applies first, nothing here, and the monitoring itself,
 * whose loop returns what a case says
 */
int __wrap_alloc_apply(const struct pqos_capability *cap_l3ca,
                       const struct pqos_capability *cap_l2ca,
                       const struct pqos_capability *cap_mba,
                       const struct pqos_capability *cap_smba,
                       const struct pqos_cpuinfo *cpu,
                       const struct pqos_devinfo *dev);
int __wrap_monitor_setup(const struct pqos_cpuinfo *cpu_info,
                         const struct pqos_capability *const cap_mon,
                         const struct pqos_devinfo *dev_info);
int __wrap_monitor_loop(void);
void __wrap_monitor_stop(void);

#endif /* __TEST_MAIN_MONITORING_H */
