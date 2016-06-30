/*
 * BSD LICENSE
 *
 * Copyright(c) 2014-2015 Intel Corporation. All rights reserved.
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
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.O
 *
 */

/**
 * @brief Implementation of PQoS Process Monitoring API.
 *
 * Perf counters are used to monitor events associated
 * with selected PID's.
 */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <linux/perf_event.h>
#include <dirent.h>

#include "perf.h"
#include "host_pidapi.h"
#include "pqos.h"
#include "log.h"
#include "types.h"


/**
 * PID monitoring event type
 */
static int cqm_event_type = 0;

/**
 * Table of structures used to store data about
 * supported PID monitoring events and their
 * mapping onto PQoS events
 */
static struct pid_supported_event {
        const char *name;
        const char *desc;
        enum pqos_mon_event event;
        int supported;
        double scale;
        struct perf_event_attr attrs;
} events_tab[] = {
        { .name = "llc_occupancy",
          .desc = "LLC Occupancy",
          .event = PQOS_MON_EVENT_L3_OCCUP,
          .supported = 0,
          .scale = 1 },
        { .name = "local_bw",
          .desc = "Local Memory B/W",
          .event = PQOS_MON_EVENT_LMEM_BW,
          .supported = 0,
          .scale = 1 },
        { .name = "total_bw",
          .desc = "Total Memory B/W",
          .event = PQOS_MON_EVENT_TMEM_BW,
          .supported = 0,
          .scale = 1 },
        { .name = "",
          .desc = "Remote Memory B/W",
          .event = PQOS_MON_EVENT_RMEM_BW,
          .supported = 0,
          .scale = 1 },
        { .name = "IPC",
          .desc = "Instructions/Cycle",
          .event = PQOS_PERF_EVENT_IPC,
          .supported = 1 }, /**< assumed support */
        { .name = "Cache Misses",
          .desc = "LLC Misses",
          .event = PQOS_PERF_EVENT_LLC_MISS,
          .supported = 1 },
};

/**
 * Event indexes in table of supported events
 */
#define PID_EVENT_INDEX_LLC       0
#define PID_EVENT_INDEX_LMBM      1
#define PID_EVENT_INDEX_TMBM      2
#define PID_EVENT_INDEX_RMBM      3
#define PID_EVENT_INDEX_IPC       4
#define PID_EVENT_INDEX_LLC_MISS  5

/**
 * IPC fd array indexes
 */
#define CYC 0 /**< cpu cycles */
#define INS 1 /**< instructions */

/**
 * All supported events mask
 */
static enum pqos_mon_event all_evt_mask = 0;

/**
 * @brief Gets event from supported events table
 *
 * @param event events bitmask of selected events
 *
 * @return event from supported event table
 * @retval NULL if not successful
 */
static struct pid_supported_event *
get_supported_event(const enum pqos_mon_event event)
{
        switch (event) {
        case PQOS_MON_EVENT_L3_OCCUP:
                return &events_tab[PID_EVENT_INDEX_LLC];
        case PQOS_MON_EVENT_LMEM_BW:
                return &events_tab[PID_EVENT_INDEX_LMBM];
        case PQOS_MON_EVENT_TMEM_BW:
                return &events_tab[PID_EVENT_INDEX_TMBM];
        case PQOS_MON_EVENT_RMEM_BW:
                return &events_tab[PID_EVENT_INDEX_RMBM];
        case PQOS_PERF_EVENT_IPC:
                return &events_tab[PID_EVENT_INDEX_IPC];
        case PQOS_PERF_EVENT_LLC_MISS:
                return &events_tab[PID_EVENT_INDEX_LLC_MISS];
        default:
                ASSERT(0);
                return NULL;
        }
}

/**
 * @brief Check if event is supported by kernel
 *
 * @param event PQoS event to check
 *
 * @retval 0 if not supported
 */
