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
#include <dirent.h>             /**< scandir() */
#include <linux/perf_event.h>

#include "pqos.h"
#include "cap.h"
#include "log.h"
#include "types.h"
#include "os_monitoring.h"
#include "perf.h"

/**
 * Event indexes in table of supported events
 */
#define OS_MON_EVT_IDX_LLC       0
#define OS_MON_EVT_IDX_LMBM      1
#define OS_MON_EVT_IDX_TMBM      2
#define OS_MON_EVT_IDX_RMBM      3
#define OS_MON_EVT_IDX_IPC       4
#define OS_MON_EVT_IDX_LLC_MISS  5

/**
 * Value marking monitoring group structure as "valid".
 * Group becomes "valid" after successful os_mon_start() call.
 */
#define GROUP_VALID_MARKER (0x00DEAD00)

/**
 * ---------------------------------------
 * Local data structures
 * ---------------------------------------
 */
static const struct pqos_cap *m_cap = NULL;
static const struct pqos_cpuinfo *m_cpu = NULL;

/**
 * Monitoring event type
 */
static int os_mon_type = 0;

/**
 * All supported events mask
 */
static enum pqos_mon_event all_evt_mask = 0;

/**
 * Paths to RDT perf event info
 */
static const char *perf_path = "/sys/devices/intel_cqm/";
static const char *perf_events = "events/";
static const char *perf_type = "type";

/**
 * Table of structures used to store data about
 * supported monitoring events and their
 * mapping onto PQoS events
 */
static struct os_supported_event {
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
        { .name = "local_bytes",
          .desc = "Local Memory B/W",
          .event = PQOS_MON_EVENT_LMEM_BW,
          .supported = 0,
          .scale = 1 },
        { .name = "total_bytes",
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
          .supported = 1 }, /**< assumed support */
};

/**
 * @brief Gets event from supported events table
 *
 * @param event events bitmask of selected events
 *
 * @return event from supported event table
 * @retval NULL if not successful
 */
