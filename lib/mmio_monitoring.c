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
 * @brief Implementation of HW PQoS monitoring API.
 *
 * CPUID and MSR operations are done on 'local' system.
 *
 */

#include "mmio_monitoring.h"

#include "cap.h"
#include "common_monitoring.h"
#include "cpu_registers.h"
#include "cpuinfo.h"
#include "log.h"
#include "machine.h"
#include "mmio.h"
#include "monitoring.h"
#include "perf_monitoring.h"
#include "utils.h"

#include <dirent.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

/**
 * Special RMID - after reset all cores are associated with it.
 *
 * The assumption is that if core is not assigned to it
 * then it is subject of monitoring activity by a different process.
 */
#define RMID0 (0)

/**
 * ---------------------------------------
 * Local data types
 * ---------------------------------------
 */
static unsigned m_rmid_max = 0; /**< max RMID */

/**
 * ---------------------------------------
 * Local Functions
 * ---------------------------------------
 */

static uint64_t scale_event(const enum pqos_mon_event event,
                            const uint64_t val);

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
mmio_mon_init(const struct pqos_cpuinfo *cpu, const struct pqos_cap *cap)
{
        int ret;
        const struct pqos_capability *item = NULL;

        UNUSED_PARAM(cpu);

        ret = pqos_cap_get_type(cap, PQOS_CAP_TYPE_MON, &item);
        if (ret != PQOS_RETVAL_OK)
                return PQOS_RETVAL_RESOURCE;

        m_rmid_max = item->u.mon->max_rmid;
        if (m_rmid_max == 0) {
                ret = PQOS_RETVAL_PARAM;
                goto mmio_mon_init_exit;
        }
        LOG_DEBUG("Max RMID per monitoring cluster is %u\n", m_rmid_max);

#ifdef __linux__
        ret = perf_mon_init(cpu, cap);
        if (ret == PQOS_RETVAL_RESOURCE)
                ret = PQOS_RETVAL_OK;
#endif

mmio_mon_init_exit:
        if (ret != PQOS_RETVAL_OK)
                mmio_mon_fini();

        return ret;
}

int
mmio_mon_fini(void)
{
        m_rmid_max = 0;

#ifdef __linux__
        perf_mon_fini();
#endif

        return PQOS_RETVAL_OK;
}

/*
 * =======================================
 * =======================================
 *
 * RMID allocation
 *
 * =======================================
 * =======================================
 */

/**
 * Refactor: Connection between max_rmid and event is not clear.
 *
 * @brief Gets max RMID number for given \a event
 *
 * @param [in] cap capabilities structure
 * @param [out] rmid resource monitoring id
 * @param [in] event Monitoring event type
 *
 * @return Operations status
 */
static int
rmid_get_event_max(const struct pqos_cap *cap,
                   pqos_rmid_t *rmid,
                   const enum pqos_mon_event event)
{
        pqos_rmid_t max_rmid;
        const struct pqos_capability *item = NULL;
        const struct pqos_cap_mon *mon = NULL;
        unsigned mask_found = 0;
        unsigned i;
        int ret;

        if (rmid == NULL)
                return PQOS_RETVAL_PARAM;

        /**
         * This is not so straight forward as it appears to be.
         * We first have to figure out max RMID
         * for given event type. In order to do so we need:
         * - go through capabilities structure
         * - find monitoring capability
         * - look for the \a event in the event list
         * - find max RMID matching the \a event
         */
        ret = pqos_cap_get_type(cap, PQOS_CAP_TYPE_MON, &item);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        ASSERT(item != NULL);
        mon = item->u.mon;

        /* Find which events are supported vs requested */
        max_rmid = m_rmid_max;
        for (i = 0; i < mon->num_events; i++)
                if (event & mon->events[i].type) {
                        mask_found |= mon->events[i].type;
                        max_rmid = (max_rmid > mon->events[i].max_rmid)
                                       ? mon->events[i].max_rmid
                                       : max_rmid;
                }

        /**
         * Check if all of the events are supported
         */
        if (event != mask_found || max_rmid == 0)
                return PQOS_RETVAL_ERROR;

        ASSERT(m_rmid_max >= max_rmid);

        *rmid = max_rmid;
        return PQOS_RETVAL_OK;
}