static int
is_event_supported(const enum pqos_mon_event event)
{
        struct pid_supported_event *pe = get_supported_event(event);

        if (pe == NULL) {
                LOG_ERROR("Unsupported event selected\n");
                return 0;
        }
        return pe->supported;
}

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
 * @brief This function stops perf pqos event counters
 *
 * Stops perf pqos event counters for a specified event
 *
 * @param group monitoring structure
 * @param fds array of fd's
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 */
static int
stop_pqos_counters(struct pqos_mon_data *group, int **fds)
{
        int i, *fd;

        ASSERT(group != NULL);
        ASSERT(fds != NULL);
        fd = *fds;
        ASSERT(fd != NULL);

        /**
         * For each TID close associated fd
         */
        for (i = 0; i < group->tid_nr; i++) {
                int ret;

                ret = perf_shutdown_counter(fd[i]);
                if (ret != PQOS_RETVAL_OK)
                        LOG_ERROR("Failed to shutdown perf "
                                  "counters for TID: %d\n",
                                  group->tid_map[i]);
        }
        free(fd);
        *fds = NULL;

        return PQOS_RETVAL_OK;
}

/**
 * @brief Small utility function to support stop_perf_counters()
 *
 * It stops perf counter and closes it.
 *
 * @param fd perf counter file descriptor
 * @param counter_str counter name
 *
 * @retrun Operation status
 * @retval PQOS_RETVAL_OK on success
 * @retval PQOS_RETVAL_ERROR on error
 */
static int stop_one_perf_counter(int fd, const char *counter_str)
{
        int ret1, ret2;

        ASSERT(counter_str != NULL);

        ret1 = perf_stop_counter(fd);
        if (ret1 != PQOS_RETVAL_OK)
                LOG_ERROR("Failed to stop perf %s counter!\n", counter_str);

        ret2 = perf_shutdown_counter(fd);
        if (ret2 != PQOS_RETVAL_OK)
                LOG_ERROR("Failed to shutdown perf %s counter!\n", counter_str);

        return (ret1 == PQOS_RETVAL_OK && ret2 == PQOS_RETVAL_OK) ?
                PQOS_RETVAL_OK : PQOS_RETVAL_ERROR;
}

/**
 * @brief This function stops perf event counters
 *
 * Stops perf event counters for a specified event
 * and closes associated file descriptors
 *
 * It also frees \a fds so this pointer is no longer valid.
 *
 * @param event pqos monitor event bitmask
 * @param fds array of fd's
 * @param tid_nr number of tasks
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 * @retval PQOS_RETVAL_ERRROR if there were errors when stopping counters
 */
static int
stop_perf_counters(const enum pqos_mon_event event, int **fds, const int tid_nr)
{
        int i, stopped = 0;

        ASSERT(fds != NULL);
        ASSERT(tid_nr > 0);

        for (i = 0; i < tid_nr; i++) {
                int *fd = fds[i];

                if (fd == NULL)
                        continue;

                if (event & PQOS_PERF_EVENT_IPC) {
                        stop_one_perf_counter(fd[CYC], "cycles");
                        stop_one_perf_counter(fd[INS], "instructions");
                        stopped++;
                } else if (event & PQOS_PERF_EVENT_LLC_MISS) {
                        stop_one_perf_counter(fd[0], "LLC misses");
                        stopped++;
                } else {
                        ASSERT(0); /* unsupported event */
                }

                free(fd);
        }

        free(fds);
        return (stopped == tid_nr) ? PQOS_RETVAL_OK : PQOS_RETVAL_ERROR;
}

