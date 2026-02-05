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
 * @brief Implementation of common PQoS monitoring API.
 *
 * CPUID and MSR operations are done on 'local' system.
 *
 */

#include "common_monitoring.h"

#include "cap.h"
#include "cpu_registers.h"
#include "cpuinfo.h"
#include "iordt.h"
#include "log.h"
#include "machine.h"
#include "monitoring.h"
#include "perf_monitoring.h"
#include "utils.h"

#include <dirent.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

/**
 * ---------------------------------------
 * Local data structures
 * ---------------------------------------
 */

/** List of non-virtual perf events */
static const enum pqos_mon_event perf_event[] = {
    PQOS_PERF_EVENT_LLC_MISS, PQOS_PERF_EVENT_LLC_REF,
    (enum pqos_mon_event)PQOS_PERF_EVENT_CYCLES,
    (enum pqos_mon_event)PQOS_PERF_EVENT_INSTRUCTIONS};

/**
 * @brief Sets up IA32 performance counters for IPC and LLC miss ratio events
 *
 * @param group monitoring data
 * @param event mask of selected monitoring events
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 */
static int
ia32_perf_counter_start(const struct pqos_mon_data *group,
                        const enum pqos_mon_event event)
{
        uint64_t global_ctrl_mask = 0;
        unsigned i;
        const unsigned *cores = group->cores;
        const unsigned num_cores = group->num_cores;

        ASSERT(cores != NULL && num_cores > 0);

        if (!(event & (PQOS_PERF_EVENT_LLC_MISS | PQOS_PERF_EVENT_LLC_REF |
                       PQOS_PERF_EVENT_IPC)))
                return PQOS_RETVAL_OK;

        if (event & PQOS_PERF_EVENT_IPC)
                global_ctrl_mask |= (0x3ULL << 32); /* fixed counters 0&1 */

        if (event & PQOS_PERF_EVENT_LLC_MISS)
                global_ctrl_mask |= 0x1ULL; /* programmable counter 0 */

        if (event & PQOS_PERF_EVENT_LLC_REF)
                global_ctrl_mask |= (0x1ULL << 1); /* programmable counter 1 */

        /**
         * Fixed counters are used for IPC calculations.
         * Programmable counters are used for LLC miss calculations.
         * Let's check if they are in use.
         */
        for (i = 0; i < num_cores; i++) {
                uint64_t global_inuse = 0;
                int ret;

                ret = msr_read(cores[i], IA32_MSR_PERF_GLOBAL_CTRL,
                               &global_inuse);
                if (ret != MACHINE_RETVAL_OK)
                        return PQOS_RETVAL_ERROR;
                if (global_inuse & global_ctrl_mask)
                        LOG_WARN("Hijacking performance counters on core %u\n",
                                 cores[i]);
        }

        /**
         * - Disable counters in global control and
         *   reset counter values to 0.
         * - Program counters for desired events
         * - Enable counters in global control
         */
        for (i = 0; i < num_cores; i++) {
                const uint64_t fixed_ctrl = 0x33ULL; /**< track usr + os */
                int ret;

                ret = msr_write(cores[i], IA32_MSR_PERF_GLOBAL_CTRL, 0);
                if (ret != MACHINE_RETVAL_OK)
                        break;

                if (event & PQOS_PERF_EVENT_IPC) {
                        ret = msr_write(cores[i], IA32_MSR_INST_RETIRED_ANY, 0);
                        if (ret != MACHINE_RETVAL_OK)
                                break;
                        ret = msr_write(cores[i], IA32_MSR_CPU_UNHALTED_THREAD,
                                        0);
                        if (ret != MACHINE_RETVAL_OK)
                                break;
                        ret = msr_write(cores[i], IA32_MSR_FIXED_CTR_CTRL,
                                        fixed_ctrl);
                        if (ret != MACHINE_RETVAL_OK)
                                break;
                }

                if (event & PQOS_PERF_EVENT_LLC_MISS) {
                        const uint64_t evtsel0_miss =
                            IA32_EVENT_LLC_MISS_MASK |
                            (IA32_EVENT_LLC_MISS_UMASK << 8) | (1ULL << 16) |
                            (1ULL << 17) | (1ULL << 22);

                        ret = msr_write(cores[i], IA32_MSR_PMC0, 0);
                        if (ret != MACHINE_RETVAL_OK)
                                break;
                        ret = msr_write(cores[i], IA32_MSR_PERFEVTSEL0,
                                        evtsel0_miss);
                        if (ret != MACHINE_RETVAL_OK)
                                break;
                }

                if (event & PQOS_PERF_EVENT_LLC_REF) {
                        const uint64_t evtsel0_ref =
                            IA32_EVENT_LLC_REF_MASK |
                            (IA32_EVENT_LLC_REF_UMASK << 8) | (1ULL << 16) |
                            (1ULL << 17) | (1ULL << 22);

                        ret = msr_write(cores[i], IA32_MSR_PMC1, 0);
                        if (ret != MACHINE_RETVAL_OK)
                                break;
                        ret = msr_write(cores[i], IA32_MSR_PERFEVTSEL1,
                                        evtsel0_ref);
                        if (ret != MACHINE_RETVAL_OK)
                                break;
                }

                ret = msr_write(cores[i], IA32_MSR_PERF_GLOBAL_CTRL,
                                global_ctrl_mask);
                if (ret != MACHINE_RETVAL_OK)
                        break;
        }

        if (i < num_cores)
                return PQOS_RETVAL_ERROR;

        return PQOS_RETVAL_OK;
}

