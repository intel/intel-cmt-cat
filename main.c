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
 * @brief Platform QoS utility module
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <ctype.h>                                      /**< isspace() */
#include <sys/types.h>                                  /**< open() */
#include <sys/stat.h>
#include <sys/ioctl.h>                                  /**< terminal ioctl */
#include <sys/time.h>                                   /**< gettimeofday() */
#include <time.h>                                       /**< localtime() */
#include <fcntl.h>

#include "pqos.h"
#include "profiles.h"

#ifdef DEBUG
#include <assert.h>
#endif

/**
 * Macros
 */
#ifndef DIM
#define DIM(x) (sizeof(x)/sizeof(x[0]))
#endif

#ifdef DEBUG
#define ASSERT assert
#else
#define ASSERT(x)
#endif

#define PQOS_MAX_SOCKETS      8
#define PQOS_MAX_SOCKET_CORES 64
#define PQOS_MAX_CORES        (PQOS_MAX_SOCKET_CORES*PQOS_MAX_SOCKETS)
#define PQOS_MAX_PIDS         128
#define PQOS_MON_EVENT_ALL    -1

/**
 * Defines used to identify CAT mask definitions
 */
#define CAT_UPDATE_SCOPE_BOTH 0    /**< update COS code & data masks */
#define CAT_UPDATE_SCOPE_DATA 1    /**< update COS data mask */
#define CAT_UPDATE_SCOPE_CODE 2    /**< update COS code mask */

/**
 * Local data structures
 *
 */
const char *xml_root_open = "<records>";
const char *xml_root_close = "</records>";
const char *xml_child_open = "<record>";
const char *xml_child_close = "</record>";
const long xml_root_close_size = DIM("</records>") - 1;

/**
 * Default CDP configuration option - don't enforce on or off
 */
static enum pqos_cdp_config selfn_cdp_config = PQOS_REQUIRE_CDP_ANY;

/**
 * Free RMID's being in use
 */
static int sel_free_in_use_rmid = 0;

/**
 * Reset CAT configuration
 */
static int sel_reset_CAT = 0;

/**
 * Number of cores that are selected in config string
 * for monitoring LLC occupancy
 */
static int sel_monitor_num = 0;

/**
 * The mask to tell which events to display
 */
static enum pqos_mon_event sel_events_max = 0;

/**
 * Maintains a table of core, event, number of events that are selected in
 * config string for monitoring LLC occupancy
 */
static struct core_group {
        char *desc;
        int num_cores;
        unsigned *cores;
        struct pqos_mon_data *pgrp;
        enum pqos_mon_event events;
} sel_monitor_core_tab[PQOS_MAX_CORES];

static struct pqos_mon_data *m_mon_grps[PQOS_MAX_CORES];

/**
 * Maintains a table of process id, event, number of events that are selected
 * in config string for monitoring
 */
static struct {
        pid_t pid;
        struct pqos_mon_data *pgrp;
        enum pqos_mon_event events;
} sel_monitor_pid_tab[PQOS_MAX_PIDS];

/**
 * Maintains the number of process id's you want to track
 */
static int sel_process_num = 0;

/**
 * Maintains number of Class of Services supported by socket for
 * L3 cache allocation
 */
static int sel_l3ca_cos_num = 0;

/**
 * Maintains table for L3 cache allocation class of service data structure
 */
static struct pqos_l3ca sel_l3ca_cos_tab[PQOS_MAX_L3CA_COS];

/**
 * Number of cores selected for cache allocation association
 */
static int sel_l3ca_assoc_num = 0;

/**
 * Maintains a table of core and class_id that are selected in config string for
 * setting up allocation policy per core
 */
static struct {
        unsigned core;
        unsigned class_id;
} sel_l3ca_assoc_tab[PQOS_MAX_CORES];

/**
 * Maintains monitoring interval that is selected in config string for
 * monitoring L3 occupancy
 */
static int sel_mon_interval = 10; /**< 10 = 10x100ms = 1s */

/**
 * Maintains TOP like output that is selected in config string for
 * monitoring L3 occupancy
 */
static int sel_mon_top_like = 0;

/**
 * Maintains monitoring time that is selected in config string for
 * monitoring L3 occupancy
 */
static int sel_timeout = -1;

/**
 * Enable showing cache allocation settings
 */
static int sel_show_allocation_config = 0;

/**
 * Maintains selected monitoring output file name
 */
static char *sel_output_file = NULL;

/**
 * Maintains selected type of monitoring output file
 */
static char *sel_output_type = NULL;

/**
 * Maintains pointer to selected log file name
 */
static char *sel_log_file = NULL;

/**
 * Maintains pointer to selected config file
 */
static char *sel_config_file = NULL;

/**
 * Maintains pointer to allocation profile from internal DB
 */
static char *sel_allocation_profile = NULL;

/**
 * Maintains verbose mode choice selected in config string
 */
static int sel_verbose_mode = 0;

/**
 * Function prototypes
 */
static void selfn_strdup(char **sel, const char *arg);

/**
 * @brief Check to determine if processes or cores are monitored
 *
 * @return Process monitoring mode status
 * @retval 0 monitoring cores
 * @retval 1 monitoring processes
 */
static inline int
process_mode(void)
{
        return (sel_process_num <= 0) ? 0 : 1;
}

/**
 * @brief Function to safely translate an unsigned int
 *        value to a string
 *
 * @param val value to be translated
 *
 * @return Pointer to allocated string
 */
static char*
uinttostr(const unsigned val)
{
        char buf[16], *str = NULL;

        memset(buf, 0, sizeof(buf));
        snprintf(buf, sizeof(buf), "%u", val);
        selfn_strdup(&str, buf);

        return str;
}

/**
 * @brief Function to check if a value is already contained in a table
 *
 * @param tab table of values to check
 * @param size size of the table
 * @param val value to search for
 *
 * @return If the value is already in the table
 * @retval 1 if value if found
 * @retval 0 if value is not found
 */
static int
isdup(const uint64_t *tab, const unsigned size, const uint64_t val)
{
        unsigned i;

        for (i = 0; i < size; i++)
                if (tab[i] == val)
                        return 1;
        return 0;
}

/**
 * @brief Function to set cores group values
 *
 * @param cg pointer to core_group structure
 * @param desc string containing core group description
 * @param cores pointer to table of core values
 * @param num_cores number of cores contained in the table
 *
 * @return Operational status
 * @retval 0 on success
 * @retval -1 on error
 */
static int
set_cgrp(struct core_group *cg, char *desc,
         const uint64_t *cores, const int num_cores)
{
        int i;

        ASSERT(cg != NULL);
        ASSERT(desc != NULL);
        ASSERT(cores != NULL);
        ASSERT(num_cores > 0);

        cg->desc = desc;
        cg->cores = malloc(sizeof(unsigned)*num_cores);
        if (cg->cores == NULL) {
                printf("Error allocating core group table\n");
                return -1;
        }
        cg->num_cores = num_cores;

        /**
         * Transfer cores from buffer to table
         */
        for (i = 0; i < num_cores; i++)
                cg->cores[i] = (unsigned)cores[i];

        return 0;
}

/**
 * @brief Converts string into 64-bit unsigned number.
 *
 * Numbers can be in decimal or hexadecimal format.
 *
 * On error, this functions causes process to exit with FAILURE code.
 *
 * @param s string to be converted into 64-bit unsigned number
 *
 * @return Numeric value of the string representing the number
 */
static uint64_t
strtouint64(const char *s)
{
        const char *str = s;
        int base = 10;
        uint64_t n = 0;
        char *endptr = NULL;

        ASSERT(s != NULL);

        if (strncasecmp(s, "0x", 2) == 0) {
                base = 16;
                s += 2;
        }

        n = strtoull(s, &endptr, base);

        if (!(*s != '\0' && *endptr == '\0')) {
                printf("Error converting '%s' to unsigned number!\n", str);
                exit(EXIT_FAILURE);
        }

        return n;
}

/**
 * @brief Converts string of characters representing list of
 *        numbers into table of numbers.
 *
 * Allowed formats are:
 *     0,1,2,3
 *     0-10,20-18
 *     1,3,5-8,10,0x10-12
 *
 * Numbers can be in decimal or hexadecimal format.
 *
 * On error, this functions causes process to exit with FAILURE code.
 *
 * @param s string representing list of unsigned numbers.
 * @param tab table to put converted numeric values into
 * @param max maximum number of elements that \a tab can accommodate
 *
 * @return Number of elements placed into \a tab
 */
static unsigned
strlisttotab(char *s, uint64_t *tab, const unsigned max)
{
        unsigned index = 0;
        char *saveptr = NULL;

        if (s == NULL || tab == NULL || max == 0)
                return index;

        for (;;) {
                char *p = NULL;
                char *token = NULL;

                token = strtok_r(s, ",", &saveptr);
                if (token == NULL)
                        break;

                s = NULL;

                /* get rid of leading spaces & skip empty tokens */
                while (isspace(*token))
                        token++;
                if (*token == '\0')
                        continue;

                p = strchr(token, '-');
                if (p != NULL) {
                        /**
                         * range of numbers provided
                         * example: 1-5 or 12-9
                         */
                        uint64_t n, start, end;
                        *p = '\0';
                        start = strtouint64(token);
                        end = strtouint64(p+1);
                        if (start > end) {
                                /**
                                 * no big deal just swap start with end
                                 */
                                n = start;
                                start = end;
                                end = n;
                        }
                        for (n = start; n <= end; n++) {
                                if (!(isdup(tab, index, n))) {
                                        tab[index] = n;
                                        index++;
                                }
                                if (index >= max)
                                        return index;
                        }
                } else {
                        /**
                         * single number provided here
                         * remove duplicates if necessary
                         */
                        uint64_t val = strtouint64(token);

                        if (!(isdup(tab, index, val))) {
                                tab[index] = val;
                                index++;
                        }
                        if (index >= max)
                                return index;
                }
        }

        return index;
}