int
mmio_mon_assoc_unused(struct pqos_mon_poll_ctx *ctx,
                      const enum pqos_mon_event event,
                      pqos_rmid_t min_rmid,
                      pqos_rmid_t max_rmid,
                      const struct pqos_mon_options *opt)
{
        const struct pqos_cpuinfo *cpu = _pqos_get_cpu();
        const struct pqos_cap *cap = _pqos_get_cap();
        int ret = PQOS_RETVAL_OK;
        unsigned rmid = 0;
        unsigned *core_list = NULL;
        unsigned i, core_count;
        uint8_t *rmid_list = NULL;

        ASSERT(ctx != NULL);

#ifndef PQOS_RMID_CUSTOM
        UNUSED_PARAM(opt);
#endif
        /* Getting max RMID for given event */
        ret = rmid_get_event_max(cap, &rmid, event);
        if (ret != PQOS_RETVAL_OK)
                return ret;
        if (rmid - 1 < max_rmid)
                max_rmid = rmid - 1;
        if (min_rmid < 1)
                min_rmid = 1;

        /* list of used RMIDs */
        rmid_list = (uint8_t *)calloc(max_rmid + 2, sizeof(rmid_list[0]));
        if (rmid_list == NULL)
                return PQOS_RETVAL_RESOURCE;

        /**
         * Check for free RMID in the cluster by reading current associations.
         */
        core_list = pqos_cpu_get_cores_l3id(cpu, ctx->cluster, &core_count);
        if (core_list == NULL) {
                ret = PQOS_RETVAL_ERROR;
                goto rmid_alloc_error;
        }
        ASSERT(core_count > 0);

        /* Mark RMIDs used for core monitoring */
        for (i = 0; i < core_count; i++) {
                pqos_rmid_t rmid;

                ret = mmio_mon_assoc_read(core_list[i], &rmid);
                if (ret != PQOS_RETVAL_OK)
                        goto rmid_alloc_error;
                if (rmid <= max_rmid)
                        rmid_list[rmid] = 1;
        }

#ifdef PQOS_RMID_CUSTOM
        if (opt->rmid.type == PQOS_RMID_TYPE_MAP) {
                if (opt->rmid.rmid < min_rmid || opt->rmid.rmid > max_rmid) {
                        LOG_ERROR("Custom RMID %u not in range %u-%u\n",
                                  opt->rmid.rmid, min_rmid, max_rmid);
                        ret = PQOS_RETVAL_PARAM;
                        goto rmid_alloc_error;
                }

                if (opt->rmid.rmid > max_rmid ||
                    rmid_list[opt->rmid.rmid] != 0) {
                        LOG_ERROR("Custom RMID %u in use\n", opt->rmid.rmid);
                        ret = PQOS_RETVAL_ERROR;
                        goto rmid_alloc_error;
                }

                ctx->rmid = opt->rmid.rmid;

        } else if (opt->rmid.type == PQOS_RMID_TYPE_DEFAULT) {
#endif
                ret = PQOS_RETVAL_ERROR;
                for (i = min_rmid; i <= max_rmid; i++)
                        if (rmid_list[i] == 0) {
                                ret = PQOS_RETVAL_OK;
                                ctx->rmid = i;
                                break;
                        }
#ifdef PQOS_RMID_CUSTOM
        } else {
                LOG_ERROR("RMID Custom: Unsupported rmid type: %u\n",
                          opt->rmid.type);
        }
#endif

rmid_alloc_error:
        if (rmid_list != NULL)
                free(rmid_list);
        if (core_list != NULL)
                free(core_list);
        return ret;
}

/*
 * =======================================
 * =======================================
 *
 * Monitoring
 *
 * =======================================
 * =======================================
 */

