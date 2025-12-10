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
#include "iordt.h"
#include "log.h"
#include "machine.h"
#include "mmio.h"
#include "mmio_common.h"
#include "monitoring.h"
#include "perf_monitoring.h"
#include "utils.h"

#include <dirent.h>
#include <inttypes.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

/**
 * ---------------------------------------
 * Local data types
 * ---------------------------------------
 */
static unsigned m_rmid_max = 0; /**< max RMID */

struct rmid_list_t {
        uint16_t domain_id;
        pqos_rmid_t *rmids;
};

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

/**
 * @brief Provides Channel's corresponding Domain ID and index of dev_agents
 * array of structure
 *
 * @param [in] channel channel ID
 * @param [out] domain_id Channel's corresponding Domain ID
 * @param [out] domain_id_idx Channel's corresponding index of dev_agents index
 * of dev_agents
 *
 * @return Operations status
 */
static int
get_dev_domain_info(pqos_channel_t channel,
                    uint16_t *domain_id,
                    uint16_t *domain_id_idx)
{
        unsigned int idx = 0;
        const struct pqos_channels_domains *channels_domains =
            _pqos_get_channels_domains();

        for (idx = 0; idx < channels_domains->num_channel_ids; idx++)
                if (channel == channels_domains->channel_ids[idx]) {
                        if (domain_id != NULL)
                                *domain_id = channels_domains->domain_ids[idx];
                        if (domain_id_idx != NULL)
                                *domain_id_idx =
                                    channels_domains->domain_id_idxs[idx];

                        /* Channel ID is available in IRDT & ERDT */
                        return PQOS_RETVAL_OK;
                }

        /* Channel ID is available in IRDT. But not in ERDT */
        return PQOS_RETVAL_UNAVAILABLE;
}

static int
get_dev_rmid_list(struct pqos_mon_poll_ctx *ctx,
                  const struct pqos_erdt_info *erdt,
                  const struct pqos_devinfo *dev,
                  struct rmid_list_t *dev_rmid_list,
                  pqos_rmid_t max_rmid)
{
        int ret = PQOS_RETVAL_OK;
        unsigned i = 0;

        for (i = 0; i < dev->num_channels; ++i) {
                const struct pqos_channel *channel = &dev->channels[i];
                pqos_rmid_t rmid;
                uint16_t domain_id = USHRT_MAX;
                uint16_t domain_id_idx = USHRT_MAX;

                if (!channel->rmid_tagging)
                        continue;

                ret = get_dev_domain_info(channel->channel_id, &domain_id,
                                          &domain_id_idx);
                if (ret == PQOS_RETVAL_OK) {
                        if (domain_id != ctx->cluster)
                                continue;
                } else if (ret == PQOS_RETVAL_UNAVAILABLE)
                        continue;
                else
                        break;

                ret = iordt_mon_assoc_read(channel->channel_id, &rmid);
                if (ret != PQOS_RETVAL_OK)
                        break;

                if (rmid <= max_rmid) {
                        if (domain_id_idx >= erdt->num_dev_agents) {
                                LOG_ERROR("Wrong domain_id_idx %d "
                                          "Dev Agents %d\n",
                                          domain_id_idx, erdt->num_dev_agents);
                                return PQOS_RETVAL_UNAVAILABLE;
                        }

                        if (dev_rmid_list[domain_id_idx].domain_id == domain_id)
                                dev_rmid_list[domain_id_idx].rmids[rmid] = 1;
                        else
                                LOG_WARN(
                                    "Wrong Domain ID in dev_rmid_list!. "
                                    "Channel ID %lx rmid %x Domain ID %x "
                                    "Domain ID in struct rmid_list_t %x\n",
                                    channel->channel_id, rmid, domain_id,
                                    dev_rmid_list[domain_id_idx].domain_id);
                }
        }

        return ret;
}