/**
 * @brief This function starts perf pqos event counters
 *
 * Starts perf pqos event counters for a specific event
 *
 * Used to start pqos counters and request file
 * descriptors used to read the counters
 *
 * @param group monitoring structure
 * @param pe supported event structure
 * @param fds array to store fd's
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 */
static int
start_pqos_counters(const struct pqos_mon_data *group,
                    struct pid_supported_event *pe,
                    int **fds)
{
        int i, *fd;

        ASSERT(group != NULL);
        ASSERT(pe != NULL);
        ASSERT(fds != NULL);

        fd = malloc(sizeof(fd[0])*group->tid_nr);
        if (fd == NULL)
                return PQOS_RETVAL_ERROR;
        /**
         * For each TID assign fd to read counter
         */
        for (i = 0; i < group->tid_nr; i++) {
                int ret;

                ret = perf_setup_counter(&pe->attrs,
                                         group->tid_map[i],
                                         -1, -1, 0, &fd[i]);
                if (ret != PQOS_RETVAL_OK) {
                        LOG_ERROR("Failed to start perf "
                                  "counters for %s\n", pe->name);
                        free(fd);
                        return PQOS_RETVAL_ERROR;
                }
        }
        *fds = fd;

        return PQOS_RETVAL_OK;
}

/**
 * @brief This function starts perf event counters
 *
 * Starts perf counters for specified events
 *
 * Used to start counters and request file
 * descriptors used to read the counters
 *
 * @param group monitoring structure
 * @param pe supported event structure
 * @param fds array to store fd's
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 */
static int
start_perf_counters(const struct pqos_mon_data *group,
                    struct pid_supported_event *pe,
                    int ***fds)
{
        int i = 0, to_stop = 0;
        int ret = PQOS_RETVAL_ERROR;
        int **fd_array = NULL;
        struct perf_event_attr attr;

        ASSERT(group != NULL);
        ASSERT(pe != NULL);
        ASSERT(fds != NULL);

        memset(&attr, 0, sizeof(attr));
        attr.disabled = 1;
        attr.type = PERF_TYPE_HARDWARE;

        /**
         * Allocate array to hold pointers to event fd's
         * and setup counters for selected events
         */
        fd_array = malloc(sizeof(fd_array[0]) * group->tid_nr);
        if (fd_array == NULL)
                return PQOS_RETVAL_ERROR;

        if (pe->event & PQOS_PERF_EVENT_IPC) {
                /**
                 * For every task, allocate memory to hold fd's
                 * and setup counters for selected events
                 */
                for (i = 0; i < group->tid_nr; i++) {
                        int *fd = malloc(sizeof(fd[0]) * 2);

                        to_stop = i;
                        if (fd == NULL) {
                                ret = PQOS_RETVAL_RESOURCE;
                                break;
                        }

                        /* set attributes for cpu cycles event */
                        attr.config = PERF_COUNT_HW_CPU_CYCLES;
                        ret = perf_setup_counter(&attr, group->tid_map[i],
                                                 -1, -1, 0, &fd[CYC]);
                        if (ret != PQOS_RETVAL_OK) {
                                free(fd);
                                break;
                        }
                        /* set attributes for instructions event */
                        attr.disabled = 0;
                        attr.config = PERF_COUNT_HW_INSTRUCTIONS;
                        ret = perf_setup_counter(&attr, group->tid_map[i],
                                                 -1, fd[CYC], 0, &fd[INS]);
                        if (ret != PQOS_RETVAL_OK) {
                                stop_one_perf_counter(fd[CYC], "cycles");
                                free(fd);
                                break;
                        }
                        /**
                         * Start counters by enabling the group leader
                         */
                        fd_array[i] = fd;
                        ret = perf_start_counter(fd[CYC]);
                        if (ret != PQOS_RETVAL_OK) {
                                /* it will be stopped by error handler below */
                                to_stop = i + 1;
                                break;
                        }
                }
        } else if (pe->event & PQOS_PERF_EVENT_LLC_MISS) {
                for (i = 0; i < group->tid_nr; i++) {
                        int *fd = malloc(sizeof(fd[0]));

                        to_stop = i;
                        if (fd == NULL) {
                                ret = PQOS_RETVAL_RESOURCE;
                                break;
                        }

                        /* set attributes for cache misses event */
                        attr.config = PERF_COUNT_HW_CACHE_MISSES;
                        ret = perf_setup_counter(&attr, group->tid_map[i],
                                                 -1, -1, 0, &fd[0]);
                        if (ret != PQOS_RETVAL_OK) {
                                free(fd);
                                break;
                        }
                        fd_array[i] = fd;
                        ret = perf_start_counter(fd[0]);
                        if (ret != PQOS_RETVAL_OK) {
                                /* it will be stopped by error handler below */
                                to_stop = i + 1;
                                break;
                        }
                }
        } else {
                /* unsupported event */
                free(fd_array);
                return PQOS_RETVAL_ERROR;
        }

        if (i < group->tid_nr) {
                /* error condition, ret holds error code */
                LOG_ERROR("Failed to setup perf counter for %s\n",
                          pe->name);
                stop_perf_counters(pe->event, fd_array, to_stop);
                return ret;
        }

        *fds = fd_array;
        return PQOS_RETVAL_OK;
}