/**
 * @brief Function to set the descriptions and cores for each core group
 *
 * Takes a string containing individual cores and groups of cores and
 * breaks it into substrings which are used to set core group values
 *
 * @param s string containing cores to be divided into substrings
 * @param tab table of core groups to set values in
 * @param max maximum number of core groups allowed
 *
 * @return Number of core groups set up
 * @retval -1 on error
 */
static int
strtocgrps(char *s, struct core_group *tab, const unsigned max)
{
        unsigned i, n, index = 0;
        uint64_t cbuf[PQOS_MAX_CORES];
        char *non_grp = NULL;

        ASSERT(tab != NULL);
        ASSERT(max > 0);

        if (s == NULL)
                return index;

        while ((non_grp = strsep(&s, "[")) != NULL) {
                int ret;
                /**
                 * If group contains single core
                 */
                if ((strlen(non_grp)) > 0) {
                        n = strlisttotab(non_grp, cbuf, (max-index));
                        if ((index+n) > max)
                                return -1;
                        /* set core group info */
                        for (i = 0; i < n; i++) {
                                char *desc = uinttostr((unsigned)cbuf[i]);

                                ret = set_cgrp(&tab[index], desc, &cbuf[i], 1);
                                if (ret < 0)
                                        return -1;
                                index++;
                        }
                }
                /**
                 * If group contains multiple cores
                 */
                char *grp = strsep(&s, "]");

                if (grp != NULL) {
                        char *desc = NULL;

                        selfn_strdup(&desc, grp);
                        n = strlisttotab(grp, cbuf, (max-index));
                        if (index+n > max)
                                return -1;
                        /* set core group info */
                        ret = set_cgrp(&tab[index], desc, cbuf, n);
                        if (ret < 0)
                                return -1;
                        index++;
                }
        }

        return index;
}

/**
 * @brief Function to compare cores in 2 core groups
 *
 * This function takes 2 core groups and compares their core values
 *
 * @param cg_a pointer to core group a
 * @param cg_b pointer to core group b
 *
 * @return Whether both groups contain some/none/all of the same cores
 * @retval 1 if both groups contain the same cores
 * @retval 0 if none of their cores match
 * @retval -1 if some but not all cores match
 */
static int
cmp_cgrps(const struct core_group *cg_a,
          const struct core_group *cg_b)
{
        int i, found = 0;
        const int sz_a = cg_a->num_cores;
        const int sz_b = cg_b->num_cores;
        const unsigned *tab_a = cg_a->cores;
        const unsigned *tab_b = cg_b->cores;

        ASSERT(cg_a != NULL);
        ASSERT(cg_b != NULL);

        for (i = 0; i < sz_a; i++) {
                int j;

                for (j = 0; j < sz_b; j++)
                        if (tab_a[i] == tab_b[j])
                                found++;
        }
        /* if no cores are the same */
        if (!found)
                return 0;
        /* if group contains same cores */
        if (sz_a == sz_b && sz_b == found)
                return 1;
        /* if not all cores are the same */
        return -1;
}

/**
 * @brief Common function to handle string parsing errors
 *
 * On error, this function causes process to exit with FAILURE code.
 *
 * @param arg string that caused error when parsing
 * @param note context and information about encountered error
 */
static void
parse_error(const char *arg, const char *note)
{
        printf("Error parsing \"%s\" command line argument. %s\n",
               arg ? arg : "<null>",
               note ? note : "");
        exit(EXIT_FAILURE);
}

/**
 * @brief Common function to parse selected events
 *
 * @param str string of the event
 * @param evt pointer to the selected events so far
 */

static void
parse_event(char *str, enum pqos_mon_event *evt)
{
        ASSERT(str != NULL);
        ASSERT(evt != NULL);
        /**
         * Set event value and sel_event_max which determines
         * what events to display (out of all possible)
         */
        if (strncasecmp(str, "llc:", 4) == 0) {
		*evt = PQOS_MON_EVENT_L3_OCCUP;
                sel_events_max |= *evt;
        } else if (strncasecmp(str, "mbr:", 4) == 0) {
                *evt = PQOS_MON_EVENT_RMEM_BW;
                sel_events_max |= *evt;
        } else if (strncasecmp(str, "mbl:", 4) == 0) {
                *evt = PQOS_MON_EVENT_LMEM_BW;
                sel_events_max |= *evt;
        } else if (strncasecmp(str, "all:", 4) == 0 ||
                   strncasecmp(str, ":", 1) == 0)
                *evt = PQOS_MON_EVENT_ALL;
        else
                parse_error(str, "Unrecognized monitoring event type");
}

/**
 * @brief Verifies and translates monitoring config string into
 *        internal monitoring configuration.
 *
 * @param str string passed to -m command line option
 */
static void
parse_monitor_event(char *str)
{
        int i = 0, n = 0;
        enum pqos_mon_event evt = 0;
        struct core_group cgrp_tab[PQOS_MAX_CORES];

        parse_event(str, &evt);

        n = strtocgrps(strchr(str, ':') + 1,
                       cgrp_tab, PQOS_MAX_CORES);
        if (n < 0) {
                printf("Error: Too many cores selected\n");
                exit(EXIT_FAILURE);
        }
        /**
         *  For each core group we are processing:
         *  - if it's already in the sel_monitor_core_tab
         *    =>  update the entry
         *  - else
         *    => add it to the sel_monitor_core_tab
         */
        for (i = 0; i < n; i++) {
                int j, found;

                for (found = 0, j = 0; j < sel_monitor_num && found == 0; j++) {
                        found = cmp_cgrps(&sel_monitor_core_tab[j],
                                             &cgrp_tab[i]);
                        if (found < 0) {
                                printf("Error: cannot monitor same "
                                       "cores in different groups\n");
                                exit(EXIT_FAILURE);
                        }
                        if (found)
                                sel_monitor_core_tab[j].events |= evt;
                }
                if (!found) {
                        sel_monitor_core_tab[sel_monitor_num] = cgrp_tab[i];
                        sel_monitor_core_tab[sel_monitor_num].events = evt;
			m_mon_grps[sel_monitor_num] =
                                malloc(sizeof(**m_mon_grps));
                        if (m_mon_grps[sel_monitor_num] == NULL) {
                                printf("Error with memory allocation");
                                exit(EXIT_FAILURE);
                        }
                        sel_monitor_core_tab[sel_monitor_num].pgrp =
                                m_mon_grps[sel_monitor_num];
                        ++sel_monitor_num;
                }
        }
        return;
}

/**
 * @brief Verifies and translates multiple monitoring config strings into
 *        internal core monitoring configuration
 *
 * @param str string passed to -m command line option
 */
static void
selfn_monitor_cores(const char *arg)
{
        char *cp = NULL, *str = NULL;
        char *saveptr = NULL;

        if (arg == NULL)
                parse_error(arg, "NULL pointer!");

        if (strlen(arg) <= 0)
                parse_error(arg, "Empty string!");
        /**
         * The parser will add to the display only necessary columns
         */
        sel_events_max = 0;

        selfn_strdup(&cp, arg);

        for (str = cp; ; str = NULL) {
                char *token = NULL;

                token = strtok_r(str, ";", &saveptr);
                if (token == NULL)
                        break;
                parse_monitor_event(token);
        }

        free(cp);
}

/**
 * @brief Starts monitoring on selected cores
 *
 * @param cpu_info cpu information structure
 *
 * @return Operation status
 * @retval 0 OK
 * @retval -1 error
 */
static int
setup_monitoring(const struct pqos_cpuinfo *cpu_info,
                 const struct pqos_capability const *cap_mon)
{
        unsigned i;
        int ret;
        enum pqos_mon_event all_events = 0;

        ASSERT(sel_monitor_num >= 0);
        ASSERT(sel_process_num >= 0);

        /* get all available events */
        for (i = 0; i < cap_mon->u.mon->num_events; i++)
                all_events |= (cap_mon->u.mon->events[i].type);
        /**
         * If no cores and events selected through command line
         * by default let's monitor all cores
         */
        if (sel_monitor_num == 0 && sel_process_num == 0) {
	        sel_events_max = all_events;
                for (i = 0; i < cpu_info->num_cores; i++) {
                        unsigned lcore  = cpu_info->cores[i].lcore;
                        uint64_t core = (uint64_t)lcore;

                        ret = set_cgrp(&sel_monitor_core_tab[sel_monitor_num],
                                       uinttostr(lcore), &core, 1);
                        sel_monitor_core_tab[sel_monitor_num].events =
			        sel_events_max;
			m_mon_grps[sel_monitor_num] =
			        malloc(sizeof(**m_mon_grps));
			if (m_mon_grps[sel_monitor_num] == NULL) {
			        printf("Error with memory allocation");
				exit(EXIT_FAILURE);
			}
			sel_monitor_core_tab[sel_monitor_num].pgrp =
			        m_mon_grps[sel_monitor_num];
                        sel_monitor_num++;
                }
        }

	/* check for process and core tracking, can not have both */
	if (sel_process_num > 0 && sel_monitor_num > 0) {
	        /**
		 * The error raised also if two instances of PQoS
		 * attempt to use the same core id.
		 */
		printf("Monitoring start error, process and core"
		       " tracking can not be done simultaneously\n");
		return -1;
	}

        /* check for processes tracking */
	if (!process_mode()) {
                const enum pqos_mon_event evt_all =
                        (enum pqos_mon_event)PQOS_MON_EVENT_ALL;
                /**
                 * Make calls to pqos_mon_start - track cores
                 */
                for (i = 0; i < (unsigned)sel_monitor_num; i++) {
                        struct core_group *cg = &sel_monitor_core_tab[i];

                        /* check if all available events were selected */
                        if (cg->events == evt_all) {
                                cg->events = all_events;
                                sel_events_max |= all_events;
                        }
                        ret = pqos_mon_start(cg->num_cores, cg->cores,
                                             cg->events, (void *)cg->desc,
                                             cg->pgrp);
                        ASSERT(ret == PQOS_RETVAL_OK);
                        /**
                         * The error raised also if two instances of PQoS
                         * attempt to use the same core id.
                         */
                        if (ret != PQOS_RETVAL_OK) {
                                printf("Monitoring start error on core(s) "
                                       "%s, status %d\n",
                                       cg->desc, ret);
                                return -1;
                        }
                }
	} else {
                /**
                 * Make calls to pqos_mon_start_pid - track PIDs
                 */
                for (i = 0; i < (unsigned)sel_process_num; i++) {
                        ret = pqos_mon_start_pid(sel_monitor_pid_tab[i].pid,
                                                 sel_monitor_pid_tab[i].events,
                                                 NULL,
                                                 sel_monitor_pid_tab[i].pgrp);
                        ASSERT(ret == PQOS_RETVAL_OK);
                        /**
                         * Any problem with monitoring the process?
                         */
                        if (ret != PQOS_RETVAL_OK) {
                                printf("PID %d monitoring start error,"
                                       "status %d\n",
                                       sel_monitor_pid_tab[i].pid, ret);
                                return -1;
                        }
                }
	}
        return 0;
}

