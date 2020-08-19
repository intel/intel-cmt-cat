/*
 * BSD LICENSE
 *
 * Copyright(c) 2017-2020 Intel Corporation. All rights reserved.
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

#include <stdlib.h>
#include <string.h>
#include <unistd.h> /**< pid_t */
#include <dirent.h> /**< scandir() */

#include "pqos.h"
#include "cap.h"
#include "log.h"
#include "types.h"
#include "monitoring.h"
#include "os_monitoring.h"
#include "perf_monitoring.h"
#include "resctrl.h"
#include "resctrl_monitoring.h"

/**
 * ---------------------------------------
 * Local data structures
 * ---------------------------------------
 */
static const struct pqos_cpuinfo *m_cpu = NULL;

/** List of non virtual events */
const enum pqos_mon_event os_mon_event[] = {
    PQOS_MON_EVENT_L3_OCCUP,
    PQOS_MON_EVENT_LMEM_BW,
    PQOS_MON_EVENT_TMEM_BW,
    PQOS_PERF_EVENT_LLC_MISS,
    (enum pqos_mon_event)PQOS_PERF_EVENT_CYCLES,
    (enum pqos_mon_event)PQOS_PERF_EVENT_INSTRUCTIONS};

/**
 * @brief Filter directory filenames
 *
 * This function is used by the scandir function
 * to filter hidden (dot) files
 *
 * @param dir dirent structure containing directory info
 *
 * @return if directory entry should be included in scandir() output list
 * @retval 0 means don't include the entry  ("." in our case)
 * @retval 1 means include the entry
 */
static int
filter(const struct dirent *dir)
{
        return (dir->d_name[0] == '.') ? 0 : 1;
}

/**
 * @brief This function stops started events
 *
 * @param group monitoring structure
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 * @retval PQOS_RETVAL_ERROR on error
 */
static int
stop_events(struct pqos_mon_data *group)
{
        int ret;
        enum pqos_mon_event stopped_evts = (enum pqos_mon_event)0;
        unsigned i;

        ASSERT(group != NULL);

        for (i = 0; i < DIM(os_mon_event); i++) {
                enum pqos_mon_event evt = os_mon_event[i];

                /**
                 * Stop perf event
                 */
                if (group->intl->perf.event & evt) {
                        ret = perf_mon_stop(group, evt);
                        if (ret == PQOS_RETVAL_OK)
                                stopped_evts |= evt;
                }
        }

        if (group->intl->resctrl.event) {
                ret = resctrl_lock_exclusive();
                if (ret != PQOS_RETVAL_OK)
                        goto stop_event_error;

                ret = resctrl_mon_stop(group);
                if (ret == PQOS_RETVAL_OK)
                        stopped_evts |= group->intl->resctrl.event;

                resctrl_lock_release();
        }

stop_event_error:
        if ((stopped_evts & PQOS_MON_EVENT_LMEM_BW) &&
            (stopped_evts & PQOS_MON_EVENT_TMEM_BW))
                stopped_evts |= (enum pqos_mon_event)PQOS_MON_EVENT_RMEM_BW;

        if ((stopped_evts & PQOS_PERF_EVENT_CYCLES) &&
            (stopped_evts & PQOS_PERF_EVENT_INSTRUCTIONS))
                stopped_evts |= (enum pqos_mon_event)PQOS_PERF_EVENT_IPC;

        if (group->intl->perf.ctx != NULL) {
                free(group->intl->perf.ctx);
                group->intl->perf.ctx = NULL;
        }

        if ((group->intl->perf.event & stopped_evts) !=
            group->intl->perf.event) {
                LOG_ERROR("Failed to stop all perf events\n");
                return PQOS_RETVAL_ERROR;
        }
        group->intl->perf.event = (enum pqos_mon_event)0;

        if ((group->intl->resctrl.event & stopped_evts) !=
            group->intl->resctrl.event) {
                LOG_ERROR("Failed to stop resctrl events\n");
                return PQOS_RETVAL_ERROR;
        }
        group->intl->resctrl.event = (enum pqos_mon_event)0;

        return PQOS_RETVAL_OK;
}

/**
 * @brief This function starts selected events
 *
 * @param group monitoring structure
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 * @retval PQOS_RETVAL_ERROR on error
 */
