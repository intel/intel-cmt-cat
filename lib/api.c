/*
 * BSD LICENSE
 *
 * Copyright(c) 2017-2022 Intel Corporation. All rights reserved.
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

#include "api.h"

#include "allocation.h"
#include "cap.h"
#include "cpuinfo.h"
#include "hw_monitoring.h"
#include "lock.h"
#include "log.h"
#include "monitoring.h"
#include "os_allocation.h"
#include "os_monitoring.h"

#include <stdlib.h>
#include <string.h>

/**
 * Value marking monitoring group structure as "valid".
 * Group becomes "valid" after successful pqos_mon_start() or
 * pqos_mon_start_pid() call.
 */
#define GROUP_VALID_MARKER (0x00DEAD00)

/**
 * PQoS API functions
 */
static struct pqos_api {
        /** Resets monitoring */
        int (*mon_reset)(void);
        /** Reads RMID association lcore */
        int (*mon_assoc_get)(const unsigned lcore, pqos_rmid_t *rmid);
        /** Starts resource monitoring on selected group of cores */
        int (*mon_start)(const unsigned num_cores,
                         const unsigned *cores,
                         const enum pqos_mon_event event,
                         void *context,
                         struct pqos_mon_data *group);
        /** Starts resource monitoring of selected pids */
        int (*mon_start_pids)(const unsigned num_pids,
                              const pid_t *pids,
                              const enum pqos_mon_event event,
                              void *context,
                              struct pqos_mon_data *group);
        /** Adds pids to the resource monitoring grpup */
        int (*mon_add_pids)(const unsigned num_pids,
                            const pid_t *pids,
                            struct pqos_mon_data *group);
        /** Remove pids from the resource monitoring group */
        int (*mon_remove_pids)(const unsigned num_pids,
                               const pid_t *pids,
                               struct pqos_mon_data *group);
        /** Starts uncore monitoring */
        int (*mon_start_uncore)(const unsigned num_sockets,
                                const unsigned *sockets,
                                const enum pqos_mon_event event,
                                void *context,
                                struct pqos_mon_data *group);
        /** Stops resource monitoring data for selected monitoring group */
        int (*mon_stop)(struct pqos_mon_data *group);

        /** Associates lcore with given class of service */
        int (*alloc_assoc_set)(const unsigned lcore, const unsigned class_id);
        /** Reads association of lcore with class of service */
        int (*alloc_assoc_get)(const unsigned lcore, unsigned *class_id);
        /** Associate task with given class of service */
        int (*alloc_assoc_set_pid)(const pid_t task, const unsigned class_id);
        /** Read association of task with class of service */
        int (*alloc_assoc_get_pid)(const pid_t task, unsigned *class_id);
        /** Assign first available COS */
        int (*alloc_assign)(const unsigned technology,
                            const unsigned *core_array,
                            const unsigned core_num,
                            unsigned *class_id);
        /** Reassign cores to default COS */
        int (*alloc_release)(const unsigned *core_array,
                             const unsigned core_num);
        /** Assign first available COS to tasks */
        int (*alloc_assign_pid)(const unsigned technology,
                                const pid_t *task_array,
                                const unsigned task_num,
                                unsigned *class_id);
        /** Reassign tasks to default COS */
        int (*alloc_release_pid)(const pid_t *task_array,
                                 const unsigned task_num);
        /** Resets configuration of allocation technologies */
        int (*alloc_reset)(const enum pqos_cdp_config l3_cdp_cfg,
                           const enum pqos_cdp_config l2_cdp_cfg,
                           const enum pqos_mba_config mba_cfg);

        /** Sets L3 classes of service */
        int (*l3ca_set)(const unsigned l3cat_id,
                        const unsigned num_cos,
                        const struct pqos_l3ca *ca);
        /** Reads L3 classes of service */
        int (*l3ca_get)(const unsigned l3cat_id,
                        const unsigned max_num_ca,
                        unsigned *num_ca,
                        struct pqos_l3ca *ca);
        /** Get minimum L3 CBM bits */
        int (*l3ca_get_min_cbm_bits)(unsigned *min_cbm_bits);

        /** Sets L2 classes of service */
        int (*l2ca_set)(const unsigned l2id,
                        const unsigned num_cos,
                        const struct pqos_l2ca *ca);
        /** Reads L2 classes of service */
        int (*l2ca_get)(const unsigned l2id,
                        const unsigned max_num_ca,
                        unsigned *num_ca,
                        struct pqos_l2ca *ca);
        /** Get minimum L2 CBM bits */
        int (*l2ca_get_min_cbm_bits)(unsigned *min_cbm_bits);

