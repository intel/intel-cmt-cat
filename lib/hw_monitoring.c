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

#include "hw_monitoring.h"

#include "cap.h"
#include "cpu_registers.h"
#include "cpuinfo.h"
#include "iordt.h"
#include "log.h"
#include "machine.h"
#include "monitoring.h"
#include "perf_monitoring.h"
#include "uncore_monitoring.h"
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

/**
 * ---------------------------------------
 * Local data structures
 * ---------------------------------------
 */
static unsigned m_rmid_max = 0; /**< max RMID */

/** List of non-virtual perf events */
static const enum pqos_mon_event perf_event[] = {
    PQOS_PERF_EVENT_LLC_MISS, PQOS_PERF_EVENT_LLC_REF,
    (enum pqos_mon_event)PQOS_PERF_EVENT_CYCLES,
    (enum pqos_mon_event)PQOS_PERF_EVENT_INSTRUCTIONS};

/**
 * ---------------------------------------
 * Local Functions
 * ---------------------------------------
 */

static unsigned get_event_id(const enum pqos_mon_event event);

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
hw_mon_init(const struct pqos_cpuinfo *cpu, const struct pqos_cap *cap)
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
                goto hw_mon_init_exit;
        }
        LOG_DEBUG("Max RMID per monitoring cluster is %u\n", m_rmid_max);

#ifdef __linux__
        ret = perf_mon_init(cpu, cap);
        if (ret != PQOS_RETVAL_RESOURCE && ret != PQOS_RETVAL_OK)
                goto hw_mon_init_exit;
#endif

        ret = uncore_mon_init(cpu, cap);
        /* uncore is not supported */
        if (ret == PQOS_RETVAL_RESOURCE)
                ret = PQOS_RETVAL_OK;
        else if (ret != PQOS_RETVAL_OK)
                goto hw_mon_init_exit;

hw_mon_init_exit:
        if (ret != PQOS_RETVAL_OK)
                hw_mon_fini();

        return ret;
}