static struct os_supported_event *
get_supported_event(const enum pqos_mon_event event)
{
        switch (event) {
        case PQOS_MON_EVENT_L3_OCCUP:
                return &events_tab[OS_MON_EVT_IDX_LLC];
        case PQOS_MON_EVENT_LMEM_BW:
                return &events_tab[OS_MON_EVT_IDX_LMBM];
        case PQOS_MON_EVENT_TMEM_BW:
                return &events_tab[OS_MON_EVT_IDX_TMBM];
        case PQOS_MON_EVENT_RMEM_BW:
                return &events_tab[OS_MON_EVT_IDX_RMBM];
        case PQOS_PERF_EVENT_IPC:
                return &events_tab[OS_MON_EVT_IDX_IPC];
        case PQOS_PERF_EVENT_LLC_MISS:
                return &events_tab[OS_MON_EVT_IDX_LLC_MISS];
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
        struct os_supported_event *se = get_supported_event(event);

        if (se == NULL) {
                LOG_ERROR("Unsupported event selected\n");
                return 0;
        }
        return se->supported;
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
 * @brief Read perf RDT monitoring type from file system
 *
 * @return Operational Status
 * @retval OK on success
 */
static int
set_mon_type(void)
{
        FILE *fd;
        char file[64], evt[8];

        snprintf(file, sizeof(file)-1, "%s%s", perf_path, perf_type);
        fd = fopen(file, "r");
	if (fd == NULL) {
                LOG_INFO("OS monitoring not supported. "
                         "Kernel version 4.6 or higher required.\n");
                return PQOS_RETVAL_RESOURCE;
	}
        if (fgets(evt, sizeof(evt), fd) == NULL) {
		LOG_ERROR("Failed to read OS monitoring type!\n");
		fclose(fd);
                return PQOS_RETVAL_ERROR;
        }
	fclose(fd);

        os_mon_type = (int) strtol(evt, NULL, 0);
        if (os_mon_type == 0) {
                LOG_ERROR("Failed to convert OS monitoring type!\n");
                return PQOS_RETVAL_ERROR;
        }
        return PQOS_RETVAL_OK;
}

/**
 * @brief Set architectural perf event attributes
 *        in events table and update event mask
 *
 * @param [out] events event mask to be updated
 *
 * @return Operational Status
 * @retval PQOS_RETVAL_OK on success
 */
static int
set_arch_event_attrs(enum pqos_mon_event *events)
{
        struct perf_event_attr attr;

        if (events == NULL)
                return PQOS_RETVAL_PARAM;
        /**
         * Set event attributes
         */
        memset(&attr, 0, sizeof(attr));
        attr.disabled = 1;
        attr.type = PERF_TYPE_HARDWARE;

        /**
         * Set LLC misses event attributes
         */
        events_tab[OS_MON_EVT_IDX_LLC_MISS].attrs = attr;
        events_tab[OS_MON_EVT_IDX_LLC_MISS].attrs.config =
                PERF_COUNT_HW_CACHE_MISSES;

        *events |= PQOS_PERF_EVENT_LLC_MISS;

        /**
         * Set IPC event attributes
         * Note: Set to cycles first
         *       Structure can be reused later to
         *       start instructions event counter
         */
        events_tab[OS_MON_EVT_IDX_IPC].attrs = attr;
        events_tab[OS_MON_EVT_IDX_IPC].attrs.config =
                PERF_COUNT_HW_CPU_CYCLES;

        *events |= PQOS_PERF_EVENT_IPC;

        return PQOS_RETVAL_OK;
}

/**
 * @brief Sets RDT perf event attributes
 *
 * Reads RDT perf event attributes from the file system and sets
 * attribute values in the events table
 *
 * @param idx index of the events table
 * @param fname name of event file
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 */
static int
set_rdt_event_attrs(const int idx, const char *fname)
{
        FILE *fd;
        int config, ret;
        double sf = 0;
        char file[128], buf[32], *p = buf;

        /**
         * Read event type from file system
         */
        snprintf(file, sizeof(file)-1, "%s%s%s", perf_path, perf_events, fname);
        fd = fopen(file, "r");
        if (fd == NULL) {
                LOG_ERROR("Failed to open %s!\n", file);
                return PQOS_RETVAL_ERROR;
        }
        if (fgets(p, sizeof(buf), fd) == NULL) {
                LOG_ERROR("Failed to read OS monitoring event!\n");
                fclose(fd);
                return PQOS_RETVAL_ERROR;
        }
        fclose(fd);
        strsep(&p, "=");
        if (p == NULL) {
                LOG_ERROR("Failed to parse OS monitoring event value!\n");
                return PQOS_RETVAL_ERROR;
        }
        config = (int)strtol(p, NULL, 0);
        p = buf;

        /**
         * Read scale factor from file system
         */
        snprintf(file, sizeof(file)-1, "%s%s%s.scale",
                 perf_path, perf_events, fname);
        fd = fopen(file, "r");
        if (fd == NULL) {
                LOG_ERROR("Failed to open OS monitoring event scale file!\n");
                return PQOS_RETVAL_ERROR;
        }
        ret = fscanf(fd, "%10lf", &sf);
        fclose(fd);
        if (ret < 1) {
                LOG_ERROR("Failed to read OS monitoring event scale factor!\n");
                return PQOS_RETVAL_ERROR;
        }
        events_tab[idx].scale = sf;
        events_tab[idx].supported = 1;

        /**
         * Set event attributes
         */
        memset(&events_tab[idx].attrs, 0, sizeof(events_tab[0].attrs));
        events_tab[idx].attrs.type = os_mon_type;
        events_tab[idx].attrs.config = config;
        events_tab[idx].attrs.size = sizeof(struct perf_event_attr);
        events_tab[idx].attrs.inherit = 1;
        events_tab[idx].attrs.disabled = 0;
        events_tab[idx].attrs.enable_on_exec = 0;

        return PQOS_RETVAL_OK;
}

/**
 * @brief Function to detect OS support for
 *        perf events and update events table
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK on success
 */
static int
set_mon_events(void)
{
        char dir[64];
	int files, i, ret = PQOS_RETVAL_OK;
        enum pqos_mon_event events = 0;
        struct dirent **namelist = NULL;

        /**
         * Read and store event data in table
         */
        snprintf(dir, sizeof(dir)-1, "%s%s", perf_path, perf_events);
        files = scandir(dir, &namelist, filter, NULL);
	if (files <= 0) {
		LOG_ERROR("Failed to read OS monitoring events directory!\n");
		return PQOS_RETVAL_ERROR;
	}
	/**
         * Loop through each file in the RDT perf directory
         */
	for (i = 0; i < files; i++) {
                unsigned j;
		/**
                 * Check if event exists and if
                 * so, set up event attributes
                 */
		for (j = 0; j < DIM(events_tab); j++) {
			if ((strcmp(events_tab[j].name,
                                    namelist[i]->d_name)) != 0)
                                continue;

                        if (set_rdt_event_attrs(j, namelist[i]->d_name)
                            != PQOS_RETVAL_OK) {
                                ret = PQOS_RETVAL_ERROR;
                                goto init_pqos_events_exit;
                        }
                        events |= events_tab[j].event;
		}
	}
        /**
         * If both local and total MBM are supported
         * then remote MBM is also supported
         */
        if (events_tab[OS_MON_EVT_IDX_LMBM].supported &&
            events_tab[OS_MON_EVT_IDX_TMBM].supported) {
                events_tab[OS_MON_EVT_IDX_RMBM].supported = 1;
                events |= events_tab[OS_MON_EVT_IDX_RMBM].event;
        }
        if (events == 0) {
                LOG_ERROR("Failed to find OS monitoring events!\n");
                ret = PQOS_RETVAL_RESOURCE;
                goto init_pqos_events_exit;
        }

        (void) set_arch_event_attrs(&events);

        all_evt_mask |= events;

 init_pqos_events_exit:
        if (files > 0) {
                for (i = 0; i < files; i++)
                        free(namelist[i]);
                free(namelist);
        }
        return ret;
}

/**
 * @brief Update monitoring capability structure with supported events
 *
 * @param cap pqos capability structure
 *
 * @return Operational Status
 * @retval PQOS_RETVAL_OK on success
 */
static int
set_mon_caps(const struct pqos_cap *cap)
{
        int ret;
        unsigned i;
        const struct pqos_capability *p_cap = NULL;

        if (cap == NULL)
                return PQOS_RETVAL_PARAM;

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
                        struct pqos_monitor *mon = &p_cap->u.mon->events[j];

                        if (events_tab[i].event != mon->type)
                                continue;
                        mon->os_support = 1;
                        LOG_INFO("Detected OS monitoring support"
                                 " for %s\n", events_tab[j].desc);
                        break;
                }
        }
        return ret;
}

