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
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.O
 *
 */

/**
 * @brief Internal header file to PQoS HW monitoring module
 */
#ifndef __PQOS_HW_MON_H__
#define __PQOS_HW_MON_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "pqos.h"

/**
 * @brief Initializes hardware monitoring sub-module of the library (CMT)
 *
 * @param cpu cpu topology structure
 * @param cap capabilities structure
 * @param cfg library configuration structure
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK success
 */
int hw_mon_init(const struct pqos_cpuinfo *cpu,
                const struct pqos_cap *cap,
                const struct pqos_config *cfg);

/**
 * @brief Shuts down hardware monitoring sub-module of the library
 *
 * @return Operation status
 */
int hw_mon_fini(void);

/**
 * @brief Hardware interface to reset monitoring by binding all cores with RMID0
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 */
int hw_mon_reset(void);

/**
 * @brief Hardware interface to read RMID association of the \a lcore
 *
 * @param [in] lcore CPU logical core id
 * @param [out] rmid place to store resource monitoring id
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 */
int hw_mon_assoc_get(const unsigned lcore, pqos_rmid_t *rmid);

/**
 * @brief Hardware interface to start resource monitoring on selected
 * group of cores
 *
 * The function sets up content of the \a group structure.
 *
 * Note that \a event cannot select PQOS_PERF_EVENT_IPC or
 * PQOS_PERF_EVENT_L3_MISS events without any PQoS event
 * selected at the same time.
 *
 * @param [in] num_cores number of cores in \a cores array
 * @param [in] cores array of logical core id's
 * @param [in] event combination of monitoring events
 * @param [in] context a pointer for application's convenience
 *            (unused by the library)
 * @param [in,out] group a pointer to monitoring structure
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 */
int hw_mon_start(const unsigned num_cores,
                 const unsigned *cores,
                 const enum pqos_mon_event event,
                 void *context,
                 struct pqos_mon_data *group);

/**
 * @brief Hardware interface to stop resource monitoring data for selected
 * monitoring group
 *
 * @param [in] group monitoring context for selected number of cores
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 */
int hw_mon_stop(struct pqos_mon_data *group);

/**
 * @brief Hardware interface poll monitoring data
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
int hw_mon_poll(struct pqos_mon_data *group, const enum pqos_mon_event event);

#ifdef __cplusplus
}
#endif

#endif /* __PQOS_HW_MON_H__ */