/**
 * @brief Stops monitoring on selected cores
 *
 */
static void
stop_monitoring(void)
{
        unsigned i, mon_number;
        int ret;

	if (!process_mode())
	        mon_number = (unsigned)sel_monitor_num;
	else
	        mon_number = (unsigned)sel_process_num;

	for (i = 0; i < mon_number; i++) {
	        ret = pqos_mon_stop(m_mon_grps[i]);
		ASSERT(ret == PQOS_RETVAL_OK);
		if (ret != PQOS_RETVAL_OK)
		        printf("Monitoring stop error!\n");
	}
        if (!process_mode()) {
                for (i = 0; (int)i < sel_monitor_num; i++) {
                        free(sel_monitor_core_tab[i].desc);
                        free(sel_monitor_core_tab[i].cores);
                }
        }
}

/**
 * @brief Sets up allocation classes of service on selected CPU sockets
 *
 * @param sock_count number of CPU sockets
 * @param sockets arrays with CPU socket id's
 *
 * @return Number of classes of service set
 * @retval 0 no class of service set (nor selected)
 * @retval negative error
 * @retval positive success
 */
static int
set_allocation_class(unsigned sock_count,
                     const unsigned *sockets)
{
        int ret;

        while (sock_count > 0 && sel_l3ca_cos_num > 0) {
                ret = pqos_l3ca_set(*sockets,
                                    (unsigned)sel_l3ca_cos_num,
                                    sel_l3ca_cos_tab);
                ASSERT(ret == PQOS_RETVAL_OK);
                if (ret != PQOS_RETVAL_OK) {
                        printf("Setting up cache allocation class of service "
                               "failed!\n");
                        return -1;
                }
                sock_count--;
                sockets++;
        }

        return sel_l3ca_cos_num;
}

/**
 * @brief Converts string describing CAT COS into ID and mask scope
 *
 * Current string format is: <ID>[CcDd]
 *
 * Some examples:
 *  1d  - class 1, data mask
 *  5C  - class 5, code mask
 *  0   - class 0, common mask for code & data
 *
 * @param [in] str string describing CAT COS. Function may modify
 *             the last character of the string in some conditions.
 * @param [out] scope indicates if string \a str refers to both COS masks
 *              or just one of them
 * @param [out] id class ID referred to in the string \a str
 */
static void
parse_cos_mask_type(char *str, int *scope, unsigned *id)
{
        size_t len = 0;

        ASSERT(str != NULL);
        ASSERT(scope != NULL);
        ASSERT(id != NULL);
        len = strlen(str);
        if (len > 1 && (str[len - 1] == 'c' || str[len - 1] == 'C')) {
                *scope = CAT_UPDATE_SCOPE_CODE;
                str[len - 1] = '\0';
        } else if (len > 1 && (str[len - 1] == 'd' || str[len - 1] == 'D')) {
                *scope = CAT_UPDATE_SCOPE_DATA;
                str[len - 1] = '\0';
        } else {
                *scope = CAT_UPDATE_SCOPE_BOTH;
        }
        *id = (unsigned) strtouint64(str);
}

/**
 * @brief Verifies and translates definition of single
 *        allocation class of service
 *        from text string into internal configuration.
 *
 * @param str fragment of string passed to -e command line option
 */
static void
parse_allocation_cos(char *str)
{
        char *p = NULL;
        unsigned class_id = 0;
        uint64_t mask = 0;
        int j = 0;
        int update_scope = CAT_UPDATE_SCOPE_BOTH;

        p = strchr(str, '=');
        if (p == NULL)
                parse_error(str, "invalid class of service definition");
        *p = '\0';

        parse_cos_mask_type(str, &update_scope, &class_id);
        mask = strtouint64(p+1);

        for (j = 0; j < sel_l3ca_cos_num; j++)
                if (sel_l3ca_cos_tab[j].class_id == class_id)
                        break;

        if (j < sel_l3ca_cos_num) {
                /**
                 * This class is already on the list.
                 * Don't allow for mixing CDP and non-CDP formats.
                 */
                if ((sel_l3ca_cos_tab[j].cdp &&
                     update_scope == CAT_UPDATE_SCOPE_BOTH) ||
                    (!sel_l3ca_cos_tab[j].cdp &&
                     update_scope != CAT_UPDATE_SCOPE_BOTH)) {
                        printf("error: COS%u defined twice using CDP and "
                               "non-CDP format\n", class_id);
                        exit(EXIT_FAILURE);
                }

                switch (update_scope) {
                case CAT_UPDATE_SCOPE_BOTH:
                        sel_l3ca_cos_tab[j].ways_mask = mask;
                        break;
                case CAT_UPDATE_SCOPE_CODE:
                        sel_l3ca_cos_tab[j].code_mask = mask;
                        break;
                case CAT_UPDATE_SCOPE_DATA:
                        sel_l3ca_cos_tab[j].data_mask = mask;
                        break;
                }
        } else {
                /**
                 * New class selected - extend the list
                 */
                unsigned k = (unsigned) sel_l3ca_cos_num;

                if (k >= DIM(sel_l3ca_cos_tab))
                        parse_error(str,
                                    "too many allocation classes selected");

                sel_l3ca_cos_tab[k].class_id = class_id;

                switch (update_scope) {
                case CAT_UPDATE_SCOPE_BOTH:
                        sel_l3ca_cos_tab[k].cdp = 0;
                        sel_l3ca_cos_tab[k].ways_mask = mask;
                        break;
                case CAT_UPDATE_SCOPE_CODE:
                        sel_l3ca_cos_tab[k].cdp = 1;
                        sel_l3ca_cos_tab[k].code_mask = mask;
                        /**
                         * This will result in error during set operation
                         * if data mask is not defined by the user.
                         */
                        sel_l3ca_cos_tab[k].data_mask = (uint64_t) (-1LL);
                        break;
                case CAT_UPDATE_SCOPE_DATA:
                        sel_l3ca_cos_tab[k].cdp = 1;
                        sel_l3ca_cos_tab[k].data_mask = mask;
                        /**
                         * This will result in error during set operation
                         * if code mask is not defined by the user.
                         */
                        sel_l3ca_cos_tab[k].code_mask = (uint64_t) (-1LL);
                        break;
                }

                sel_l3ca_cos_num++;
        }
}

/**
 * @brief Verifies and translates definition of allocation class of service
 *        from text string into internal configuration.
 *
 * @param str string passed to -e command line option
 */
static void
parse_allocation_class(char *str)
{
        char *p = NULL;
        char *saveptr = NULL;

        if (strncasecmp(str, "llc:", 4) != 0)
                parse_error(str, "Unrecognized allocation type");

        for (p = str+strlen("llc:"); ; p = NULL) {
                char *token = NULL;

                token = strtok_r(p, ",", &saveptr);
                if (token == NULL)
                        break;
                parse_allocation_cos(token);
        }
}

/**
 * @brief Defines allocation class of service
 *
 * @param arg string passed to -e command line option
 */
static void
selfn_allocation_class(const char *arg)
{
        char *cp = NULL, *str = NULL;
        char *saveptr = NULL;

        if (arg == NULL)
                parse_error(arg, "NULL pointer!");

        if (strlen(arg) <= 0)
                parse_error(arg, "Empty string!");

        selfn_strdup(&cp, arg);

        for (str = cp; ; str = NULL) {
                char *token = NULL;

                token = strtok_r(str, ";", &saveptr);
                if (token == NULL)
                        break;
                parse_allocation_class(token);
        }

        free(cp);
}

/**
 * @brief Sets up association between cores and allocation classes of service
 *
 * @return Number of associations made
 * @retval 0 no association made (nor requested)
 * @retval negative error
 * @retval positive success
 */
static int
set_allocation_assoc(void)
{
        int i;
        int ret;

        for (i = 0; i < sel_l3ca_assoc_num; i++) {
                ret = pqos_l3ca_assoc_set(sel_l3ca_assoc_tab[i].core,
                                          sel_l3ca_assoc_tab[i].class_id);
                ASSERT(ret == PQOS_RETVAL_OK);
                if (ret != PQOS_RETVAL_OK) {
                        printf("Setting allocation class of service "
                               "association failed!\n");
                        return -1;
                }
        }

        return sel_l3ca_assoc_num;
}

