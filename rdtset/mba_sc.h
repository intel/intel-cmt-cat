/*
 *   BSD LICENSE
 *
 *   Copyright(c) 2018-2020 Intel Corporation. All rights reserved.
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

#ifndef _MBA_SC_H
#define _MBA_SC_H

#include <unistd.h>
#include "common.h"

#define MBA_SC_SAMPLING_INTERVAL 100 /**< Sampling interval in ms */
#define MBA_SC_DEF_INIT_MBA      100 /**< Default, initial MBA value, 100% */

/**
 * @brief Initializes SW controller module
 *
 * @return status
 * @retval 0 on success
 * @retval negative on error (-errno)
 */
int mba_sc_init(void);

/**
 * @brief Shuts down SW controller module
 */
void mba_sc_fini(void);

/**
 * @brief Reverts MBA configuration and stop monitoring
 */
void mba_sc_exit(void);

/**
 * @brief Checks if MBA SC is configured
 *
 * @param[in] cfg rdtset configuration
 */
int mba_sc_mode(const struct rdtset *cfg);

/**
 * @brief Main loop of SW controller
 *
 * @param[in] pid Child pid to monitor for exit status
 *
 * @return status
 * @retval 0 on success
 * @retval negative on error (-errno)
 */
int mba_sc_main(pid_t pid);

#endif /* #define _MBA_SC_H */