/**
 * @brief Disables IA32 performance counters
 *
 * @param num_cores number of cores in \a cores table
 * @param cores table with core id's
 * @param event mask of selected monitoring events
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 */
static int
ia32_perf_counter_stop(const unsigned num_cores,
                       const unsigned *cores,
                       const enum pqos_mon_event event)
{
        int retval = PQOS_RETVAL_OK;
        unsigned i;

        ASSERT(cores != NULL && num_cores > 0);

        if (!(event & (PQOS_PERF_EVENT_LLC_MISS | PQOS_PERF_EVENT_LLC_REF |
                       PQOS_PERF_EVENT_IPC)))
                return retval;

        for (i = 0; i < num_cores; i++) {
                int ret = msr_write(cores[i], IA32_MSR_PERF_GLOBAL_CTRL, 0);

                if (ret != MACHINE_RETVAL_OK)
                        retval = PQOS_RETVAL_ERROR;
        }
        return retval;
}

// Helper functions

int
mon_events_valid(const struct pqos_cap *cap,
                 const enum pqos_mon_event event,
                 const int iordt)
{
        unsigned i;

        /**
         * Validate if event is listed in capabilities
         */
        for (i = 0; i < (sizeof(event) * CHAR_BIT); i++) {
                int retval;
                const enum pqos_mon_event evt_mask =
                    (enum pqos_mon_event)(1U << i);
                const struct pqos_monitor *ptr = NULL;

                if (!(evt_mask & event))
                        continue;

                retval = pqos_cap_get_event(cap, evt_mask, &ptr);
                if (retval != PQOS_RETVAL_OK || ptr == NULL)
                        return PQOS_RETVAL_ERROR;
                if (iordt && !ptr->iordt)
                        return PQOS_RETVAL_ERROR;
        }

        return PQOS_RETVAL_OK;
}

int
mon_assoc_write(const unsigned lcore, const pqos_rmid_t rmid)
{
        int ret = 0;
        uint32_t reg = 0;
        uint64_t val = 0;

        reg = PQOS_MSR_ASSOC;
        ret = msr_read(lcore, reg, &val);
        if (ret != MACHINE_RETVAL_OK)
                return PQOS_RETVAL_ERROR;

        val &= PQOS_MSR_ASSOC_QECOS_MASK;
        val |= (uint64_t)(rmid & PQOS_MSR_ASSOC_RMID_MASK);

        ret = msr_write(lcore, reg, val);
        if (ret != MACHINE_RETVAL_OK)
                return PQOS_RETVAL_ERROR;

        return PQOS_RETVAL_OK;
}