static int
start_events(struct pqos_mon_data *group)
{
        int ret = PQOS_RETVAL_OK;
        unsigned num_ctrs, i;
        enum pqos_mon_event events;
        enum pqos_mon_event started_evts = (enum pqos_mon_event)0;

        ASSERT(group != NULL);

        if (group->num_cores > 0)
                num_ctrs = group->num_cores;
        else if (group->tid_nr > 0)
                num_ctrs = group->tid_nr;
        else
                return PQOS_RETVAL_ERROR;

        events = group->event;
        group->intl->perf.event = (enum pqos_mon_event)0;
        group->intl->perf.ctx =
            malloc(sizeof(group->intl->perf.ctx[0]) * num_ctrs);
        if (group->intl->perf.ctx == NULL) {
                LOG_ERROR("Memory allocation failed\n");
                return PQOS_RETVAL_ERROR;
        }

        if (events & PQOS_MON_EVENT_RMEM_BW)
                events |= (enum pqos_mon_event)(PQOS_MON_EVENT_LMEM_BW |
                                                PQOS_MON_EVENT_TMEM_BW);
        if (events & PQOS_PERF_EVENT_IPC)
                events |= (enum pqos_mon_event)(PQOS_PERF_EVENT_CYCLES |
                                                PQOS_PERF_EVENT_INSTRUCTIONS);

        /**
         * Determine selected events and Perf counters
         */
        for (i = 0; i < DIM(os_mon_event); i++) {
                enum pqos_mon_event evt = os_mon_event[i];

                if (events & evt) {
                        if (perf_mon_is_event_supported(evt)) {
                                ret = perf_mon_start(group, evt);
                                if (ret != PQOS_RETVAL_OK)
                                        goto start_event_error;
                                group->intl->perf.event |= evt;
                                continue;
                        }

                        if (resctrl_mon_is_event_supported(evt)) {
                                group->intl->resctrl.event |= evt;
                                continue;
                        }

                        /**
                         * Event is not supported
                         */
                        ret = PQOS_RETVAL_ERROR;
                        goto start_event_error;
                }
        }
        started_evts |= group->intl->perf.event;

        if (group->intl->resctrl.event != 0) {
                ret = resctrl_lock_exclusive();
                if (ret != PQOS_RETVAL_OK)
                        goto start_event_error;

                ret = resctrl_mon_start(group);
                resctrl_lock_release();
                if (ret != PQOS_RETVAL_OK)
                        goto start_event_error;
        }
        started_evts |= group->intl->resctrl.event;

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

start_event_error:
        /*  Check if all selected events were started */
        if ((group->event & started_evts) != group->event) {
                stop_events(group);
                LOG_ERROR("Failed to start all selected "
                          "OS monitoring events\n");
                free(group->intl->perf.ctx);
                group->intl->perf.ctx = NULL;
        }
        return ret;
}

/**
 * @brief This function polls selected events
 *
 * @param group monitoring structure
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 * @retval PQOS_RETVAL_ERROR on error
 */
static int
poll_events(struct pqos_mon_data *group)
{
        unsigned i;
        int ret = PQOS_RETVAL_OK;

        if (group->intl->resctrl.event != 0) {
                ret = resctrl_lock_shared();
                if (ret != PQOS_RETVAL_OK)
                        return ret;
        }

        for (i = 0; i < DIM(os_mon_event); i++) {
                enum pqos_mon_event evt = os_mon_event[i];

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
        }

        /**
         * Calculate values of virtual events
         */
        if (group->event & PQOS_MON_EVENT_RMEM_BW) {
                group->values.mbm_remote_delta = 0;
                if (group->values.mbm_total_delta >
                    group->values.mbm_local_delta)
                        group->values.mbm_remote_delta =
                            group->values.mbm_total_delta -
                            group->values.mbm_local_delta;
        }
        if (group->event & PQOS_PERF_EVENT_IPC) {
                if (group->values.ipc_unhalted_delta > 0)
                        group->values.ipc =
                            (double)group->values.ipc_retired_delta /
                            (double)group->values.ipc_unhalted_delta;
                else
                        group->values.ipc = 0;
        }

poll_events_exit:
        if (group->intl->resctrl.event != 0)
                resctrl_lock_release();

        return ret;
}