        /** Set MBA */
        int (*mba_get)(const unsigned mba_id,
                       const unsigned max_num_cos,
                       unsigned *num_cos,
                       struct pqos_mba *mba_tab);
        /** Get MBA mask */
        int (*mba_set)(const unsigned mba_id,
                       const unsigned num_cos,
                       const struct pqos_mba *requested,
                       struct pqos_mba *actual);

        /** Retrieves tasks associated with COS */
        unsigned *(*pid_get_pid_assoc)(const unsigned class_id,
                                       unsigned *count);

} api;

/*
 * =======================================
 * Init module
 * =======================================
 */
int
api_init(int interface, enum pqos_vendor vendor)
{
        if (interface != PQOS_INTER_MSR && interface != PQOS_INTER_OS &&
            interface != PQOS_INTER_OS_RESCTRL_MON)
                return PQOS_RETVAL_PARAM;

        memset(&api, 0, sizeof(api));

        if (interface == PQOS_INTER_MSR) {
                api.mon_reset = hw_mon_reset;
                api.mon_assoc_get = hw_mon_assoc_get;
                api.mon_start = hw_mon_start;
                api.mon_stop = hw_mon_stop;
                api.alloc_assoc_set = hw_alloc_assoc_set;
                api.alloc_assoc_get = hw_alloc_assoc_get;
                api.alloc_assign = hw_alloc_assign;
                api.alloc_release = hw_alloc_release;
                api.alloc_reset = hw_alloc_reset;
                api.l3ca_set = hw_l3ca_set;
                api.l3ca_get = hw_l3ca_get;
                api.l3ca_get_min_cbm_bits = hw_l3ca_get_min_cbm_bits;
                api.l2ca_set = hw_l2ca_set;
                api.l2ca_get = hw_l2ca_get;
                api.l2ca_get_min_cbm_bits = hw_l2ca_get_min_cbm_bits;
                if (vendor == PQOS_VENDOR_AMD) {
                        api.mba_get = hw_mba_get_amd;
                        api.mba_set = hw_mba_set_amd;
                } else {
                        api.mon_start_uncore = hw_mon_start_uncore;
                        api.mba_get = hw_mba_get;
                        api.mba_set = hw_mba_set;
                }

#ifdef __linux__
        } else if (interface == PQOS_INTER_OS ||
                   interface == PQOS_INTER_OS_RESCTRL_MON) {
                api.mon_reset = os_mon_reset;
                api.mon_start = os_mon_start;
                api.mon_start_pids = os_mon_start_pids;
                api.mon_add_pids = os_mon_add_pids;
                api.mon_remove_pids = os_mon_remove_pids;
                api.mon_stop = os_mon_stop;
                api.alloc_assoc_set = os_alloc_assoc_set;
                api.alloc_assoc_get = os_alloc_assoc_get;
                api.alloc_assoc_set_pid = os_alloc_assoc_set_pid;
                api.alloc_assoc_get_pid = os_alloc_assoc_get_pid;
                api.alloc_assign = os_alloc_assign;
                api.alloc_release = os_alloc_release;
                api.alloc_assign_pid = os_alloc_assign_pid;
                api.alloc_release_pid = os_alloc_release_pid;
                api.alloc_reset = os_alloc_reset;
                api.l3ca_set = os_l3ca_set;
                api.l3ca_get = os_l3ca_get;
                api.l3ca_get_min_cbm_bits = os_l3ca_get_min_cbm_bits;
                api.l2ca_set = os_l2ca_set;
                api.l2ca_get = os_l2ca_get;
                api.l2ca_get_min_cbm_bits = os_l2ca_get_min_cbm_bits;
                if (vendor == PQOS_VENDOR_AMD) {
                        api.mba_get = os_mba_get_amd;
                        api.mba_set = os_mba_set_amd;
                } else {
                        api.mba_get = os_mba_get;
                        api.mba_set = os_mba_set;
                }
                api.pid_get_pid_assoc = os_pid_get_pid_assoc;
#endif
        }

        return PQOS_RETVAL_OK;
}

#define UNSUPPORTED_INTERFACE "Interface not supported!\n"