/**
 * @brief Verifies and translates allocation association config string into
 *        internal configuration.
 *
 * @param str string passed to -a command line option
 */
static void
parse_allocation_assoc(char *str)
{
        uint64_t cores[PQOS_MAX_CORES];
        unsigned i = 0, n = 0, cos = 0;
        char *p = NULL;

        if (strncasecmp(str, "llc:", 4) != 0)
                parse_error(str, "Unrecognized allocation type");

        str += strlen("llc:");
        p = strchr(str, '=');
        if (p == NULL)
                parse_error(str,
                            "Invalid allocation class of service "
                            "association format");
        *p = '\0';

        cos = (unsigned) strtouint64(str);

        n = strlisttotab(p+1, cores, DIM(cores));
        if (n == 0)
                return;

        if (sel_l3ca_assoc_num <= 0) {
                for (i = 0; i < n; i++) {
                        if (i >= DIM(sel_l3ca_assoc_tab))
                                parse_error(str,
                                            "too many cores selected for "
                                            "allocation association");
                        sel_l3ca_assoc_tab[i].core = (unsigned) cores[i];
                        sel_l3ca_assoc_tab[i].class_id = cos;
                }
                sel_l3ca_assoc_num = (int) n;
                return;
        }

        for (i = 0; i < n; i++) {
                int j;

                for (j = 0; j < sel_l3ca_assoc_num; j++)
                        if (sel_l3ca_assoc_tab[j].core == (unsigned) cores[i])
                                break;

                if (j < sel_l3ca_assoc_num) {
                        /**
                         * this core is already on the list
                         * - update COS but warn about it
                         */
                        printf("warn: updating COS for core %u from %u to %u\n",
                               (unsigned) cores[i],
                               sel_l3ca_assoc_tab[j].class_id, cos);
                        sel_l3ca_assoc_tab[j].class_id = cos;
                } else {
                        /**
                         * New core is selected - extend the list
                         */
                        unsigned k = (unsigned) sel_l3ca_assoc_num;

                        if (k >= DIM(sel_l3ca_assoc_tab))
                                parse_error(str,
                                            "too many cores selected for "
                                            "allocation association");

                        sel_l3ca_assoc_tab[k].core = (unsigned) cores[i];
                        sel_l3ca_assoc_tab[k].class_id = cos;
                        sel_l3ca_assoc_num++;
                }
        }
}

/**
 * @brief Associates cores with selected class of service
 *
 * @param arg string passed to -a command line option
 */
static void
selfn_allocation_assoc(const char *arg)
{
        char *cp = NULL, *str = NULL;
        char *saveptr = NULL;

        if (arg == NULL)
                parse_error(arg, "NULL pointer!");

        if (strlen(arg) <= 0)
                parse_error(arg, "Empty string!");

        selfn_strdup(&cp, arg);

        for (str = cp; ; str = NULL) {
                char *token = NULL;

                token = strtok_r(str, ";", &saveptr);
                if (token == NULL)
                        break;
                parse_allocation_assoc(token);
        }

        free(cp);
}

/**
 * @brief Prints information about cache allocation settings in the system
 *
 * @param sock_count number of detected CPU sockets
 * @param sockets arrays with detected CPU socket id's
 * @param cpu_info cpu information structure
 */
static void
print_allocation_config(const struct pqos_capability *cap_mon,
			const struct pqos_capability *cap_l3ca,
			const unsigned sock_count,
			const unsigned *sockets,
			const struct pqos_cpuinfo *cpu_info)
{
        int ret;
        unsigned i;

        for (i = 0; (i < sock_count) && (cap_l3ca != NULL); i++) {
                struct pqos_l3ca tab[PQOS_MAX_L3CA_COS];
                unsigned num = 0;
                unsigned n = 0;

                ret = pqos_l3ca_get(sockets[i], PQOS_MAX_L3CA_COS,
                                    &num, tab);
                if (ret != PQOS_RETVAL_OK)
                        continue;

                printf("L3CA COS definitions for Socket %u:\n",
                       sockets[i]);
                for (n = 0; n < num; n++) {
                        if (tab[n].cdp) {
                                printf("    L3CA COS%u => DATA 0x%llx,"
                                       "CODE 0x%llx\n",
                                       tab[n].class_id,
                                       (unsigned long long)tab[n].data_mask,
                                       (unsigned long long)tab[n].code_mask);
                        } else {
                                printf("    L3CA COS%u => MASK 0x%llx\n",
                                       tab[n].class_id,
                                       (unsigned long long)tab[n].ways_mask);
                        }
                }
        }

        for (i = 0; i < sock_count; i++) {
                unsigned lcores[PQOS_MAX_SOCKET_CORES];
                unsigned lcount = 0, n = 0;

                ret = pqos_cpu_get_cores(cpu_info, sockets[i],
                                         PQOS_MAX_SOCKET_CORES,
                                         &lcount, &lcores[0]);
                if (ret != PQOS_RETVAL_OK) {
                        printf("Error retrieving core information!\n");
                        return;
                }
                ASSERT(ret == PQOS_RETVAL_OK);
                printf("Core information for socket %u:\n",
                       sockets[i]);
                for (n = 0; n < lcount; n++) {
                        unsigned class_id = 0;
                        pqos_rmid_t rmid = 0;
                        int ret1 = PQOS_RETVAL_OK;
                        int ret2 = PQOS_RETVAL_OK;

			if (cap_l3ca != NULL)
				ret1 = pqos_l3ca_assoc_get(lcores[n],
                                                           &class_id);
			if (cap_mon != NULL)
				ret2 = pqos_mon_assoc_get(lcores[n], &rmid);

                        if (ret1 == PQOS_RETVAL_OK && ret2 == PQOS_RETVAL_OK) {
				if (cap_l3ca != NULL && cap_mon != NULL)
					printf("    Core %u => COS%u, RMID%u\n",
					       lcores[n], class_id,
                                               (unsigned)rmid);
				if (cap_l3ca == NULL && cap_mon != NULL)
					printf("    Core %u => RMID%u\n",
					       lcores[n], (unsigned)rmid);
				if (cap_l3ca != NULL && cap_mon == NULL)
					printf("    Core %u => COS%u\n",
					       lcores[n], class_id);
                        } else {
                                printf("    Core %u => ERROR\n", lcores[n]);
                        }
                }
        }
}

/**
 * @brief Selects monitoring time
 *
 * @param arg string passed to -t command line option
 */
static void
selfn_monitor_time(const char *arg)
{
        if (!strcasecmp(arg, "inf") || !strcasecmp(arg, "infinite"))
                sel_timeout = -1; /**< infinite timeout */
        else
                sel_timeout = (int) strtouint64(arg);
}

/**
 * @brief Duplicates \a arg and stores at \a sel
 *
 * @param sel place to store duplicate of \a arg
 * @param arg string passed through command line option
 */
static void
selfn_strdup(char **sel, const char *arg)
{
        ASSERT(sel != NULL && arg != NULL);
        if (*sel != NULL) {
                free(*sel);
                *sel = NULL;
        }
        *sel = strdup(arg);
        ASSERT(*sel != NULL);
        if (*sel == NULL) {
                printf("String duplicate error!\n");
                exit(EXIT_FAILURE);
        }
}

/**
 * @brief Selects monitoring output file
 *
 * @param arg string passed to -o command line option
 */
static void
selfn_monitor_file(const char *arg)
{
        selfn_strdup(&sel_output_file, arg);
}

/**
 * @brief Selects type of monitoring output file
 *
 * @param arg string passed to -u command line option
 */
static void
selfn_monitor_file_type(const char *arg)
{
        selfn_strdup(&sel_output_type, arg);
}

/**
 * @brief Selects monitoring interval
 *
 * @param arg string passed to -i command line option
 */
static void
selfn_monitor_interval(const char *arg)
{
        sel_mon_interval = (int) strtouint64(arg);
}

/**
 * @brief Selects top-like monitoring format
 *
 * @param arg not used
 */
static void
selfn_monitor_top_like(const char *arg)
{
        arg = arg;
        sel_mon_top_like = 1;
}

/**
 * @brief Selects allocation profile from internal DB
 *
 * @param arg string passed to -c command line option
 */
static void
selfn_allocation_select(const char *arg)
{
        selfn_strdup(&sel_allocation_profile, arg);
}

/**
 * @brief Selects log file
 *
 * @param arg string passed to -l command line option
 */
static void
selfn_log_file(const char *arg)
{
        selfn_strdup(&sel_log_file, arg);
}

/**
 * @brief Selects verbose mode on
 *
 * @param arg not used
 */
static void
selfn_verbose_mode(const char *arg)
{
        arg = arg;
        sel_verbose_mode = 1;
}

/**
 * @brief Sets CAT reset flag
 *
 * @param arg not used
 */
static void
selfn_reset_cat(const char *arg)
{
        arg = arg;
        sel_reset_CAT = 1;
}

/**
 * @brief Stores the process id's given in a table for future use
 *
 * @param str string of process id's
 */
