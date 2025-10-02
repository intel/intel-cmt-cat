/*
 * BSD LICENSE
 *
 * Copyright(c) 2020-2023 Intel Corporation. All rights reserved.
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
 *
 */

/**
 * @brief Internal header file to PQoS MMIO monitoring module
 */
#ifndef __PQOS_MMIO_MON_H__
#define __PQOS_MMIO_MON_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "erdt.h"
#include "monitoring.h"
#include "pqos.h"
#include "pqos_internal.h"
#include "types.h"

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
PQOS_LOCAL int mmio_mon_init(const struct pqos_cpuinfo *cpu,
                             const struct pqos_cap *cap);

/**
 * @brief Shuts down hardware monitoring sub-module of the library
 *
 * @return Operation status
 */
PQOS_LOCAL int mmio_mon_fini(void);

/**
 * @brief Hardware interface to reset monitoring by binding all cores with RMID0
 *
 * As part of monitoring reset I/O RDT * SNC reconfiguration can be performed.
 * This can be requested via \a cfg.
 *
 * @param [in] cfg requested configuration
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 */
PQOS_LOCAL int mmio_mon_reset(const struct pqos_mon_config *cfg);

/**
 * @brief Writes \a lcore to RMID association
 *
 * This function doesn't acquire API lock
 * and can be used internally when lock is already taken.
 *
 * @param lcore logical core id
 * @param rmid resource monitoring ID
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 */
PQOS_LOCAL int mmio_mon_assoc_write(const unsigned lcore,
                                    const pqos_rmid_t rmid);

/**
 * @brief Reads \a lcore to RMID association
 *
 * @param lcore logical core id
 * @param rmid place to store RMID \a lcore is assigned to
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK success
 * @retval PQOS_RETVAL_ERROR on error
 */
PQOS_LOCAL int mmio_mon_assoc_read(const unsigned lcore, pqos_rmid_t *rmid);

/**
 * @brief Get used RMIDs on ctx->cluster for cores
 *
 * @param [in,out] ctx poll context
 * @param [in] event Monitoring event type
 * @param [in] min_rmid min RMID
 * @param [in] max_rmid max RMID
 *
 * @return Operations status
 */
PQOS_LOCAL int mmio_mon_assoc_unused(struct pqos_mon_poll_ctx *ctx,
                                     const enum pqos_mon_event event,
                                     pqos_rmid_t min_rmid,
                                     pqos_rmid_t max_rmid,
                                     const struct pqos_mon_options *opt);

/**
 * @brief Get used RMIDs on ctx->cluster for I/O RDT channels
 *
 * @param [in,out] ctx poll context
 * @param [in] event Monitoring event type
 * @param [in] min_rmid min RMID
 * @param [in] max_rmid max RMID
 *
 * @return Operations status
 */
PQOS_LOCAL int
mmio_mon_channels_assoc_unused(struct pqos_mon_poll_ctx *ctx,
                               pqos_rmid_t min_rmid,
                               pqos_rmid_t max_rmid,
                               const struct pqos_mon_options *opt);

/**
 * @brief Hardware interface to read RMID association of the \a lcore
 *
 * @param [in] lcore CPU logical core id
 * @param [out] rmid place to store resource monitoring id
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 */
PQOS_LOCAL int mmio_mon_assoc_get_core(const unsigned lcore, pqos_rmid_t *rmid);

/**
 * @brief Start perf monitoring counters
 *
 * @param group monitoring structure
 * @param event PQoS event type
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 */
PQOS_LOCAL int mmio_mon_start_perf(struct pqos_mon_data *group,
                                   enum pqos_mon_event event);

/**
 * @brief Stop perf monitoring counters
 *
 * @param group monitoring structure
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 */
PQOS_LOCAL int mmio_mon_stop_perf(struct pqos_mon_data *group);

/**
 * @brief Start HW monitoring counters
 *
 * @param group monitoring structure
 * @param event PQoS event type
 * @param [in] opt extended options
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 */
PQOS_LOCAL int mmio_mon_start_counter(struct pqos_mon_data *group,
                                      enum pqos_mon_event event,
                                      const struct pqos_mon_options *opt);

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
 * @param [in] opt extended options
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 */
PQOS_LOCAL int mmio_mon_start_cores(const unsigned num_cores,
                                    const unsigned *cores,
                                    const enum pqos_mon_event event,
                                    void *context,
                                    struct pqos_mon_mem_region *mem_region,
                                    struct pqos_mon_data *group,
                                    const struct pqos_mon_options *opt);

/**
 * @brief Hardware interface to start resource monitoring on selected
 * group of channels
 *
 * The function sets up content of the \a group structure.
 *
 * @param [in] num_channels number of channels in \a channels array
 * @param [in] channels array of channel id's
 * @param [in] event combination of monitoring events
 * @param [in] context a pointer for application's convenience
 *            (unused by the library)
 * @param [in,out] group a pointer to monitoring structure
 * @param [in] opt extended options
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 */
PQOS_LOCAL int mmio_mon_start_channels(const unsigned num_channels,
                                       const pqos_channel_t *channels,
                                       const enum pqos_mon_event event,
                                       void *context,
                                       struct pqos_mon_data *group,
                                       const struct pqos_mon_options *opt);

/**
 * @brief Hardware interface to stop resource monitoring data for selected
 * monitoring group
 *
 * @param [in] group monitoring context for selected number of cores
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 */
PQOS_LOCAL int mmio_mon_stop(struct pqos_mon_data *group);

/**
 * @brief Read HW counter
 *
 * Reads counters for all events and stores values
 *
 * @param group monitoring structure
 * @param event PQoS event
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 * @retval PQOS_RETVAL_ERROR if error occurs
 * @retval PQOS_RETVAL_OVERFLOW if overflow occurs
 * @retval PQOS_RETVAL_UNAVAILABLE if data is unavailable
 */
PQOS_LOCAL int mmio_mon_read_counter(struct pqos_mon_data *group,
                                     const enum pqos_mon_event event);

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
PQOS_LOCAL int mmio_mon_poll(struct pqos_mon_data *group,
                             const enum pqos_mon_event event);

#ifdef __cplusplus
}
#endif

#endif /* __PQOS_MMIO_MON_H__ */
