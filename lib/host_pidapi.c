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

#include "host_pidapi.h"
#include "pqos.h"
#include "log.h"
#include "types.h"

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
        int evt_value;
        struct perf_event_attr attrs;
} events_tab[] = {
        { .name = "llc_occupancy",
          .desc = "LLC Occupancy",
          .event = PQOS_MON_EVENT_L3_OCCUP,
          .supported = 0 },
        { .name = "llc_local_bw",
          .desc = "Local Memory B/W",
          .event = PQOS_MON_EVENT_LMEM_BW,
          .supported = 0 },
        { .name = "llc_total_bw",
          .desc = "Total Memory B/W",
          .event = PQOS_MON_EVENT_TMEM_BW,
          .supported = 0 },
        { .name = "",
          .desc = "Remote Memory B/W",
          .event = PQOS_MON_EVENT_RMEM_BW,
          .supported = 0 },
};

/**
 * Event indexes in table of supported events
 */
#define PID_EVENT_INDEX_LLC 0
#define PID_EVENT_INDEX_LMBM 1
#define PID_EVENT_INDEX_TMBM 2
#define PID_EVENT_INDEX_RMBM 3

/**
 * PID monitoring event type
 */
static int cqm_event_type = 0;

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
 * @brief This function starts perf counters
 *
 * Starts perf counters for a specific event
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
start_counters(struct pqos_mon_data *group,
               struct pid_supported_event *pe,
               int **fds)
{
        int i, *fds_ptr;

        ASSERT(group != NULL);
        ASSERT(pe != NULL);
        ASSERT(fds != NULL);

        fds_ptr = malloc(sizeof(fds_ptr[0])*group->tid_nr);
        if (fds_ptr == NULL)
                return PQOS_RETVAL_ERROR;
        /**
         * For each TID assign fd to read counter
         */
        for (i = 0; i < group->tid_nr; i++) {
                fds_ptr[i] = syscall(__NR_perf_event_open,
                                     &pe->attrs,
                                     group->tid_map[i],
                                     -1, -1, 0);
                if (fds_ptr[i] < 0) {
                        LOG_ERROR("Failed to start perf "
                                  "counters for %s\n", pe->name);
                        free(fds_ptr);
                        return PQOS_RETVAL_ERROR;
                }
        }
        *fds = fds_ptr;
        return PQOS_RETVAL_OK;
}

/**
 * @brief This function stops perf counters
 *
 * Stops perf counters for a specific event
 *
 * @param group monitoring structure
 * @param fds array of fd's
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 */
static int
stop_counters(struct pqos_mon_data *group, int **fds)
{
        int i, *fds_ptr;

        ASSERT(group != NULL);
        ASSERT(fds != NULL);
        fds_ptr = *fds;
        ASSERT(fds_ptr != NULL);

        /**
         * For each TID close associated fd
         */
        for (i = 0; i < group->tid_nr; i++) {
                ASSERT(fds_ptr[i] >= 0);
                if (close(fds_ptr[i]) < 0) {
                        LOG_ERROR("Failed to stop perf "
                                  "counter for TID: %d\n",
                                  group->tid_map[i]);
                }
        }
        free(fds_ptr);
        *fds = NULL;
        return PQOS_RETVAL_OK;
}

/**
 * @brief This function reads perf counters
 *
 * Reads counters and stores values for a specific event
 *
 * @param group monitoring structure
 * @param evt_value destination to store value
 * @param fds array of fd's
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 */
static int
read_counters(struct pqos_mon_data *group,
              uint64_t *evt_value,
              int *fds)
{
        int i;
        uint64_t total_value = 0;

        ASSERT(group != NULL);
        ASSERT(evt_value != NULL);
        ASSERT(fds != NULL);

        /**
         * For each TID read counter and
         * return sum of all counter values
         */
        for (i = 0; i < group->tid_nr; i++) {
                uint64_t counter_value;
                int nb = read(fds[i], &counter_value,
                              sizeof(counter_value));
                if (nb != sizeof(counter_value)) {
                        LOG_ERROR("Failed to read counter value\n");
                        return PQOS_RETVAL_ERROR;
                }
                total_value += counter_value;
        }
        *evt_value = total_value;

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
                ret = stop_counters(group, &group->fds_llc);
                if (ret != PQOS_RETVAL_OK)
                        return PQOS_RETVAL_ERROR;
                stopped_evts |= PQOS_MON_EVENT_L3_OCCUP;
        }
        if  ((events & PQOS_MON_EVENT_LMEM_BW) ||
             (events & PQOS_MON_EVENT_RMEM_BW)) {
                ret = stop_counters(group, &group->fds_mbl);
                if (ret != PQOS_RETVAL_OK)
                        return PQOS_RETVAL_ERROR;
                stopped_evts |= PQOS_MON_EVENT_LMEM_BW;
        }
        if  ((events & PQOS_MON_EVENT_TMEM_BW) ||
             (events & PQOS_MON_EVENT_RMEM_BW)) {
                ret = stop_counters(group, &group->fds_mbt);
                if (ret != PQOS_RETVAL_OK)
                        return PQOS_RETVAL_ERROR;
                stopped_evts |= PQOS_MON_EVENT_LMEM_BW;
        }
        if (events ^ stopped_evts) {
                LOG_ERROR("Failed to stop all events\n");
                return PQOS_RETVAL_ERROR;
        }
        return PQOS_RETVAL_OK;
}