static void
sel_store_process_id(char *str)
{
        uint64_t processes[PQOS_MAX_PIDS];
        unsigned i = 0, n = 0;
        enum pqos_mon_event evt = 0;

	parse_event(str, &evt);

        n = strlisttotab(strchr(str, ':') + 1, processes, DIM(processes));

        if (n == 0)
                parse_error(str, "No process id selected for monitoring");

        if (n >= DIM(sel_monitor_pid_tab))
                parse_error(str,
                            "too many processes selected "
                            "for monitoring");

        /**
         *  For each process:
         *  - if it's already there in the sel_monitor_pid_tab
         *  - update the entry
         *  - else - add it to the sel_monitor_pid_tab
         */
        for (i = 0; i < n; i++) {
                unsigned found;
                int j;

                for (found = 0, j = 0; j < sel_process_num && found == 0; j++) {
                        if ((unsigned) sel_monitor_pid_tab[j].pid
			    == processes[i]) {
                                sel_monitor_pid_tab[j].events |= evt;
                                found = 1;
                        }
                }
		if (!found) {
		        sel_monitor_pid_tab[sel_process_num].pid =
			        (pid_t) processes[i];
			sel_monitor_pid_tab[sel_process_num].events = evt;
			m_mon_grps[sel_process_num] =
			        malloc(sizeof(**m_mon_grps));
			if (m_mon_grps[sel_process_num] == NULL) {
			        printf("Error with memory allocation");
			        exit(EXIT_FAILURE);
			}
			sel_monitor_pid_tab[sel_process_num].pgrp =
			        m_mon_grps[sel_process_num];
			++sel_process_num;
		}
        }
}

/**
 * @brief Verifies and translates multiple monitoring config strings into
 *        internal PID monitoring configuration
 *
 * @param arg argument passed to -p command line option
 */
static void
selfn_monitor_pids(const char *arg)
{
        char *cp = NULL, *str = NULL;
	char *saveptr = NULL;

        if (arg == NULL)
                parse_error(arg, "NULL pointer!");

        if (strlen(arg) <= 0)
                parse_error(arg, "Empty string!");

        /**
         * The parser will add to the display only necessary columns
         */
        selfn_strdup(&cp, arg);
	sel_events_max = 0;

        for (str = cp; ; str = NULL) {
                char *token = NULL;

                token = strtok_r(str, ";", &saveptr);
                if (token == NULL)
                        break;
                sel_store_process_id(token);
        }

        free(cp);
}

/**
 * @brief Selects showing allocation settings
 *
 * @param arg not used
 */
static void
selfn_show_allocation(const char *arg)
{
        arg = arg;
        sel_show_allocation_config = 1;
}

/**
 * @brief Processes configuration setting
 *
 * For now CDP config settings are only accepted.
 *
 * @param arg string detailing configuration setting
 */
static void
selfn_set_config(const char *arg)
{
        if (strcasecmp(arg, "cdp-on") == 0)
                selfn_cdp_config = PQOS_REQUIRE_CDP_ON;
        else if (strcasecmp(arg, "cdp-off") == 0)
                selfn_cdp_config = PQOS_REQUIRE_CDP_OFF;
        else if (strcasecmp(arg, "cdp-any") == 0)
                selfn_cdp_config = PQOS_REQUIRE_CDP_ANY;
        else {
                printf("Unrecognized '%s' setting!\n", arg);
                exit(EXIT_FAILURE);
        }
}

/**
 * @brief Opens configuration file and parses its contents
 *
 * @param fname Name of the file with configuration parameters
 */
static void
parse_config_file(const char *fname)
{
        static const struct {
                const char *option;
                void (*fn)(const char *);
        } optab[] = {
                { "show-alloc:",            selfn_show_allocation },  /**< -s */
                { "log-file:",              selfn_log_file },         /**< -l */
                { "verbose-mode:",          selfn_verbose_mode },     /**< -v */
                { "alloc-class-set:",       selfn_allocation_class }, /**< -e */
                { "alloc-assoc-set:",       selfn_allocation_assoc }, /**< -a */
                { "alloc-class-select:",    selfn_allocation_select },/**< -c */
                { "monitor-pids:",          selfn_monitor_pids },     /**< -p */
                { "monitor-cores:",         selfn_monitor_cores },    /**< -m */
                { "monitor-time:",          selfn_monitor_time },     /**< -t */
                { "monitor-interval:",      selfn_monitor_interval }, /**< -i */
                { "monitor-file:",          selfn_monitor_file },     /**< -o */
                { "monitor-file-type:",     selfn_monitor_file_type },/**< -u */
                { "monitor-top-like:",      selfn_monitor_top_like }, /**< -T */
                { "set-config:",            selfn_set_config },       /**< -S */
                { "reset-cat:",             selfn_reset_cat },        /**< -R */
        };
        FILE *fp = NULL;
        char cb[256];

        fp = fopen(fname, "r");
        if (fp == NULL)
                parse_error(fname, "cannot open configuration file!");

        memset(cb, 0, sizeof(cb));

        while (fgets(cb, sizeof(cb)-1, fp) != NULL) {
                int i, j, remain;
                char *cp = NULL;

                for (j = 0; j < (int)sizeof(cb)-1; j++)
                        if (!isspace(cb[j]))
                                break;

                if (j >= (int)(sizeof(cb)-1))
                        continue; /**< blank line */

                if (strlen(cb+j) == 0)
                        continue; /**< blank line */

                if (cb[j] == '#')
                        continue; /**< comment */

                cp = cb+j;
                remain = (int)strlen(cp);

                /**
                 * remove trailing white spaces
                 */
                for (i = (int)strlen(cp)-1; i > 0; i--)
                        if (!isspace(cp[i])) {
                                cp[i+1] = '\0';
                                break;
                        }

                for (i = 0; i < (int)DIM(optab); i++) {
                        int len = (int)strlen(optab[i].option);

                        if (len > remain)
                                continue;

                        if (strncasecmp(cp, optab[i].option, (size_t)len) != 0)
                                continue;

                        while (isspace(cp[len]))
                                len++; /**< skip space characters */

                        optab[i].fn(cp+len);
                        break;
                }

                if (i >= (int)DIM(optab))
                        parse_error(cp,
                                    "Unrecognized configuration file command");
        }

        fclose(fp);
}

/**
 * @brief Compare LLC occupancy in two monitoring data sets
 *
 * @param a monitoring data A
 * @param b monitoring data B
 *
 * @return LLC monitoring data compare status for descending order
 * @retval 0 if \a  = \b
 * @retval >0 if \b > \a
 * @retval <0 if \b < \a
 */
static int
mon_qsort_llc_cmp_desc(const void *a, const void *b)
{
        const struct pqos_mon_data *const *app =
                (const struct pqos_mon_data * const *)a;
        const struct pqos_mon_data *const *bpp =
                (const struct pqos_mon_data * const *)b;
        const struct pqos_mon_data *ap = *app;
        const struct pqos_mon_data *bp = *bpp;
        /**
         * This (b-a) is to get descending order
         * otherwise it would be (a-b)
         */
        return (int) (((int64_t)bp->values.llc) - ((int64_t)ap->values.llc));
}

/**
 * @brief Compare core id in two monitoring data sets
 *
 * @param a monitoring data A
 * @param b monitoring data B
 *
 * @return Core id compare status for ascending order
 * @retval 0 if \a  = \b
 * @retval >0 if \b > \a
 * @retval <0 if \b < \a
 */
static int
mon_qsort_coreid_cmp_asc(const void *a, const void *b)
{
        const struct pqos_mon_data * const *app =
                (const struct pqos_mon_data * const *)a;
        const struct pqos_mon_data * const *bpp =
                (const struct pqos_mon_data * const *)b;
        const struct pqos_mon_data *ap = *app;
        const struct pqos_mon_data *bp = *bpp;
        /**
         * This (a-b) is to get ascending order
         * otherwise it would be (b-a)
         */
        return (int) (((unsigned)ap->cores[0]) - ((unsigned)bp->cores[0]));
}

/**
 * Stop monitoring indicator for infinite monitoring loop
 */
static int stop_monitoring_loop = 0;

/**
 * @brief CTRL-C handler for infinite monitoring loop
 *
 * @param signo signal number
 */
static void monitoring_ctrlc(int signo)
{
        signo = signo;
        stop_monitoring_loop = 1;
}

/**
 * @brief Gets scale factors to display events data
 *
 * LLC factor is scaled to kilobytes (1024 bytes = 1KB)
 * MBM factors are scaled to megabytes / s (1024x1024 bytes = 1MB)
 *
 * @param cap capability structure
 * @param llc_factor cache occupancy monitoring data
 * @param mbr_factor remote memory bandwidth monitoring data
 * @param mbl_factor local memory bandwidth monitoring data
 * @return operation status
 * @retval PQOS_RETVAL_OK on success
 */
static int
get_event_factors(const struct pqos_cap * const cap,
                  double * const llc_factor,
                  double * const mbr_factor,
                  double * const mbl_factor)
{
        const struct pqos_monitor *l3mon = NULL, *mbr_mon = NULL,
                *mbl_mon = NULL;
        int ret = PQOS_RETVAL_OK;

        if ((cap == NULL) || (llc_factor == NULL) ||
            (mbr_factor == NULL) || (mbl_factor == NULL))
                return PQOS_RETVAL_PARAM;

        if (sel_events_max & PQOS_MON_EVENT_L3_OCCUP) {
                ret = pqos_cap_get_event(cap, PQOS_MON_EVENT_L3_OCCUP, &l3mon);
                if (ret != PQOS_RETVAL_OK) {
                        printf("Failed to obtain LLC occupancy event data!\n");
                        return PQOS_RETVAL_ERROR;
                }
                *llc_factor = ((double)l3mon->scale_factor) / 1024.0;
        } else {
                *llc_factor = 1.0;
        }

        if (sel_events_max & PQOS_MON_EVENT_RMEM_BW) {
                ret = pqos_cap_get_event(cap, PQOS_MON_EVENT_RMEM_BW, &mbr_mon);
                if (ret != PQOS_RETVAL_OK) {
                        printf("Failed to obtain MBR event data!\n");
                        return PQOS_RETVAL_ERROR;
                }
                *mbr_factor = ((double) mbr_mon->scale_factor) /
                        (1024.0*1024.0);
        } else {
                *mbr_factor = 1.0;
        }

        if (sel_events_max & PQOS_MON_EVENT_LMEM_BW) {
                ret = pqos_cap_get_event(cap, PQOS_MON_EVENT_LMEM_BW, &mbl_mon);
                if (ret != PQOS_RETVAL_OK) {
                        printf("Failed to obtain MBL occupancy event data!\n");
                        return PQOS_RETVAL_ERROR;
                }
                *mbl_factor = ((double)mbl_mon->scale_factor) /
                        (1024.0*1024.0);
        } else {
                *mbl_factor = 1.0;
        }

        return PQOS_RETVAL_OK;
}