#define API_CALL(API, PARAMS...)                                               \
        ({                                                                     \
                int ret;                                                       \
                                                                               \
                lock_get();                                                    \
                do {                                                           \
                        ret = _pqos_check_init(1);                             \
                        if (ret != PQOS_RETVAL_OK)                             \
                                break;                                         \
                                                                               \
                        if (api.API != NULL)                                   \
                                ret = api.API(PARAMS);                         \
                        else {                                                 \
                                LOG_INFO(UNSUPPORTED_INTERFACE);               \
                                ret = PQOS_RETVAL_RESOURCE;                    \
                        }                                                      \
                } while (0);                                                   \
                lock_release();                                                \
                                                                               \
                ret;                                                           \
        })

/*
 * =======================================
 * Allocation Technology
 * =======================================
 */
int
pqos_alloc_assoc_set(const unsigned lcore, const unsigned class_id)
{
        return API_CALL(alloc_assoc_set, lcore, class_id);
}

int
pqos_alloc_assoc_get(const unsigned lcore, unsigned *class_id)
{
        if (class_id == NULL)
                return PQOS_RETVAL_PARAM;

        return API_CALL(alloc_assoc_get, lcore, class_id);
}

int
pqos_alloc_assoc_set_pid(const pid_t task, const unsigned class_id)
{
        return API_CALL(alloc_assoc_set_pid, task, class_id);
}

int
pqos_alloc_assoc_get_pid(const pid_t task, unsigned *class_id)
{
        if (class_id == NULL)
                return PQOS_RETVAL_PARAM;

        return API_CALL(alloc_assoc_get_pid, task, class_id);
}

int
pqos_alloc_assign(const unsigned technology,
                  const unsigned *core_array,
                  const unsigned core_num,
                  unsigned *class_id)
{
        const int l2_req = ((technology & (1 << PQOS_CAP_TYPE_L2CA)) != 0);
        const int l3_req = ((technology & (1 << PQOS_CAP_TYPE_L3CA)) != 0);
        const int mba_req = ((technology & (1 << PQOS_CAP_TYPE_MBA)) != 0);

        if (core_num == 0 || core_array == NULL || class_id == NULL ||
            !(l2_req || l3_req || mba_req))
                return PQOS_RETVAL_PARAM;

        return API_CALL(alloc_assign, technology, core_array, core_num,
                        class_id);
}

int
pqos_alloc_release(const unsigned *core_array, const unsigned core_num)
{
        if (core_num == 0 || core_array == NULL)
                return PQOS_RETVAL_PARAM;

        return API_CALL(alloc_release, core_array, core_num);
}

int
pqos_alloc_assign_pid(const unsigned technology,
                      const pid_t *task_array,
                      const unsigned task_num,
                      unsigned *class_id)
{
        if (task_array == NULL || task_num == 0 || class_id == NULL)
                return PQOS_RETVAL_PARAM;

        return API_CALL(alloc_assign_pid, technology, task_array, task_num,
                        class_id);
}

int
pqos_alloc_release_pid(const pid_t *task_array, const unsigned task_num)
{
        if (task_array == NULL || task_num == 0)
                return PQOS_RETVAL_PARAM;

        return API_CALL(alloc_release_pid, task_array, task_num);
}

int
pqos_alloc_reset(const enum pqos_cdp_config l3_cdp_cfg,
                 const enum pqos_cdp_config l2_cdp_cfg,
                 const enum pqos_mba_config mba_cfg)
{
        if (l3_cdp_cfg != PQOS_REQUIRE_CDP_ON &&
            l3_cdp_cfg != PQOS_REQUIRE_CDP_OFF &&
            l3_cdp_cfg != PQOS_REQUIRE_CDP_ANY) {
                LOG_ERROR("Unrecognized L3 CDP configuration setting %d!\n",
                          l3_cdp_cfg);
                return PQOS_RETVAL_PARAM;
        }

        if (l2_cdp_cfg != PQOS_REQUIRE_CDP_ON &&
            l2_cdp_cfg != PQOS_REQUIRE_CDP_OFF &&
            l2_cdp_cfg != PQOS_REQUIRE_CDP_ANY) {
                LOG_ERROR("Unrecognized L2 CDP configuration setting %d!\n",
                          l2_cdp_cfg);
                return PQOS_RETVAL_PARAM;
        }

        if (mba_cfg != PQOS_MBA_ANY && mba_cfg != PQOS_MBA_DEFAULT &&
            mba_cfg != PQOS_MBA_CTRL) {
                LOG_ERROR("Unrecognized MBA configuration setting %d!\n",
                          mba_cfg);
                return PQOS_RETVAL_PARAM;
        }

        return API_CALL(alloc_reset, l3_cdp_cfg, l2_cdp_cfg, mba_cfg);
}