/**
 * @brief Function to read perf pqos event counters
 *
 * Reads pqos counters and stores values for a specified event
 *
 * @param group monitoring structure
 * @param value destination to store value
 * @param fds array of fd's
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 */
static int
read_pqos_counters(struct pqos_mon_data *group,
                   uint64_t *value, int *fds)
{
        int i;
        uint64_t total_value = 0;

        ASSERT(group != NULL);
        ASSERT(value != NULL);
        ASSERT(fds != NULL);

        /**
         * For each TID read counter and
         * return sum of all counter values
         */
        for (i = 0; i < group->tid_nr; i++) {
                uint64_t counter_value;
                int n = perf_read_counter(fds[i], &counter_value);

                if (n != PQOS_RETVAL_OK)
                        return n;
                total_value += counter_value;
        }
        *value = total_value;

        return PQOS_RETVAL_OK;
}

/**
 * @brief This function reads perf IPC counters
 *
 * Reads perf counters, calculates IPC and
 * stores values in monitoring structure \group
 *
 * @param group monitoring structure
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 */
static int
read_ipc_counters(struct pqos_mon_data *group)
{
        int i, ret = PQOS_RETVAL_OK;
        uint64_t val = 0, cycles = 0, instructions = 0;

        ASSERT(group != NULL);
        /**
         * For each task, read counters and aggregate values
         */
        for (i = 0; i < group->tid_nr; i++) {
                int *fd = group->fds_ipc[i];

                ret = perf_read_counter(fd[CYC], &val);
                if (ret != PQOS_RETVAL_OK)
                        return ret;
                cycles += val;

                ret = perf_read_counter(fd[INS], &val);
                if (ret != PQOS_RETVAL_OK)
                        return ret;
                instructions += val;
        }
        /**
         * Calculate and set IPC value
         */
        if (cycles > 0)
                group->values.ipc = (double)instructions/(double)cycles;
        else
                group->values.ipc = 0;

        return PQOS_RETVAL_OK;
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
                ret = stop_pqos_counters(group, &group->fds_llc);
                if (ret == PQOS_RETVAL_OK)
                        stopped_evts |= PQOS_MON_EVENT_L3_OCCUP;
        }
        if  (events & PQOS_MON_EVENT_LMEM_BW) {
                ret = stop_pqos_counters(group, &group->fds_mbl);
                if (ret == PQOS_RETVAL_OK)
                        stopped_evts |= PQOS_MON_EVENT_LMEM_BW;
        }
        if  (events & PQOS_MON_EVENT_TMEM_BW) {
                ret = stop_pqos_counters(group, &group->fds_mbt);
                if (ret == PQOS_RETVAL_OK)
                        stopped_evts |= PQOS_MON_EVENT_TMEM_BW;
        }
        if (events & PQOS_MON_EVENT_RMEM_BW) {
                int ret2;

                if (!(events & PQOS_MON_EVENT_LMEM_BW))
                        ret = stop_pqos_counters(group, &group->fds_mbl);
                else
                        ret = PQOS_RETVAL_OK;

                if (!(events & PQOS_MON_EVENT_TMEM_BW))
                        ret2 = stop_pqos_counters(group, &group->fds_mbt);
                else
                        ret2 = PQOS_RETVAL_OK;

                if (ret == PQOS_RETVAL_OK && ret2 == PQOS_RETVAL_OK)
                        stopped_evts |= PQOS_MON_EVENT_RMEM_BW;
        }
        if (events & PQOS_PERF_EVENT_IPC) {
                ret = stop_perf_counters(PQOS_PERF_EVENT_IPC,
                                         group->fds_ipc,
                                         group->tid_nr);
                if (ret == PQOS_RETVAL_OK)
                        stopped_evts |= PQOS_PERF_EVENT_IPC;
        }
        if (events & PQOS_PERF_EVENT_LLC_MISS) {
                ret = stop_perf_counters(PQOS_PERF_EVENT_LLC_MISS,
                                         group->fds_misses,
                                         group->tid_nr);
                if (ret == PQOS_RETVAL_OK)
                        stopped_evts |= PQOS_PERF_EVENT_LLC_MISS;
        }
        if (events ^ stopped_evts) {
                LOG_ERROR("Failed to stop all events\n");
                return PQOS_RETVAL_ERROR;
        }
        return PQOS_RETVAL_OK;
}