int
hw_mon_fini(void)
{
        m_rmid_max = 0;

        uncore_mon_fini();

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

/**
 * @brief Obtain socket number for \a numa
 *
 * @param [in] cpu CPU information structure
 * @param [in] numa NUMA node
 * @param [out] socket obtained socket id
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK success
 */
static int
get_socket(const struct pqos_cpuinfo *cpu, unsigned numa, unsigned *socket)
{
        unsigned i;

        for (i = 0; i < cpu->num_cores; ++i) {
                const struct pqos_coreinfo *coreinfo = &(cpu->cores[i]);

                if (coreinfo->numa == numa) {
                        *socket = coreinfo->socket;
                        return PQOS_RETVAL_OK;
                }
        }

        return PQOS_RETVAL_ERROR;
}

int
hw_mon_assoc_unused(struct pqos_mon_poll_ctx *ctx,
                    const enum pqos_mon_event event,
                    pqos_rmid_t min_rmid,
                    pqos_rmid_t max_rmid,
                    const struct pqos_mon_options *opt)
{
        const struct pqos_cpuinfo *cpu = _pqos_get_cpu();
        const struct pqos_cap *cap = _pqos_get_cap();
        const struct pqos_devinfo *dev = _pqos_get_dev();
        int ret = PQOS_RETVAL_OK;
        unsigned rmid = 0;
        unsigned *core_list = NULL;
        unsigned i, core_count;
        uint8_t *rmid_list = NULL;
        int iordt;

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

        ret = pqos_mon_iordt_enabled(cap, NULL, &iordt);
        if (ret != PQOS_RETVAL_OK)
                goto rmid_alloc_error;

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

                ret = hw_mon_assoc_read(core_list[i], &rmid);
                if (ret != PQOS_RETVAL_OK)
                        goto rmid_alloc_error;
                if (rmid <= max_rmid)
                        rmid_list[rmid] = 1;
        }

        /* mark used RMIDs for channels */
        if (iordt && dev != NULL)
                for (i = 0; i < dev->num_channels; ++i) {
                        const struct pqos_channel *channel = &dev->channels[i];
                        pqos_rmid_t rmid;
                        unsigned socket;
                        unsigned numa;

                        if (!channel->rmid_tagging)
                                continue;

                        ret = iordt_get_numa(dev, channel->channel_id, &numa);
                        if (ret == PQOS_RETVAL_OK)
                                ret = get_socket(cpu, numa, &socket);
                        if (ret == PQOS_RETVAL_OK) {
                                if (socket != ctx->cluster)
                                        continue;
                        } else if (ret != PQOS_RETVAL_RESOURCE)
                                goto rmid_alloc_error;

                        ret = iordt_mon_assoc_read(channel->channel_id, &rmid);
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
hw_mon_assoc_write(const unsigned lcore, const pqos_rmid_t rmid)
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
hw_mon_assoc_read(const unsigned lcore, pqos_rmid_t *rmid)
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
hw_mon_assoc_get_core(const unsigned lcore, pqos_rmid_t *rmid)
{
        int ret = PQOS_RETVAL_OK;
        const struct pqos_cpuinfo *cpu = _pqos_get_cpu();

        if (rmid == NULL)
                return PQOS_RETVAL_PARAM;

        ret = pqos_cpu_check_core(cpu, lcore);
        if (ret != PQOS_RETVAL_OK)
                return PQOS_RETVAL_PARAM;

        ret = hw_mon_assoc_read(lcore, rmid);

        return ret;
}

static int
hw_mon_set_snc_mode(const struct pqos_cpuinfo *cpu, enum pqos_snc_config ns)
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
hw_mon_assoc_get_channel(const pqos_channel_t channel_id, pqos_rmid_t *rmid)
{
        const struct pqos_cap *cap = _pqos_get_cap();
        const struct pqos_devinfo *dev = _pqos_get_dev();
        const struct pqos_channel *channel;
        int ret;
        int supported;
        int enabled;

        if (rmid == NULL)
                return PQOS_RETVAL_PARAM;

        channel = pqos_devinfo_get_channel(dev, channel_id);
        if (channel == NULL)
                return PQOS_RETVAL_PARAM;
        if (!channel->rmid_tagging)
                return PQOS_RETVAL_PARAM;

        ret = pqos_mon_iordt_enabled(cap, &supported, &enabled);
        if (ret != PQOS_RETVAL_OK)
                return ret;
        if (!supported)
                return PQOS_RETVAL_RESOURCE;
        if (!enabled)
                return PQOS_RETVAL_ERROR;

        return iordt_mon_assoc_read(channel_id, rmid);
}

int
hw_mon_reset_iordt(const struct pqos_cpuinfo *cpu, const int enable)
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
                goto hw_mon_reset_iordt_exit;
        }

        for (j = 0; j < sockets_num; j++) {
                uint64_t reg = 0;
                unsigned core = 0;

                ret = pqos_cpu_get_one_core(cpu, sockets[j], &core);
                if (ret != PQOS_RETVAL_OK)
                        goto hw_mon_reset_iordt_exit;

                ret = msr_read(core, PQOS_MSR_L3_IO_QOS_CFG, &reg);
                if (ret != MACHINE_RETVAL_OK) {
                        ret = PQOS_RETVAL_ERROR;
                        goto hw_mon_reset_iordt_exit;
                }

                if (enable)
                        reg |= PQOS_MSR_L3_IO_QOS_MON_EN;
                else
                        reg &= ~PQOS_MSR_L3_IO_QOS_MON_EN;

                ret = msr_write(core, PQOS_MSR_L3_IO_QOS_CFG, reg);
                if (ret != MACHINE_RETVAL_OK) {
                        ret = PQOS_RETVAL_ERROR;
                        goto hw_mon_reset_iordt_exit;
                }
        }

hw_mon_reset_iordt_exit:
        if (sockets != NULL)
                free(sockets);

        return ret;
}

int
hw_mon_reset(const struct pqos_mon_config *cfg)
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
                int retval = hw_mon_assoc_write(cpu->cores[i].lcore, RMID0);

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
                        ret = hw_mon_reset_iordt(cpu, 1);
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
                        ret = hw_mon_reset_iordt(cpu, 0);
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
                                ret = hw_mon_set_snc_mode(cpu, cfg->snc);
                }
        }

        return ret;
}

