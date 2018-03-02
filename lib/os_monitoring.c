/*
 * BSD LICENSE
 *
 * Copyright(c) 2017 Intel Corporation. All rights reserved.
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
#include <unistd.h>             /**< pid_t */
#include <dirent.h>             /**< scandir() */

#include "pqos.h"
#include "cap.h"
#include "log.h"
#include "types.h"
#include "os_monitoring.h"
#include "perf_monitoring.h"
#include "perf.h"

/**
 * ---------------------------------------
 * Local data structures
 * ---------------------------------------
 */
static const struct pqos_cap *m_cap = NULL;
static const struct pqos_cpuinfo *m_cpu = NULL;

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
 * @param events bitmask of started events
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 * @retval PQOS_RETVAL_ERROR on error
 */
static int
stop_events(struct pqos_mon_data *group,
            const enum pqos_mon_event events)
{
        int ret;
        enum pqos_mon_event stopped_evts = 0;

        ASSERT(group != NULL);
        ASSERT(events != 0);
        /**
         * Determine events, close associated
         * fd's and free associated memory
         */
        if (events & PQOS_MON_EVENT_L3_OCCUP) {
                ret = perf_mon_stop(group, &group->fds_llc);
                if (ret == PQOS_RETVAL_OK)
                        stopped_evts |= PQOS_MON_EVENT_L3_OCCUP;
        }
        if  (events & PQOS_MON_EVENT_LMEM_BW) {
                ret = perf_mon_stop(group, &group->fds_mbl);
                if (ret == PQOS_RETVAL_OK)
                        stopped_evts |= PQOS_MON_EVENT_LMEM_BW;
        }
        if  (events & PQOS_MON_EVENT_TMEM_BW) {
                ret = perf_mon_stop(group, &group->fds_mbt);
                if (ret == PQOS_RETVAL_OK)
                        stopped_evts |= PQOS_MON_EVENT_TMEM_BW;
        }
        if (events & PQOS_MON_EVENT_RMEM_BW) {
                int ret2;

                if (!(events & PQOS_MON_EVENT_LMEM_BW))
                        ret = perf_mon_stop(group, &group->fds_mbl);
                else
                        ret = PQOS_RETVAL_OK;

                if (!(events & PQOS_MON_EVENT_TMEM_BW))
                        ret2 = perf_mon_stop(group, &group->fds_mbt);
                else
                        ret2 = PQOS_RETVAL_OK;

                if (ret == PQOS_RETVAL_OK && ret2 == PQOS_RETVAL_OK)
                        stopped_evts |= PQOS_MON_EVENT_RMEM_BW;
        }
        if (events & PQOS_PERF_EVENT_IPC) {
                int ret2;
                /* stop instructions counter */
                ret = perf_mon_stop(group, &group->fds_inst);
                /* stop cycle counter */
                ret2 = perf_mon_stop(group, &group->fds_cyc);

                if (ret == PQOS_RETVAL_OK && ret2 == PQOS_RETVAL_OK)
                        stopped_evts |= PQOS_PERF_EVENT_IPC;
        }
        if  (events & PQOS_PERF_EVENT_LLC_MISS) {
                ret = perf_mon_stop(group, &group->fds_llc_misses);
                if (ret == PQOS_RETVAL_OK)
                        stopped_evts |= PQOS_PERF_EVENT_LLC_MISS;
        }
        if (events != stopped_evts) {
                LOG_ERROR("Failed to stop all events\n");
                return PQOS_RETVAL_ERROR;
        }
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
        enum pqos_mon_event started_evts = 0;

        ASSERT(group != NULL);
         /**
         * Determine selected events and start Perf counters
         */
        if (group->event & PQOS_MON_EVENT_L3_OCCUP) {
                if (!perf_mon_is_event_supported(PQOS_MON_EVENT_L3_OCCUP))
                        return PQOS_RETVAL_ERROR;
                ret = perf_mon_start(group, PQOS_MON_EVENT_L3_OCCUP,
                                     &group->fds_llc);
                if (ret != PQOS_RETVAL_OK)
                        goto start_event_error;

                started_evts |= PQOS_MON_EVENT_L3_OCCUP;
        }
        if (group->event & PQOS_MON_EVENT_LMEM_BW) {
                if (!perf_mon_is_event_supported(PQOS_MON_EVENT_LMEM_BW))
                        return PQOS_RETVAL_ERROR;
                ret = perf_mon_start(group, PQOS_MON_EVENT_LMEM_BW,
                                     &group->fds_mbl);
                if (ret != PQOS_RETVAL_OK)
                        goto start_event_error;

                started_evts |= PQOS_MON_EVENT_LMEM_BW;
        }
        if (group->event & PQOS_MON_EVENT_TMEM_BW) {
                if (!perf_mon_is_event_supported(PQOS_MON_EVENT_TMEM_BW))
                        return PQOS_RETVAL_ERROR;
                ret = perf_mon_start(group, PQOS_MON_EVENT_TMEM_BW,
                                     &group->fds_mbt);
                if (ret != PQOS_RETVAL_OK)
                        goto start_event_error;

                started_evts |= PQOS_MON_EVENT_TMEM_BW;
        }
        if (group->event & PQOS_MON_EVENT_RMEM_BW) {
                if (!perf_mon_is_event_supported(PQOS_MON_EVENT_LMEM_BW) ||
                    !perf_mon_is_event_supported(PQOS_MON_EVENT_TMEM_BW)) {
                        ret = PQOS_RETVAL_ERROR;
                        goto start_event_error;
                }
                if ((started_evts & PQOS_MON_EVENT_LMEM_BW) == 0) {
                        ret = perf_mon_start(group, PQOS_MON_EVENT_LMEM_BW,
                                             &group->fds_mbl);
                        if (ret != PQOS_RETVAL_OK)
                                goto start_event_error;
                }
                if ((started_evts & PQOS_MON_EVENT_TMEM_BW) == 0) {
                        ret = perf_mon_start(group, PQOS_MON_EVENT_TMEM_BW,
                                             &group->fds_mbt);
                        if (ret != PQOS_RETVAL_OK)
                                goto start_event_error;
                }
                group->values.mbm_remote = 0;
                started_evts |= PQOS_MON_EVENT_RMEM_BW;
        }
        if (group->event & PQOS_PERF_EVENT_IPC) {
                if (!perf_mon_is_event_supported(PQOS_PERF_EVENT_CYCLES) ||
                    !perf_mon_is_event_supported(PQOS_PERF_EVENT_INSTRUCTIONS)
                ) {
                        ret = PQOS_RETVAL_ERROR;
                        goto start_event_error;
                }

                ret = perf_mon_start(group, PQOS_PERF_EVENT_INSTRUCTIONS,
                                     &group->fds_inst);
                if (ret != PQOS_RETVAL_OK)
                        goto start_event_error;

                ret = perf_mon_start(group, PQOS_PERF_EVENT_CYCLES,
                                     &group->fds_cyc);
                if (ret != PQOS_RETVAL_OK)
                        goto start_event_error;

                group->values.ipc = 0;
                started_evts |= PQOS_PERF_EVENT_IPC;
        }
        if (group->event & PQOS_PERF_EVENT_LLC_MISS) {
                if (!perf_mon_is_event_supported(PQOS_PERF_EVENT_LLC_MISS))
                        return PQOS_RETVAL_ERROR;
                ret = perf_mon_start(group, PQOS_PERF_EVENT_LLC_MISS,
                                     &group->fds_llc_misses);
                if (ret != PQOS_RETVAL_OK)
                        goto start_event_error;

                started_evts |= PQOS_PERF_EVENT_LLC_MISS;
        }
 start_event_error:
        /*  Check if all selected events were started */
        if (group->event != started_evts) {
                stop_events(group, started_evts);
                LOG_ERROR("Failed to start all selected "
                          "OS monitoring events\n");
        }
        return ret;
}