int
pqos_pid_start(struct pqos_mon_data *group)
{
        DIR *dir;
        int i, ret, n;
        char dir_buf[64];
        pid_t pid, *tids;
        struct pid_supported_event *pe;
        struct dirent **namelist = NULL;
        enum pqos_mon_event started_evts = 0;

        /**
         * Check PID exists
         */
        pid = group->pid;
        snprintf(dir_buf, sizeof(dir_buf)-1, "/proc/%d", (int)pid);
        dir = opendir(dir_buf);
        if (dir == NULL)
                return PQOS_RETVAL_PARAM;
        closedir(dir);

        /**
         * Get TID's for each task
         */
	snprintf(dir_buf, sizeof(dir_buf)-1,
                 "/proc/%d/task", (int)pid);
	n = scandir(dir_buf, &namelist, filter, NULL);
	if (n <= 0) {
		LOG_ERROR("Failed to read TID's\n");
		return PQOS_RETVAL_ERROR;
        }
        tids = malloc(sizeof(tids[0])*n);
        if (tids == NULL) {
                LOG_ERROR("TID map memory allocation error\n");
                return PQOS_RETVAL_ERROR;
        }
	for (i = 0; i < n; i++)
		tids[i] = atoi(namelist[i]->d_name);
        free(namelist);

        group->tid_nr = n;
        group->tid_map = tids;

        /**
         * Determine if user selected a PID or TID
         * If TID selected, only monitor events for that task
         * otherwise monitor all tasks in the process
         */
        if (pid != tids[0]) {
                group->tid_nr = 1;
                group->tid_map[0] = pid;
        }
        /**
         * Determine selected events and start perf counters
         */
        if (group->event & PQOS_MON_EVENT_L3_OCCUP) {
                if (!is_event_supported(PQOS_MON_EVENT_L3_OCCUP))
                        return PQOS_RETVAL_ERROR;
                pe = get_supported_event(PQOS_MON_EVENT_L3_OCCUP);
                ret = start_pqos_counters(group, pe, &group->fds_llc);
                if (ret != PQOS_RETVAL_OK)
                        return PQOS_RETVAL_ERROR;
                started_evts |= PQOS_MON_EVENT_L3_OCCUP;
        }
        if ((group->event & PQOS_MON_EVENT_LMEM_BW) ||
            (group->event & PQOS_MON_EVENT_RMEM_BW)) {
                if (!is_event_supported(PQOS_MON_EVENT_LMEM_BW))
                        return PQOS_RETVAL_ERROR;
                pe = get_supported_event(PQOS_MON_EVENT_LMEM_BW);
                ret = start_pqos_counters(group, pe, &group->fds_mbl);
                if (ret != PQOS_RETVAL_OK)
                        return PQOS_RETVAL_ERROR;
                started_evts |= PQOS_MON_EVENT_LMEM_BW;
        }
        if ((group->event & PQOS_MON_EVENT_TMEM_BW) ||
            (group->event & PQOS_MON_EVENT_RMEM_BW)) {
                if (!is_event_supported(PQOS_MON_EVENT_TMEM_BW))
                        return PQOS_RETVAL_ERROR;
                pe = get_supported_event(PQOS_MON_EVENT_TMEM_BW);
                ret = start_pqos_counters(group, pe, &group->fds_mbt);
                if (ret != PQOS_RETVAL_OK)
                        return PQOS_RETVAL_ERROR;
                started_evts |= PQOS_MON_EVENT_TMEM_BW;
        }
        if (group->event & PQOS_MON_EVENT_RMEM_BW) {
                group->values.mbm_remote = 0;
                started_evts |= PQOS_MON_EVENT_RMEM_BW;
        }
        if (group->event & PQOS_PERF_EVENT_IPC) {
                if (!is_event_supported(PQOS_PERF_EVENT_IPC))
                        return PQOS_RETVAL_ERROR;
                pe = get_supported_event(PQOS_PERF_EVENT_IPC);
                ret = start_perf_counters(group, pe, &group->fds_ipc);
                if (ret != PQOS_RETVAL_OK)
                        return PQOS_RETVAL_ERROR;
                started_evts |= PQOS_PERF_EVENT_IPC;
        }
        if (group->event & PQOS_PERF_EVENT_LLC_MISS) {
                if (!is_event_supported(PQOS_PERF_EVENT_LLC_MISS))
                        return PQOS_RETVAL_ERROR;
                pe = get_supported_event(PQOS_PERF_EVENT_LLC_MISS);
                ret = start_perf_counters(group, pe, &group->fds_misses);
                if (ret != PQOS_RETVAL_OK)
                        return PQOS_RETVAL_ERROR;
                started_evts |= PQOS_PERF_EVENT_LLC_MISS;
        }
        /**
         * Check if all selected events were started
         */
        if (group->event ^ started_evts) {
                stop_events(group, started_evts);
                LOG_ERROR("Failed to start all selected "
                          "PID monitoring events\n");
                return PQOS_RETVAL_ERROR;
        }

        return PQOS_RETVAL_OK;
}