/**
 * @brief Fills in single text column in the monitoring table
 *
 * @param val numerical value to be put into the column
 * @param data place to put formatted column into
 * @param sz_data available size for the column
 * @param is_monitored if true then \a val holds valid data
 * @param is_column_present if true then corresponding event is
 *        selected for display
 * @return Number of characters added to \a data excluding NULL
 */
static size_t
fillin_text_column(const double val, char data[], const size_t sz_data,
                   const int is_monitored, const int is_column_present)
{
        const char blank_column[] = "           ";
        size_t offset = 0;

        if (is_monitored) {
                /**
                 * This is monitored and we have the data
                 */
                snprintf(data, sz_data - 1, "%11.1f", val);
                offset = strlen(data);
        } else if (is_column_present) {
                /**
                 * the column exists though there's no data
                 */
                strncpy(data, blank_column, sz_data - 1);
                offset = strlen(data);
        }

        return offset;
}

/**
 * @brief Fills in text row in the monitoring table
 *
 * @param data the table to store the data row
 * @param sz_data the size of the table
 * @param mon_event events selected in monitoring group
 * @param llc LLC occupancy data
 * @param mbr remote memory bandwidth data
 * @param mbl local memory bandwidth data
 */
static void
fillin_text_row(char data[], const size_t sz_data,
                const enum pqos_mon_event mon_event,
                const double llc, const double mbr,
                const double mbl)
{
        size_t offset = 0;

        ASSERT(sz_data >= 64);

        memset(data, 0, sz_data);

        offset += fillin_text_column(llc, data + offset, sz_data - offset,
                                     mon_event & PQOS_MON_EVENT_L3_OCCUP,
                                     sel_events_max & PQOS_MON_EVENT_L3_OCCUP);

        offset += fillin_text_column(mbl, data + offset, sz_data - offset,
                                     mon_event & PQOS_MON_EVENT_LMEM_BW,
                                     sel_events_max & PQOS_MON_EVENT_LMEM_BW);

        offset += fillin_text_column(mbr, data + offset, sz_data - offset,
                                     mon_event & PQOS_MON_EVENT_RMEM_BW,
                                     sel_events_max & PQOS_MON_EVENT_RMEM_BW);
}

/**
 * @brief Fills in single XML column in the monitoring table
 *
 * @param val numerical value to be put into the column
 * @param data place to put formatted column into
 * @param sz_data available size for the column
 * @param is_monitored if true then \a val holds valid data
 * @param is_column_present if true then corresponding event is
 *        selected for display
 * @param node_name defines XML node name for the column
 * @return Number of characters added to \a data excluding NULL
 */
static size_t
fillin_xml_column(const double val, char data[], const size_t sz_data,
                  const int is_monitored, const int is_column_present,
                  const char node_name[])
{
        size_t offset = 0;

        if (is_monitored) {
                /**
                 * This is monitored and we have the data
                 */
                snprintf(data, sz_data - 1, "\t<%s>%.1f</%s>\n",
                         node_name, val, node_name);
                offset = strlen(data);
        } else if (is_column_present) {
                /**
                 * the column exists though there's no data
                 */
                snprintf(data, sz_data - 1, "\t<%s></%s>\n",
                         node_name, node_name);
                offset = strlen(data);
        }

        return offset;
}

/**
 * @brief Fills in the row in the XML file with the monitoring data
 *
 * @param data the table to store the data row
 * @param sz_data the size of the table
 * @param mon_event events selected in monitoring group
 * @param llc LLC occupancy data
 * @param mbr remote memory bandwidth data
 * @param mbl local memory bandwidth data
 */
static void
fillin_xml_row(char data[], const size_t sz_data,
               const enum pqos_mon_event mon_event,
               const double llc, const double mbr,
               const double mbl)
{
        size_t offset = 0;

        ASSERT(sz_data >= 128);

        memset(data, 0, sz_data);

        offset += fillin_xml_column(llc, data + offset, sz_data - offset,
                                    mon_event & PQOS_MON_EVENT_L3_OCCUP,
                                    sel_events_max & PQOS_MON_EVENT_L3_OCCUP,
                                    "l3_occupancy_kB");

        offset += fillin_xml_column(mbl, data + offset, sz_data - offset,
                                    mon_event & PQOS_MON_EVENT_LMEM_BW,
                                    sel_events_max & PQOS_MON_EVENT_LMEM_BW,
                                    "mbm_local_MB");

        offset += fillin_xml_column(mbr, data + offset, sz_data - offset,
                                    mon_event & PQOS_MON_EVENT_RMEM_BW,
                                    sel_events_max & PQOS_MON_EVENT_RMEM_BW,
                                    "mbm_remote_MB");
}

/**
 * @brief Reads monitoring event data at given \a interval for \a sel_time time span
 *
 * @param fp FILE to be used for storing monitored data
 * @param sel_time time span to monitor data for, negative value indicates infinite time
 * @param interval time interval to gather data and print to the file, this is in 100ms units
 * @param top_mode output style similar to top utility
 * @param cap detected PQoS capabilities
 * @param output_type text or XML output file
 */