unsigned *
pqos_pid_get_pid_assoc(const unsigned class_id, unsigned *count)
{
        int ret;
        unsigned *tasks = NULL;

        if (count == NULL)
                return NULL;

        lock_get();

        ret = _pqos_check_init(1);
        if (ret != PQOS_RETVAL_OK) {
                lock_release();
                return NULL;
        }

        if (api.pid_get_pid_assoc != NULL) {
                tasks = api.pid_get_pid_assoc(class_id, count);
                if (tasks == NULL)
                        LOG_ERROR("Error retrieving task information!\n");
        } else
                LOG_INFO(UNSUPPORTED_INTERFACE);

        lock_release();

        return tasks;
}

/*
 * =======================================
 * L3 cache allocation
 * =======================================
 */

/**
 * @brief Tests if \a bitmask is contiguous
 *
 * Zero bit mask is regarded as not contiguous.
 *
 * The function shifts out first group of contiguous 1's in the bit mask.
 * Next it checks remaining bitmask content to make a decision.
 *
 * @param bitmask bit mask to be validated for contiguity
 *
 * @return Bit mask contiguity check result
 * @retval 0 not contiguous
 * @retval 1 contiguous
 */
static int
is_contiguous(uint64_t bitmask)
{
        if (bitmask == 0)
                return 0;

        while ((bitmask & 1) == 0) /**< Shift until 1 found at position 0 */
                bitmask >>= 1;

        while ((bitmask & 1) != 0) /**< Shift until 0 found at position 0 */
                bitmask >>= 1;

        return (bitmask) ? 0 : 1; /**< non-zero bitmask is not contiguous */
}

int
pqos_l3ca_set(const unsigned l3cat_id,
              const unsigned num_cos,
              const struct pqos_l3ca *ca)
{
        unsigned i;

        if (ca == NULL || num_cos == 0)
                return PQOS_RETVAL_PARAM;

        /**
         * Check if class bitmasks are contiguous.
         */
        for (i = 0; i < num_cos; i++) {
                int is_contig = 0;

                if (ca[i].cdp) {
                        is_contig = is_contiguous(ca[i].u.s.data_mask) &&
                                    is_contiguous(ca[i].u.s.code_mask);
                } else
                        is_contig = is_contiguous(ca[i].u.ways_mask);

                if (!is_contig) {
                        LOG_ERROR("L3 COS%u bit mask is not contiguous!\n",
                                  ca[i].class_id);
                        return PQOS_RETVAL_PARAM;
                }
        }

        return API_CALL(l3ca_set, l3cat_id, num_cos, ca);
}

int
pqos_l3ca_get(const unsigned l3cat_id,
              const unsigned max_num_ca,
              unsigned *num_ca,
              struct pqos_l3ca *ca)
{
        if (num_ca == NULL || ca == NULL || max_num_ca == 0)
                return PQOS_RETVAL_PARAM;

        return API_CALL(l3ca_get, l3cat_id, max_num_ca, num_ca, ca);
}

int
pqos_l3ca_get_min_cbm_bits(unsigned *min_cbm_bits)
{
        if (min_cbm_bits == NULL)
                return PQOS_RETVAL_PARAM;

        return API_CALL(l3ca_get_min_cbm_bits, min_cbm_bits);
}

/*
 * =======================================
 * L2 cache allocation
 * =======================================
 */

int
pqos_l2ca_set(const unsigned l2id,
              const unsigned num_cos,
              const struct pqos_l2ca *ca)
{
        unsigned i;

        if (ca == NULL || num_cos == 0)
                return PQOS_RETVAL_PARAM;

        /**
         * Check if class bitmasks are contiguous
         */
        for (i = 0; i < num_cos; i++) {
                int is_contig = 0;

                if (ca[i].cdp) {
                        is_contig = is_contiguous(ca[i].u.s.data_mask) &&
                                    is_contiguous(ca[i].u.s.code_mask);
                } else
                        is_contig = is_contiguous(ca[i].u.ways_mask);

                if (!is_contig) {
                        LOG_ERROR("L2 COS%u bit mask is not contiguous!\n",
                                  ca[i].class_id);
                        return PQOS_RETVAL_PARAM;
                }
        }

        return API_CALL(l2ca_set, l2id, num_cos, ca);
}