/**
 * Refactor: Make it according to the spec.
 * @brief Scale event values to bytes
 *
 * Retrieve event scale factor and scale value to bytes
 *
 * @param [in] event event scale factor to retrieve
 * @param [in] val value to be scaled
 *
 * @return scaled value
 * @retval value in bytes
 */
static uint64_t
scale_event(const enum pqos_mon_event event, const uint64_t val)
{
        const struct pqos_cap *cap = _pqos_get_cap();
        const struct pqos_capability *cap_mon =
            _pqos_cap_get_type(PQOS_CAP_TYPE_MON);
        const struct pqos_monitor *pmon;
        int ret;

        ASSERT(cap != NULL);
        ASSERT(cap_mon != NULL);

        ret = pqos_cap_get_event(cap, event, &pmon);
        ASSERT(ret == PQOS_RETVAL_OK);
        if (ret != PQOS_RETVAL_OK)
                return val;
        else
                return val * pmon->scale_factor / cap_mon->u.mon->snc_num;
}

int
mmio_mon_assoc_write(const unsigned lcore, const pqos_rmid_t rmid)
{
        return mon_assoc_write(lcore, rmid);
}

int
mmio_mon_assoc_read(const unsigned lcore, pqos_rmid_t *rmid)
{
        return mon_assoc_read(lcore, rmid);
}

int
mmio_mon_assoc_get_core(const unsigned lcore, pqos_rmid_t *rmid)
{
        return mon_assoc_get_core(lcore, rmid);
}

int
mmio_mon_reset(const struct pqos_mon_config *cfg)
{
        int ret = PQOS_RETVAL_OK;
        unsigned i;
        const struct pqos_cpuinfo *cpu = _pqos_get_cpu();
        const struct pqos_cap *cap = _pqos_get_cap();
        const struct pqos_capability *cap_mon;

        UNUSED_PARAM(cfg);

        ret = pqos_cap_get_type(cap, PQOS_CAP_TYPE_MON, &cap_mon);
        if (ret != PQOS_RETVAL_OK) {
                LOG_ERROR("Monitoring not present!\n");
                return ret;
        }

        /* reset core assoc */
        for (i = 0; i < cpu->num_cores; i++) {
                int retval = mmio_mon_assoc_write(cpu->cores[i].lcore, RMID0);

                if (retval != PQOS_RETVAL_OK)
                        ret = retval;
        }

        return ret;
}

/**
 * @brief Gives the difference between two values with regard to the possible
 *        overrun and counter length
 *
 * @param event event counter length to retrieve
 * @param old_value previous value
 * @param new_value current value
 *
 * @return difference between the two values
 */
static uint64_t
get_delta(const enum pqos_mon_event event,
          const uint64_t old_value,
          const uint64_t new_value)
{
        const struct pqos_cap *cap = _pqos_get_cap();
        const struct pqos_monitor *pmon;
        int ret;
        uint64_t max_value = 1LLU << 24;

        ret = pqos_cap_get_event(cap, event, &pmon);
        if (ret == PQOS_RETVAL_OK)
                max_value = 1LLU << pmon->counter_length;

        if (old_value > new_value)
                return (max_value - old_value) + new_value;
        else
                return new_value - old_value;
}

int
mmio_mon_start_perf(struct pqos_mon_data *group, enum pqos_mon_event event)
{
        return mon_start_perf(group, event);
}

int
mmio_mon_stop_perf(struct pqos_mon_data *group)
{
        return mon_stop_perf(group);
}