int
os_mon_init(const struct pqos_cpuinfo *cpu, const struct pqos_cap *cap)
{
        unsigned ret;

        ASSERT(cpu != NULL);
        ASSERT(cap != NULL);

        if (cpu == NULL || cap == NULL)
                return PQOS_RETVAL_PARAM;

        ret = perf_mon_init(cpu, cap);
        if (ret == PQOS_RETVAL_RESOURCE)
                ret = resctrl_mon_init(cpu, cap);

        if (ret != PQOS_RETVAL_OK)
                return ret;

        m_cpu = cpu;

        return ret;
}

int
os_mon_fini(void)
{
        m_cpu = NULL;

        perf_mon_fini();
        resctrl_mon_fini();

        return PQOS_RETVAL_OK;
}

int
os_mon_reset(void)
{
        return resctrl_mon_reset();
}

int
os_mon_stop(struct pqos_mon_data *group)
{
        int ret;

        ASSERT(group != NULL);

        if (group->num_cores == 0 && group->tid_nr == 0)
                return PQOS_RETVAL_PARAM;

        /* stop all started events */
        ret = stop_events(group);

        /* free memory */
        if (group->num_cores > 0) {
                free(group->cores);
                group->cores = NULL;
        }
        if (group->tid_nr > 0) {
                free(group->tid_map);
                group->tid_map = NULL;
        }
        memset(group, 0, sizeof(*group));

        return ret;
}

int
os_mon_start(const unsigned num_cores,
             const unsigned *cores,
             const enum pqos_mon_event event,
             void *context,
             struct pqos_mon_data *group)
{
        unsigned i = 0;
        int ret;
        const struct pqos_cap *cap;
        const struct pqos_cpuinfo *cpu;

        ASSERT(group != NULL);
        ASSERT(cores != NULL);
        ASSERT(num_cores > 0);
        ASSERT(event > 0);

        _pqos_cap_get(&cap, &cpu);

        /**
         * Validate if event is listed in capabilities
         */
        for (i = 0; i < (sizeof(event) * 8); i++) {
                const enum pqos_mon_event evt_mask =
                    (enum pqos_mon_event)(1U << i);
                const struct pqos_monitor *ptr = NULL;

                if (!(evt_mask & event))
                        continue;

                ret = pqos_cap_get_event(cap, evt_mask, &ptr);
                if (ret != PQOS_RETVAL_OK || ptr == NULL)
                        return PQOS_RETVAL_PARAM;
        }
        /**
         * Check if all requested cores are valid
         * and not used by other monitoring processes.
         */
        for (i = 0; i < num_cores; i++) {
                const unsigned lcore = cores[i];

                ret = pqos_cpu_check_core(cpu, lcore);
                if (ret != PQOS_RETVAL_OK)
                        return PQOS_RETVAL_PARAM;
        }

        /**
         * Fill in the monitoring group structure
         */
        group->event = event;
        group->context = context;
        group->cores = (unsigned *)malloc(sizeof(group->cores[0]) * num_cores);
        if (group->cores == NULL)
                return PQOS_RETVAL_RESOURCE;

        group->num_cores = num_cores;
        for (i = 0; i < num_cores; i++)
                group->cores[i] = cores[i];

        ret = start_events(group);
        if (ret != PQOS_RETVAL_OK && group->cores != NULL)
                free(group->cores);

        return ret;
}

/**
 * @brief Check if \a tid is in \a tid_map
 *
 * @param[in] tid TID number to search for
 * @param[in] tid_nr length of \a tid_map
 * @param[in] tid_map list of TIDs
 *
 * @retval 1 if found
 */
static int
tid_exists(const pid_t tid, const unsigned tid_nr, const pid_t *tid_map)
{
        unsigned i;

        if (tid_map == NULL)
                return 0;

        for (i = 0; i < tid_nr; i++)
                if (tid_map[i] == tid)
                        return 1;

        return 0;
}

/**
 * @brief Add TID to \a tid_map
 *
 * @param[in] tid TID number to add
 * @param[in] tid_nr length of \a tid_map
 * @param[in] tid_map list of TIDs
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK on success
 */