int
pqos_l2ca_get(const unsigned l2id,
              const unsigned max_num_ca,
              unsigned *num_ca,
              struct pqos_l2ca *ca)
{
        if (num_ca == NULL || ca == NULL || max_num_ca == 0)
                return PQOS_RETVAL_PARAM;

        return API_CALL(l2ca_get, l2id, max_num_ca, num_ca, ca);
}

int
pqos_l2ca_get_min_cbm_bits(unsigned *min_cbm_bits)
{
        if (min_cbm_bits == NULL)
                return PQOS_RETVAL_PARAM;

        return API_CALL(l2ca_get_min_cbm_bits, min_cbm_bits);
}

/*
 * =======================================
 * Memory Bandwidth Allocation
 * =======================================
 */

int
pqos_mba_set(const unsigned mba_id,
             const unsigned num_cos,
             const struct pqos_mba *requested,
             struct pqos_mba *actual)
{
        int ret;
        unsigned i;

        if (requested == NULL || num_cos == 0)
                return PQOS_RETVAL_PARAM;

        lock_get();

        ret = _pqos_check_init(1);
        if (ret != PQOS_RETVAL_OK) {
                lock_release();
                return ret;
        }

        /**
         * Check if MBA rate is within allowed range
         */
        for (i = 0; i < num_cos; i++) {
                const struct cpuinfo_config *vconfig;

                cpuinfo_get_config(&vconfig);
                if (requested[i].ctrl == 0 &&
                    (requested[i].mb_max == 0 ||
                     requested[i].mb_max > vconfig->mba_max)) {
                        LOG_ERROR("MBA COS%u rate out of range (from 1-%d)!\n",
                                  requested[i].class_id, vconfig->mba_max);
                        lock_release();
                        return PQOS_RETVAL_PARAM;
                }
        }

        if (api.mba_set != NULL)
                ret = api.mba_set(mba_id, num_cos, requested, actual);
        else {
                LOG_INFO(UNSUPPORTED_INTERFACE);
                ret = PQOS_RETVAL_RESOURCE;
        }

        lock_release();

        return ret;
}

int
pqos_mba_get(const unsigned mba_id,
             const unsigned max_num_cos,
             unsigned *num_cos,
             struct pqos_mba *mba_tab)
{
        if (num_cos == NULL || mba_tab == NULL || max_num_cos == 0)
                return PQOS_RETVAL_PARAM;

        return API_CALL(mba_get, mba_id, max_num_cos, num_cos, mba_tab);
}

/*
 * =======================================
 * Monitoring
 * =======================================
 */

int
pqos_mon_reset(void)
{
        return API_CALL(mon_reset);
}

int
pqos_mon_assoc_get(const unsigned lcore, pqos_rmid_t *rmid)
{
        if (rmid == NULL)
                return PQOS_RETVAL_PARAM;

        return API_CALL(mon_assoc_get, lcore, rmid);
}

int
pqos_mon_start(const unsigned num_cores,
               const unsigned *cores,
               const enum pqos_mon_event event,
               void *context,
               struct pqos_mon_data *group)
{
        int ret;

        if (group == NULL || cores == NULL || num_cores == 0 || event == 0)
                return PQOS_RETVAL_PARAM;

        if (group->valid == GROUP_VALID_MARKER)
                return PQOS_RETVAL_PARAM;

        /**
         * Validate event parameter
         * - only combinations of events allowed
         * - do not allow non-PQoS events to be monitored on its own
         */
        if (event & (~(PQOS_MON_EVENT_L3_OCCUP | PQOS_MON_EVENT_LMEM_BW |
                       PQOS_MON_EVENT_TMEM_BW | PQOS_MON_EVENT_RMEM_BW |
                       PQOS_PERF_EVENT_IPC | PQOS_PERF_EVENT_LLC_MISS |
                       PQOS_PERF_EVENT_LLC_REF)))
                return PQOS_RETVAL_PARAM;