int
pqos_pid_init(void)
{
        FILE *fd;
	int i, files;
	char name[64], evt[8];
	struct dirent **namelist = NULL;
        enum pqos_mon_event events = 0;

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
                        char event_cb[32];
                        char *event_ptr = event_cb;

			/**
                         * Check if event exists and if
                         * so, set up event attributes
                         */
			if ((strcmp(events_tab[j].name,
                                    namelist[i]->d_name)) != 0)
                                continue;

                        snprintf(name, sizeof(name)-1,
                                 "/sys/devices/intel_cqm/events/%s",
                                 namelist[i]->d_name);
                        fd = fopen(name, "r");
                        if (fd == NULL) {
                                LOG_ERROR("Failed to open PID monitoring "
                                          "event file\n");
                                return PQOS_RETVAL_ERROR;
                        }
                        if (fgets(event_ptr, sizeof(event_cb), fd) == NULL) {
                                fclose(fd);
                                LOG_ERROR("Failed to read PID "
                                          "monitoring event\n");
                                return PQOS_RETVAL_ERROR;
                        }
                        fclose(fd);

                        strsep(&event_ptr, "=");
                        if (event_ptr == NULL) {
                                LOG_ERROR("Failed to parse PID "
                                          "monitoring event value\n");
                                return PQOS_RETVAL_ERROR;
                        }

                        events_tab[j].evt_value =
                                (int) strtol(event_ptr, NULL, 0);
                        events_tab[j].supported = 1;

                        /**
                         * Set event attributes
                         */
                        memset(&events_tab[j].attrs, 0,
                               sizeof(events_tab[0].attrs));
                        events_tab[j].attrs.type =
                                cqm_event_type;
                        events_tab[j].attrs.config =
                                events_tab[j].evt_value;
                        events_tab[j].attrs.size =
                                sizeof(struct perf_event_attr);
                        events_tab[j].attrs.inherit = 1;
                        events_tab[j].attrs.disabled = 0;
                        events_tab[j].attrs.enable_on_exec = 0;
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
        all_evt_mask = events;
        unsigned j;

        /**
         * Log supported events
         */
        LOG_INFO("PID monitoring capability detected\n");
        for (j = 0; j < DIM(events_tab); j++)
                if (events_tab[j].supported)
                        LOG_INFO("PID mon event: %s supported\n",
                                 events_tab[j].desc);
        return PQOS_RETVAL_OK;
}

int
pqos_pid_fini(void)
{
        return PQOS_RETVAL_OK;
}

