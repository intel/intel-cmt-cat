/*
 * BSD LICENSE
 *
 * Copyright(c) 2018-2020 Intel Corporation. All rights reserved.
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
 * @brief Internal header file for perf monitoring functions
 */

#ifndef __PQOS_PERF_MON_H__
#define __PQOS_PERF_MON_H__

#ifdef __cplusplus
extern "C" {
#endif

#define PERF_MON_PATH "/sys/devices/intel_cqm/"

/**
 * Local monitor event types
 */
enum perf_mon_event {
        PQOS_PERF_EVENT_INSTRUCTIONS = 0x1000, /**< Retired CPU Instructions */
        PQOS_PERF_EVENT_CYCLES = 0x2000,       /**< Unhalted CPU Clock Cycles */
};

/**
 * @brief Initializes Perf structures used for OS monitoring interface
 *
 * @param cpu cpu topology structure
 * @param cap capabilities structure
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK success
 */
int perf_mon_init(const struct pqos_cpuinfo *cpu, const struct pqos_cap *cap);

/**
 * @brief Shuts down monitoring sub-module for perf monitoring
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK success
 */
int perf_mon_fini(void);

/**
 * @brief This function starts Perf pqos event counters
 *
 * Used to start pqos counters and request file
 * descriptors used to read the counters
 *
 * @param group monitoring structure
 * @param event PQoS event type
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 */
int perf_mon_start(struct pqos_mon_data *group,
                   const enum pqos_mon_event event);

/**
 * @brief Function to stop Perf event counters
 *
 * @param group monitoring structure
 * @param event PQoS event type
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 */
int perf_mon_stop(struct pqos_mon_data *group, const enum pqos_mon_event event);

/**
 * @brief This function polls all perf counters
 *
 * Reads counters for all events and stores values
 *
 * @param group monitoring structure
 * @param event PQoS event type
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 * @retval PQOS_RETVAL_ERROR if error occurs
 */
int perf_mon_poll(struct pqos_mon_data *group, const enum pqos_mon_event event);

/**
 * @brief Check if event is supported by perf
 *
 * @param event PQoS event to check
 *
 * @retval 0 if not supported
 */
int perf_mon_is_event_supported(const enum pqos_mon_event event);

#ifdef __cplusplus
}
#endif

#endif /* __PQOS_PERF_MON_H__ */