        if ((event & (PQOS_MON_EVENT_L3_OCCUP | PQOS_MON_EVENT_LMEM_BW |
                      PQOS_MON_EVENT_TMEM_BW | PQOS_MON_EVENT_RMEM_BW)) == 0 &&
            (event & (PQOS_PERF_EVENT_IPC | PQOS_PERF_EVENT_LLC_MISS |
                      PQOS_PERF_EVENT_LLC_REF)) != 0) {
                LOG_ERROR("Only PMU events selected for monitoring\n");
                return PQOS_RETVAL_PARAM;
        }

        lock_get();

        ret = _pqos_check_init(1);
        if (ret != PQOS_RETVAL_OK) {
                lock_release();
                return ret;
        }

        memset(group, 0, sizeof(*group));
        group->intl =
            (struct pqos_mon_data_internal *)malloc(sizeof(*group->intl));
        if (group->intl == NULL) {
                ret = PQOS_RETVAL_RESOURCE;
                goto pqos_mon_start_exit;
        }
        memset(group->intl, 0, sizeof(*group->intl));

        if (api.mon_start != NULL)
                ret = api.mon_start(num_cores, cores, event, context, group);
        else {
                LOG_INFO(UNSUPPORTED_INTERFACE);
                ret = PQOS_RETVAL_RESOURCE;
        }

pqos_mon_start_exit:
        if (ret == PQOS_RETVAL_OK)
                group->valid = GROUP_VALID_MARKER;
        else if (group->intl != NULL)
                free(group->intl);

        lock_release();

        return ret;
}

int
pqos_mon_stop(struct pqos_mon_data *group)
{
        int ret;
        int manage_memory;

        if (group == NULL)
                return PQOS_RETVAL_PARAM;

        if (group->valid != GROUP_VALID_MARKER)
                return PQOS_RETVAL_PARAM;

        lock_get();

        ret = _pqos_check_init(1);
        if (ret != PQOS_RETVAL_OK) {
                lock_release();
                return ret;
        }

        if (api.mon_stop != NULL)
                ret = api.mon_stop(group);
        else {
                LOG_INFO(UNSUPPORTED_INTERFACE);
                ret = PQOS_RETVAL_RESOURCE;
        }

        manage_memory = group->intl->manage_memory;
        free(group->intl);
        if (manage_memory)
                free(group);
        else
                memset(group, 0, sizeof(*group));

        lock_release();

        return ret;
}

int
pqos_mon_poll(struct pqos_mon_data **groups, const unsigned num_groups)
{
        int ret;
        unsigned i;

        if (groups == NULL || num_groups == 0 || *groups == NULL)
                return PQOS_RETVAL_PARAM;

        for (i = 0; i < num_groups; i++) {
                if (groups[i] == NULL)
                        return PQOS_RETVAL_PARAM;
                if (groups[i]->valid != GROUP_VALID_MARKER)
                        return PQOS_RETVAL_PARAM;
                if (groups[i]->event == 0)
                        return PQOS_RETVAL_PARAM;
        }

        lock_get();

        ret = _pqos_check_init(1);
        if (ret != PQOS_RETVAL_OK) {
                lock_release();
                return ret;
        }

        for (i = 0; i < num_groups; i++) {
                int retval = pqos_mon_poll_events(groups[i]);

                if (retval != PQOS_RETVAL_OK) {
                        LOG_WARN("Failed to poll event on group number %u\n",
                                 i);
                        ret = retval;
                }
        }
        lock_release();

        return ret;
}

int
pqos_mon_start_pid(const pid_t pid,
                   const enum pqos_mon_event event,
                   void *context,
                   struct pqos_mon_data *group)
{
        return pqos_mon_start_pids(1, &pid, event, context, group);
}

int
pqos_mon_start_pids(const unsigned num_pids,
                    const pid_t *pids,
                    const enum pqos_mon_event event,
                    void *context,
                    struct pqos_mon_data *group)
{
        int ret;

        if (num_pids == 0 || pids == NULL || group == NULL || event == 0)
                return PQOS_RETVAL_PARAM;

        if (group->valid == GROUP_VALID_MARKER)
                return PQOS_RETVAL_PARAM;

        /**
         * Validate event parameter
         * - only combinations of events allowed
         * - do not allow non-PQoS events to be monitored on its own
         */
        if (event & (~(PQOS_MON_EVENT_L3_OCCUP | PQOS_MON_EVENT_LMEM_BW |
                       PQOS_MON_EVENT_TMEM_BW | PQOS_MON_EVENT_RMEM_BW |
                       PQOS_PERF_EVENT_IPC | PQOS_PERF_EVENT_LLC_MISS |
                       PQOS_PERF_EVENT_LLC_REF)))
                return PQOS_RETVAL_PARAM;

