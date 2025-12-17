/*
 * BSD LICENSE
 *
 * Copyright(c) 2022-2023 Intel Corporation. All rights reserved.
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
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.O
 *
 */

#ifndef __MONITOR_TEXT_H__
#define __MONITOR_TEXT_H__

#include "pqos.h"

/**
 * @brief Start text output
 *
 * @param fp file descriptor
 */
void monitor_text_begin(FILE * fp,
                        const int num_mem_regions,
                        const int *region_num);

/**
 * @brief Print text header
 *
 * @param fp file descriptor
 * @param [in] timestamp data timestamp
 * @param [in] mem_region memory regions information
 */
void monitor_text_header(FILE *fp,
                         const char *timestamp,
                         const int num_mem_regions,
                         const int *region_num);

/**
 * @brief Print monitoring data in text format
 *
 * @param fp file descriptor
 * @param [in] timestamp data timestamp
 * @param [in] data monitoring data
 */
void monitor_text_row(FILE *fp,
                      const char *timestamp,
                      const struct pqos_mon_data *data);

/**
 * @brief Print region monitoring data in text format
 *
 * @param fp file descriptor
 * @param [in] timestamp data timestamp
 * @param [in] data monitoring data
 */
void monitor_text_region_row(FILE *fp,
                             const char *timestamp,
                             const struct pqos_mon_data *data);

/**
 * @brief Print text footer
 *
 * @param fp file descriptor
 */
void monitor_text_footer(FILE *fp);

/**
 * @brief Finalize text output
 *
 * @param fp file descriptor
 */
void monitor_text_end(FILE *fp);

#endif /* #define __MONITOR_TEXT_H__ */