int
hw_mon_read(const unsigned lcore,
            const pqos_rmid_t rmid,
            const unsigned event,
            uint64_t *value)
{
        int retries = 0, retval = PQOS_RETVAL_ERROR;
        uint64_t val = 0;
        uint64_t val_evtsel = 0;
        int flag_wrt = 1;

        /**
         * Set event selection register (RMID + event id)
         */
        val_evtsel = ((uint64_t)rmid) & PQOS_MSR_MON_EVTSEL_RMID_MASK;
        val_evtsel <<= PQOS_MSR_MON_EVTSEL_RMID_SHIFT;
        val_evtsel |= ((uint64_t)event) & PQOS_MSR_MON_EVTSEL_EVTID_MASK;

        for (retries = 0; retries < 4; retries++) {
                if (flag_wrt) {
                        if (msr_write(lcore, PQOS_MSR_MON_EVTSEL, val_evtsel) !=
                            MACHINE_RETVAL_OK)
                                break;
                }
                if (msr_read(lcore, PQOS_MSR_MON_QMC, &val) !=
                    MACHINE_RETVAL_OK)
                        break;
                if ((val & PQOS_MSR_MON_QMC_ERROR) != 0ULL) {
                        /* Read back IA32_QM_EVTSEL register
                         * to check for content change.
                         */
                        if (msr_read(lcore, PQOS_MSR_MON_EVTSEL, &val) !=
                            MACHINE_RETVAL_OK)
                                break;
                        if (val != val_evtsel) {
                                flag_wrt = 1;
                                continue;
                        }
                }
                if ((val & PQOS_MSR_MON_QMC_UNAVAILABLE) != 0ULL) {
                        /**
                         * Waiting for monitoring data
                         */
                        flag_wrt = 0;
                        continue;
                }
                retval = PQOS_RETVAL_OK;
                break;
        }
        /**
         * Store event value
         */
        if (retval == PQOS_RETVAL_OK)
                *value = (val & PQOS_MSR_MON_QMC_DATA_MASK);
        else
                LOG_WARN("Error reading event %u on core %u (RMID%u)!\n", event,
                         lcore, (unsigned)rmid);

        return retval;
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

int
hw_mon_start_perf(struct pqos_mon_data *group, enum pqos_mon_event event)
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
hw_mon_stop_perf(struct pqos_mon_data *group)
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
hw_mon_start_counter(struct pqos_mon_data *group,
                     enum pqos_mon_event event,
                     const struct pqos_mon_options *opt)
{
        const unsigned num_cores = group->num_cores;
        const struct pqos_cpuinfo *cpu = _pqos_get_cpu();
        const struct pqos_capability *cap_mon =
            _pqos_cap_get_type(PQOS_CAP_TYPE_MON);
        pqos_rmid_t core2rmid[num_cores];
        struct pqos_mon_poll_ctx *ctxs = NULL;
        unsigned num_ctxs = 0;
        unsigned i;
        int ret = PQOS_RETVAL_OK;
        enum pqos_mon_event ctx_event = (enum pqos_mon_event)(
            event & (PQOS_MON_EVENT_L3_OCCUP | PQOS_MON_EVENT_LMEM_BW |
                     PQOS_MON_EVENT_TMEM_BW | PQOS_MON_EVENT_RMEM_BW));
        pqos_rmid_t rmid_min = 1;
        pqos_rmid_t rmid_numa =
            cap_mon->u.mon->max_rmid / cap_mon->u.mon->snc_num;
        pqos_rmid_t rmid_max = rmid_numa - 1;

        ctxs = calloc(num_cores * cap_mon->u.mon->snc_num, sizeof(*ctxs));
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
                unsigned numa = 0;

                ret = pqos_cpu_get_clusterid(cpu, lcore, &cluster);
                if (ret != PQOS_RETVAL_OK) {
                        ret = PQOS_RETVAL_PARAM;
                        goto hw_mon_start_counter_exit;
                }

                /* When SNC in default mode RMID's are assigned on numa node
                 * basis
                 */
                if (cap_mon->u.mon->snc_num > 1 &&
                    cap_mon->u.mon->snc_mode == PQOS_SNC_LOCAL) {
                        ret = pqos_cpu_get_numaid(cpu, lcore, &numa);
                        if (ret != PQOS_RETVAL_OK) {
                                ret = PQOS_RETVAL_PARAM;
                                goto hw_mon_start_counter_exit;
                        }
                        numa = numa % cap_mon->u.mon->snc_num;
                        rmid_min = rmid_numa * numa + 1;
                        rmid_max = rmid_numa * (numa + 1) - 1;
                }

                /* CORES in the same coluster/numa node share RMID */
                for (j = 0; j < num_ctxs; j++)
                        if (ctxs[j].lcore == lcore ||
                            (ctxs[j].cluster == cluster &&
                             ctxs[j].numa == numa)) {
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
                        ctxs[num_ctxs].numa = numa;

                        ret = hw_mon_assoc_unused(&ctxs[num_ctxs], ctx_event,
                                                  rmid_min, rmid_max, opt);
                        if (ret != PQOS_RETVAL_OK)
                                goto hw_mon_start_counter_exit;

                        core2rmid[i] = ctx->rmid;
                        num_ctxs++;

                        /* In shared mode - monitor all numa nodes */
                        if (cap_mon->u.mon->snc_mode == PQOS_SNC_TOTAL) {
                                unsigned c;

                                for (c = 1; c < cap_mon->u.mon->snc_num; ++c) {
                                        ctxs[num_ctxs] = ctxs[num_ctxs - 1];
                                        ctxs[num_ctxs].rmid += rmid_numa;
                                        ctxs[num_ctxs].quiet = 1;
                                        num_ctxs++;
                                }
                        }
                }
        }

        group->intl->hw.ctx = realloc(ctxs, num_ctxs * sizeof(*ctxs));
        if (group->intl->hw.ctx == NULL) {
                ret = PQOS_RETVAL_RESOURCE;
                goto hw_mon_start_counter_exit;
        }
        ctxs = NULL;

        /**
         * Associate requested cores with
         * the allocated RMID
         */
        group->num_cores = num_cores;
        for (i = 0; i < num_cores; i++) {
                pqos_rmid_t rmid = core2rmid[i];

                ret = hw_mon_assoc_write(group->cores[i], rmid);
                if (ret != PQOS_RETVAL_OK)
                        break;
        }

        if (ret == PQOS_RETVAL_OK) {
                group->intl->hw.num_ctx = num_ctxs;
                group->intl->hw.event |= ctx_event;
        } else {
                for (i = 0; i < num_cores; i++)
                        (void)hw_mon_assoc_write(group->cores[i], RMID0);
                if (group->intl->hw.ctx != NULL)
                        free(group->intl->hw.ctx);
        }

hw_mon_start_counter_exit:
        if (ctxs != NULL)
                free(ctxs);

        return ret;
}