/**
 * @brief This function starts Perf pqos event counters
 *
 * Used to start pqos counters and request file
 * descriptors used to read the counters
 *
 * @param group monitoring structure
 * @param se supported event structure
 * @param fds array to store fd's
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 */
static int
start_pqos_counters(const struct pqos_mon_data *group,
                    struct os_supported_event *se,
                    int **fds)
{
        int *fd, group_fd = -1;
        unsigned i;
        unsigned long flags = 0;
        pid_t tid = -1;

        ASSERT(group != NULL);
        ASSERT(se != NULL);
        ASSERT(fds != NULL);

        fd = malloc(sizeof(fd[0])*group->num_cores);
        if (fd == NULL)
                return PQOS_RETVAL_ERROR;
        /**
         * For each core assign fd to read counter
         */
        for (i = 0; i < group->num_cores; i++) {
                int ret;

                ret = perf_setup_counter(&se->attrs, tid, group->cores[i],
                                         group_fd, flags, &fd[i]);
                if (ret != PQOS_RETVAL_OK) {
                        LOG_ERROR("Failed to start perf "
                                  "counters for %s\n", se->name);
                        free(fd);
                        return PQOS_RETVAL_ERROR;
                }
        }
        *fds = fd;

        return PQOS_RETVAL_OK;
}

/**
 * @brief Function to stop Perf event counters
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
        int *fd;
        unsigned i;

        ASSERT(group != NULL);
        ASSERT(fds != NULL);

        fd = *fds;
        /**
         * For each counter, close associated file descriptor
         */
        for (i = 0; i < group->num_cores; i++) {
                int ret;

                ret = perf_shutdown_counter(fd[i]);
                if (ret != PQOS_RETVAL_OK)
                        LOG_ERROR("Failed to shutdown perf "
                                  "counters for core: %d\n",
                                  group->cores[i]);
        }
        free(fd);
        *fds = NULL;

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
        if (events ^ (stopped_evts | /* workaround */
                PQOS_PERF_EVENT_IPC | PQOS_PERF_EVENT_LLC_MISS)) {
                LOG_ERROR("Failed to stop all events\n");
                return PQOS_RETVAL_ERROR;
        }
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
        unsigned i;
        uint64_t total_value = 0;

        ASSERT(group != NULL);
        ASSERT(value != NULL);
        ASSERT(fds != NULL);

        /**
         * For each core/TID read counter and
         * return sum of all counter values
         */
        for (i = 0; i < group->num_cores; i++) {
                uint64_t counter_value;
                int ret = perf_read_counter(fds[i], &counter_value);

                if (ret != PQOS_RETVAL_OK)
                        return ret;
                total_value += counter_value;
        }
        *value = total_value;

        return PQOS_RETVAL_OK;
}