static int
tid_add(const pid_t tid, unsigned *tid_nr, pid_t **tid_map)
{
        pid_t *tids;

        if (tid_exists(tid, *tid_nr, *tid_map))
                return PQOS_RETVAL_OK;

        tids = realloc(*tid_map, sizeof(pid_t) * (*tid_nr + 1));
        if (tids == NULL) {
                LOG_ERROR("TID map allocation error!\n");
                return PQOS_RETVAL_ERROR;
        }

        tids[*tid_nr] = tid;
        (*tid_nr)++;
        *tid_map = tids;

        return PQOS_RETVAL_OK;
}

/**
 * @brief Find process TID's and add them to the list
 *
 * @param[in] pid peocess id
 * @param[in,out] tid_nr number of tids
 * @param[in,out] tid_map tid mapping
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 */
static int
tid_find(const pid_t pid, unsigned *tid_nr, pid_t **tid_map)
{
        char buf[64];
        pid_t tid;
        int num_tasks, i;
        struct dirent **namelist = NULL;
        int ret;

        snprintf(buf, sizeof(buf) - 1, "/proc/%d/task", (int)pid);
        num_tasks = scandir(buf, &namelist, filter, NULL);
        if (num_tasks <= 0) {
                LOG_ERROR("Failed to read proc tasks!\n");
                return PQOS_RETVAL_ERROR;
        }

        /**
         * Determine if user selected a PID or TID
         * If TID selected, only monitor events for that task
         * otherwise monitor all tasks in the process
         */
        tid = atoi(namelist[0]->d_name);
        if (pid != tid)
                ret = tid_add(pid, tid_nr, tid_map);
        else
                for (i = 0; i < num_tasks; i++) {
                        ret = tid_add((pid_t)atoi(namelist[i]->d_name), tid_nr,
                                      tid_map);
                        if (ret != PQOS_RETVAL_OK)
                                break;
                }

        for (i = 0; i < num_tasks; i++)
                free(namelist[i]);

        free(namelist);

        return ret;
}

/**
 * @brief Verify is PID is correct
 *
 * @param[in] pid PID number to check
 *
 * @retval 1 if PID is correct
 */
static int
tid_verify(const pid_t pid)
{
        int found = 0;
        char buf[64];
        DIR *dir;

        snprintf(buf, sizeof(buf) - 1, "/proc/%d", (int)pid);
        dir = opendir(buf);
        if (dir != NULL) {
                found = 1;
                closedir(dir);
        }

        return found;
}

int
os_mon_start_pids(const unsigned num_pids,
                  const pid_t *pids,
                  const enum pqos_mon_event event,
                  void *context,
                  struct pqos_mon_data *group)
{
        int ret;
        unsigned i;
        pid_t *tid_map = NULL;
        unsigned tid_nr = 0;

        ASSERT(group != NULL);
        ASSERT(num_pids > 0);
        ASSERT(event > 0);
        ASSERT(pids != NULL);

        /**
         * Check if all PIDs exists
         */
        for (i = 0; i < num_pids; i++) {
                pid_t pid = pids[i];

                if (!tid_verify(pid)) {
                        LOG_ERROR("Task %d does not exist!\n", (int)pid);
                        return PQOS_RETVAL_PARAM;
                }
        }

        /**
         * Get TID's for selected tasks
         */
        for (i = 0; i < num_pids; i++) {
                ret = tid_find(pids[i], &tid_nr, &tid_map);
                if (ret != PQOS_RETVAL_OK)
                        goto os_mon_start_pids_exit;
        }

        group->pids = (pid_t *)malloc(sizeof(pid_t) * num_pids);
        if (group->pids == NULL) {
                ret = PQOS_RETVAL_RESOURCE;
                goto os_mon_start_pids_exit;
        }

        group->context = context;
        group->tid_nr = tid_nr;
        group->tid_map = tid_map;
        group->event = event;
        group->num_pids = num_pids;

        for (i = 0; i < num_pids; i++)
                group->pids[i] = pids[i];

        ret = start_events(group);

os_mon_start_pids_exit:
        if (ret != PQOS_RETVAL_OK && tid_map != NULL)
                free(tid_map);

        return ret;
}

int
os_mon_add_pids(const unsigned num_pids,
                const pid_t *pids,
                struct pqos_mon_data *group)
{
        int ret;
        unsigned i;
        pid_t *tid_map = NULL;
        pid_t *ptr;
        unsigned tid_nr = 0;
        struct pqos_mon_data added;
        struct pqos_mon_perf_ctx *ctx;
        unsigned num_duplicated = 0;