/**
 * @brief Validate if event list contains events listed in capabilities
 *
 * @param [in] event combination of monitoring events
 * @param [in] iordt require I/O RDT support
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 */
static int
hw_mon_events_valid(const struct pqos_cap *cap,
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
hw_mon_start_cores(const unsigned num_cores,
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
        ret = hw_mon_events_valid(cap, event, 0);
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

                ret = hw_mon_assoc_read(lcore, &rmid);
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

        /* start perf events */
        retval = hw_mon_start_perf(group, req_events);
        if (retval != PQOS_RETVAL_OK)
                goto pqos_mon_start_error;

        /* start MBM/CMT events */
        retval = hw_mon_start_counter(group, req_events, opt);
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
                hw_mon_stop_perf(group);

                if (group->cores != NULL)
                        free(group->cores);
        }

        return retval;
}

int
hw_mon_start_uncore(const unsigned num_sockets,
                    const unsigned *sockets,
                    const enum pqos_mon_event event,
                    void *context,
                    struct pqos_mon_data *group)
{
        int ret;
        unsigned i;
        const struct pqos_cap *cap = _pqos_get_cap();
        const struct pqos_cpuinfo *cpu = _pqos_get_cpu();
        enum pqos_mon_event started_evts = (enum pqos_mon_event)0;

        ASSERT(group != NULL);
        ASSERT(sockets != NULL);
        ASSERT(num_sockets > 0);
        ASSERT(event > 0);