int
mmio_mon_start_counter(struct pqos_mon_data *group,
                       enum pqos_mon_event event,
                       const struct pqos_mon_options *opt)
{
        const unsigned num_cores = group->num_cores;
        const struct pqos_cpuinfo *cpu = _pqos_get_cpu();
        pqos_rmid_t core2rmid[num_cores];
        const struct pqos_erdt_info *erdt = _pqos_get_erdt();
        const struct pqos_cores_domains *cores_domains =
            _pqos_get_cores_domains();
        struct pqos_mon_poll_ctx *ctxs = NULL;
        unsigned num_ctxs = 0;
        unsigned i;
        int ret = PQOS_RETVAL_OK;
        enum pqos_mon_event ctx_event = (enum pqos_mon_event)(
            event & (PQOS_MON_EVENT_L3_OCCUP | PQOS_MON_EVENT_LMEM_BW |
                     PQOS_MON_EVENT_TMEM_BW | PQOS_MON_EVENT_RMEM_BW));
        pqos_rmid_t rmid_min = 1;

        ctxs = calloc(num_cores, sizeof(*ctxs));
        if (ctxs == NULL)
                return PQOS_RETVAL_RESOURCE;

        /*
         * Initialize poll context table:
         * - get core cluster
         * - allocate RMID
         */
        for (i = 0; i < group->num_cores; i++) {
                const unsigned lcore = group->cores[i];
                unsigned j;
                unsigned cluster = 0;

                ret = pqos_cpu_get_clusterid(cpu, lcore, &cluster);
                if (ret != PQOS_RETVAL_OK) {
                        ret = PQOS_RETVAL_PARAM;
                        goto mmio_mon_start_counter_exit;
                }

                /* CORES in the same cluster/numa node share RMID */
                for (j = 0; j < num_ctxs; j++)
                        if (ctxs[j].lcore == lcore ||
                            ctxs[j].cluster == cluster) {
                                core2rmid[i] = ctxs[j].rmid;
                                break;
                        }

                if (j >= num_ctxs) {
                        struct pqos_mon_poll_ctx *ctx = &ctxs[num_ctxs];

                        /**
                         * New cluster is found
                         * - save cluster id in the table
                         * - allocate RMID for the cluster
                         */
                        ctxs[num_ctxs].lcore = lcore;
                        ctxs[num_ctxs].cluster = cluster;

                        ret = mmio_mon_assoc_unused(
                            &ctxs[num_ctxs], ctx_event, rmid_min,
                            erdt->cpu_agents[cores_domains->domains[lcore]]
                                .rmdd.max_rmids,
                            opt);
                        if (ret != PQOS_RETVAL_OK)
                                goto mmio_mon_start_counter_exit;

                        core2rmid[i] = ctx->rmid;
                        num_ctxs++;
                }
        }

        group->intl->hw.ctx = realloc(ctxs, num_ctxs * sizeof(*ctxs));
        if (group->intl->hw.ctx == NULL) {
                ret = PQOS_RETVAL_RESOURCE;
                goto mmio_mon_start_counter_exit;
        }
        ctxs = NULL;

        /**
         * Associate requested cores with
         * the allocated RMID
         */
        group->num_cores = num_cores;
        for (i = 0; i < num_cores; i++) {
                pqos_rmid_t rmid = core2rmid[i];

                ret = mmio_mon_assoc_write(group->cores[i], rmid);
                if (ret != PQOS_RETVAL_OK)
                        break;
        }

        if (ret == PQOS_RETVAL_OK) {
                group->intl->hw.num_ctx = num_ctxs;
                group->intl->hw.event |= ctx_event;
        } else {
                for (i = 0; i < num_cores; i++)
                        (void)mmio_mon_assoc_write(group->cores[i], RMID0);
                if (group->intl->hw.ctx != NULL)
                        free(group->intl->hw.ctx);
        }

mmio_mon_start_counter_exit:
        if (ctxs != NULL)
                free(ctxs);

        return ret;
}

static int
mmio_mon_events_valid(const struct pqos_cap *cap,
                      const enum pqos_mon_event event,
                      const int iordt)
{
        return mon_events_valid(cap, event, iordt);
}

