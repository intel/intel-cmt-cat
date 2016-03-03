/*
 * BSD LICENSE
 *
 * Copyright(c) 2014-2016 Intel Corporation. All rights reserved.
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
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.O
 *
 */

/**
 * @brief CPU sockets and cores enumeration module.
 */

#ifndef __PQOS_CPUINFO_H__
#define __PQOS_CPUINFO_H__

#ifdef __cplusplus
extern "C" {
#endif

struct cpuinfo_core {
        unsigned lcore;                 /**< logical core id */
        unsigned socket;                /**< socket id in the system */
        unsigned cluster;               /**< cluster id in the system */
};

struct cpuinfo_topology {
        unsigned num_cores;             /**< number of cores in the system */
        struct cpuinfo_core cores[];
};

#define CPUINFO_RETVAL_OK    0          /**< all good */
#define CPUINFO_RETVAL_ERROR 1          /**< generic error */
#define CPUINFO_RETVAL_PARAM 2          /**< parameter error */

/**
 * @brief Initializes CPU information module
 *
 * CPU topology detection method is OS dependant.
 *
 * Passing \a topology is optional and it can be NULL.
 * After successful init cpuinfo_get() can be used
 * anytime to retrieve detected topology.
 *
 * @param [out] topology place to store pointer to CPU topology data
 *
 * @return Operation status
 * @retval CPUINFO_RETVAL_OK on success
 */
int cpuinfo_init(const struct cpuinfo_topology **topology);

/**
 * @brief Shuts down CPU information module
 *
 * @return Operation status
 * @retval CPUINFO_RETVAL_OK on success
 */
int cpuinfo_fini(void);

/**
 * @brief Provides system CPU information
 *
 * @param [out] topology place to store pointer to CPU topology data
 *
 * @return Operation status
 * @retval CPUINFO_RETVAL_OK on success
 */
int cpuinfo_get(const struct cpuinfo_topology **topology);

#ifdef __cplusplus
}
#endif

#endif /* __PQOS_CPUINFO_H__ */