        ret = hw_mon_events_valid(cap, event, 0);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        /* Check if all requested sockets are valid */
        for (i = 0; i < num_sockets; i++) {
                unsigned lcore;

                ret = pqos_cpu_get_one_core(cpu, sockets[i], &lcore);
                if (ret != PQOS_RETVAL_OK)
                        return PQOS_RETVAL_PARAM;
        }

        /**
         * Fill in the monitoring group structure
         */
        group->event = event;
        group->context = context;
        group->intl->uncore.num_sockets = num_sockets;
        group->intl->uncore.sockets = (unsigned *)malloc(
            sizeof(group->intl->uncore.sockets[0]) * num_sockets);
        if (group->intl->uncore.sockets == NULL)
                return PQOS_RETVAL_RESOURCE;
        for (i = 0; i < num_sockets; i++)
                group->intl->uncore.sockets[i] = sockets[i];

        ret = uncore_mon_start(group, event);
        if (ret != PQOS_RETVAL_OK)
                goto hw_mon_start_uncore__exit;

        started_evts |= group->intl->hw.event;

        /*  Check if all selected events were started */
        if ((group->event & started_evts) != group->event) {
                LOG_ERROR("Failed to start all selected "
                          "HW monitoring events\n");
                ret = PQOS_RETVAL_ERROR;
        }

hw_mon_start_uncore__exit:
        if (ret != PQOS_RETVAL_OK) {
                uncore_mon_stop(group);

                if (group->intl->uncore.sockets != NULL)
                        free(group->intl->uncore.sockets);
        }

        return ret;
}

int
hw_mon_start_channels(const unsigned num_channels,
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
        const struct pqos_cpuinfo *cpu = _pqos_get_cpu();
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
        if (req_events & PQOS_MON_EVENT_RMEM_BW)
                req_events |= (enum pqos_mon_event)(PQOS_MON_EVENT_LMEM_BW |
                                                    PQOS_MON_EVENT_TMEM_BW);

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

        /* Validate if event is listed in capabilities */
        ret = hw_mon_events_valid(cap, event, 1);
        if (ret != PQOS_RETVAL_OK)
                return PQOS_RETVAL_PARAM;

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
                            (unsigned long long)channel_id);
                        return PQOS_RETVAL_RESOURCE;
                }

                ret = iordt_mon_assoc_read(channel_id, &rmid);
                if (ret != PQOS_RETVAL_OK)
                        return PQOS_RETVAL_PARAM;

                if (rmid != RMID0) {
                        /* If not RMID0 then it is already monitored */
                        LOG_INFO("Channel %16llx is already monitored with "
                                 "RMID%u.\n",
                                 (unsigned long long)channel, rmid);
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
                const struct pqos_capability *item = NULL;
                unsigned numa;
                unsigned socket;
                unsigned c;

                if (channel == NULL) {
                        ret = PQOS_RETVAL_PARAM;
                        goto hw_mon_start_channels_exit;
                }

                /* Device assigned to numa */
                ret = iordt_get_numa(dev, channel_id, &numa);
                if (ret != PQOS_RETVAL_OK)
                        goto hw_mon_start_channels_exit;

                /* Obtain socket number */
                ret = get_socket(cpu, numa, &socket);
                if (ret != PQOS_RETVAL_OK)
                        goto hw_mon_start_channels_exit;

                ret = pqos_cap_get_type(cap, PQOS_CAP_TYPE_MON, &item);
                if (ret != PQOS_RETVAL_OK)
                        goto hw_mon_start_channels_exit;

                if (item->u.mon->max_rmid == 0) {
                        ret = PQOS_RETVAL_PARAM;
                        goto hw_mon_start_channels_exit;
                }

                /* check if we can reuse context already exists */
                for (c = 0; c < num_ctx; ++c)
                        if (ctxs[c].cluster == socket &&
                            ctxs[c].rmid <= item->u.mon->max_rmid)
                                ctx = &ctxs[c];
                if (ctx != NULL) {
                        ret = iordt_mon_assoc_write(channel_id, ctx->rmid);
                        if (ret != PQOS_RETVAL_OK)
                                goto hw_mon_start_channels_exit;
                        continue;
                }

                ctx = &ctxs[num_ctx++];
                ctx->cluster = socket;

                ret = pqos_cpu_get_one_core(cpu, ctx->cluster, &ctx->lcore);
                if (ret != PQOS_RETVAL_OK)
                        goto hw_mon_start_channels_exit;

                ret = hw_mon_assoc_unused(ctx, event, 0, item->u.mon->max_rmid,
                                          opt);
                if (ret != PQOS_RETVAL_OK)
                        goto hw_mon_start_channels_exit;

                ret = iordt_mon_assoc_write(channel_id, ctx->rmid);
                if (ret != PQOS_RETVAL_OK)
                        goto hw_mon_start_channels_exit;
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
                        goto hw_mon_start_channels_exit;
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
                goto hw_mon_start_channels_exit;
        }
        for (i = 0; i < group->num_channels; i++)
                group->channels[i] = channels[i];

        group->intl->hw.num_ctx = num_ctx;
        group->intl->hw.ctx = ctxs;
        group->intl->hw.event |= req_events;

