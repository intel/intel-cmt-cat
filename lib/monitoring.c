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
 */

#include "pqos.h"
#include "cap.h"
#include "monitoring.h"
#include "os_monitoring.h"
#include "hw_monitoring.h"
#include "perf_monitoring.h"
#ifdef __linux__
#include "resctrl.h"
#include "resctrl_monitoring.h"
#endif

#include "types.h"
#include "log.h"

/**
 * ---------------------------------------
 * Local macros
 * ---------------------------------------
 */

/**
 * ---------------------------------------
 * Local data types
 * ---------------------------------------
 */

/**
 * ---------------------------------------
 * Local data structures
 * ---------------------------------------
 */
#ifdef __linux__
static int m_interface = PQOS_INTER_MSR;
#endif
/**
 * ---------------------------------------
 * Local Functions
 * ---------------------------------------
 */

/*
 * =======================================
 * =======================================
 *
 * initialize and shutdown
 *
 * =======================================
 * =======================================
 */

int
pqos_mon_init(const struct pqos_cpuinfo *cpu,
              const struct pqos_cap *cap,
              const struct pqos_config *cfg)
{
        const struct pqos_capability *item = NULL;
        int ret;

        ASSERT(cfg != NULL);
        /**
         * If monitoring capability has been discovered
         * then get max RMID supported by a CPU socket
         * and allocate memory for RMID table
         */
        ret = pqos_cap_get_type(cap, PQOS_CAP_TYPE_MON, &item);
        if (ret != PQOS_RETVAL_OK) {
                ret = PQOS_RETVAL_RESOURCE;
                goto pqos_mon_init_exit;
        }

        ASSERT(item != NULL);

#ifdef __linux__
        if (cfg->interface == PQOS_INTER_OS ||
            cfg->interface == PQOS_INTER_OS_RESCTRL_MON)
                ret = os_mon_init(cpu, cap);
        if (ret != PQOS_RETVAL_OK)
                return ret;
#endif
        if (cfg->interface == PQOS_INTER_MSR)
                ret = hw_mon_init(cpu, cap, cfg);

pqos_mon_init_exit:
#ifdef __linux__
        m_interface = cfg->interface;
#endif
        return ret;
}

int
pqos_mon_fini(void)
{
        int ret = PQOS_RETVAL_OK;

#ifdef __linux__
        if (m_interface == PQOS_INTER_OS ||
            m_interface == PQOS_INTER_OS_RESCTRL_MON)
                ret = os_mon_fini();
        if (m_interface == PQOS_INTER_MSR)
                ret = hw_mon_fini();
#else
        ret = hw_mon_fini();
#endif

        return ret;
}

int
pqos_mon_poll_events(struct pqos_mon_data *group)
{
        unsigned i;
        int ret = PQOS_RETVAL_OK;

        /** List of non virtual events */
        const enum pqos_mon_event mon_event[] = {
            PQOS_MON_EVENT_L3_OCCUP,
            PQOS_MON_EVENT_LMEM_BW,
            PQOS_MON_EVENT_TMEM_BW,
            PQOS_PERF_EVENT_LLC_MISS,
            (enum pqos_mon_event)PQOS_PERF_EVENT_CYCLES,
            (enum pqos_mon_event)PQOS_PERF_EVENT_INSTRUCTIONS};

#ifdef __linux__
        if (group->intl->resctrl.event != 0) {
                ret = resctrl_lock_shared();
                if (ret != PQOS_RETVAL_OK)
                        return ret;
        }
#endif

        for (i = 0; i < DIM(mon_event); i++) {
                enum pqos_mon_event evt = mon_event[i];

                /**
                 * poll hw event
                 */
                if (group->intl->hw.event & evt) {
                        ret = hw_mon_poll(group, evt);
                        if (ret != PQOS_RETVAL_OK)
                                goto poll_events_exit;
                }

#ifdef __linux__
                /**
                 * poll perf event
                 */
                if (group->intl->perf.event & evt) {
                        ret = perf_mon_poll(group, evt);
                        if (ret != PQOS_RETVAL_OK)
                                goto poll_events_exit;
                }

                /**
                 * poll resctrl event
                 */
                if (group->intl->resctrl.event & evt) {
                        ret = resctrl_mon_poll(group, evt);
                        if (ret != PQOS_RETVAL_OK)
                                goto poll_events_exit;
                }
#endif
        }

        /**
         * Calculate values of virtual events
         */
        if (group->event & PQOS_MON_EVENT_RMEM_BW) {
                const struct pqos_cap *cap;
                const struct pqos_monitor *pmon;
                uint64_t max_value = 0;

                group->values.mbm_remote_delta = 0;
                if (group->values.mbm_total_delta >
                    group->values.mbm_local_delta)
                        group->values.mbm_remote_delta =
                            group->values.mbm_total_delta -
                            group->values.mbm_local_delta;

                _pqos_cap_get(&cap, NULL);

                ret = pqos_cap_get_event(cap, PQOS_MON_EVENT_RMEM_BW, &pmon);
                if (ret == PQOS_RETVAL_OK)
                        max_value = 1LLU << pmon->counter_length;

                if (max_value > 0 &&
                    group->values.mbm_local > group->values.mbm_total)
                        group->values.mbm_remote = max_value -
                                                   group->values.mbm_local +
                                                   group->values.mbm_total;
                else
                        group->values.mbm_remote =
                            group->values.mbm_total - group->values.mbm_local;
        }
        if (group->event & PQOS_PERF_EVENT_IPC) {
                if (group->values.ipc_unhalted_delta > 0)
                        group->values.ipc =
                            (double)group->values.ipc_retired_delta /
                            (double)group->values.ipc_unhalted_delta;
                else
                        group->values.ipc = 0;
        }

        if (ret == PQOS_RETVAL_OK)
                group->intl->valid_mbm_read = 1;

poll_events_exit:
#ifdef __linux__
        if (group->intl->resctrl.event != 0)
                resctrl_lock_release();
#endif

        return ret;
}