int
pqos_pid_stop(struct pqos_mon_data *group)
{
        int ret;
        /**
         * Stop all started events
         */
        ret = stop_events(group, group->event);
        free(group->tid_map);
        group->tid_map = NULL;

        return ret;
}

int
pqos_pid_poll(struct pqos_mon_data *group)
{
        int ret;

        /**
         * Read and store counter values
         * for each event
         */
        if (group->event & PQOS_MON_EVENT_L3_OCCUP) {
                ret = read_pqos_counters(group,
                                         &group->values.llc,
                                         group->fds_llc);
                if (ret != PQOS_RETVAL_OK)
                        return PQOS_RETVAL_ERROR;

                group->values.llc = group->values.llc *
                        events_tab[PID_EVENT_INDEX_LLC].scale;
        }
        if ((group->event & PQOS_MON_EVENT_LMEM_BW) ||
            (group->event & PQOS_MON_EVENT_RMEM_BW)) {
                ret = read_pqos_counters(group,
                                         &group->values.mbm_local_delta,
                                         group->fds_mbl);
                if (ret != PQOS_RETVAL_OK)
                        return PQOS_RETVAL_ERROR;
        }
        if ((group->event & PQOS_MON_EVENT_TMEM_BW) ||
            (group->event & PQOS_MON_EVENT_RMEM_BW)) {
                ret = read_pqos_counters(group,
                                         &group->values.mbm_total_delta,
                                         group->fds_mbt);
                if (ret != PQOS_RETVAL_OK)
                        return PQOS_RETVAL_ERROR;
        }
        if (group->event & PQOS_MON_EVENT_RMEM_BW) {
                group->values.mbm_remote_delta =
                        group->values.mbm_total_delta -
                        group->values.mbm_local_delta;
        }
        if (group->event & PQOS_PERF_EVENT_IPC) {
                ret = read_ipc_counters(group);
                if (ret != PQOS_RETVAL_OK)
                        return PQOS_RETVAL_ERROR;
        }
        if (group->event & PQOS_PERF_EVENT_LLC_MISS) {
                int i;
                uint64_t missed = 0, val = 0;
                struct pqos_event_values *pv = &group->values;

                for (i = 0; i < group->tid_nr; i++) {
                        int *fd = group->fds_misses[i];

                        ret = perf_read_counter(fd[0], &val);
                        if (ret != PQOS_RETVAL_OK)
                                return PQOS_RETVAL_ERROR;
                        missed += val;
                }
                pv->llc_misses_delta = missed - pv->llc_misses;
                pv->llc_misses = missed;
        }
        return PQOS_RETVAL_OK;
}