static void
monitoring_loop(FILE *fp,
                const int sel_time,
                long interval,
                const int top_mode,
                const struct pqos_cap *cap,
                const char *output_type)
{
#define TERM_MIN_NUM_LINES 3

        double llc_factor = 1, mbr_factor = 1, mbl_factor = 1;
        struct timeval tv_start;
        int ret = PQOS_RETVAL_OK;
        int istty = 0;
        unsigned max_lines = 0;
        const int istext = !strcasecmp(output_type, "text");

        /* for the dynamic display */
        const size_t sz_header = 128, sz_data = 128;
        char *header = NULL;
        char data[sz_data];
	unsigned mon_number = 0;
	struct pqos_mon_data **mon_data = NULL;

	if (!process_mode())
	        mon_number = (unsigned) sel_monitor_num;
	else
	        mon_number = (unsigned) sel_process_num;

	mon_data = malloc(sizeof(*mon_data) * mon_number);
	if (mon_data == NULL) {
	        printf("Error with memory allocation");
		exit(EXIT_FAILURE);
	}

        if ((!istext)  && (strcasecmp(output_type, "xml") != 0)) {
                printf("Invalid selection of output file type '%s'!\n",
                       output_type);
                free(mon_data);
                return;
        }

        ret = get_event_factors(cap, &llc_factor, &mbr_factor, &mbl_factor);
        if (ret != PQOS_RETVAL_OK) {
                printf("Error in retrieving monitoring scale factors!\n");
                free(mon_data);
                return;
        }

        /**
         * capture ctrl-c to gracefully stop the loop
         */
        if (signal(SIGINT, monitoring_ctrlc) == SIG_ERR)
                printf("Failed to catch SIGINT!\n");
        if (signal(SIGHUP, monitoring_ctrlc) == SIG_ERR)
                printf("Failed to catch SIGHUP!\n");

        istty = isatty(fileno(fp));

        if (istty) {
                struct winsize w;

                if (ioctl(fileno(fp), TIOCGWINSZ, &w) != -1) {
                        max_lines = w.ws_row;
                        if (max_lines < TERM_MIN_NUM_LINES)
                                max_lines = TERM_MIN_NUM_LINES;
                }
        }

        /**
         * A coefficient to display the data as MB / s
         */
        double coeff = 10.0 / (double)interval;

        /**
         * Interval is passed in  100[ms] units
         * This converts interval to microseconds
         */
        interval = interval * 100000LL;

        gettimeofday(&tv_start, NULL);

        /**
         * Build the header
         */
        if (istext) {
                header = (char *) alloca(sz_header);
                if (header == NULL) {
                        printf("Failed to allocate stack frame memory!\n");
                        free(mon_data);
                        return;
                }
                memset(header, 0, sz_header);
		/* Different header for process id's */
		if (!process_mode())
		        strncpy(header,
				"SKT     CORE     RMID",
				sz_header - 1);
		else
		        strncpy(header,
				"PID      CORE     RMID",
				sz_header - 1);

                if (sel_events_max & PQOS_MON_EVENT_L3_OCCUP)
                        strncat(header, "    LLC[KB]",
                                sz_header - strlen(header) - 1);
                if (sel_events_max & PQOS_MON_EVENT_LMEM_BW)
                        strncat(header, "  MBL[MB/s]",
                                sz_header - strlen(header) - 1);
                if (sel_events_max & PQOS_MON_EVENT_RMEM_BW)
                        strncat(header, "  MBR[MB/s]",
                                sz_header - strlen(header) - 1);
        }

        while (!stop_monitoring_loop) {
		struct timeval tv_s, tv_e;
                struct tm *ptm = NULL;
                unsigned i = 0;
                struct timespec req, rem;
                long usec_start = 0, usec_end = 0, usec_diff = 0;
                char cb_time[64];

                gettimeofday(&tv_s, NULL);
		ret = pqos_mon_poll(m_mon_grps,
				    (unsigned) mon_number);
		if (ret != PQOS_RETVAL_OK) {
		        printf("Failed to poll monitoring data!\n");
			free(mon_data);
			return;
		}

		memcpy(mon_data, m_mon_grps,
		       mon_number * sizeof(m_mon_grps[0]));

                if (istty)
                        fprintf(fp, "\033[2J");   /**< clear screen */

                ptm = localtime(&tv_s.tv_sec);
                if (ptm != NULL) {
                        /**
                         * Print time
                         */
                        strftime(cb_time, DIM(cb_time)-1,
                                 "%Y-%m-%d %H:%M:%S", ptm);

                        if (istty)
                                fprintf(fp, "\033[0;0H"); /**< Move to
                                                             position 0:0 */
                        if (istext)
                                fprintf(fp, "TIME %s\n", cb_time);
                } else {
                        strncpy(cb_time, "error", DIM(cb_time)-1);
                }

                if (top_mode) {
		        qsort(mon_data, mon_number, sizeof(mon_data[0]),
			      mon_qsort_llc_cmp_desc);
		} else if (!process_mode()) {
		        qsort(mon_data, mon_number, sizeof(mon_data[0]),
			      mon_qsort_coreid_cmp_asc);
		}

                if (max_lines > 0) {
                        if ((mon_number+TERM_MIN_NUM_LINES-1) > max_lines)
                                mon_number = max_lines - TERM_MIN_NUM_LINES + 1;
                }

                if (istext)
                        fputs(header, fp);

                for (i = 0; i < mon_number; i++) {
                        double llc = ((double)mon_data[i]->values.llc) *
                                llc_factor;
			double mbr =
                                ((double)mon_data[i]->values.mbm_remote_delta) *
                                mbr_factor * coeff;
			double mbl =
                                ((double)mon_data[i]->values.mbm_local_delta) *
                                mbl_factor * coeff;

                        if (istext) {
                                /* Text */
				/* Checking what to print,
				 * cores or process id's */
			        fillin_text_row(data, sz_data,
						mon_data[i]->event,
						llc, mbr, mbl);
				if (!process_mode()) {
				        fprintf(fp, "\n%3u %8.8s %8u%s",
                                                mon_data[i]->socket,
						(char *)mon_data[i]->context,
						mon_data[i]->rmid,
						data);
				} else {
				        fprintf(fp, "\n%6u %6s %8s%s",
						mon_data[i]->pid,
						"N/A",
						"N/A",
						data);
				}
                        } else {
                                /* XML */
			        fillin_xml_row(data, sz_data,
					       mon_data[i]->event,
					       llc, mbr, mbl);
			        if (!process_mode()) {
					fprintf(fp,
						"%s\n"
						"\t<time>%s</time>\n"
						"\t<socket>%u</socket>\n"
						"\t<core>%s</core>\n"
						"\t<rmid>%u</rmid>\n"
						"%s"
						"%s\n"
						"%s",
						xml_child_open,
						cb_time,
						mon_data[i]->socket,
						(char *)mon_data[i]->context,
						mon_data[i]->rmid,
						data,
						xml_child_close,
						xml_root_close);
				} else {
					fprintf(fp,
						"%s\n"
						"\t<time>%s</time>\n"
						"\t<pid>%u</pid>\n"
						"\t<core>%s</core>\n"
						"\t<rmid>%s</rmid>\n"
						"%s"
						"%s\n"
						"%s",
						xml_child_open,
						cb_time,
						mon_data[i]->pid,
						"N/A",
						"N/A",
						data,
						xml_child_close,
						xml_root_close);
				}
				if (fseek(fp, -xml_root_close_size,
                                          SEEK_CUR) == -1) {
                                        perror("File seek error");
                                        return;
                                }
			}
                }
                fflush(fp);

                /**
                 * Move to position 0:0
                 */
                if (istty)
                        fputs("\033[0;0", fp);

                gettimeofday(&tv_e, NULL);

                if (stop_monitoring_loop)
                        break;

                /**
                 * Calculate microseconds to the nearest measurement interval
                 */
                usec_start = ((long)tv_s.tv_usec) +
                        ((long)tv_s.tv_sec * 1000000L);
                usec_end = ((long)tv_e.tv_usec) +
                        ((long)tv_e.tv_sec * 1000000L);
                usec_diff = usec_end - usec_start;

                if (usec_diff < interval) {
                        memset(&rem, 0, sizeof(rem));
                        memset(&req, 0, sizeof(req));

                        req.tv_sec = (interval - usec_diff) / 1000000L;
                        req.tv_nsec =
                                ((interval - usec_diff) % 1000000L) * 1000L;
                        if (nanosleep(&req, &rem) == -1) {
                                /**
                                 * nanosleep interrupted by a signal
                                 */
                                if (stop_monitoring_loop)
                                        break;
                                req = rem;
                                memset(&rem, 0, sizeof(rem));
                                nanosleep(&req, &rem);
                        }
                }

                if (sel_time >= 0) {
                        gettimeofday(&tv_e, NULL);
                        if ((tv_e.tv_sec - tv_start.tv_sec) > sel_time)
                                break;
                }

        }

        if (istty)
                fputs("\n\n", fp);

	free(mon_data);

}

const char *m_cmd_name = "pqos";                          /**< command name */

/**
 * @brief Displays help information
 *
 */
static void print_help(void)
{
        printf("Usage: %s [-h] [-H]\n"
               "       %s [-f <config_file_name>]\n"
               "       %s [-l <log_file_name>]\n"
               "       %s [-m <event_type>:<core_list> | -p <event_type>:"
               "<pid_list>]\n"
               "          [-t <time in sec>]\n"
               "          [-i <interval in 100ms>] [-T]\n"
               "          [-o <output_file>] [-u <output_type>] [-r]\n"
               "       %s [-e <allocation_type>:<class_num>=<class_definition>;"
               "...]\n"
               "          [-c <allocation_type>:<profile_name>;...]\n"
               "          [-a <allocation_type>:<class_num>=<core_list>;"
               "...]\n"
               "       %s [-R]\n"
               "       %s [-S cdp-on|cdp-off|cdp-any]\n"
               "       %s [-s]\n"
               "Notes:\n"
               "\t-h\thelp\n"
               "\t-v\tverbose mode\n"
               "\t-H\tlist of supported allocation profiles\n"
               "\t-f\tloads parameters from selected configuration file\n"
               "\t-l\tlogs messages into selected log file\n"
               "\t-e\tdefine allocation classes, example: \"llc:0=0xffff;"
               "llc:1=0x00ff;\"\n"
               "\t-c\tselect a profile of predefined allocation classes, "
               "see -H to list available profiles\n"
               "\t-a\tassociate cores with allocation classes, example: "
               "\"llc:0=0,2,4,6-10;llc:1=1\"\n"
               "\t-r\tuses all RMID's and cores in the system\n"
               "\t-R\tresets CAT configuration\n"
               "\t-s\tshow current cache allocation configuration\n"
               "\t-S\tset a configuration setting:\n"
               "\t\tcdp-on\tsets CDP on\n"
               "\t\tcdp-off\tsets CDP off\n"
               "\t\tcdp-any\tkeep current CDP setting (default)\n"
               "\t\tNOTE: change of CDP on/off setting results in CAT reset.\n"
               "\t-m\tselect cores and events for monitoring, example: "
               "\"all:0,2,4-10;llc:1,3;mbr:11-12\"\n"
               "\t\tNOTE: group core statistics together by enclosing the core "
               "list in\n\t\tsquare brackets, example: "
               "\"llc:[0-3];all:[4,5,6];mbr:[0-3],7,8\"\n"
               "\t-o\tselect output file to store monitored data in. "
               "stdout by default.\n"
               "\t-u\tselect output format type for monitored data. "
               "\"text\" (default) and \"xml\" are the options.\n"
               "\t-i\tdefine monitoring sampling interval, 1=100ms, "
               "default 10=10x100ms=1s\n"
               "\t-T\ttop like monitoring output\n"
               "\t-t\tdefine monitoring time (use 'inf' or 'infinite' for "
               "infinite loop monitoring loop)\n"
               "\t-p\tselect process ids and events to monitor, "
	       "example: \"llc:22,2,7-15\""
	       ", it is not possible to track both processes and cores\n",
               m_cmd_name, m_cmd_name, m_cmd_name, m_cmd_name, m_cmd_name,
               m_cmd_name, m_cmd_name, m_cmd_name);
}