int
os_mon_init(const struct pqos_cpuinfo *cpu, const struct pqos_cap *cap)
{
        unsigned ret;

	if (cpu == NULL || cap == NULL)
		return PQOS_RETVAL_PARAM;

        ret = perf_mon_init(cpu, cap);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        m_cap = cap;
	m_cpu = cpu;

        return ret;
}

int
os_mon_fini(void)
{
        m_cap = NULL;
        m_cpu = NULL;

        perf_mon_fini();

        return PQOS_RETVAL_OK;
}

int
os_mon_stop(struct pqos_mon_data *group)
{
        int ret;

        ASSERT(group != NULL);

        if (group->num_cores == 0 && group->tid_nr == 0)
                return PQOS_RETVAL_PARAM;

        /* stop all started events */
        ret = stop_events(group, group->event);

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

        ASSERT(group != NULL);
        ASSERT(cores != NULL);
        ASSERT(num_cores > 0);
        ASSERT(event > 0);

        ASSERT(m_cpu != NULL);

        /**
         * Validate if event is listed in capabilities
         */
        for (i = 0; i < (sizeof(event) * 8); i++) {
                const enum pqos_mon_event evt_mask = (1 << i);
                const struct pqos_monitor *ptr = NULL;

                if (!(evt_mask & event))
                        continue;

                ret = pqos_cap_get_event(m_cap, evt_mask, &ptr);
                if (ret != PQOS_RETVAL_OK || ptr == NULL)
                        return PQOS_RETVAL_PARAM;
        }
        /**
         * Check if all requested cores are valid
         * and not used by other monitoring processes.
         */
        for (i = 0; i < num_cores; i++) {
                const unsigned lcore = cores[i];

                ret = pqos_cpu_check_core(m_cpu, lcore);
                if (ret != PQOS_RETVAL_OK)
                        return PQOS_RETVAL_PARAM;
        }

        /**
         * Fill in the monitoring group structure
         */
        memset(group, 0, sizeof(*group));
        group->event = event;
        group->context = context;
        group->cores = (unsigned *) malloc(sizeof(group->cores[0]) * num_cores);
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

int
os_mon_start_pid(struct pqos_mon_data *group)
{
        DIR *dir;
        pid_t pid, *tid_map;
        int i, ret, num_tasks;
        char buf[64];
        struct dirent **namelist = NULL;

        ASSERT(group != NULL);

        /**
         * Check PID exists
         */
        pid = group->pid;
        snprintf(buf, sizeof(buf)-1, "/proc/%d", (int)pid);
        dir = opendir(buf);
        if (dir == NULL) {
                LOG_ERROR("Task %d does not exist!\n", pid);
                return PQOS_RETVAL_PARAM;
        }
        closedir(dir);

        /**
         * Get TID's for selected task
         */
	snprintf(buf, sizeof(buf)-1, "/proc/%d/task", (int)pid);
	num_tasks = scandir(buf, &namelist, filter, NULL);
	if (num_tasks <= 0) {
		LOG_ERROR("Failed to read proc tasks!\n");
		return PQOS_RETVAL_ERROR;
        }
        tid_map = malloc(sizeof(tid_map[0])*num_tasks);
        if (tid_map == NULL) {
                LOG_ERROR("TID map allocation error!\n");
                return PQOS_RETVAL_ERROR;
        }
	for (i = 0; i < num_tasks; i++)
		tid_map[i] = atoi(namelist[i]->d_name);
        free(namelist);

        group->tid_nr = num_tasks;
        group->tid_map = tid_map;

        /**
         * Determine if user selected a PID or TID
         * If TID selected, only monitor events for that task
         * otherwise monitor all tasks in the process
         */
        if (pid != tid_map[0]) {
                group->tid_nr = 1;
                group->tid_map[0] = pid;
        }

        ret = start_events(group);
        if (ret != PQOS_RETVAL_OK && group->tid_map != NULL)
                free(group->tid_map);

        return ret;
}

int
os_mon_start_pids(const unsigned num_pids,
                  const pid_t *pids,
                  const enum pqos_mon_event event,
                  void *context,
                  struct pqos_mon_data *group)
{
        if (num_pids == 1) {
                memset(group, 0, sizeof(*group));
                group->event = event;
                group->pid = *pids;
                group->context = context;
                return os_mon_start_pid(group);
        }

        LOG_ERROR("Pid group monitoring is not implemented\n");
        return PQOS_RETVAL_ERROR;
}

int
os_mon_add_pids(const unsigned num_pids,
                const pid_t *pids,
                struct pqos_mon_data *group)
{
        UNUSED_PARAM(num_pids);
        UNUSED_PARAM(pids);
        UNUSED_PARAM(group);

        LOG_ERROR("Pid group monitoring is not implemented\n");
        return PQOS_RETVAL_ERROR;
}

int
os_mon_remove_pids(const unsigned num_pids,
                   const pid_t *pids,
                   struct pqos_mon_data *group)
{
        UNUSED_PARAM(num_pids);
        UNUSED_PARAM(pids);
        UNUSED_PARAM(group);

        LOG_ERROR("Pid group monitoring is not implemented\n");
        return PQOS_RETVAL_ERROR;
}

int
os_mon_poll(struct pqos_mon_data **groups,
              const unsigned num_groups)
{
        int ret = PQOS_RETVAL_OK;
        unsigned i = 0;

        ASSERT(groups != NULL);
        ASSERT(num_groups > 0);

        for (i = 0; i < num_groups; i++) {
                /**
                 * If monitoring core/PID then read
                 * counter values
                 */
                ret = perf_mon_poll(groups[i]);
                if (ret != PQOS_RETVAL_OK)
                        LOG_WARN("Failed to read event on "
                                 "group number %u\n", i);
        }

        return PQOS_RETVAL_OK;
}