hw_mon_start_channels_exit:
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
hw_mon_stop(struct pqos_mon_data *group)
{
        int ret = PQOS_RETVAL_OK;
        int retval = PQOS_RETVAL_OK;
        unsigned i = 0;
        const struct pqos_cpuinfo *cpu = _pqos_get_cpu();

        ASSERT(group != NULL);

        if ((group->num_cores == 0 && group->num_channels == 0 &&
             group->intl->uncore.num_sockets == 0))
                return PQOS_RETVAL_PARAM;
        if (group->num_cores != 0 && group->cores == NULL)
                return PQOS_RETVAL_PARAM;
        if (group->num_channels != 0 && group->channels == NULL)
                return PQOS_RETVAL_PARAM;
        if (group->intl->uncore.num_sockets != 0 &&
            group->intl->uncore.sockets == NULL)
                return PQOS_RETVAL_PARAM;
        if ((group->num_cores > 0 || group->num_channels > 0) &&
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
                ret = hw_mon_assoc_read(lcore, &rmid);
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
                        ret = hw_mon_assoc_write(group->cores[i], RMID0);
                        if (ret != PQOS_RETVAL_OK)
                                retval = PQOS_RETVAL_RESOURCE;
                }

        /* Associate channels from the group back with RMID0 */
        if (group->channels != NULL)
                for (i = 0; i < group->num_channels; ++i) {
                        ret = iordt_mon_assoc_write(group->channels[i], RMID0);
                        if (ret != PQOS_RETVAL_OK)
                                retval = PQOS_RETVAL_RESOURCE;
                }

        /* stop perf counters */
        ret = hw_mon_stop_perf(group);
        if (ret != PQOS_RETVAL_OK)
                retval = ret;

        /* stop uncore counters */
        ret = uncore_mon_stop(group);
        if (ret != PQOS_RETVAL_OK)
                retval = ret;

        /**
         * Free poll contexts, core list and clear the group structure
         */
        if (group->cores != NULL)
                free(group->cores);
        if (group->channels != NULL)
                free(group->channels);
        free(group->intl->hw.ctx);

        return retval;
}