int main(int argc, char **argv)
{
        struct pqos_config cfg;
        const struct pqos_cpuinfo *p_cpu = NULL;
        const struct pqos_cap *p_cap = NULL;
        const struct pqos_capability *cap_mon = NULL, *cap_l3ca = NULL;
        unsigned sock_count, sockets[PQOS_MAX_SOCKETS];
        int cmd, ret, exit_val = EXIT_SUCCESS;
        FILE *fp_monitor = NULL;
        int j = 0;

        m_cmd_name = argv[0];

        while ((cmd = getopt(argc, argv, "Hhf:i:m:Tt:l:o:u:e:c:a:p:S:srvR"))
               != -1) {
                switch (cmd) {
                case 'h':
                        print_help();
                        return EXIT_SUCCESS;
                case 'H':
                        profile_l3ca_list(stdout);
                        return EXIT_SUCCESS;
                case 'S':
                        selfn_set_config(optarg);
                        break;
                case 'f':
                        if (sel_config_file != NULL) {
                                printf("Only one config file argument is "
                                       "accepted!\n");
                                return EXIT_FAILURE;
                        }
                        selfn_strdup(&sel_config_file, optarg);
                        parse_config_file(sel_config_file);
                        break;
                case 'i':
                        selfn_monitor_interval(optarg);
                        break;
                case 'p':
		        selfn_monitor_pids(optarg);
                        break;
                case 'm':
                        selfn_monitor_cores(optarg);
                        break;
                case 't':
                        selfn_monitor_time(optarg);
                        break;
                case 'T':
                        selfn_monitor_top_like(NULL);
                        break;
                case 'l':
                        selfn_log_file(optarg);
                        break;
                case 'o':
                        selfn_monitor_file(optarg);
                        break;
                case 'u':
                        selfn_monitor_file_type(optarg);
                        break;
                case 'e':
                        selfn_allocation_class(optarg);
                        break;
                case 'r':
                        sel_free_in_use_rmid = 1;
                        break;
                case 'R':
                        selfn_reset_cat(NULL);
                        break;
                case 'a':
                        selfn_allocation_assoc(optarg);
                        break;
                case 'c':
                        selfn_allocation_select(optarg);
                        break;
                case 's':
                        selfn_show_allocation(NULL);
                        break;
                case 'v':
                        selfn_verbose_mode(NULL);
                        break;
                default:
                        printf("Unsupported option: -%c. "
                               "See option -h for help.\n", optopt);
                        return EXIT_FAILURE;
                        break;
                case '?':
                        print_help();
                        return EXIT_SUCCESS;
                        break;
                }
        }

        memset(&cfg, 0, sizeof(cfg));
        cfg.verbose = sel_verbose_mode;
        cfg.free_in_use_rmid = sel_free_in_use_rmid;
        cfg.cdp_cfg = selfn_cdp_config;

        /**
         * Check output file type
         */
        if (sel_output_type == NULL)
                sel_output_type = strdup("text");

        if (sel_output_type == NULL) {
                printf("Memory allocation error!\n");
                exit_val = EXIT_FAILURE;
                goto error_exit_1;
        }

        if (strcasecmp(sel_output_type, "text") != 0 &&
            strcasecmp(sel_output_type, "xml") != 0) {
                printf("Invalid selection of file output type'%s'!\n",
                       sel_output_type);
                exit_val = EXIT_FAILURE;
                goto error_exit_1;
        }

        /**
         * Set up file descriptor for monitored data
         */
        if (sel_output_file == NULL) {
                fp_monitor = stdout;
        } else {
                if (strcasecmp(sel_output_type, "xml") == 0)
                        fp_monitor = fopen(sel_output_file, "w+");
                else
                        fp_monitor = fopen(sel_output_file, "a");
                if (fp_monitor == NULL) {
                        perror("Monitoring output file open error:");
                        printf("Error opening '%s' output file!\n",
                               sel_output_file);
                        exit_val = EXIT_FAILURE;
                        goto error_exit_1;
                }
                if (strcasecmp(sel_output_type, "xml") == 0) {
                        if (fseek(fp_monitor, 0, SEEK_END) == -1) {
                                perror("File seek error");
                                exit_val = EXIT_FAILURE;
                                goto error_exit_1;
                        }
                        if (ftell(fp_monitor) == 0)
                                fprintf(fp_monitor,
                                        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                                        "%s\n%s",
                                        xml_root_open, xml_root_close);
                        if (fseek(fp_monitor,
                                  -xml_root_close_size, SEEK_CUR) == -1) {
                                perror("File seek error");
                                exit_val = EXIT_FAILURE;
                                goto error_exit_1;
                        }
                }
        }

        /**
         * Set up file descriptor for message log
         */
        if (sel_log_file == NULL) {
                cfg.fd_log = STDOUT_FILENO;
        } else {
                cfg.fd_log = open(sel_log_file, O_WRONLY|O_CREAT,
                                  S_IRUSR|S_IWUSR);
                if (cfg.fd_log == -1) {
                        printf("Error opening %s log file!\n", sel_log_file);
                        exit_val = EXIT_FAILURE;
                        goto error_exit_2;
                }
        }

        ret = pqos_init(&cfg);
        if (ret != PQOS_RETVAL_OK) {
                printf("Error initializing PQoS library!\n");
                exit_val = EXIT_FAILURE;
                goto error_exit_1;
        }

        ret = pqos_cap_get(&p_cap, &p_cpu);
        if (ret != PQOS_RETVAL_OK) {
                printf("Error retrieving PQoS capabilities!\n");
                exit_val = EXIT_FAILURE;
                goto error_exit_2;
        }

        ret = pqos_cpu_get_sockets(p_cpu, PQOS_MAX_SOCKETS,
                                   &sock_count,
                                   sockets);
        if (ret != PQOS_RETVAL_OK) {
                printf("Error retrieving CPU socket information!\n");
                exit_val = EXIT_FAILURE;
                goto error_exit_2;
        }
        ret = pqos_cap_get_type(p_cap, PQOS_CAP_TYPE_MON, &cap_mon);
        ret = pqos_cap_get_type(p_cap, PQOS_CAP_TYPE_L3CA, &cap_l3ca);

        if (sel_allocation_profile != NULL) {
                /**
                 * Allocation profile selected
                 */
                unsigned cnum = 0;
                const char * const *cptr = NULL;

                if (cap_l3ca != NULL &&
		    profile_l3ca_get(sel_allocation_profile,
                                     cap_l3ca->u.l3ca,
                                     &cnum, &cptr) == PQOS_RETVAL_OK) {
                        /**
                         * All profile classes are defined as strings
                         * in format that is friendly with command line
                         * options.
                         * This effectively simulates series of -e command
                         * line options. "llc:" is glued to each of the strings
                         * so that profile class definitions don't have to
                         * include it.
                         */
                        char cb[64];
                        unsigned i = 0, offset = 0;

                        memset(cb, 0, sizeof(cb));
                        strcpy(cb, "llc:");
                        offset = (unsigned)strlen("llc:");

                        for (i = 0; i < cnum; i++) {
                                strncpy(cb+offset, cptr[i],
                                        sizeof(cb)-1-offset);
                                selfn_allocation_class(cb);
                        }
                } else {
                        printf("Allocation profile '%s' not found or "
                               "cache allocation not supported!\n",
                               sel_allocation_profile);
                        exit_val = EXIT_FAILURE;
                        goto error_exit_2;
                }
        }

        if (sel_reset_CAT) {
                /**
                 * Reset CAT configuration to after-reset state and exit
                 */
                if (pqos_l3ca_reset(p_cap, p_cpu) != PQOS_RETVAL_OK) {
                        exit_val = EXIT_FAILURE;
                        printf("CAT reset failed!\n");
                } else
                        printf("CAT reset successful\n");
                goto allocation_exit;
        }

        if (sel_show_allocation_config) {
                /**
                 * Show info about allocation config and exit
                 */
		print_allocation_config(cap_mon, cap_l3ca, sock_count,
                                        sockets, p_cpu);
                goto allocation_exit;
        }

        if (cap_l3ca != NULL) {
                /**
                 * If allocation config changed then exit.
                 * For monitoring, start the program again unless
                 * config file was provided
                 */
                int ret_assoc = 0, ret_cos = 0;

                ret_cos = set_allocation_class(sock_count, sockets);
                if (ret_cos < 0) {
                        printf("Allocation configuration error!\n");
                        exit_val = EXIT_FAILURE;
                        goto error_exit_2;
                }

                ret_assoc = set_allocation_assoc();
                if (ret_assoc < 0) {
                        printf("CAT association error!\n");
                        exit_val = EXIT_FAILURE;
                        goto error_exit_2;
                }

                if (ret_assoc > 0 || ret_cos > 0) {
                        printf("Allocation configuration altered.\n");
                        goto allocation_exit;
                }
        } else {
                if (sel_l3ca_assoc_num > 0 || sel_l3ca_cos_num > 0 ||
                    sel_config_file != NULL || sel_allocation_profile != NULL) {
                        printf("Allocation capability not detected!\n");
                        exit_val = EXIT_FAILURE;
                        goto error_exit_2;
                }
        }

        /**
         * Just monitoring option left on the table now
         */
        if (cap_mon == NULL) {
                printf("Monitoring capability not detected!\n");
                exit_val = EXIT_FAILURE;
                goto error_exit_2;
        }

        if (setup_monitoring(p_cpu, cap_mon) != 0) {
                exit_val = EXIT_FAILURE;
                goto error_exit_2;
        }

        monitoring_loop(fp_monitor, sel_timeout, sel_mon_interval,
                        sel_mon_top_like, p_cap, sel_output_type);

        stop_monitoring();

 allocation_exit:
 error_exit_2:
        ret = pqos_fini();
        ASSERT(ret == PQOS_RETVAL_OK);
        if (ret != PQOS_RETVAL_OK)
                printf("Error shutting down PQoS library!\n");

 error_exit_1:
        /**
         * Close file descriptor for monitoring output
         */
        if (fp_monitor != NULL && fp_monitor != stdout)
                fclose(fp_monitor);

        /**
         * Close file descriptor for message log
         */
        if (cfg.fd_log > 0 && cfg.fd_log != STDOUT_FILENO)
                close(cfg.fd_log);

        /**
         * Free allocated memory
         */
        if (sel_output_file != NULL)
                free(sel_output_file);
        if (sel_output_type != NULL)
                free(sel_output_type);
        if (sel_allocation_profile != NULL)
                free(sel_allocation_profile);
        if (sel_log_file != NULL)
                free(sel_log_file);
        if (sel_config_file != NULL)
                free(sel_config_file);

        for (j = 0; j < sel_monitor_num; j++)
                free(m_mon_grps[j]);

        return exit_val;
}