int
mmio_mon_channels_assoc_unused(struct pqos_mon_poll_ctx *ctx,
                               pqos_rmid_t min_rmid,
                               pqos_rmid_t max_rmid,
                               const struct pqos_mon_options *opt)
{
        const struct pqos_cap *cap = _pqos_get_cap();
        const struct pqos_devinfo *dev = _pqos_get_dev();
        const struct pqos_erdt_info *erdt = _pqos_get_erdt();
        int ret = PQOS_RETVAL_OK;
        unsigned i;
        struct rmid_list_t *dev_rmid_list = NULL;
        int iordt;
        unsigned idx;
        uint16_t domain_id_idx = USHRT_MAX;

        ASSERT(ctx != NULL);

#ifndef PQOS_RMID_CUSTOM
        UNUSED_PARAM(opt);
#endif

        if (max_rmid == 0) {
                LOG_ERROR("Maximum RMID is 0!\n");
                return PQOS_RETVAL_ERROR;
        }
        if (min_rmid < 1)
                min_rmid = 1;

        ret = pqos_mon_iordt_enabled(cap, NULL, &iordt);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        dev_rmid_list = (struct rmid_list_t *)calloc(
            erdt->num_dev_agents, sizeof(struct rmid_list_t));
        if (dev_rmid_list == NULL)
                return PQOS_RETVAL_RESOURCE;

        /* Initialize device domain_ids */
        for (idx = 0; idx < erdt->num_dev_agents; idx++) {
                dev_rmid_list[idx].domain_id =
                    erdt->dev_agents[idx].rmdd.domain_id;

                dev_rmid_list[idx].rmids = (pqos_rmid_t *)calloc(
                    erdt->dev_agents[idx].rmdd.max_rmids, sizeof(pqos_rmid_t));
                if (dev_rmid_list[idx].rmids == NULL) {
                        LOG_ERROR("Unable to allocate memory for rmids in "
                                  "dev_rmid_list idx %d\n",
                                  idx);
                        ret = PQOS_RETVAL_RESOURCE;
                        goto rmid_alloc_error;
                }
        }

        /* Mark used RMIDs for channels */
        if (iordt && dev != NULL) {
                ret =
                    get_dev_rmid_list(ctx, erdt, dev, dev_rmid_list, max_rmid);
                if (ret != PQOS_RETVAL_OK)
                        goto rmid_alloc_error;
        }

        domain_id_idx = USHRT_MAX;
        for (idx = 0; idx < erdt->num_dev_agents; idx++)
                if (ctx->cluster == dev_rmid_list[idx].domain_id)
                        domain_id_idx = idx;

        if (domain_id_idx == USHRT_MAX) {
                LOG_ERROR("Unable to find Domain ID in rmid_list\n");
                ret = PQOS_RETVAL_ERROR;
                goto rmid_alloc_error;
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
                    dev_rmid_list[domain_id_idx].rmids[opt->rmid.rmid] != 0) {
                        LOG_ERROR("Custom RMID %u in use\n", opt->rmid.rmid);
                        ret = PQOS_RETVAL_ERROR;
                        goto rmid_alloc_error;
                }

                ctx->rmid = opt->rmid.rmid;

        } else if (opt->rmid.type == PQOS_RMID_TYPE_DEFAULT) {
#endif
                ret = PQOS_RETVAL_ERROR;
                for (i = min_rmid; i <= max_rmid; i++)
                        if (dev_rmid_list[domain_id_idx].rmids[i] == 0) {
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
        if (dev_rmid_list != NULL) {
                for (idx = 0; idx < erdt->num_dev_agents; idx++)
                        if (dev_rmid_list[idx].rmids != NULL)
                                free(dev_rmid_list[idx].rmids);
                free(dev_rmid_list);
        }
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
        return mon_reset(cfg);
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
            event & (PQOS_MON_EVENT_L3_OCCUP | PQOS_MON_EVENT_TMEM_BW));
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
mmio_mon_start_channels(const unsigned num_channels,
                        const pqos_channel_t *channels,
                        const enum pqos_mon_event event,
                        void *context,
                        struct pqos_mon_data *group,
                        const struct pqos_mon_options *opt)
{
        int ret = PQOS_RETVAL_OK;
        unsigned i;
        const struct pqos_devinfo *dev = _pqos_get_dev();
        const struct pqos_cap *cap = _pqos_get_cap();
        const struct pqos_erdt_info *erdt = _pqos_get_erdt();
        struct pqos_mon_poll_ctx *ctxs = NULL;
        unsigned num_ctx = 0;
        int enabled;
        int supported;
        enum pqos_mon_event req_events;

        ASSERT(group != NULL);
        ASSERT(channels != NULL);
        ASSERT(num_channels > 0);
        ASSERT(event > 0);
        ASSERT(opt != NULL);

#ifdef PQOS_RMID_CUSTOM
        if (opt->rmid.type != PQOS_RMID_TYPE_DEFAULT &&
            opt->rmid.type != PQOS_RMID_TYPE_MAP)
                return PQOS_RETVAL_PARAM;
#endif

        req_events = event;
        switch (req_events) {
        case PQOS_MON_EVENT_RMEM_BW:
                LOG_ERROR("I/O RDT MBR is not supported in MMIO interface!."
                          "Use io-llc/iot/iom events\n");
                return PQOS_RETVAL_RESOURCE;
        case PQOS_MON_EVENT_LMEM_BW:
                LOG_ERROR("I/O RDT MBL is not supported in MMIO interface!."
                          "Use io-llc/iot/iom events\n");
                return PQOS_RETVAL_RESOURCE;
        case PQOS_MON_EVENT_TMEM_BW:
                LOG_ERROR("I/O RDT MBT is not supported in MMIO interface!."
                          "Use io-llc/iot/iom events\n");
                return PQOS_RETVAL_RESOURCE;
        case PQOS_MON_EVENT_L3_OCCUP:
                LOG_ERROR("I/O RDT LLC is not supported in MMIO interface!."
                          "Use io-llc/iot/iom events\n");
                return PQOS_RETVAL_RESOURCE;
        default:
                break;
        }

        /* Check for I/O RDT support */
        ret = pqos_mon_iordt_enabled(cap, &supported, &enabled);
        if (ret != PQOS_RETVAL_OK)
                return ret;
        if (!supported) {
                LOG_ERROR("I/O RDT monitoring is not supported!\n");
                return PQOS_RETVAL_RESOURCE;
        } else if (!enabled) {
                LOG_ERROR("I/O RDT monitoring is disabled!\n");
                return PQOS_RETVAL_ERROR;
        }

        /* Check if all requested channels are valid */
        for (i = 0; i < num_channels; ++i) {
                const pqos_channel_t channel_id = channels[i];
                const struct pqos_channel *channel;
                pqos_rmid_t rmid;

                channel = pqos_devinfo_get_channel(dev, channel_id);
                if (channel == NULL)
                        return PQOS_RETVAL_PARAM;
                if (!channel->rmid_tagging) {
                        LOG_ERROR(
                            "Channel %016llx does not support monitoring\n",
                            (pqos_channel_t)channel_id);
                        return PQOS_RETVAL_RESOURCE;
                }

                ret = iordt_mon_assoc_read(channel_id, &rmid);
                if (ret != PQOS_RETVAL_OK)
                        return PQOS_RETVAL_PARAM;

                if (rmid != RMID0) {
                        /* If not RMID0 then it is already monitored */
                        LOG_INFO("Channel %016llx is already monitored with "
                                 "RMID%u.\n",
                                 (pqos_channel_t)channel_id, rmid);
                        return PQOS_RETVAL_RESOURCE;
                }
        }

        ctxs = (struct pqos_mon_poll_ctx *)calloc(num_channels, sizeof(*ctxs));
        if (ctxs == NULL)
                return PQOS_RETVAL_RESOURCE;

        for (i = 0; i < num_channels; ++i) {
                const pqos_channel_t channel_id = channels[i];
                const struct pqos_channel *channel =
                    pqos_devinfo_get_channel(dev, channel_id);
                struct pqos_mon_poll_ctx *ctx = NULL;
                pqos_rmid_t max_rmid = 0;
                uint16_t domain_id;
                unsigned c;
                unsigned idx = 0;

                if (channel == NULL) {
                        ret = PQOS_RETVAL_PARAM;
                        goto mmio_mon_start_channels_exit;
                }

                /* Obtain domain number */
                ret = get_dev_domain_info(channel_id, &domain_id, NULL);
                if (ret != PQOS_RETVAL_OK)
                        goto mmio_mon_start_channels_exit;

                for (idx = 0; idx < erdt->num_dev_agents; idx++)
                        if (erdt->dev_agents[idx].rmdd.domain_id == domain_id) {
                                max_rmid = erdt->dev_agents[idx].rmdd.max_rmids;
                                break;
                        }

                /* check if we can reuse context already exists */
                for (c = 0; c < num_ctx; ++c)
                        if (ctxs[c].cluster == domain_id &&
                            ctxs[c].rmid <= max_rmid)
                                ctx = &ctxs[c];
                if (ctx != NULL) {
                        ret = iordt_mon_assoc_write(channel_id, ctx->rmid);
                        if (ret != PQOS_RETVAL_OK)
                                goto mmio_mon_start_channels_exit;
                        continue;
                }

                ctx = &ctxs[num_ctx++];
                ctx->cluster = domain_id;
                ctx->channel_id = channel_id;

                ret = mmio_mon_channels_assoc_unused(ctx, 1, max_rmid, opt);
                if (ret != PQOS_RETVAL_OK)
                        goto mmio_mon_start_channels_exit;

                ret = iordt_mon_assoc_write(channel_id, ctx->rmid);
                if (ret != PQOS_RETVAL_OK)
                        goto mmio_mon_start_channels_exit;
        }

        /* shrink memory used by ctx */
        /* ignore scan-build false positives due to realloc use */
#ifndef __clang_analyzer__
        {
                struct pqos_mon_poll_ctx *ctxs_ptr =
                    (struct pqos_mon_poll_ctx *)realloc(
                        ctxs, num_ctx * sizeof(*ctxs));

                if (ctxs_ptr == NULL) {
                        ret = PQOS_RETVAL_ERROR;
                        goto mmio_mon_start_channels_exit;
                }

                ctxs = ctxs_ptr;
        }
#endif
        /* Fill in the monitoring group structure */
        group->event = event;
        group->context = context;
        group->num_channels = num_channels;
        group->channels =
            (pqos_channel_t *)malloc(sizeof(group->channels[0]) * num_channels);
        if (group->channels == NULL) {
                ret = PQOS_RETVAL_RESOURCE;
                goto mmio_mon_start_channels_exit;
        }

        for (i = 0; i < group->num_channels; i++)
                group->channels[i] = channels[i];

        group->intl->hw.num_ctx = num_ctx;
        group->intl->hw.ctx = ctxs;
        group->intl->hw.event |= req_events;

mmio_mon_start_channels_exit:
        if (ret != PQOS_RETVAL_OK) {
                if (ctxs != NULL) {
                        /* Assign channels back to RMID0 */
                        for (i = 0; i < num_channels; ++i) {
                                const pqos_channel_t channel_id = channels[i];

                                iordt_mon_assoc_write(channel_id, RMID0);
                        }
                        free(ctxs);
                }
                if (group->channels != NULL)
                        free(group->channels);
        }

        return ret;
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
        struct pqos_region_aware_event_values *region_values =
            &group->region_values;
        unsigned i;
        int j;
        uint64_t value = 0;
        uint64_t tmp_rmid_val = 0;
        l3_mbm_rmid_t values[PQOS_MAX_MEM_REGIONS] = {0};
        l3_mbm_rmid_t tmp_values[PQOS_MAX_MEM_REGIONS] = {0};
        uint16_t domain_id_idx = USHRT_MAX;
        int ret = PQOS_RETVAL_OK;

        ASSERT(event == PQOS_MON_EVENT_L3_OCCUP ||
               event == PQOS_MON_EVENT_TMEM_BW ||
               event == PQOS_MON_EVENT_IO_L3_OCCUP ||
               event == PQOS_MON_EVENT_IO_TOTAL_MEM_BW ||
               event == PQOS_MON_EVENT_IO_MISS_MEM_BW);

        switch (event) {
        case PQOS_MON_EVENT_L3_OCCUP:
                pv->llc = 0;
                for (i = 0; i < group->intl->hw.num_ctx; i++) {
                        const unsigned lcore = group->intl->hw.ctx[i].lcore;
                        const pqos_rmid_t rmid = group->intl->hw.ctx[i].rmid;
                        const struct pqos_erdt_cmrc *cmrc =
                            &erdt->cpu_agents[cores_domains->domains[lcore]]
                                 .cmrc;

                        get_l3_cmt_rmid_range_v1(cmrc, rmid, rmid,
                                                 &tmp_rmid_val);

                        if (!is_available_l3_cmt_rmid(tmp_rmid_val)) {
                                LOG_ERROR("RMID %u is not available for "
                                          "L3 occupancy monitoring!\n",
                                          rmid);
                                return PQOS_RETVAL_UNAVAILABLE;
                        };

                        LOG_INFO("core: %u, rmid: %u, value:%#" PRIx64 "\n",
                                 lcore, rmid,
                                 l3_cmt_rmid_to_uint64(tmp_rmid_val));

                        pv->llc += scale_llc_value(
                            cmrc, l3_cmt_rmid_to_uint64(tmp_rmid_val));
                };
                break;
        case PQOS_MON_EVENT_TMEM_BW:
                for (i = 0; i < group->intl->hw.num_ctx; i++) {
                        const unsigned lcore = group->intl->hw.ctx[i].lcore;
                        const pqos_rmid_t rmid = group->intl->hw.ctx[i].rmid;
                        const struct pqos_erdt_mmrc *mmrc =
                            &erdt->cpu_agents[cores_domains->domains[lcore]]
                                 .mmrc;

                        for (j = 0; j < group->regions.num_mem_regions; j++) {
                                unsigned region_number =
                                    group->regions.region_num[j];
                                get_l3_mbm_region_rmid_range_v1(
                                    mmrc, region_number, rmid, rmid,
                                    &tmp_values[j]);

                                if (!is_available_l3_mbm_rmid(tmp_values[j])) {
                                        LOG_ERROR(
                                            "RMID %u is not available for "
                                            "L3 memory bandwidth monitoring!\n",
                                            rmid);
                                        return PQOS_RETVAL_UNAVAILABLE;
                                };

                                if (is_overflow_l3_mbm_rmid(tmp_values[j])) {
                                        LOG_ERROR(
                                            "RMID %u is overflowed for "
                                            "L3 memory bandwidth monitoring!\n",
                                            rmid);
                                        return PQOS_RETVAL_OVERFLOW;
                                };

                                values[j] += scale_mbm_value(
                                    mmrc, rmid,
                                    l3_mbm_rmid_to_uint64(tmp_values[j]));
                        }
                };

                for (j = 0; j < group->regions.num_mem_regions; j++) {
                        region_values->mbm_total_delta[j] =
                            values[j] - region_values->mbm_total[j];
                        region_values->mbm_total[j] = values[j];
                };

                break;
        case PQOS_MON_EVENT_IO_L3_OCCUP:
                group->region_values.io_llc = 0;
                for (i = 0; i < group->intl->hw.num_ctx; i++) {
                        const pqos_channel_t channel_id =
                            group->intl->hw.ctx[i].channel_id;
                        const pqos_rmid_t rmid = group->intl->hw.ctx[i].rmid;
                        struct pqos_erdt_cmrd *cmrd;

                        ret = get_dev_domain_info(channel_id, NULL,
                                                  &domain_id_idx);
                        if (ret != PQOS_RETVAL_OK) {
                                LOG_ERROR("Unable to find Domain ID for Channel"
                                          " %016llx\n",
                                          channel_id);
                                return PQOS_RETVAL_UNAVAILABLE;
                        }

                        if (domain_id_idx >= erdt->num_dev_agents) {
                                LOG_ERROR("Wrong domain_id_idx %d "
                                          "Dev Agents %d\n",
                                          domain_id_idx, erdt->num_dev_agents);
                                return PQOS_RETVAL_UNAVAILABLE;
                        }

                        cmrd = &erdt->dev_agents[domain_id_idx].cmrd;

                        get_iol3_cmt_rmid_range_v1(cmrd, rmid, rmid,
                                                   &tmp_rmid_val);

                        if (!is_available_iol3_cmt_rmid(tmp_rmid_val)) {
                                LOG_ERROR("RMID %u is not available for "
                                          "IO L3 occupancy monitoring!\n",
                                          rmid);
                                return PQOS_RETVAL_UNAVAILABLE;
                        };

                        group->region_values.io_llc += scale_io_llc_value(
                            cmrd, iol3_cmt_rmid_to_uint64(tmp_rmid_val));
                };
                break;
        case PQOS_MON_EVENT_IO_TOTAL_MEM_BW:
                for (i = 0; i < group->intl->hw.num_ctx; i++) {
                        const pqos_channel_t channel_id =
                            group->intl->hw.ctx[i].channel_id;
                        const pqos_rmid_t rmid = group->intl->hw.ctx[i].rmid;
                        struct pqos_erdt_ibrd *ibrd;

                        ret = get_dev_domain_info(channel_id, NULL,
                                                  &domain_id_idx);
                        if (ret != PQOS_RETVAL_OK) {
                                LOG_ERROR("Unable to find Domain ID for Channel"
                                          " %016llx\n",
                                          channel_id);
                                return PQOS_RETVAL_UNAVAILABLE;
                        }

                        if (domain_id_idx >= erdt->num_dev_agents) {
                                LOG_ERROR("Wrong domain_id_idx %d "
                                          "Dev Agents %d\n",
                                          domain_id_idx, erdt->num_dev_agents);
                                return PQOS_RETVAL_UNAVAILABLE;
                        }

                        ibrd = &erdt->dev_agents[domain_id_idx].ibrd;

                        get_total_iol3_mbm_rmid_range_v1(ibrd, rmid, rmid,
                                                         &tmp_rmid_val);

                        if (!is_available_iol3_mbm_rmid(tmp_rmid_val)) {
                                LOG_ERROR("RMID %u is not available for "
                                          "IO L3 total monitoring!\n",
                                          rmid);
                                return PQOS_RETVAL_UNAVAILABLE;
                        };

                        value += scale_io_mbm_value(
                            ibrd, rmid, iol3_mbm_rmid_to_uint64(tmp_rmid_val));
                };

                group->region_values.io_total_delta =
                    value - group->region_values.io_total;
                group->region_values.io_total = value;

                break;
        case PQOS_MON_EVENT_IO_MISS_MEM_BW:
                for (i = 0; i < group->intl->hw.num_ctx; i++) {
                        const pqos_channel_t channel_id =
                            group->intl->hw.ctx[i].channel_id;
                        const pqos_rmid_t rmid = group->intl->hw.ctx[i].rmid;
                        const struct pqos_erdt_ibrd *ibrd;

                        ret = get_dev_domain_info(channel_id, NULL,
                                                  &domain_id_idx);
                        if (ret != PQOS_RETVAL_OK) {
                                LOG_ERROR("Unable to find Domain ID for Channel"
                                          " %016llx\n",
                                          channel_id);
                                return PQOS_RETVAL_UNAVAILABLE;
                        }

                        if (domain_id_idx >= erdt->num_dev_agents) {
                                LOG_ERROR("Wrong domain_id_idx %d "
                                          "Dev Agents %d\n",
                                          domain_id_idx, erdt->num_dev_agents);
                                return PQOS_RETVAL_UNAVAILABLE;
                        }

                        ibrd = &erdt->dev_agents[domain_id_idx].ibrd;

                        get_miss_iol3_mbm_rmid_range_v1(ibrd, rmid, rmid,
                                                        &tmp_rmid_val);

                        if (!is_available_iol3_mbm_rmid(tmp_rmid_val)) {
                                LOG_ERROR("RMID %u is not available for "
                                          "IO L3 miss monitoring!\n",
                                          rmid);
                                return PQOS_RETVAL_UNAVAILABLE;
                        };

                        value += scale_io_mbm_value(
                            ibrd, rmid, iol3_mbm_rmid_to_uint64(tmp_rmid_val));
                };
                group->region_values.io_miss_delta =
                    value - group->region_values.io_miss;
                group->region_values.io_miss = value;
                break;
        default:
                pv->mbm_total = 0;
                break;
        }

        return PQOS_RETVAL_OK;
}

/**
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
        case PQOS_MON_EVENT_TMEM_BW:
        case PQOS_MON_EVENT_IO_L3_OCCUP:
        case PQOS_MON_EVENT_IO_TOTAL_MEM_BW:
        case PQOS_MON_EVENT_IO_MISS_MEM_BW:
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