int
mon_assoc_read(const unsigned lcore, pqos_rmid_t *rmid)
{
        int ret = 0;
        uint32_t reg = PQOS_MSR_ASSOC;
        uint64_t val = 0;

        ASSERT(rmid != NULL);

        ret = msr_read(lcore, reg, &val);
        if (ret != MACHINE_RETVAL_OK)
                return PQOS_RETVAL_ERROR;

        val &= PQOS_MSR_ASSOC_RMID_MASK;
        *rmid = (pqos_rmid_t)val;

        return PQOS_RETVAL_OK;
}

int
mon_assoc_get_core(const unsigned lcore, pqos_rmid_t *rmid)
{
        int ret = PQOS_RETVAL_OK;
        const struct pqos_cpuinfo *cpu = _pqos_get_cpu();

        if (rmid == NULL)
                return PQOS_RETVAL_PARAM;

        ret = pqos_cpu_check_core(cpu, lcore);
        if (ret != PQOS_RETVAL_OK)
                return PQOS_RETVAL_PARAM;

        ret = mon_assoc_read(lcore, rmid);

        return ret;
}

int
mon_start_perf(struct pqos_mon_data *group, enum pqos_mon_event event)
{
        int ret = PQOS_RETVAL_OK;
        unsigned i;
        enum pqos_mon_event hw_event = (enum pqos_mon_event)0;

        group->intl->perf.ctx =
            malloc(sizeof(group->intl->perf.ctx[0]) * group->num_cores);
        if (group->intl->perf.ctx == NULL) {
                LOG_ERROR("Memory allocation failed\n");
                return PQOS_RETVAL_ERROR;
        }

        for (i = 0; i < DIM(perf_event); i++) {
                enum pqos_mon_event evt = perf_event[i];

                if (event & evt) {
#ifdef __linux__
                        if (perf_mon_is_event_supported(evt)) {
                                ret = perf_mon_start(group, evt);
                                if (ret != PQOS_RETVAL_OK)
                                        return ret;
                                group->intl->perf.event |= evt;
                                continue;
                        }
#endif
                        hw_event |= evt;
                }
        }

        if (!group->intl->perf.event) {
                free(group->intl->perf.ctx);
                group->intl->perf.ctx = NULL;
        }

        /* Start IA32 performance counters */
        if (hw_event) {
                ret = ia32_perf_counter_start(group, hw_event);
                if (ret == PQOS_RETVAL_OK)
                        group->intl->hw.event |= hw_event;
        }

        return ret;
}

int
mon_stop_perf(struct pqos_mon_data *group)
{
        int ret = PQOS_RETVAL_OK;
        unsigned i;
        enum pqos_mon_event hw_event = (enum pqos_mon_event)0;

        for (i = 0; i < DIM(perf_event); i++) {
                enum pqos_mon_event evt = perf_event[i];

#ifdef __linux__
                /* Stop perf event */
                if (group->intl->perf.event & evt) {
                        ret = perf_mon_stop(group, evt);
                        if (ret != PQOS_RETVAL_OK)
                                return ret;
                        continue;
                }
#endif

                if (group->intl->hw.event & evt)
                        hw_event |= evt;
        }

        /* Stop IA32 performance counters */
        if (hw_event) {
                ret = ia32_perf_counter_stop(group->num_cores, group->cores,
                                             group->event);
                if (ret != PQOS_RETVAL_OK)
                        return PQOS_RETVAL_RESOURCE;
        }

        if (group->intl->perf.ctx != NULL) {
                free(group->intl->perf.ctx);
                group->intl->perf.ctx = NULL;
        }

        return ret;
}

