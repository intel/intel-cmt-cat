/*
 * BSD LICENSE
 *
 * Copyright(c) 2022-2026 Intel Corporation. All rights reserved.
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

#ifndef TEST_CAP_H_
#define TEST_CAP_H_

#include <aio.h>
#include <stddef.h>

void *__real_malloc(size_t size);
char *__real_getenv(const char *name);

struct pqos_cap;
struct pqos_cpuinfo;
struct pqos_erdt_info;
struct pqos_cores_domains;
struct pqos_channels_domains;
struct pqos_devinfo;
struct pqos_mrrm_info;
struct pqos_mem_regions;

/* test_cap wraps these, and calls the originals outside its MMIO case */
int __real_erdt_init(const struct pqos_cap *cap,
                     struct pqos_cpuinfo *cpu,
                     struct pqos_erdt_info **erdt_info);
void __real_erdt_fini(void);
int __real_cores_domains_init(unsigned int num_cores,
                              const struct pqos_erdt_info *erdt,
                              struct pqos_cores_domains **cores_domains);
void __real_cores_domains_fini(void);
int
__real_channels_domains_init(unsigned int num_channels,
                             const struct pqos_erdt_info *erdt,
                             const struct pqos_devinfo *devinfo,
                             struct pqos_channels_domains **channels_domains);
int __real_mrrm_init(const struct pqos_cap *cap, struct pqos_mrrm_info **mrrm);
int __real_mem_regions_init(const struct pqos_mrrm_info *mrrm,
                            struct pqos_mem_regions **regions);

#endif /* TEST_CAP_H_ */