/**
 * @brief This function sets perf event attributes
 *
 * Reads perf event attributes from the file system, sets
 * attributes for each event and adds them to the events table
 *
 * @param idx index of the events table
 * @param fname name of event file
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 * @retval PQOS_RETVAL_ERROR on error
 */
static int
set_attrs(const int idx, const char *fname)
{
        FILE *fd;
        int config, ret;
        double sf = 0;
        char file[64], buf[32], *p = buf;

        /**
         * Read event type from file system
         */
        snprintf(file, sizeof(file)-1,
                 "/sys/devices/intel_cqm/events/%s", fname);
        fd = fopen(file, "r");
        if (fd == NULL) {
                LOG_ERROR("Failed to open PID monitoring "
                          "event file\n");
                return PQOS_RETVAL_ERROR;
        }
        if (fgets(p, sizeof(buf), fd) == NULL) {
                fclose(fd);
                LOG_ERROR("Failed to read PID "
                          "monitoring event\n");
                return PQOS_RETVAL_ERROR;
        }
        fclose(fd);
        strsep(&p, "=");
        if (p == NULL) {
                LOG_ERROR("Failed to parse PID "
                          "monitoring event value\n");
                return PQOS_RETVAL_ERROR;
        }
        config = (int)strtol(p, NULL, 0);
        p = buf;

        /**
         * Read scale factor from file system
         */
        snprintf(file, sizeof(file)-1,
                 "/sys/devices/intel_cqm/events/%s.scale", fname);
        fd = fopen(file, "r");
        if (fd == NULL) {
                LOG_ERROR("Failed to open PID "
                          "monitoring event scale file\n");
                return PQOS_RETVAL_ERROR;
        }
        ret = fscanf(fd, "%10lf", &sf);
        fclose(fd);

        if (ret < 1) {
                LOG_ERROR("Failed to read PID monitoring "
                          "event scale factor!\n");
                return PQOS_RETVAL_ERROR;
        }
        events_tab[idx].scale = sf;
        events_tab[idx].supported = 1;

        /**
         * Set event attributes
         */
        memset(&events_tab[idx].attrs, 0,
               sizeof(events_tab[0].attrs));
        events_tab[idx].attrs.type =
                cqm_event_type;
        events_tab[idx].attrs.config = config;
        events_tab[idx].attrs.size =
                sizeof(struct perf_event_attr);
        events_tab[idx].attrs.inherit = 1;
        events_tab[idx].attrs.disabled = 0;
        events_tab[idx].attrs.enable_on_exec = 0;

        return PQOS_RETVAL_OK;
}