int
pqos_pid_start(struct pqos_mon_data *group)
{
        char dir_buf[64];
	struct dirent **namelist = NULL;
	int i, ret;
        struct pid_supported_event *pe;
        enum pqos_mon_event started_evts = 0;
        DIR *dir;

        /**
         * Check PID exists
         */
        snprintf(dir_buf, sizeof(dir_buf)-1, "/proc/%d", (int)group->pid);
        dir = opendir(dir_buf);
        if (dir == NULL)
                return PQOS_RETVAL_PARAM;
        closedir(dir);
        memset(dir_buf, 0, sizeof(dir_buf));

        /**
         * Get TID's for each thread
         */
	snprintf(dir_buf, sizeof(dir_buf)-1, "/proc/%d/task", (int)group->pid);
	group->tid_nr = scandir(dir_buf, &namelist, filter, NULL);
	if (group->tid_nr <= 0) {
		LOG_ERROR("Failed to read TID's\n");
		return PQOS_RETVAL_ERROR;
        }
        group->tid_map =
                malloc(sizeof(group->tid_map[0])*group->tid_nr);
        if (group->tid_map == NULL) {
                LOG_ERROR("TID map memory allocation error\n");
                return PQOS_RETVAL_ERROR;
        }
	for (i = 0; i < group->tid_nr; i++)
		group->tid_map[i] = atoi(namelist[i]->d_name);
        free(namelist);

        /**
         * Determine if user selected a PID or TID
         * If TID selected, only monitor events for that thread
         * otherwise monitor all threads in the process
         */
        if (group->pid != group->tid_map[0]) {
                group->tid_nr = 1;
                group->tid_map[0] = group->pid;
        }

        /**
         * Determine selected events and start perf counters
         */
        if (group->event & PQOS_MON_EVENT_L3_OCCUP) {
                if (!is_event_supported(PQOS_MON_EVENT_L3_OCCUP))
                        return PQOS_RETVAL_ERROR;
                pe = get_supported_event(PQOS_MON_EVENT_L3_OCCUP);
                ret = start_counters(group, pe, &group->fds_llc);
                if (ret != PQOS_RETVAL_OK)
                        return PQOS_RETVAL_ERROR;
                started_evts |= PQOS_MON_EVENT_L3_OCCUP;
        }
        if ((group->event & PQOS_MON_EVENT_LMEM_BW) ||
            (group->event & PQOS_MON_EVENT_RMEM_BW)) {
                if (!is_event_supported(PQOS_MON_EVENT_LMEM_BW))
                        return PQOS_RETVAL_ERROR;
                pe = get_supported_event(PQOS_MON_EVENT_LMEM_BW);
                ret = start_counters(group, pe, &group->fds_mbl);
                if (ret != PQOS_RETVAL_OK)
                        return PQOS_RETVAL_ERROR;
                started_evts |= PQOS_MON_EVENT_LMEM_BW;
        }
        if ((group->event & PQOS_MON_EVENT_TMEM_BW) ||
            (group->event & PQOS_MON_EVENT_RMEM_BW)) {
                if (!is_event_supported(PQOS_MON_EVENT_TMEM_BW))
                        return PQOS_RETVAL_ERROR;
                pe = get_supported_event(PQOS_MON_EVENT_TMEM_BW);
                ret = start_counters(group, pe, &group->fds_mbt);
                if (ret != PQOS_RETVAL_OK)
                        return PQOS_RETVAL_ERROR;
                started_evts |= PQOS_MON_EVENT_TMEM_BW;
        }
        if (group->event & PQOS_MON_EVENT_RMEM_BW) {
                group->values.mbm_remote = 0;
                started_evts |= PQOS_MON_EVENT_RMEM_BW;
        }
        /**
         * Check if all selected events were started
         */
        if (group->event ^ started_evts) {
                stop_events(group, started_evts);
                LOG_ERROR("Failed to start all selected "
                          " PID monitoring events\n");
                return PQOS_RETVAL_ERROR;
        }

        return PQOS_RETVAL_OK;
}

int
pqos_pid_stop(struct pqos_mon_data *group)
{
        /**
         * Stop all started events
         */
        return stop_events(group, group->event);
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
                ret = read_counters(group,
                                    &group->values.llc,
                                    group->fds_llc);
                if (ret != PQOS_RETVAL_OK)
                        return PQOS_RETVAL_ERROR;
        }
        if ((group->event & PQOS_MON_EVENT_LMEM_BW) ||
            (group->event & PQOS_MON_EVENT_RMEM_BW)) {
                ret = read_counters(group,
                                    &group->values.mbm_local,
                                    group->fds_mbl);
                if (ret != PQOS_RETVAL_OK)
                        return PQOS_RETVAL_ERROR;
        }
        if ((group->event & PQOS_MON_EVENT_TMEM_BW) ||
            (group->event & PQOS_MON_EVENT_RMEM_BW)) {
                ret = read_counters(group,
                                    &group->values.mbm_total,
                                    group->fds_mbt);
                if (ret != PQOS_RETVAL_OK)
                        return PQOS_RETVAL_ERROR;
        }
        if (group->event & PQOS_MON_EVENT_RMEM_BW) {
                group->values.mbm_remote =
                        group->values.mbm_total -
                        group->values.mbm_local;
        }
        return PQOS_RETVAL_OK;
}