// Refactor: Adjust to MMIO.
int
mmio_mon_start_cores(const unsigned num_cores,
                     const unsigned *cores,
                     const enum pqos_mon_event event,
                     void *context,
                     struct pqos_mon_mem_region *mem_region,
                     struct pqos_mon_data *group,
                     const struct pqos_mon_options *opt)
{
        unsigned i;
        int ret = PQOS_RETVAL_OK;
        int retval = PQOS_RETVAL_OK;
        const struct pqos_cap *cap = _pqos_get_cap();
        const struct pqos_cpuinfo *cpu = _pqos_get_cpu();
        enum pqos_mon_event req_events;
        enum pqos_mon_event started_evts = (enum pqos_mon_event)0;

        ASSERT(group != NULL);
        ASSERT(cores != NULL);
        ASSERT(num_cores > 0);
        ASSERT(event > 0);

        UNUSED_PARAM(mem_region);

        if (num_cores == 0)
                return PQOS_RETVAL_PARAM;

        req_events = event;

        if (req_events & PQOS_MON_EVENT_RMEM_BW)
                req_events |= (enum pqos_mon_event)(PQOS_MON_EVENT_LMEM_BW |
                                                    PQOS_MON_EVENT_TMEM_BW);
        if (req_events & PQOS_PERF_EVENT_IPC)
                req_events |= (enum pqos_mon_event)(
                    PQOS_PERF_EVENT_CYCLES | PQOS_PERF_EVENT_INSTRUCTIONS);

        /**
         * Validate if event is listed in capabilities
         */
        ret = mmio_mon_events_valid(cap, event, 0);
        if (ret != PQOS_RETVAL_OK)
                return PQOS_RETVAL_PARAM;

        /**
         * Check if all requested cores are valid
         * and not used by other monitoring processes.
         *
         * Check if any of requested cores is already subject to monitoring
         * within this process.
         */
        for (i = 0; i < num_cores; i++) {
                const unsigned lcore = cores[i];
                pqos_rmid_t rmid = RMID0;

                ret = pqos_cpu_check_core(cpu, lcore);
                if (ret != PQOS_RETVAL_OK)
                        return PQOS_RETVAL_PARAM;

                ret = mmio_mon_assoc_read(lcore, &rmid);
                if (ret != PQOS_RETVAL_OK)
                        return PQOS_RETVAL_PARAM;

                if (rmid != RMID0) {
                        LOG_ERROR("Monitoring on core %u is already started\n",
                                  lcore);
                        /* If not RMID0 then it is already monitored */
                        LOG_INFO("Core %u is already monitored with "
                                 "RMID%u.\n",
                                 lcore, rmid);
                        return PQOS_RETVAL_RESOURCE;
                }
        }

        /**
         * Fill in the monitoring group structure
         */
        group->event = event;
        group->context = context;
        group->num_cores = num_cores;
        group->cores = (unsigned *)malloc(sizeof(group->cores[0]) * num_cores);
        if (group->cores == NULL)
                return PQOS_RETVAL_RESOURCE;
        for (i = 0; i < group->num_cores; i++)
                group->cores[i] = cores[i];

        // Fill memory regions information
        memcpy(&group->regions, mem_region, sizeof(struct pqos_mon_mem_region));

        /* start perf events */
        retval = mmio_mon_start_perf(group, req_events);
        if (retval != PQOS_RETVAL_OK)
                goto pqos_mon_start_error;

        /* start MBM/CMT events */
        retval = mmio_mon_start_counter(group, req_events, opt);
        if (retval != PQOS_RETVAL_OK)
                goto pqos_mon_start_error;

        started_evts |= group->intl->perf.event;
        started_evts |= group->intl->hw.event;

        /**
         * All events required by RMEM has been started
         */
        if ((started_evts & PQOS_MON_EVENT_LMEM_BW) &&
            (started_evts & PQOS_MON_EVENT_TMEM_BW)) {
                group->values.mbm_remote = 0;
                started_evts |= (enum pqos_mon_event)PQOS_MON_EVENT_RMEM_BW;
        }

        /**
         * All events required by IPC has been started
         */
        if ((started_evts & PQOS_PERF_EVENT_CYCLES) &&
            (started_evts & PQOS_PERF_EVENT_INSTRUCTIONS)) {
                group->values.ipc = 0;
                started_evts |= (enum pqos_mon_event)PQOS_PERF_EVENT_IPC;
        }

        /*  Check if all selected events were started */
        if ((group->event & started_evts) != group->event) {
                LOG_ERROR("Failed to start all selected "
                          "HW monitoring events\n");
                retval = PQOS_RETVAL_ERROR;
        }

pqos_mon_start_error:
        if (retval != PQOS_RETVAL_OK) {
                mmio_mon_stop_perf(group);

                if (group->cores != NULL)
                        free(group->cores);
        }

        return retval;
}

