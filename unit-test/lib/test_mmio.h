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

#ifndef TEST_MMIO_H_
#define TEST_MMIO_H_

#include "pqos.h"

#include <stdint.h>

/* the accessors the test binary wraps, reached as the library has them */
int __real_set_iol3_cbm_clos_v1(const struct pqos_erdt_card *card,
                                unsigned int clos_number,
                                uint64_t value);
int __real_get_iol3_cbm_clos_v1(const struct pqos_erdt_card *card,
                                unsigned int clos_number,
                                unsigned int block_number,
                                uint64_t *value);
int __real_get_mba_optimal_bw_region_clos_v1(const struct pqos_erdt_marc *marc,
                                             int region_num,
                                             unsigned int clos_number,
                                             unsigned int *value);
int __real_set_mba_optimal_bw_region_clos_v1(const struct pqos_erdt_marc *marc,
                                             int region_num,
                                             unsigned int clos_number,
                                             unsigned int value);
int __real_get_mba_min_bw_region_clos_v1(const struct pqos_erdt_marc *marc,
                                         int region_num,
                                         unsigned int clos_number,
                                         unsigned int *value);
int __real_set_mba_min_bw_region_clos_v1(const struct pqos_erdt_marc *marc,
                                         int region_num,
                                         unsigned int clos_number,
                                         unsigned int value);
int __real_get_mba_max_bw_region_clos_v1(const struct pqos_erdt_marc *marc,
                                         int region_num,
                                         unsigned int clos_number,
                                         unsigned int *value);
int __real_set_mba_max_bw_region_clos_v1(const struct pqos_erdt_marc *marc,
                                         int region_num,
                                         unsigned int clos_number,
                                         unsigned int value);

#endif /* TEST_MMIO_H_ */