/**
 * @brief Gives the difference between two values with regard to the possible
 *        overrun
 *
 * @param old_value previous value
 * @param new_value current value
 * @return difference between the two values
 */
static uint64_t
get_delta(const uint64_t old_value, const uint64_t new_value)
{
        if (old_value > new_value)
                return (UINT64_MAX - old_value) + new_value;
        else
                return new_value - old_value;
}

int
os_mon_init(const struct pqos_cpuinfo *cpu, const struct pqos_cap *cap)
{
        unsigned ret;

	if (cpu == NULL || cap == NULL)
		return PQOS_RETVAL_PARAM;

        /* Set RDT perf attribute type */
	ret = set_mon_type();
        if (ret != PQOS_RETVAL_OK)
                return ret;

        /* Detect and set events */
        ret = set_mon_events();
        if (ret != PQOS_RETVAL_OK)
                return ret;

        /* Update capabilities structure with perf supported events */
        ret = set_mon_caps(cap);
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

        return PQOS_RETVAL_OK;
}

int
os_mon_stop(struct pqos_mon_data *group)
{
        int ret;

        ASSERT(group != NULL);
        /**
         * Stop all started events
         */
        ret = stop_events(group, group->event);
        free(group->cores);
        group->cores = NULL;
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
        enum pqos_mon_event started_evts = 0;
        struct os_supported_event *se;
        int ret = PQOS_RETVAL_OK;
        int retval = PQOS_RETVAL_OK;

        ASSERT(group != NULL);
        ASSERT(cores != NULL);
        ASSERT(num_cores > 0);
        ASSERT(event > 0);
        ASSERT(m_cpu != NULL);

        /**
         * Validate event parameter
         * - only combinations of events allowed
         * - do not allow non-PQoS events to be monitored on its own
         */
        if (event & (~(PQOS_MON_EVENT_L3_OCCUP | PQOS_MON_EVENT_LMEM_BW |
                       PQOS_MON_EVENT_TMEM_BW | PQOS_MON_EVENT_RMEM_BW |
                       PQOS_PERF_EVENT_IPC | PQOS_PERF_EVENT_LLC_MISS))) {
                return PQOS_RETVAL_PARAM;
        }
        if ((event & (PQOS_MON_EVENT_L3_OCCUP | PQOS_MON_EVENT_LMEM_BW |
                      PQOS_MON_EVENT_TMEM_BW | PQOS_MON_EVENT_RMEM_BW)) == 0 &&
            (event & (PQOS_PERF_EVENT_IPC | PQOS_PERF_EVENT_LLC_MISS)) != 0) {
                return PQOS_RETVAL_PARAM;
        }

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

        /**
         * Determine selected events and start Perf counters
         */
        if (group->event & PQOS_MON_EVENT_L3_OCCUP) {
                if (!is_event_supported(PQOS_MON_EVENT_L3_OCCUP))
                        return PQOS_RETVAL_ERROR;
                se = get_supported_event(PQOS_MON_EVENT_L3_OCCUP);
                ret = start_pqos_counters(group, se, &group->fds_llc);
                if (ret != PQOS_RETVAL_OK) {
                        retval = PQOS_RETVAL_ERROR;
                        goto pqos_mon_start_error;
                }
                started_evts |= PQOS_MON_EVENT_L3_OCCUP;
        }
        if (group->event & PQOS_MON_EVENT_LMEM_BW) {
                if (!is_event_supported(PQOS_MON_EVENT_LMEM_BW))
                        return PQOS_RETVAL_ERROR;
                se = get_supported_event(PQOS_MON_EVENT_LMEM_BW);
                ret = start_pqos_counters(group, se, &group->fds_mbl);
                if (ret != PQOS_RETVAL_OK) {
                        retval = PQOS_RETVAL_ERROR;
                        goto pqos_mon_start_error;
                }
                started_evts |= PQOS_MON_EVENT_LMEM_BW;
        }
        if (group->event & PQOS_MON_EVENT_TMEM_BW) {
                if (!is_event_supported(PQOS_MON_EVENT_TMEM_BW))
                        return PQOS_RETVAL_ERROR;
                se = get_supported_event(PQOS_MON_EVENT_TMEM_BW);
                ret = start_pqos_counters(group, se, &group->fds_mbt);
                if (ret != PQOS_RETVAL_OK) {
                        retval = PQOS_RETVAL_ERROR;
                        goto pqos_mon_start_error;
                }
                started_evts |= PQOS_MON_EVENT_TMEM_BW;
        }
        if (group->event & PQOS_MON_EVENT_RMEM_BW) {
                if (!is_event_supported(PQOS_MON_EVENT_LMEM_BW) ||
                    !is_event_supported(PQOS_MON_EVENT_TMEM_BW)) {
                        retval = PQOS_RETVAL_ERROR;
                        goto pqos_mon_start_error;
                }
                if ((started_evts & PQOS_MON_EVENT_LMEM_BW) == 0) {
                        se = get_supported_event(PQOS_MON_EVENT_LMEM_BW);
                        ret = start_pqos_counters(group, se, &group->fds_mbl);
                        if (ret != PQOS_RETVAL_OK) {
                                retval = PQOS_RETVAL_ERROR;
                                goto pqos_mon_start_error;
                        }
                }
                if ((started_evts & PQOS_MON_EVENT_TMEM_BW) == 0) {
                        se = get_supported_event(PQOS_MON_EVENT_TMEM_BW);
                        ret = start_pqos_counters(group, se, &group->fds_mbt);
                        if (ret != PQOS_RETVAL_OK) {
                                retval = PQOS_RETVAL_ERROR;
                                goto pqos_mon_start_error;
                        }
                }
                group->values.mbm_remote = 0;
                started_evts |= PQOS_MON_EVENT_RMEM_BW;
        }
        /**
         * Check if all selected events were started
         * Note: Temporary workaround needed here to avoid
         *       checking Non-RDT events
         */
        if ((group->event != (started_evts | /* workaround */
                PQOS_PERF_EVENT_IPC | PQOS_PERF_EVENT_LLC_MISS))) {
                stop_events(group, started_evts);
                LOG_ERROR("Failed to start all selected "
                          "OS monitoring events\n");
                retval = PQOS_RETVAL_ERROR;
        }

pqos_mon_start_error:
        if (retval != PQOS_RETVAL_OK) {
                if (group->cores != NULL)
                        free(group->cores);
        } else
                group->valid = GROUP_VALID_MARKER;

        return retval;
}