int
mmio_mon_stop(struct pqos_mon_data *group)
{
        int ret = PQOS_RETVAL_OK;
        int retval = PQOS_RETVAL_OK;
        unsigned i = 0;
        const struct pqos_cpuinfo *cpu = _pqos_get_cpu();

        ASSERT(group != NULL);

        if (group->num_cores == 0)
                return PQOS_RETVAL_PARAM;
        if (group->num_cores != 0 && group->cores == NULL)
                return PQOS_RETVAL_PARAM;
        if ((group->num_cores > 0) &&
            (group->intl->hw.num_ctx == 0 || group->intl->hw.ctx == NULL))
                return PQOS_RETVAL_PARAM;

        for (i = 0; i < group->intl->hw.num_ctx; i++) {
                /**
                 * Validate core list in the group structure is correct
                 */
                const unsigned lcore = group->intl->hw.ctx[i].lcore;
                pqos_rmid_t rmid = RMID0;

                if (!group->num_cores)
                        break;

                ret = pqos_cpu_check_core(cpu, lcore);
                if (ret != PQOS_RETVAL_OK)
                        return PQOS_RETVAL_PARAM;
                ret = mmio_mon_assoc_read(lcore, &rmid);
                if (ret != PQOS_RETVAL_OK)
                        return PQOS_RETVAL_PARAM;
                if (rmid != group->intl->hw.ctx[i].rmid &&
                    !group->intl->hw.ctx[i].quiet)
                        LOG_WARN("Core %u RMID association changed from %u "
                                 "to %u! The core has been hijacked!\n",
                                 lcore, group->intl->hw.ctx[i].rmid, rmid);
        }

        /* Associate cores from the group back with RMID0 */
        if (group->cores != NULL)
                for (i = 0; i < group->num_cores; ++i) {
                        ret = mmio_mon_assoc_write(group->cores[i], RMID0);
                        if (ret != PQOS_RETVAL_OK)
                                retval = PQOS_RETVAL_RESOURCE;
                }

        /* stop perf counters */
        ret = mmio_mon_stop_perf(group);
        if (ret != PQOS_RETVAL_OK)
                retval = ret;

        /**
         * Free poll contexts, core list and clear the group structure
         */
        if (group->cores != NULL)
                free(group->cores);
        // Refactor: If mmio structure will be introcuced then clear it too.
        free(group->intl->hw.ctx);

        return retval;
}

