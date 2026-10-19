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

#ifndef __TEST_MONITOR_RMID_CUSTOM_H
#define __TEST_MONITOR_RMID_CUSTOM_H

#include "pqos_internal.h"

/* the groups' start, which no case reaches */
int __wrap_pqos_mon_start_cores_ext(const unsigned num_cores,
                                    const unsigned *cores,
                                    const enum pqos_mon_event event,
                                    void *context,
                                    struct pqos_mon_mem_region *mem_region,
                                    struct pqos_mon_data **group,
                                    const struct pqos_mon_options *opt);
int __wrap_pqos_mon_start_channels_ext(const unsigned num_channels,
                                       const pqos_channel_t *channels,
                                       const enum pqos_mon_event event,
                                       void *context,
                                       struct pqos_mon_data **group,
                                       const struct pqos_mon_options *opt);
/* the copies strdup() makes, recorded so that their frees can be counted */
char *__wrap_strdup(const char *s);
char *__real_strdup(const char *s);
/* and free(), which counts a second free of a recorded copy instead of making
 * it */
void __wrap_free(void *ptr);
void __real_free(void *ptr);

#endif /* __TEST_MONITOR_RMID_CUSTOM_H */
