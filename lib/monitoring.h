/*
 * BSD LICENSE
 *
 * Copyright(c) 2014-2020 Intel Corporation. All rights reserved.
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
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/**
 * @brief Internal header file to PQoS monitoring initialization
 */

#ifndef __PQOS_HOSTMON_H__
#define __PQOS_HOSTMON_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "pqos.h"

/**
 * Core monitoring poll context
 */
struct pqos_mon_poll_ctx {
        unsigned lcore;
        unsigned cluster;
        pqos_rmid_t rmid;
};

/**
 * Perf monitoring poll context
 */
struct pqos_mon_perf_ctx {
        int fd_llc;
        int fd_mbl;
        int fd_mbt;
        int fd_inst;
        int fd_cyc;
        int fd_llc_misses;
};

/**
 * Internal monitoring group data structure
 */
struct pqos_mon_data_internal {
        /**
         * Perf specific section
         */
        struct {
                enum pqos_mon_event event;     /**< Started perf events */
                struct pqos_mon_perf_ctx *ctx; /**< Perf poll context for each
                                                  core/tid */
        } perf;

        /**
         * Resctrl specific section
         */
        struct {
                enum pqos_mon_event event; /**< Started resctrl events */
                char *mon_group;
                struct pqos_event_values values_storage; /**< stores values
                                                            of monitoring group
                                                            that was moved to
                                                            another COS */
                unsigned *l3id;    /**< list of l3ids being monitored */
                unsigned num_l3id; /**< Number of l3ids */

        } resctrl;

        /**
         * Hw specific section
         */
        struct {
                enum pqos_mon_event event;     /**< Started hw events */
                struct pqos_mon_poll_ctx *ctx; /**< core, cluster & RMID */
                unsigned num_ctx;              /**< number of poll contexts */
        } hw;

        int valid_mbm_read; /**< flag to discard 1st invalid read */
};

/**
 * @brief Initializes monitoring sub-module of the library (CMT)
 *
 * @param cpu cpu topology structure
 * @param cap capabilities structure
 * @param cfg library configuration structure
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK success
 */
int pqos_mon_init(const struct pqos_cpuinfo *cpu,
                  const struct pqos_cap *cap,
                  const struct pqos_config *cfg);

/**
 * @brief Shuts down monitoring sub-module of the library
 *
 * @return Operation status
 */
int pqos_mon_fini(void);

/**
 * @brief Poll monitoring data from requested groups
 *
 * @param group monitoring group pointer to be be updated
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 */
int pqos_mon_poll_events(struct pqos_mon_data *group);

#ifdef __cplusplus
}
#endif

#endif /* __PQOS_HOSTMON_H__ */