int
mon_read_perf(struct pqos_mon_data *group, const enum pqos_mon_event event)
{
        struct pqos_event_values *values = &group->values;
        uint64_t val = 0;
        unsigned n;
        uint64_t reg;
        uint64_t *value;
        uint64_t *delta;

        switch (event) {
        case (enum pqos_mon_event)PQOS_PERF_EVENT_INSTRUCTIONS:
                reg = IA32_MSR_INST_RETIRED_ANY;
                value = &values->ipc_retired;
                delta = &values->ipc_retired_delta;
                break;
        case (enum pqos_mon_event)PQOS_PERF_EVENT_CYCLES:
                reg = IA32_MSR_CPU_UNHALTED_THREAD;
                value = &values->ipc_unhalted;
                delta = &values->ipc_unhalted_delta;
                break;
        case PQOS_PERF_EVENT_LLC_MISS:
                reg = IA32_MSR_PMC0;
                value = &values->llc_misses;
                delta = &values->llc_misses_delta;
                break;
        case PQOS_PERF_EVENT_LLC_REF:
                reg = IA32_MSR_PMC1;
                value = &values->llc_references;
                delta = &values->llc_references_delta;
                break;
        default:
                return PQOS_RETVAL_PARAM;
        }

        /**
         * If multiple cores monitored in one group
         * then we have to accumulate the values in the group.
         */
        for (n = 0; n < group->num_cores; n++) {
                uint64_t tmp = 0;
                int ret = msr_read(group->cores[n], reg, &tmp);

                if (ret != MACHINE_RETVAL_OK)
                        return PQOS_RETVAL_ERROR;
                val += tmp;
        }

        *delta = val - *value;
        *value = val;

        return PQOS_RETVAL_OK;
}

static int
mon_set_snc_mode(const struct pqos_cpuinfo *cpu, enum pqos_snc_config ns)
{
        uint64_t val = 0;
        uint32_t reg = PQOS_MSR_SNC_CFG;
        unsigned i, sock_count, *sockets = NULL;
        int ret;

        if (ns != PQOS_REQUIRE_SNC_TOTAL && ns != PQOS_REQUIRE_SNC_LOCAL)
                return PQOS_RETVAL_PARAM;

        if (ns == PQOS_REQUIRE_SNC_LOCAL)
                LOG_INFO("Turning SNC to local mode ...\n");
        else if (ns == PQOS_REQUIRE_SNC_TOTAL)
                LOG_INFO("Turning SNC to total mode ...\n");

        sockets = pqos_cpu_get_sockets(cpu, &sock_count);
        if (sockets == NULL || sock_count == 0) {
                printf("Error retrieving information for Sockets\n");
                goto return_error;
        }

        for (i = 0; i < sock_count; i++) {
                unsigned lcore;

                ret = pqos_cpu_get_one_core(cpu, sockets[i], &lcore);
                if (ret != PQOS_RETVAL_OK) {
                        printf("Error retrieving lcore for socket %u\n",
                               sockets[i]);
                        goto return_error;
                };

                if (msr_read(lcore, reg, &val) != MACHINE_RETVAL_OK)
                        goto return_error;

                val &= ~1;
                if (ns == PQOS_REQUIRE_SNC_LOCAL)
                        val |= 1;

                if (msr_write(lcore, reg, val) != MACHINE_RETVAL_OK)
                        goto return_error;
        };

        _pqos_cap_mon_snc_change(ns);

        ret = PQOS_RETVAL_OK;
        goto clean_up;
return_error:
        ret = PQOS_RETVAL_ERROR;
clean_up:
        if (sockets)
                free(sockets);

        return ret;
}