/**
 * @brief This function reads all perf counters
 *
 * Reads counters for all events and stores values
 *
 * @param group monitoring structure
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 * @retval PQOS_RETVAL_ERROR if error occurs
 */
static int
os_mon_perf_poll(struct pqos_mon_data *group)
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
                        events_tab[OS_MON_EVT_IDX_LLC].scale;
        }
        if ((group->event & PQOS_MON_EVENT_LMEM_BW) ||
            (group->event & PQOS_MON_EVENT_RMEM_BW)) {
                uint64_t old_value = group->values.mbm_local;

                ret = read_pqos_counters(group,
                                         &group->values.mbm_local,
                                         group->fds_mbl);
                if (ret != PQOS_RETVAL_OK)
                        return PQOS_RETVAL_ERROR;
                group->values.mbm_local_delta =
                        get_delta(old_value, group->values.mbm_local);
        }
        if ((group->event & PQOS_MON_EVENT_TMEM_BW) ||
            (group->event & PQOS_MON_EVENT_RMEM_BW)) {
                uint64_t old_value = group->values.mbm_total;

                ret = read_pqos_counters(group,
                                         &group->values.mbm_total,
                                         group->fds_mbt);
                if (ret != PQOS_RETVAL_OK)
                        return PQOS_RETVAL_ERROR;
                group->values.mbm_total_delta =
                        get_delta(old_value, group->values.mbm_total);
        }
        if (group->event & PQOS_MON_EVENT_RMEM_BW) {
                group->values.mbm_remote_delta = 0;
                if (group->values.mbm_total_delta >
                    group->values.mbm_local_delta)
                        group->values.mbm_remote_delta =
                                group->values.mbm_total_delta -
                                group->values.mbm_local_delta;
        }
        return PQOS_RETVAL_OK;
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
                ret = os_mon_perf_poll(groups[i]);
                if (ret != PQOS_RETVAL_OK)
                        LOG_WARN("Failed to read event on "
                                 "group number %u\n", i);
        }

        return PQOS_RETVAL_OK;
}