        if ((event & (PQOS_MON_EVENT_L3_OCCUP | PQOS_MON_EVENT_LMEM_BW |
                      PQOS_MON_EVENT_TMEM_BW | PQOS_MON_EVENT_RMEM_BW)) == 0 &&
            (event & (PQOS_PERF_EVENT_IPC | PQOS_PERF_EVENT_LLC_MISS |
                      PQOS_PERF_EVENT_LLC_REF)) != 0) {
                LOG_ERROR("Only PMU events selected for monitoring\n");
                return PQOS_RETVAL_PARAM;
        }

        lock_get();

        ret = _pqos_check_init(1);
        if (ret != PQOS_RETVAL_OK) {
                lock_release();
                return ret;
        }

        memset(group, 0, sizeof(*group));
        group->intl = malloc(sizeof(*group->intl));
        if (group->intl == NULL) {
                ret = PQOS_RETVAL_RESOURCE;
                goto pqos_mon_start_pids_exit;
        }
        memset(group->intl, 0, sizeof(*group->intl));

        if (api.mon_start_pids != NULL)
                ret = api.mon_start_pids(num_pids, pids, event, context, group);
        else {
                LOG_INFO(UNSUPPORTED_INTERFACE);
                ret = PQOS_RETVAL_RESOURCE;
        }

pqos_mon_start_pids_exit:
        if (ret == PQOS_RETVAL_OK)
                group->valid = GROUP_VALID_MARKER;
        else if (group->intl != NULL)
                free(group->intl);

        lock_release();

        return ret;
}

int
pqos_mon_add_pids(const unsigned num_pids,
                  const pid_t *pids,
                  struct pqos_mon_data *group)
{
        if (num_pids == 0 || pids == NULL || group == NULL)
                return PQOS_RETVAL_PARAM;

        if (group->valid != GROUP_VALID_MARKER)
                return PQOS_RETVAL_PARAM;

        return API_CALL(mon_add_pids, num_pids, pids, group);
}

int
pqos_mon_remove_pids(const unsigned num_pids,
                     const pid_t *pids,
                     struct pqos_mon_data *group)
{
        if (num_pids == 0 || pids == NULL || group == NULL)
                return PQOS_RETVAL_PARAM;

        if (group->valid != GROUP_VALID_MARKER)
                return PQOS_RETVAL_PARAM;

        return API_CALL(mon_remove_pids, num_pids, pids, group);
}

int
pqos_mon_start_uncore(const unsigned num_sockets,
                      const unsigned *sockets,
                      const enum pqos_mon_event event,
                      void *context,
                      struct pqos_mon_data **group)
{
        int ret;
        struct pqos_mon_data *data = NULL;

        if (num_sockets == 0 || sockets == NULL || group == NULL || event == 0)
                return PQOS_RETVAL_PARAM;

        /**
         * Validate event parameter
         */
        if ((event & (PQOS_PERF_EVENT_LLC_MISS_PCIE_READ |
                      PQOS_PERF_EVENT_LLC_MISS_PCIE_WRITE |
                      PQOS_PERF_EVENT_LLC_REF_PCIE_READ |
                      PQOS_PERF_EVENT_LLC_REF_PCIE_WRITE)) == 0)
                return PQOS_RETVAL_PARAM;

        lock_get();

        ret = _pqos_check_init(1);
        if (ret != PQOS_RETVAL_OK) {
                lock_release();
                return ret;
        }

        data = calloc(1, sizeof(*data));
        if (data == NULL) {
                ret = PQOS_RETVAL_RESOURCE;
                goto pqos_mon_start_uncore_exit;
        }

        data->intl = calloc(1, sizeof(*data->intl));
        if (data->intl == NULL) {
                ret = PQOS_RETVAL_RESOURCE;
                goto pqos_mon_start_uncore_exit;
        }
        data->intl->manage_memory = 1;