int
mon_reset_iordt(const struct pqos_cpuinfo *cpu, const int enable)
{
        unsigned *sockets = NULL;
        unsigned sockets_num;
        unsigned j = 0;
        int ret = PQOS_RETVAL_OK;

        ASSERT(cpu != NULL);

        LOG_INFO("%s I/O RDT monitoring across sockets...\n",
                 (enable) ? "Enabling" : "Disabling");

        sockets = pqos_cpu_get_sockets(cpu, &sockets_num);
        if (sockets == NULL || sockets_num == 0) {
                ret = PQOS_RETVAL_ERROR;
                goto mon_reset_iordt_exit;
        }

        for (j = 0; j < sockets_num; j++) {
                uint64_t reg = 0;
                unsigned core = 0;

                ret = pqos_cpu_get_one_core(cpu, sockets[j], &core);
                if (ret != PQOS_RETVAL_OK)
                        goto mon_reset_iordt_exit;

                ret = msr_read(core, PQOS_MSR_L3_IO_QOS_CFG, &reg);
                if (ret != MACHINE_RETVAL_OK) {
                        ret = PQOS_RETVAL_ERROR;
                        goto mon_reset_iordt_exit;
                }

                if (enable)
                        reg |= PQOS_MSR_L3_IO_QOS_MON_EN;
                else
                        reg &= ~PQOS_MSR_L3_IO_QOS_MON_EN;

                ret = msr_write(core, PQOS_MSR_L3_IO_QOS_CFG, reg);
                if (ret != MACHINE_RETVAL_OK) {
                        ret = PQOS_RETVAL_ERROR;
                        goto mon_reset_iordt_exit;
                }
        }

mon_reset_iordt_exit:
        if (sockets != NULL)
                free(sockets);

        return ret;
}

int
mon_reset(const struct pqos_mon_config *cfg)
{
        int ret = PQOS_RETVAL_OK;
        unsigned i;
        const struct pqos_cpuinfo *cpu = _pqos_get_cpu();
        const struct pqos_cap *cap = _pqos_get_cap();
        const struct pqos_devinfo *dev = _pqos_get_dev();
        const struct pqos_capability *cap_mon;

        ret = pqos_cap_get_type(cap, PQOS_CAP_TYPE_MON, &cap_mon);
        if (ret != PQOS_RETVAL_OK) {
                LOG_ERROR("Monitoring not present!\n");
                return ret;
        }

        if (cfg != NULL && cfg->l3_iordt == PQOS_REQUIRE_IORDT_ON &&
            !cap_mon->u.mon->iordt) {
                LOG_ERROR("I/O RDT monitoring requested but not supported by "
                          "the platform!\n");
                return PQOS_RETVAL_PARAM;
        }

        /* reset core assoc */
        for (i = 0; i < cpu->num_cores; i++) {
                int retval = mon_assoc_write(cpu->cores[i].lcore, RMID0);

                if (retval != PQOS_RETVAL_OK)
                        ret = retval;
        }

        /* reset I/O RDT channel assoc */
        if (cap_mon->u.mon->iordt && cap_mon->u.mon->iordt_on && dev != NULL) {
                int retval = iordt_mon_assoc_reset(dev);

                if (retval != PQOS_RETVAL_OK)
                        ret = retval;
        }

        if (ret != PQOS_RETVAL_OK)
                return ret;

        if (cfg != NULL) {
                if (cfg->l3_iordt == PQOS_REQUIRE_IORDT_ON &&
                    !cap_mon->u.mon->iordt_on) {
                        LOG_INFO("Turning I/O RDT Monitoring ON ...\n");
                        ret = mon_reset_iordt(cpu, 1);
                        if (ret != PQOS_RETVAL_OK) {
                                LOG_ERROR("I/O RDT Monitoring enable error!\n");
                                return ret;
                        }

                        /* reset channel assoc - initialize mmio tables */
                        if (dev != NULL)
                                ret = iordt_mon_assoc_reset(dev);
                }

                if (cfg->l3_iordt == PQOS_REQUIRE_IORDT_OFF &&
                    cap_mon->u.mon->iordt_on) {
                        LOG_INFO("Turning I/O RDT Monitoring OFF ...\n");
                        ret = mon_reset_iordt(cpu, 0);
                        if (ret != PQOS_RETVAL_OK) {
                                LOG_ERROR("I/O RDT Monitoring disable "
                                          "error!\n");
                                return ret;
                        }
                }
                _pqos_cap_mon_iordt_change(cfg->l3_iordt);

                if (cfg->snc != PQOS_REQUIRE_SNC_ANY) {
                        if (cap_mon->u.mon->snc_num == 1) {
                                LOG_ERROR("SNC requested but not supported by "
                                          "the platform!\n");
                                ret = PQOS_RETVAL_PARAM;
                        } else
                                ret = mon_set_snc_mode(cpu, cfg->snc);
                }
        }

        return ret;
}