int
hw_mon_read_counter(struct pqos_mon_data *group,
                    const enum pqos_mon_event event)
{
        struct pqos_event_values *pv = &group->values;
        uint64_t value = 0;
        uint64_t max_value = 1LLU << 24;
        const struct pqos_cap *cap = _pqos_get_cap();
        const struct pqos_monitor *pmon;
        unsigned i;
        int ret;

        ASSERT(event == PQOS_MON_EVENT_L3_OCCUP ||
               event == PQOS_MON_EVENT_LMEM_BW ||
               event == PQOS_MON_EVENT_TMEM_BW);

        ret = pqos_cap_get_event(cap, event, &pmon);
        if (ret == PQOS_RETVAL_OK)
                max_value = 1LLU << pmon->counter_length;

        for (i = 0; i < group->intl->hw.num_ctx; i++) {
                uint64_t tmp = 0;
                const unsigned lcore = group->intl->hw.ctx[i].lcore;
                const pqos_rmid_t rmid = group->intl->hw.ctx[i].rmid;
                int retval;

                retval = hw_mon_read(lcore, rmid, get_event_id(event), &tmp);
                if (retval != MACHINE_RETVAL_OK)
                        return PQOS_RETVAL_ERROR;

                value += tmp;

                if (value >= max_value)
                        value -= max_value;
        }

        switch (event) {
        case PQOS_MON_EVENT_L3_OCCUP:
                pv->llc = scale_event(event, value);
                break;
        case PQOS_MON_EVENT_LMEM_BW:
                if (pmon->counter_length == 32 && pv->mbm_local > value)
                        ret = PQOS_RETVAL_OVERFLOW;
                if (group->intl->valid_mbm_read) {
                        pv->mbm_local_delta =
                            get_delta(event, pv->mbm_local, value);
                        pv->mbm_local_delta =
                            scale_event(event, pv->mbm_local_delta);
                } else
                        /* Report zero memory bandwidth with first read */
                        pv->mbm_local_delta = 0;
                pv->mbm_local = value;
                break;
        case PQOS_MON_EVENT_TMEM_BW:
                if (pmon->counter_length == 32 && pv->mbm_local > value)
                        ret = PQOS_RETVAL_OVERFLOW;
                if (group->intl->valid_mbm_read) {
                        pv->mbm_total_delta =
                            get_delta(event, pv->mbm_total, value);
                        pv->mbm_total_delta =
                            scale_event(event, pv->mbm_total_delta);
                } else
                        /* Report zero memory bandwidth with first read */
                        pv->mbm_total_delta = 0;
                pv->mbm_total = value;
                break;
        default:
                return PQOS_RETVAL_PARAM;
        }

        if (ret == PQOS_RETVAL_OVERFLOW) {
                LOG_WARN(
                    "Counter overflow reading event %u on core %u (RMID%u)!\n",
                    event, group->intl->hw.ctx[0].lcore,
                    (unsigned)group->intl->hw.ctx[0].rmid);
                group->intl->valid_mbm_read = 0;
        }

        return ret;
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
hw_mon_read_perf(struct pqos_mon_data *group, const enum pqos_mon_event event)
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

int
hw_mon_poll(struct pqos_mon_data *group, const enum pqos_mon_event event)
{
        int ret = PQOS_RETVAL_OK;

        switch (event) {
        case PQOS_MON_EVENT_L3_OCCUP:
        case PQOS_MON_EVENT_LMEM_BW:
        case PQOS_MON_EVENT_TMEM_BW:
                ret = hw_mon_read_counter(group, event);
                if (ret != PQOS_RETVAL_OK)
                        goto hw_mon_poll__exit;
                break;
        case (enum pqos_mon_event)PQOS_PERF_EVENT_CYCLES:
        case (enum pqos_mon_event)PQOS_PERF_EVENT_INSTRUCTIONS:
        case PQOS_PERF_EVENT_LLC_MISS:
        case PQOS_PERF_EVENT_LLC_REF:
                ret = hw_mon_read_perf(group, event);
                if (ret != PQOS_RETVAL_OK)
                        goto hw_mon_poll__exit;
                break;
        case PQOS_PERF_EVENT_LLC_MISS_PCIE_READ:
        case PQOS_PERF_EVENT_LLC_MISS_PCIE_WRITE:
        case PQOS_PERF_EVENT_LLC_REF_PCIE_READ:
        case PQOS_PERF_EVENT_LLC_REF_PCIE_WRITE:
                ret = uncore_mon_poll(group, event);
                if (ret != PQOS_RETVAL_OK)
                        goto hw_mon_poll__exit;
                break;
        default:
                ret = PQOS_RETVAL_PARAM;
        }

hw_mon_poll__exit:
        return ret;
}
/*
 * =======================================
 * =======================================
 *
 * Small utils
 *
 * =======================================
 * =======================================
 */

/**
 * @brief Maps PQoS API event onto an MSR event id
 *
 * @param [in] event PQoS API event id
 *
 * @return MSR event id
 * @retval 0 if not successful
 */
static unsigned
get_event_id(const enum pqos_mon_event event)
{
        switch (event) {
        case PQOS_MON_EVENT_L3_OCCUP:
                return 1;
                break;
        case PQOS_MON_EVENT_LMEM_BW:
                return 3;
                break;
        case PQOS_MON_EVENT_TMEM_BW:
                return 2;
                break;
        case PQOS_MON_EVENT_RMEM_BW:
        default:
                ASSERT(0); /**< this means bug */
                break;
        }
        return 0;
}
