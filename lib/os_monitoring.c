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