        if (api.mon_start_uncore != NULL)
                ret = api.mon_start_uncore(num_sockets, sockets, event, context,
                                           data);
        else {
                LOG_INFO(UNSUPPORTED_INTERFACE);
                ret = PQOS_RETVAL_RESOURCE;
        }

pqos_mon_start_uncore_exit:
        if (ret == PQOS_RETVAL_OK) {
                data->valid = GROUP_VALID_MARKER;
                *group = data;
        } else if (data != NULL) {
                free(data->intl);
                free(data);
        }

        lock_release();

        return ret;
}

int
pqos_mon_get_value(const struct pqos_mon_data *const group,
                   const enum pqos_mon_event event_id,
                   uint64_t *value,
                   uint64_t *delta)
{
        int ret;
        uint64_t _value;
        uint64_t _delta;

        if (event_id == PQOS_PERF_EVENT_IPC) {
                LOG_ERROR("PQOS_PERF_EVENT_IPC is unsupported, please use "
                          "pqos_mon_get_ipc function");
                return PQOS_RETVAL_PARAM;
        }

        if (group == NULL)
                return PQOS_RETVAL_PARAM;

        if (group->valid != GROUP_VALID_MARKER)
                return PQOS_RETVAL_PARAM;

        if ((group->event & event_id) == 0)
                return PQOS_RETVAL_PARAM;

        lock_get();

        ret = _pqos_check_init(1);
        if (ret != PQOS_RETVAL_OK) {
                lock_release();
                return ret;
        }

        switch (event_id) {
        case PQOS_MON_EVENT_L3_OCCUP:
                if (delta != NULL)
                        LOG_WARN("Counter delta is undefined for "
                                 "PQOS_MON_EVENT_L3_OCCUP\n");
                _value = group->values.llc;
                _delta = 0;
                break;
        case PQOS_MON_EVENT_LMEM_BW:
                _value = group->values.mbm_local;
                _delta = group->values.mbm_local_delta;
                break;
        case PQOS_MON_EVENT_TMEM_BW:
                _value = group->values.mbm_total;
                _delta = group->values.mbm_total_delta;
                break;
        case PQOS_MON_EVENT_RMEM_BW:
                _value = group->values.mbm_remote;
                _delta = group->values.mbm_remote_delta;
                break;
        case PQOS_PERF_EVENT_LLC_MISS:
                _value = group->values.llc_misses;
                _delta = group->values.llc_misses_delta;
                break;
        case PQOS_PERF_EVENT_LLC_REF:
#if PQOS_VERSION >= 50000
                _value = group->values.llc_references;
                _delta = group->values.llc_references_delta;
#else
                _value = group->intl->values.llc_references;
                _delta = group->intl->values.llc_references_delta;
#endif
                break;
        case PQOS_PERF_EVENT_LLC_MISS_PCIE_READ:
                _value = group->intl->values.pcie.llc_misses.read;
                _delta = group->intl->values.pcie.llc_misses.read_delta;
                break;
        case PQOS_PERF_EVENT_LLC_MISS_PCIE_WRITE:
                _value = group->intl->values.pcie.llc_misses.write;
                _delta = group->intl->values.pcie.llc_misses.write_delta;
                break;
        case PQOS_PERF_EVENT_LLC_REF_PCIE_READ:
                _value = group->intl->values.pcie.llc_references.read;
                _delta = group->intl->values.pcie.llc_references.read_delta;
                break;
        case PQOS_PERF_EVENT_LLC_REF_PCIE_WRITE:
                _value = group->intl->values.pcie.llc_references.write;
                _delta = group->intl->values.pcie.llc_references.write_delta;
                break;
        default:
                LOG_ERROR("Unknown event %x\n", event_id);
                ret = PQOS_RETVAL_PARAM;
        }

        if (ret == PQOS_RETVAL_OK) {
                if (value != NULL)
                        *value = _value;
                if (delta != NULL)
                        *delta = _delta;
        }

        lock_release();

        return ret;
}

int
pqos_mon_get_ipc(const struct pqos_mon_data *const group, double *value)
{
        int ret;

        if (group == NULL || value == NULL)
                return PQOS_RETVAL_PARAM;

        if (group->valid != GROUP_VALID_MARKER)
                return PQOS_RETVAL_PARAM;

        if ((group->event & PQOS_PERF_EVENT_IPC) == 0)
                return PQOS_RETVAL_PARAM;

        lock_get();

        ret = _pqos_check_init(1);
        if (ret != PQOS_RETVAL_OK) {
                lock_release();
                return ret;
        }

        *value = group->values.ipc;

        lock_release();

        return ret;
}