int
mmio_mon_read_counter(struct pqos_mon_data *group,
                      const enum pqos_mon_event event)
{
        const struct pqos_cores_domains *cores_domains =
            _pqos_get_cores_domains();
        const struct pqos_erdt_info *erdt = _pqos_get_erdt();
        struct pqos_event_values *pv = &group->values;
        unsigned i;
        int j;

        ASSERT(event == PQOS_MON_EVENT_L3_OCCUP ||
               event == PQOS_MON_EVENT_LMEM_BW ||
               event == PQOS_MON_EVENT_TMEM_BW);

        if (event == PQOS_MON_EVENT_L3_OCCUP) {
                l3_cmt_rmid_t tmp_rmid_val;
                uint64_t value = 0;

                for (i = 0; i < group->intl->hw.num_ctx; i++) {
                        const unsigned lcore = group->intl->hw.ctx[i].lcore;
                        const pqos_rmid_t rmid = group->intl->hw.ctx[i].rmid;

                        get_l3_cmt_rmid_range_v1(
                            &erdt->cpu_agents[cores_domains->domains[lcore]]
                                 .cmrc,
                            rmid, rmid, &tmp_rmid_val);

                        if (!is_available_l3_cmt_rmid(tmp_rmid_val)) {
                                LOG_ERROR("RMID %u is not available for "
                                          "L3 occupancy monitoring!\n",
                                          rmid);
                                return PQOS_RETVAL_UNAVAILABLE;
                        };

                        value += l3_cmt_rmid_to_uint64(tmp_rmid_val);
                };
                pv->llc = scale_event(event, value);
        } else if (event == PQOS_MON_EVENT_LMEM_BW) {
                l3_mbm_rmid_t tmp_rmid_val[PQOS_MAX_MEM_REGIONS],
                    value[PQOS_MAX_MEM_REGIONS] = {0};
                for (i = 0; i < group->intl->hw.num_ctx; i++) {
                        const unsigned lcore = group->intl->hw.ctx[i].lcore;
                        const pqos_rmid_t rmid = group->intl->hw.ctx[i].rmid;

                        for (j = 0; j < group->regions.num_mem_regions; j++) {
                                unsigned region_number =
                                    group->regions.region_num[j];
                                get_l3_mbm_region_rmid_range_v1(
                                    &erdt->cpu_agents[cores_domains
                                                          ->domains[lcore]]
                                         .mmrc,
                                    region_number, rmid, rmid,
                                    &tmp_rmid_val[j]);

                                if (!is_available_l3_mbm_rmid(
                                        tmp_rmid_val[j])) {
                                        LOG_ERROR(
                                            "RMID %u is not available for "
                                            "L3 memory bandwidth monitoring!\n",
                                            rmid);
                                        return PQOS_RETVAL_UNAVAILABLE;
                                };

                                if (is_overflow_l3_mbm_rmid(tmp_rmid_val[j])) {
                                        LOG_ERROR(
                                            "RMID %u is overflowed for "
                                            "L3 memory bandwidth monitoring!\n",
                                            rmid);
                                        return PQOS_RETVAL_OVERFLOW;
                                };

                                value[j] +=
                                    l3_mbm_rmid_to_uint64(tmp_rmid_val[j]);
                        }
                };

                for (j = 0; j < group->regions.num_mem_regions; j++) {
                        group->region_values.mbm_local[j] =
                            scale_event(event, value[j]);
                        group->region_values.mbm_local_delta[j] = get_delta(
                            event, group->region_values.mbm_local[j], value[j]);
                        group->region_values.mbm_local_delta[j] = scale_event(
                            event, group->region_values.mbm_local_delta[j]);
                };
        } else {
                pv->mbm_total = 0;
        }

        return PQOS_RETVAL_OK;
}

/**
 * Refactor: Make publicly available and move to common code base.
 *
 * @brief Read HW perf counter
 *
 * @param group monitoring structure
 * @param event PQoS event
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 * @retval PQOS_RETVAL_ERROR if error occurs
 */
static int
mmio_mon_read_perf(struct pqos_mon_data *group, const enum pqos_mon_event event)
{
        return mon_read_perf(group, event);
}

int
mmio_mon_poll(struct pqos_mon_data *group, const enum pqos_mon_event event)
{
        int ret = PQOS_RETVAL_OK;

        switch (event) {
        case PQOS_MON_EVENT_L3_OCCUP:
        case PQOS_MON_EVENT_LMEM_BW:
        case PQOS_MON_EVENT_TMEM_BW:
                ret = mmio_mon_read_counter(group, event);
                if (ret != PQOS_RETVAL_OK)
                        goto mmio_mon_poll__exit;
                break;
        case (enum pqos_mon_event)PQOS_PERF_EVENT_CYCLES:
        case (enum pqos_mon_event)PQOS_PERF_EVENT_INSTRUCTIONS:
        case PQOS_PERF_EVENT_LLC_MISS:
        case PQOS_PERF_EVENT_LLC_REF:
                ret = mmio_mon_read_perf(group, event);
                if (ret != PQOS_RETVAL_OK)
                        goto mmio_mon_poll__exit;
                break;
        default:
                ret = PQOS_RETVAL_PARAM;
        }

mmio_mon_poll__exit:
        return ret;
}
