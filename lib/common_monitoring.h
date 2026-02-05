/*
 * BSD LICENSE
 *
 * Copyright(c) 2020-2026 Intel Corporation. All rights reserved.
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
 * @brief Internal header file to PQoS common monitoring module sharing
 * common code between HW and MMIO monitoring
 */
#ifndef __PQOS_CMN_MON_H__
#define __PQOS_CMN_MON_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "monitoring.h"
#include "pqos.h"
#include "pqos_internal.h"
#include "types.h"

/**
 * Special RMID - after reset all cores are associated with it.
 *
 * The assumption is that if core is not assigned to it
 * then it is subject of monitoring activity by a different process.
 */
#define RMID0 (0)

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
PQOS_LOCAL int mon_assoc_write(const unsigned lcore, const pqos_rmid_t rmid);

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
PQOS_LOCAL int mon_assoc_read(const unsigned lcore, pqos_rmid_t *rmid);

/**
 * @brief Interface to read RMID association of the \a lcore
 *
 * @param [in] lcore CPU logical core id
 * @param [out] rmid place to store resource monitoring id
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 */
PQOS_LOCAL int mon_assoc_get_core(const unsigned lcore, pqos_rmid_t *rmid);

/**
 * @brief Start perf monitoring counters
 *
 * @param group monitoring structure
 * @param event PQoS event type
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 */
PQOS_LOCAL int mon_start_perf(struct pqos_mon_data *group,
                              enum pqos_mon_event event);

/**
 * @brief Stop perf monitoring counters
 *
 * @param group monitoring structure
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 */
PQOS_LOCAL int mon_stop_perf(struct pqos_mon_data *group);

/**
 * @brief Validate if event list contains events listed in capabilities
 *
 * @param [in] event combination of monitoring events
 * @param [in] iordt require I/O RDT support.
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 */
int PQOS_LOCAL mon_events_valid(const struct pqos_cap *cap,
                                const enum pqos_mon_event event,
                                const int iordt);

/**
 * @brief Read perf counter
 *
 * @param group monitoring structure
 * @param event PQoS event
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 * @retval PQOS_RETVAL_ERROR if error occurs
 */
PQOS_LOCAL int mon_read_perf(struct pqos_mon_data *group,
                             const enum pqos_mon_event event);
/**
 * @brief Enables or disables I/O RDT monitoring across selected CPU sockets
 *
 * @param [in] cpu CPU information
 * @param [in] enable I/O RDT enable/disable flag, 1 - enable, 0 - disable
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 * @retval PQOS_RETVAL_ERROR on failure, MSR read/write error
 */
PQOS_LOCAL int mon_reset_iordt(const struct pqos_cpuinfo *cpu,
                               const int enable);

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
PQOS_LOCAL int mon_reset(const struct pqos_mon_config *cfg);

#ifdef __cplusplus
}
#endif

#endif /* __PQOS_CMN_MON_H__ */