        ASSERT(group != NULL);
        ASSERT(num_pids > 0);
        ASSERT(pids != NULL);

        memset(&added, 0, sizeof(added));

        /**
         * Check if all PIDs exists
         */
        for (i = 0; i < num_pids; i++) {
                pid_t pid = pids[i];

                if (!tid_verify(pid)) {
                        LOG_ERROR("Task %d does not exist!\n", (int)pid);
                        return PQOS_RETVAL_PARAM;
                }
        }

        /**
         * Get TID's for added tasks
         */
        for (i = 0; i < num_pids; i++) {
                ret = tid_find(pids[i], &tid_nr, &tid_map);
                if (ret != PQOS_RETVAL_OK)
                        goto os_mon_add_pids_exit;
        }

        /**
         * Find duplicated tids
         */
        for (i = 0; i < tid_nr; i++) {
                if (tid_exists(tid_map[i], group->tid_nr, group->tid_map)) {
                        num_duplicated++;
                        continue;
                }

                tid_map[i - num_duplicated] = tid_map[i];
        }
        tid_nr -= num_duplicated;
        if (tid_nr == 0) {
                LOG_INFO("No new TIDs to be added\n");
                ret = PQOS_RETVAL_OK;
                goto os_mon_add_pids_exit;
        }

        /**
         * Start monitoring for the new TIDs
         */
        added.tid_nr = tid_nr;
        added.tid_map = tid_map;
        added.event = group->event;
        added.num_pids = num_pids;
        added.intl = malloc(sizeof(*added.intl));
        if (added.intl == NULL)
                goto os_mon_add_pids_exit;
        memset(added.intl, 0, sizeof(*added.intl));
        if (group->intl->resctrl.mon_group != NULL) {
                added.intl->resctrl.mon_group =
                    strdup(group->intl->resctrl.mon_group);
                if (added.intl->resctrl.mon_group == NULL) {
                        ret = PQOS_RETVAL_RESOURCE;
                        goto os_mon_add_pids_exit;
                }
        }

        ret = start_events(&added);
        if (ret != PQOS_RETVAL_OK)
                goto os_mon_add_pids_exit;

        /**
         * Update mon group
         */
        ptr = realloc(group->tid_map, sizeof(group->tid_map[0]) *
                                          (group->tid_nr + added.tid_nr));
        if (ptr == NULL) {
                ret = PQOS_RETVAL_RESOURCE;
                goto os_mon_add_pids_exit;
        }
        group->tid_map = ptr;

        ctx =
            realloc(group->intl->perf.ctx, sizeof(group->intl->perf.ctx[0]) *
                                               (group->tid_nr + added.tid_nr));
        if (ctx == NULL) {
                ret = PQOS_RETVAL_RESOURCE;
                goto os_mon_add_pids_exit;
        }
        group->intl->perf.ctx = ctx;

        ptr = realloc(group->pids,
                      sizeof(group->pids[0]) * (group->num_pids + num_pids));
        if (ptr == NULL) {
                ret = PQOS_RETVAL_RESOURCE;
                goto os_mon_add_pids_exit;
        }
        group->pids = ptr;

        for (i = 0; i < added.tid_nr; i++) {
                group->tid_map[group->tid_nr] = added.tid_map[i];
                group->intl->perf.ctx[group->tid_nr] = added.intl->perf.ctx[i];
                group->tid_nr++;
        }
        for (i = 0; i < num_pids; i++) {
                group->pids[group->num_pids] = pids[i];
                group->num_pids++;
        }

os_mon_add_pids_exit:
        if (added.intl != NULL && added.intl->resctrl.mon_group != NULL) {
                free(added.intl->resctrl.mon_group);
                added.intl->resctrl.mon_group = NULL;
        }
        if (ret == PQOS_RETVAL_RESOURCE) {
                LOG_ERROR("Memory allocation error!\n");
                stop_events(&added);
        }
        if (added.intl != NULL) {
                if (added.intl->perf.ctx != NULL)
                        free(added.intl->perf.ctx);
                free(added.intl);
        }
        if (tid_map != NULL)
                free(tid_map);
        return ret;
}

int
os_mon_remove_pids(const unsigned num_pids,
                   const pid_t *pids,
                   struct pqos_mon_data *group)
{