/**
 * @brief Function to detect kernel support for perf
 *        pqos events and update events table
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK on success
 */
static int
init_pqos_events(void)
{
	int files, i;
        enum pqos_mon_event events = 0;
        struct dirent **namelist = NULL;

        /**
         * Read and store event data in table
         */
        files = scandir("/sys/devices/intel_cqm/events",
                        &namelist, filter, NULL);
	if (files <= 0) {
		LOG_ERROR("Failed to read PID monitoring "
                          "event files\n");
		return PQOS_RETVAL_ERROR;
	}
	/**
         * Loop through each file
         */
	for (i = 0; i < files; i++) {
                unsigned j;

		/**
                 * Loop through each event
                 */
		for (j = 0; j < DIM(events_tab); j++) {
                        /**
                         * Check if event exists and if
                         * so, set up event attributes
                         */
			if ((strcmp(events_tab[j].name,
                                    namelist[i]->d_name)) != 0)
                                continue;

                        if (set_attrs(j, namelist[i]->d_name)
                            != PQOS_RETVAL_OK)
                                return PQOS_RETVAL_ERROR;

                        events |= events_tab[j].event;
		}
	}
        /**
         * If both local and total mbm are supported
         * then remote mbm is also supported
         */
        if (events_tab[PID_EVENT_INDEX_LMBM].supported &&
            events_tab[PID_EVENT_INDEX_TMBM].supported) {
                events_tab[PID_EVENT_INDEX_RMBM].supported = 1;
                events |= events_tab[PID_EVENT_INDEX_RMBM].event;
        }
        if (events == 0) {
                LOG_ERROR("Failed to find PID monitoring events\n");
                return PQOS_RETVAL_RESOURCE;
        }

        all_evt_mask |= events;

        return PQOS_RETVAL_OK;
}

int
pqos_pid_init(const struct pqos_cap *cap)
{
        FILE *fd;
	char evt[8];
        unsigned i, ret;

        if (cap == NULL)
                return PQOS_RETVAL_PARAM;
        /**
         * Check if kernel supports PID monitoring
         */
	fd = fopen("/sys/devices/intel_cqm/type", "r");
	if (fd == NULL) {
                LOG_INFO("PID monitoring not supported. "
                         "Kernel version 4.1 or higher required.\n");
                return PQOS_RETVAL_RESOURCE;
	}
        if (fgets(evt, sizeof(evt), fd) == NULL) {
		LOG_ERROR("Failed to read cqm_event type\n");
		fclose(fd);
                return PQOS_RETVAL_ERROR;
        }
	fclose(fd);

        cqm_event_type = (int) strtol(evt, NULL, 0);
        if (cqm_event_type == 0) {
                LOG_ERROR("Failed to convert cqm_event type\n");
                return PQOS_RETVAL_ERROR;
        }
        ret = init_pqos_events();
        if (ret != PQOS_RETVAL_OK)
                return ret;
        /**
         * Update capabilities structure with
         * perf supported events
         */
        const struct pqos_capability *p_cap = NULL;

        /* find monitoring capability */
        ret = pqos_cap_get_type(cap, PQOS_CAP_TYPE_MON, &p_cap);
        if (ret != PQOS_RETVAL_OK)
                return PQOS_RETVAL_OK;

        /* update capabilities structure */
        for (i = 0; i < DIM(events_tab); i++) {
                unsigned j;

                if (!events_tab[i].supported)
                        continue;
                for (j = 0; j < p_cap->u.mon->num_events; j++) {
                        struct pqos_monitor *mon =
                                &p_cap->u.mon->events[j];

                        if (events_tab[i].event != mon->type)
                                continue;
                        mon->pid_support = 1;
                        LOG_INFO("Detected PID API (perf) support"
                                 " for %s\n", events_tab[j].desc);
                        break;
                }
        }
        return PQOS_RETVAL_OK;
}


int
pqos_pid_fini(void)
{
        return PQOS_RETVAL_OK;
}