        int ret = PQOS_RETVAL_OK;
        unsigned i;
        pid_t *keep_tid_map = NULL; /* List of not removed TIDs */
        unsigned keep_tid_nr = 0;
        struct pqos_mon_data remove;
        unsigned removed;

        ASSERT(num_pids > 0);
        ASSERT(pids != NULL);
        ASSERT(group != NULL);

        memset(&remove, 0, sizeof(remove));

        /**
         * Find TID's for not removed tasks
         */
        for (i = 0; i < group->num_pids; i++) {
                /* skip PIDs on removed list */
                if (tid_exists(group->pids[i], num_pids, pids))
                        continue;

                /* pid no longer exists */
                if (!tid_verify(group->pids[i]))
                        continue;

                ret = tid_find(group->pids[i], &keep_tid_nr, &keep_tid_map);
                if (ret != PQOS_RETVAL_OK)
                        goto os_mon_remove_pids_exit;
        }

        remove.intl =
            (struct pqos_mon_data_internal *)malloc(sizeof(*remove.intl));
        if (remove.intl == NULL)
                goto os_mon_remove_pids_exit;
        memset(remove.intl, 0, sizeof(*remove.intl));
        remove.intl->perf.event = group->intl->perf.event;
        remove.intl->resctrl.event = group->intl->resctrl.event;
        remove.pids = NULL;
        remove.num_pids = num_pids;
        remove.tid_map = malloc(sizeof(remove.tid_map[0]) * group->tid_nr);
        if (remove.tid_map == NULL)
                goto os_mon_remove_pids_exit;
        remove.intl->perf.ctx =
            malloc(sizeof(remove.intl->perf.ctx[0]) * group->tid_nr);
        if (remove.intl->perf.ctx == NULL)
                goto os_mon_remove_pids_exit;

        /* Add tid's for removal */
        for (i = 0; i < group->tid_nr; i++) {
                /* TID is not removed */
                if (tid_exists(group->tid_map[i], keep_tid_nr, keep_tid_map))
                        continue;

                remove.tid_map[remove.tid_nr] = group->tid_map[i];
                remove.intl->perf.ctx[remove.tid_nr] = group->intl->perf.ctx[i];
                remove.tid_nr++;
        }

        ret = stop_events(&remove);
        if (ret != PQOS_RETVAL_OK)
                goto os_mon_remove_pids_exit;

        /**
         * Update mon group
         */
        removed = 0;
        for (i = 0; i < group->tid_nr; i++) {
                /* TID does not exists on the not keep list */
                if (!tid_exists(group->tid_map[i], keep_tid_nr, keep_tid_map)) {
                        removed++;
                        continue;
                }

                group->tid_map[i - removed] = group->tid_map[i];
                group->intl->perf.ctx[i - removed] = group->intl->perf.ctx[i];
        }
        group->tid_nr -= removed;
        group->tid_map =
            realloc(group->tid_map, sizeof(group->tid_map[0]) * group->tid_nr);
        group->intl->perf.ctx =
            realloc(group->intl->perf.ctx,
                    sizeof(group->intl->perf.ctx[0]) * group->tid_nr);
        removed = 0;
        for (i = 0; i < group->num_pids; i++) {
                if (tid_exists(group->pids[i], num_pids, pids)) {
                        removed++;
                        continue;
                }

                group->pids[i - removed] = group->pids[i];
        }
        group->num_pids -= removed;
        group->pids =
            realloc(group->pids, sizeof(group->pids[0]) * group->num_pids);

os_mon_remove_pids_exit:
        if (remove.tid_map != NULL)
                free(remove.tid_map);
        if (remove.intl != NULL) {
                if (remove.intl->perf.ctx)
                        free(remove.intl->perf.ctx);
                free(remove.intl);
        }
        if (keep_tid_map != NULL)
                free(keep_tid_map);
        return ret;
}

int
os_mon_poll(struct pqos_mon_data **groups, const unsigned num_groups)
{
        unsigned i = 0;

        ASSERT(groups != NULL);
        ASSERT(num_groups > 0);

        for (i = 0; i < num_groups; i++) {
                int ret = poll_events(groups[i]);

                if (ret != PQOS_RETVAL_OK)
                        LOG_WARN("Failed to poll event on "
                                 "group number %u\n",
                                 i);
        }

        return PQOS_RETVAL_OK;
}
