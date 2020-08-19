/*
 * BSD LICENSE
 *
 * Copyright(c) 2014-2020 Intel Corporation. All rights reserved.
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
 */

/**
 * @brief Platform QoS utility - monitoring module
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
#include <dirent.h>                                     /**< for dir list*/

#include "pqos.h"

#include "common.h"
#include "main.h"
#include "monitor.h"

#define PQOS_MAX_PID_MON_GROUPS         256
#define PQOS_MON_EVENT_ALL    ((enum pqos_mon_event)~PQOS_MON_EVENT_TMEM_BW)
#define PID_COL_STATUS (3) /**< col for process status letter*/
#define PID_COL_UTIME (14) /**< col for cpu-user time in /proc/pid/stat*/
#define PID_COL_STIME (15) /**< col for cpu-kernel time in /proc/pid/stat*/
#define PID_COL_CORE  (39) /**< col for core number in /proc/pid/stat*/
#define PID_CPU_TIME_DELAY_USEC (1200000) /**< delay for cpu stats */
#define TOP_PROC_MAX (10) /**< maximum number of top-pids to be handled*/
#define NUM_TIDS_MAX (128) /**< maximum number of TIDs */

/**
 * Local data structures
 *
 */
static const char *xml_root_open = "<records>";
static const char *xml_root_close = "</records>";
static const char *xml_child_open = "<record>";
static const char *xml_child_close = "</record>";

/**
 * Location of directory with PID's in the system
 */
static const char *proc_pids_dir = "/proc";

/**
 * White-list of process status fields that can go into top-pids list
 */
static const char *proc_stat_whitelist = "RSD";

/**
 * Number of cores that are selected in config string
 * for monitoring LLC occupancy
 */
static int sel_monitor_num = 0;

/** Trigger for disabling ipc monitoring */
static int sel_disable_ipc = 0;
/** Trigger for disabling llc_miss monitoring */
static int sel_disable_llc_miss = 0;

/**
 * The mask to tell which events to display
 */
static enum pqos_mon_event sel_events_max = (enum pqos_mon_event)0;

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

/**
 * Maintains a table of process id, event, number of events that are selected
 * in config string for monitoring
 */
static struct pid_group {
        char *desc;
        int num_pids;
        pid_t *pids;
        struct pqos_mon_data *pgrp;
        enum pqos_mon_event events;
} sel_monitor_pid_tab[PQOS_MAX_PID_MON_GROUPS];

/**
 * Maintains the number of process id's you want to track
 */
static int sel_process_num = 0;

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
 * Maintains selected monitoring output file name
 */
static char *sel_output_file = NULL;

/**
 * Maintains selected type of monitoring output file
 */
static char *sel_output_type = NULL;

/**
 * Stop monitoring indicator for infinite monitoring loop
 */
static int stop_monitoring_loop = 0;

/**
 * File descriptor for writing monitored data into
 */
static FILE *fp_monitor = NULL;

/**
 * Maintains process statistics. It is used for getting N pids to be displayed
 * in top-pid monitoring mode.
 */
struct proc_stats {
        pid_t pid; /**< process pid */
        unsigned long ticks_delta; /**< current cpu_time - previous ticks */
        double cpu_avg_ratio; /**< cpu usage/running time ratio*/
        int valid; /**< marks if statisctics are fully processed */
};

/**
 * Maintains single linked list implementation
 */
struct slist {
        void *data; /**< abstract data that is hold by a list element */
        struct slist *next; /**< ptr to next list element */
};

/**
 * Stores display format for LLC (kilobytes/percent)
 */
static enum llc_format {
        LLC_FORMAT_KILOBYTES = 0,
        LLC_FORMAT_PERCENT
} sel_llc_format = LLC_FORMAT_KILOBYTES;

/**
 * Manages llc entry with data to be displayed with current llc_format
 * (value may be displayed as kilobytes value or as percent of total cache)
 */
struct llc_entry_data {
        double val;
        enum llc_format format;
};

/**
 * @brief Scale byte value up to KB
 *
 * @param bytes value to be scaled up
 * @return scaled up value in KB's
 */
static inline double bytes_to_kb(const double bytes)
{
        return bytes / 1024.0;
}

/**
 * @brief Scale byte value up to MB
 *
 * @param bytes value to be scaled up
 * @return scaled up value in MB's
 */
static inline double bytes_to_mb(const double bytes)
{
        return bytes / (1024.0 * 1024.0);
}

/**
 * @brief Check to determine if processes or cores are monitored
 *
 * @return Process monitoring mode status
 * @retval 0 monitoring cores
 * @retval 1 monitoring processes
 */
static inline int process_mode(void)
{
        return (sel_process_num <= 0) ? 0 : 1;
}

/**
 * @brief Function to safely translate an unsigned int
 *        value to a string without memory allocation
 *
 * @param buf buffer string will be copied into
 * @param buf_len length of buffer
 * @param val value to be translated
 *
 * @return length of generated string
 * @retval < 0 on error
 */
static int
uinttostr_noalloc(char *buf, const int buf_len, const unsigned val)
{
        ASSERT(buf != NULL);
        int ret;

        memset(buf, 0, buf_len);
        ret = snprintf(buf, buf_len, "%u", val);

        /* Return -1 when output was truncated */
        if (ret >= buf_len)
                ret = -1;

        return ret;
}

/**
 * @brief Function to safely translate an unsigned int
 *        value to a string
 *
 * @param val value to be translated
 *
 * @return Pointer to allocated string
 */
static char *uinttostr(const unsigned val)
{
        char buf[16], *str = NULL;

        (void)uinttostr_noalloc(buf, sizeof(buf), val);
        selfn_strdup(&str, buf);

        return str;
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
 * @brief Function to set pid group values
 *
 * @param pg pointer to pid_group structure
 * @param desc string containing pid group description
 * @param pids pointer to table of pid values
 * @param num_pids number of pids contained in the table
 *
 * @return Operational status
 * @retval 0 on success
 * @retval -1 on error
 */
static int
set_pgrp(struct pid_group *pg, char *desc,
         const uint64_t *pids, const int num_pids)
{
        int i;

        ASSERT(pg != NULL);
        ASSERT(desc != NULL);
        ASSERT(pids != NULL);
        ASSERT(num_pids > 0);

        pg->desc = desc;
        pg->pids = malloc(sizeof(pid_t) * num_pids);
        if (pg->pids == NULL) {
                printf("Error allocating pid group table\n");
                return -1;
        }
        pg->num_pids = num_pids;

        /**
         * Transfer pids from buffer to table
         */
        for (i = 0; i < num_pids; i++)
                pg->pids[i] = (pid_t)pids[i];

        return 0;
}

/**
 * @brief Function to set the descriptions and cores/pids for each monitoring
 * group
 *
 * Takes a string containing individual cores/pids and groups of cores/pids and
 * breaks it into substrings which are used to set group values
 *
 * @param s string containing cores/pids to be divided into substrings
 * @param ctab table of core groups to set values in
 * @param ptab table of pids groups to set values in
 * @param max maximum number of groups allowed
 *
 * @return Number of core groups set up
 * @retval -1 on error
 */
static int
strtogrps(char *s,
          struct core_group *ctab,
          struct pid_group *ptab,
          const unsigned max)
{
        unsigned i, group_count = 0;
        uint64_t cbuf[PQOS_MAX_CORES];
        char *non_grp = NULL;

        ASSERT((ctab != NULL) ^ (ptab != NULL));
        ASSERT(max > 0);

        if (s == NULL)
                return group_count;

        while ((non_grp = strsep(&s, "[")) != NULL) {
                /**
                 * Ungrouped cores/pids
                 */
                if (*non_grp != '\0') {
                        /* for separate cores/pids - each will get his own
                         * group so strlisttotab result is treated as the
                         * number of new groups
                         */
                        unsigned new_groups_count = strlisttotab(
                                        non_grp, cbuf, DIM(cbuf));

                        if (group_count + new_groups_count > max)
                                return -1;

                        /* set group info */
                        for (i = 0; i < new_groups_count; i++) {
                                char *desc = uinttostr((unsigned)cbuf[i]);
                                int ret;

                                if (ctab != NULL)
                                        ret = set_cgrp(&ctab[group_count],
                                                       desc, &cbuf[i], 1);
                                else
                                        ret = set_pgrp(&ptab[group_count],
                                                       desc, &cbuf[i], 1);
                                if (ret < 0) {
                                        free(desc);
                                        return -1;
                                }
                                group_count++;
                        }
                }
                /**
                 * If group contains multiple cores/pids
                 */
                char *grp = strsep(&s, "]");

                if (grp != NULL) {
                        char *desc = NULL;
                        unsigned element_count;
                        int ret;

                        if (group_count >= max)
                                return -1;

                        selfn_strdup(&desc, grp);

                        /* for grouped pids/cores, all elements are in
                         * one group so strlisttotab result is the number
                         * of elements in that one group
                         */
                        element_count = strlisttotab(grp, cbuf, DIM(cbuf));

                        /* set group info */
                        if (ctab != NULL)
                                ret = set_cgrp(&ctab[group_count], desc, cbuf,
                                               element_count);
                        else
                                ret = set_pgrp(&ptab[group_count], desc, cbuf,
                                               element_count);
                        if (ret < 0) {
                                free(desc);
                                return -1;
                        }
                        group_count++;
                }
        }

        return group_count;
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

        ASSERT(cg_a != NULL);
        ASSERT(cg_b != NULL);

        const int sz_a = cg_a->num_cores;
        const int sz_b = cg_b->num_cores;
        const unsigned *tab_a = cg_a->cores;
        const unsigned *tab_b = cg_b->cores;

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
 * @brief Function to compare pids in 2 pid groups
 *
 * This function takes 2 pid groups and compares their pid values
 *
 * @param pg_a pointer to pid group a
 * @param pg_b pointer to pid group b
 *
 * @return Whether both groups contain some/none/all of the same pids
 * @retval 1 if both groups contain the same pids
 * @retval 0 if none of their pids match
 * @retval -1 if some but not all pids match
 */
static int
cmp_pgrps(const struct pid_group *pg_a,
          const struct pid_group *pg_b)
{
        int i, found = 0;

        ASSERT(pg_a != NULL);
        ASSERT(pg_b != NULL);

        const int sz_a = pg_a->num_pids;
        const int sz_b = pg_b->num_pids;
        const pid_t *tab_a = pg_a->pids;
        const pid_t *tab_b = pg_b->pids;

        for (i = 0; i < sz_a; i++) {
                int j;

                for (j = 0; j < sz_b; j++)
                        if (tab_a[i] == tab_b[j])
                                found++;
        }
        /* if no pids are the same */
        if (!found)
                return 0;
        /* if group contains same pids */
        if (sz_a == sz_b && sz_b == found)
                return 1;
        /* if not all pids are the same */
        return -1;
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
        if (strncasecmp(str, "llc:", 4) == 0)
                *evt = PQOS_MON_EVENT_L3_OCCUP;
        else if (strncasecmp(str, "mbr:", 4) == 0)
                *evt = PQOS_MON_EVENT_RMEM_BW;
        else if (strncasecmp(str, "mbl:", 4) == 0)
                *evt = PQOS_MON_EVENT_LMEM_BW;
        else if (strncasecmp(str, "mbt:", 4) == 0)
                *evt = PQOS_MON_EVENT_TMEM_BW;
        else if (strncasecmp(str, "all:", 4) == 0 ||
                   strncasecmp(str, ":", 1) == 0)
                *evt = (enum pqos_mon_event) PQOS_MON_EVENT_ALL;
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
parse_monitor_cores(char *str)
{
        int i = 0, n = 0;
        enum pqos_mon_event evt = (enum pqos_mon_event)0;
        struct core_group *cgrp_tab = calloc(PQOS_MAX_CORES, sizeof(*cgrp_tab));

        if (cgrp_tab == NULL) {
                printf("Error with memory allocation!\n");
                exit(EXIT_FAILURE);
        }

        parse_event(str, &evt);

        n = strtogrps(strchr(str, ':') + 1, cgrp_tab, NULL, PQOS_MAX_CORES);
        if (n < 0) {
                printf("Error: Too many cores/groups selected\n");
                goto error_exit;
        }
        /**
         *  For each core group we are processing:
         *  - if it's already in the sel_monitor_core_tab
         *    =>  update the entry
         *  - else
         *    => add it to the sel_monitor_core_tab
         */
        for (i = 0; i < n; i++) {
                int j, found = 0;

                for (j = 0; j < sel_monitor_num &&
                             j < (int)DIM(sel_monitor_core_tab); j++) {
                        found = cmp_cgrps(&sel_monitor_core_tab[j],
                                          &cgrp_tab[i]);
                        if (found < 0) {
                                printf("Error: cannot monitor same "
                                       "cores in different groups\n");
                                goto error_exit;
                        }
                        if (found) {
                                sel_monitor_core_tab[j].events |= evt;
                                break;
                        }
                }
                if (!found &&
                    sel_monitor_num < (int) DIM(sel_monitor_core_tab)) {
                        struct core_group *cg =
                                &sel_monitor_core_tab[sel_monitor_num];
                        *cg = cgrp_tab[i];
                        cg->events = evt;
                        cg->pgrp = malloc(sizeof(struct pqos_mon_data));
                        if (cg->pgrp == NULL) {
                                printf("Error with memory allocation");
                                goto error_exit;
                        }
                        ++sel_monitor_num;
                } else {
                        free(cgrp_tab[i].cores);
                        free(cgrp_tab[i].desc);
                }
        }

        free(cgrp_tab);
        return;

error_exit:
        free(cgrp_tab);
        exit(EXIT_FAILURE);
}

void selfn_monitor_file_type(const char *arg)
{
        selfn_strdup(&sel_output_type, arg);
}

void selfn_monitor_file(const char *arg)
{
        selfn_strdup(&sel_output_file, arg);
}

void selfn_monitor_set_llc_percent(void)
{
        sel_llc_format = LLC_FORMAT_PERCENT;
}

void selfn_monitor_disable_ipc(const char *arg)
{
        UNUSED_ARG(arg);
        sel_disable_ipc = 1;
}

void selfn_monitor_disable_llc_miss(const char *arg)
{
        UNUSED_ARG(arg);
        sel_disable_llc_miss = 1;
}

void selfn_monitor_cores(const char *arg)
{
        char *cp = NULL, *str = NULL;
        char *saveptr = NULL;

        if (arg == NULL)
                parse_error(arg, "NULL pointer!");

        if (*arg == '\0')
                parse_error(arg, "Empty string!");

        selfn_strdup(&cp, arg);

        for (str = cp; ; str = NULL) {
                char *token = NULL;

                token = strtok_r(str, ";", &saveptr);
                if (token == NULL)
                        break;
                parse_monitor_cores(token);
        }

        free(cp);
}

/**
 * Update list of events to be monitored
 *
 * @param [in,out] events List of monitoring events
 * @param [in] cap_mon monitoring capability
 */
static void monitor_setup_events(enum pqos_mon_event *events,
                                 const struct pqos_capability * const cap_mon)
{
        unsigned i;
        enum pqos_mon_event all_evts = (enum pqos_mon_event)0;

        /**
         * get all available events on this platform
         */
        for (i = 0; i < cap_mon->u.mon->num_events; i++) {
                struct pqos_monitor *mon = &cap_mon->u.mon->events[i];

                all_evts |= mon->type;
        }

        /* Disable IPC monitoring */
        if (sel_disable_ipc)
                all_evts &= (enum pqos_mon_event)(~PQOS_PERF_EVENT_IPC);
        /* Disable LLC miss monitoring */
        if (sel_disable_llc_miss)
                all_evts &= (enum pqos_mon_event)(~PQOS_PERF_EVENT_LLC_MISS);

        /* check if all available events were selected */
        if ((*events & PQOS_MON_EVENT_ALL) == PQOS_MON_EVENT_ALL) {
                *events = (enum pqos_mon_event)(all_evts & *events);

        /* Start IPC and LLC miss monitoring if available */
        } else {
                if (all_evts & PQOS_PERF_EVENT_IPC)
                        *events |= (enum pqos_mon_event)PQOS_PERF_EVENT_IPC;
                if (all_evts & PQOS_PERF_EVENT_LLC_MISS)
                        *events |= (enum pqos_mon_event)
                                PQOS_PERF_EVENT_LLC_MISS;
        }

        sel_events_max |= *events;
}

int monitor_setup(const struct pqos_cpuinfo *cpu_info,
                  const struct pqos_capability * const cap_mon)
{
        unsigned i;
        int ret;

        ASSERT(sel_monitor_num >= 0);
        ASSERT(sel_process_num >= 0);

        /**
         * Check output file type
         */
        if (sel_output_type == NULL)
                sel_output_type = strdup("text");

        if (sel_output_type == NULL) {
                printf("Memory allocation error!\n");
                return -1;
        }

        if (strcasecmp(sel_output_type, "text") != 0 &&
            strcasecmp(sel_output_type, "xml") != 0 &&
            strcasecmp(sel_output_type, "csv") != 0) {
                printf("Invalid selection of file output type '%s'!\n",
                       sel_output_type);
                return -1;
        }

        /**
         * Set up file descriptor for monitored data
         */
        if (sel_output_file == NULL) {
                fp_monitor = stdout;
        } else {
                if (strcasecmp(sel_output_type, "xml") == 0 ||
                    strcasecmp(sel_output_type, "csv") == 0)
                        fp_monitor = safe_fopen(sel_output_file, "w+");
                else
                        fp_monitor = safe_fopen(sel_output_file, "a");
                if (fp_monitor == NULL) {
                        perror("Monitoring output file open error:");
                        printf("Error opening '%s' output file!\n",
                               sel_output_file);
                        return -1;
                }
        }
        if (strcasecmp(sel_output_type, "xml") == 0)
                fprintf(fp_monitor,
                        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n%s\n",
                        xml_root_open);

        /**
         * If no cores and events selected through command line
         * by default let's monitor all cores
         */
        if (sel_monitor_num == 0 && sel_process_num == 0) {
                for (i = 0; i < cpu_info->num_cores; i++) {
                        unsigned lcore  = cpu_info->cores[i].lcore;
                        uint64_t core = (uint64_t)lcore;
                        struct core_group *pg =
                                &sel_monitor_core_tab[sel_monitor_num];

                        if ((unsigned) sel_monitor_num >=
                            DIM(sel_monitor_core_tab))
                                break;
                        ret = set_cgrp(pg, uinttostr(lcore), &core, 1);
                        if (ret != 0) {
                                printf("Core group setup error!\n");
                                exit(EXIT_FAILURE);
                        }
                        pg->events = (enum pqos_mon_event) PQOS_MON_EVENT_ALL;
                        pg->pgrp = malloc(sizeof(*pg->pgrp));
                        if (pg->pgrp == NULL) {
                                printf("Error with memory allocation!\n");
                                exit(EXIT_FAILURE);
                        }
                        sel_monitor_num++;
                }
        }
        if (sel_process_num > 0 && sel_monitor_num > 0) {
                printf("Monitoring start error, process and core"
                       " tracking can not be done simultaneously\n");
                return -1;
        }
        if (!process_mode()) {
                /**
                 * Make calls to pqos_mon_start - track cores
                 */
                for (i = 0; i < (unsigned)sel_monitor_num; i++) {
                        struct core_group *cg = &sel_monitor_core_tab[i];

                        monitor_setup_events(&cg->events, cap_mon);

                        ret = pqos_mon_start(cg->num_cores, cg->cores,
                                             cg->events, (void *)cg->desc,
                                             cg->pgrp);
                        ASSERT(ret == PQOS_RETVAL_OK);
                        /**
                         * The error raised also if two instances of PQoS
                         * attempt to use the same core id.
                         */
                        if (ret != PQOS_RETVAL_OK) {
                                unsigned j;

                                if (ret == PQOS_RETVAL_PERF_CTR)
                                        printf("Use -r option to start "
                                               "monitoring anyway.\n");
                                printf("Monitoring start error on core(s) "
                                       "%s, status %d\n",
                                       cg->desc, ret);

                                /**
                                 * Stop mon groups that are already started
                                 */
                                for (j = 0; j < i; j++) {
                                        cg = &sel_monitor_core_tab[j];
                                        pqos_mon_stop(cg->pgrp);
                                }
                                return -1;
                        }
                }
        } else {
                /**
                 * Make calls to pqos_mon_start_pid - track PIDs
                 */
                for (i = 0; i < (unsigned)sel_process_num; i++) {
                        struct pid_group *pg = &sel_monitor_pid_tab[i];

                        monitor_setup_events(&pg->events, cap_mon);

                        ret = pqos_mon_start_pids(pg->num_pids, pg->pids,
                                                  pg->events, (void *)pg->desc,
                                                  pg->pgrp);
                        ASSERT(ret == PQOS_RETVAL_OK);
                        /**
                         * Any problem with monitoring the process?
                         */
                        if (ret != PQOS_RETVAL_OK) {
                                unsigned j;

                                printf("PID %s monitoring start error,"
                                       "status %d\n",
                                       sel_monitor_pid_tab[i].desc, ret);

                                /**
                                 * Stop mon groups that are already started
                                 */
                                for (j = 0; j < i; j++) {
                                        pg = &sel_monitor_pid_tab[j];
                                        pqos_mon_stop(pg->pgrp);
                                }
                                return -1;
                        }
                }
        }
        return 0;
}

void monitor_stop(void)
{
        int i;

        if (!process_mode())
                for (i = 0; i < sel_monitor_num; i++) {
                        int ret = pqos_mon_stop(sel_monitor_core_tab[i].pgrp);

                        if (ret != PQOS_RETVAL_OK)
                                printf("Monitoring stop error!\n");
                        free(sel_monitor_core_tab[i].desc);
                        free(sel_monitor_core_tab[i].cores);
                        free(sel_monitor_core_tab[i].pgrp);
                }
        else
                for (i = 0; i < sel_process_num; i++) {
                        int ret = pqos_mon_stop(sel_monitor_pid_tab[i].pgrp);

                        if (ret != PQOS_RETVAL_OK)
                                printf("Monitoring stop error!\n");
                        free(sel_monitor_pid_tab[i].desc);
                        free(sel_monitor_pid_tab[i].pids);
                        free(sel_monitor_pid_tab[i].pgrp);
                }
}

void selfn_monitor_time(const char *arg)
{
        if (arg == NULL)
                parse_error(arg, "NULL monitor time argument!");

        if (!strcasecmp(arg, "inf") || !strcasecmp(arg, "infinite"))
                sel_timeout = -1; /**< infinite timeout */
        else
                sel_timeout = (int) strtouint64(arg);
}

void selfn_monitor_interval(const char *arg)
{
        sel_mon_interval = (int) strtouint64(arg);
}

void selfn_monitor_top_like(const char *arg)
{
        UNUSED_ARG(arg);
        sel_mon_top_like = 1;
}


/**
 * @brief Adds pids for monitoring in pids monitoring mode
 *
 * @param[in] pgrp pid group
 * @param[in] evt events to be monitored
 *
 * @retval 1 when group is added
 * @retval 0 when group was not added
 */
static int
add_pids_for_monitoring(const struct pid_group *pgrp,
                        const enum pqos_mon_event evt)
{
        int j, found = 0;

        /**
         *  For each process:
         *  - if it's already there in the sel_monitor_pid_tab
         *  - update the entry
         *  - else - add it to the sel_monitor_pid_tab
         */
        for (j = 0; j < sel_process_num &&
                     j < (int)DIM(sel_monitor_pid_tab); j++) {
                found = cmp_pgrps(&sel_monitor_pid_tab[j], pgrp);
                if (found < 0) {
                        printf("Error: cannot monitor same "
                               "pids in different groups\n");
                        exit(EXIT_FAILURE);
                }
                if (found) {
                        sel_monitor_pid_tab[j].events |= evt;
                        break;
                }
        }
        if (!found &&
            sel_process_num < (int) DIM(sel_monitor_pid_tab)) {
                struct pid_group *pg = &sel_monitor_pid_tab[sel_process_num];
                *pg = *pgrp;
                pg->events = evt;
                pg->pgrp = malloc(sizeof(struct pqos_mon_data));
                if (pg->pgrp == NULL) {
                        printf("Error with memory allocation");
                        exit(EXIT_FAILURE);
                }
                ++sel_process_num;

                return 1;
        }

        return 0;
}


/**
 * @brief Verifies and translates monitoring config string into
 *        internal monitoring configuration.
 *
 * @param str string passed to -m command line option
 */
static void
parse_monitor_pids(char *str)
{
        int i = 0, n = 0;
        enum pqos_mon_event evt = (enum pqos_mon_event)0;
        struct pid_group *pgrp_tab = calloc(PQOS_MAX_PID_MON_GROUPS,
                                            sizeof(*pgrp_tab));

        if (pgrp_tab == NULL) {
                printf("Error with memory allocation!\n");
                exit(EXIT_FAILURE);
        }

        parse_event(str, &evt);

        n = strtogrps(strchr(str, ':') + 1, NULL, pgrp_tab,
                      PQOS_MAX_PID_MON_GROUPS);
        if (n < 0) {
                printf("Error: Too many pids/groups selected\n");
                goto error_exit;
        } else if (n == 0)
                parse_error(str, "No process id selected for monitoring");

        for (i = 0; i < n; i++)
                if (!add_pids_for_monitoring(&pgrp_tab[i], evt)) {
                        free(pgrp_tab[i].pids);
                        free(pgrp_tab[i].desc);
                }

        free(pgrp_tab);
        return;

 error_exit:
        free(pgrp_tab);
        exit(EXIT_FAILURE);
}

/**
 * @brief Verifies and translates multiple monitoring config strings into
 *        internal PID monitoring configuration
 *
 * @param arg argument passed to -p command line option
 */
void
selfn_monitor_pids(const char *arg)
{
        char *cp = NULL, *str = NULL;
        char *saveptr = NULL;

        if (arg == NULL)
                parse_error(arg, "NULL pointer!");

        if (*arg == '\0')
                parse_error(arg, "Empty string!");

        selfn_strdup(&cp, arg);

        for (str = cp; ; str = NULL) {
                char *token = NULL;

                token = strtok_r(str, ";", &saveptr);
                if (token == NULL)
                        break;

                parse_monitor_pids(token);
        }

        free(cp);
}

/**
 * @brief Opens /proc/[pid]/stat file for reading and returns pointer to
 *        associated FILE handle
 *
 * @param proc_pid_dir_name name of target PID directory e.g, "1234"
 *
 * @return ptr to FILE handle for /proc/[pid]/stat file opened for reading
 */
static FILE *
open_proc_stat_file(const char *proc_pid_dir_name)
{
        char path_buf[256];
        const char *proc_stat_path_fmt = "%s/%s/stat";

        ASSERT(proc_pid_dir_name != NULL);

        snprintf(path_buf, sizeof(path_buf) - 1, proc_stat_path_fmt,
                 proc_pids_dir, proc_pid_dir_name);

        return fopen(path_buf, "r");
}

/**
 * @brief Helper function for checking remainder-string filled out by
 *        strtoul function - checks if conversion succeeded or failed
 *
 * @param str_remainder remainder string from strtoul function
 *
 * @return boolean status of conversion
 * @retval 0 conversion failed
 * @retval 1 conversion succeeded
 */
static int
is_str_conversion_ok(const char *str_remainder)
{
        if (str_remainder == NULL)
                /* invalid remainder, cannot tell if content is OK or not*/
                return 0;

        if (*str_remainder == '\0')
                /* if nothing left for parsing, conversion succeeded*/
                return 1;
        else
                /* conversion failed*/
                return 0;
}

/**
 * @brief Returns value in /proc/<pid>/stat file at user defined column
 *
 * @param proc_pid_dir_name name of target PID directory e.g, "1234"
 * @param column value of the requested column number in
 *        the /proc/<pid>/stat file
 * @param len_val length of buffer user is going to pass the value into
 * @param val[out] value in column of the /proc/<pid>/stat file
 *
 * @return operation status
 * @retval 0 in case of success
 * @retval -1 in case of error
 */
static int
get_pid_stat_val(const char *proc_pid_dir_name, const int column,
                 const unsigned len_val, char *val)
{
        FILE *fproc_pid_stats;
        char buf[512];/* line in /proc/PID/stat is quite lengthy*/
        const char *delim = " ";
        size_t n_read;
        char *token, *saveptr;
        int col_idx = 1;/*starts from '1' like indexes on 'stat' man-page*/

        if (proc_pid_dir_name == NULL || val == NULL)
                return -1;

        /*open /proc/<pid>/stat file for reading*/
        fproc_pid_stats = open_proc_stat_file(proc_pid_dir_name);
        if (fproc_pid_stats == NULL)/*failure in reading if file is empty*/
                return -1;

        memset(buf, 0, sizeof(buf));

        /*put file into buffer to parse values from*/
        n_read = fread(buf, sizeof(char), sizeof(buf) - 1, fproc_pid_stats);

        /*close file as its not needed*/
        fclose(fproc_pid_stats);

        /*if buffer is empty, error*/
        if (n_read == 0)
                return -1;

        /*split buffer*/
        token = strtok_r(buf, delim, &saveptr);

        if (token == NULL)
                return -1;

        /*check each value from the split and disregard if not needed*/
        do {
                if (col_idx == column) {
                        /*check to see if value will fit in users buffer*/
                        if (len_val <= (strlen(token)+1)) {
                                return -1;
                        } else {
                                strncpy(val, token, len_val);
                                val[len_val - 1] = '\0';
                                return 0;/*value can be read from *val param*/
                        }
                }
                col_idx++;
        /* Loop continues until value is found
         * or until there is nothing left in the buffer
         */
        } while ((token = strtok_r(NULL, delim, &saveptr)) != NULL);

        return -1; /*error if while loop finishes and nothing left in buffer*/
}

/**
 * @brief Returns combined ticks that process spent by using cpu both in
 *        user mode and kernel mode
 *
 * @param proc_pid_dir_name name of target PID directory e.g, "1234"
 * @param cputicks[out] cputicks value for given PID, it is filled as 'out'
 *                      value and has to be != NULL
 *
 * @return operation status
 * @retval 0 in case of success
 * @retval -1 in case of error
 */
static int
get_pid_cputicks(const char *proc_pid_dir_name, uint64_t *cputicks)
{
        unsigned i;
        const int col_val[3] = {PID_COL_STATUS, PID_COL_UTIME, PID_COL_STIME};

        if (proc_pid_dir_name == NULL || cputicks == NULL)
                return -1;

        /* Loops through the /proc/<pid>/stat file three times to get values
         * for pid status, user time, and kernel time
         */
        for (i = 0; i < DIM(col_val); i++) {
                char time_str[64];
                char *tmp;
                uint64_t time_int = 0;
                int time_success = 0;

                memset(time_str, 0, sizeof(time_str));/*set time buffer to 0*/

                time_success = get_pid_stat_val(proc_pid_dir_name,
                                                col_val[i], sizeof(time_str),
                                                time_str);
                if (time_success != 0)
                        return -1;

                if (col_val[i] == PID_COL_STATUS) {
                        /* Checking status column in order to find valid
                         * status for top-pid mode processes and eliminate
                         * processes that are zombies, stopped etc.
                         */
                        if (strpbrk(time_str, proc_stat_whitelist) == NULL)
                                /* Not valid status,ignoring entry*/
                                return -1;
                        else
                                continue;
                }

                time_int = (uint64_t)strtoull(time_str, &tmp, 10);
                /* Check to make sure string converted
                 * to int correctly
                 */
                if (is_str_conversion_ok(tmp))
                        *cputicks += time_int;
                else
                        return -1;
        }

        /* Value for cputicks can be read from *cputicks param*/
        return 0;
}

/**
 * @brief Returns core number \a pid last ran on
 *
 * @param pid process ID of target PID e.g. "1234"
 * @param core[out] core number that \a pid last ran on
 *
 * @return operation status
 * @retval 0 in case of success
 * @retval -1 in case of error
 */
static int
get_pid_core_num(const pid_t pid, unsigned *core)
{
        char core_s[64];
        char pid_s[64];
        char *tmp;
        int ret;

        if (core == NULL || pid < 0)
                return -1;

        memset(core_s, 0, sizeof(core_s));
        ret = uinttostr_noalloc(pid_s, sizeof(pid_s), pid);
        if (ret < 0)
                return -1;

        ret = get_pid_stat_val(pid_s, PID_COL_CORE, sizeof(core_s), core_s);
        if (ret != 0)
                return -1;

        *core = strtoul(core_s, &tmp, 10);

        if (is_str_conversion_ok(tmp) == 0)
                return -1;

        return 0;
}

/**
 * @brief Comparator for unsigned - needed for qsort
 *
 * @param a unsigned value to be compared
 * @param b unsigned value to be compared
 *
 * @return Comparison status
 * @retval negative number when (a < b)
 * @retval 0 when (a == b)
 * @retval positive number when (a > b)
 */
static int
unsigned_cmp(const void *a, const void *b)
{
        const unsigned *pa = (const unsigned *)a;
        const unsigned *pb = (const unsigned *)b;

        if (*pa < *pb)
                return -1;
        else if (*pa > *pb)
                return 1;
        else
                return 0;
}

/**
 * @brief Function to return a comma separated list of all cores that PIDs
 *        in \a mon_data last ran on.
 *
 * @param mon_data struct with info on the group of pids to be monitored
 * @param cores_s[out] char pointer to hold string of cores the PIDs last ran on
 * @param len length of cores_s
 *
 * @param retval 0 in case of success, -1 for error
 */
static int
get_pid_cores(const struct pqos_mon_data *mon_data, char *cores_s,
              const int len)
{
        char core[16];
        unsigned i;
        int str_len = 0;
        int cores_s_len = 0;
        int comma_len = 1;
        unsigned *cores;
        const pid_t *tids;
        unsigned num_tids;
        int result = 0;

        ASSERT(mon_data != NULL);
        ASSERT(cores_s != NULL);

        num_tids = mon_data->tid_nr;
        tids = mon_data->tid_map;
        if (tids == NULL)
                return -1;

        cores = calloc(num_tids, sizeof(*cores));
        if (cores == NULL) {
                printf("Error allocating memory\n");
                return -1;
        }

        for (i = 0; i < num_tids; i++)
                if (get_pid_core_num(tids[i], &cores[i]) == -1) {
                        result = -1;
                        goto free_memory;
                }

        qsort(cores, num_tids, sizeof(*cores), unsigned_cmp);

        for (i = 0; i < num_tids; i++) {

                /* check for duplicate cores and skips them*/
                if (i != 0 && cores[i] == cores[i-1])
                        continue;

                str_len = uinttostr_noalloc(core, sizeof(core), cores[i]);
                if (str_len < 0) {
                        result = -1;
                        goto free_memory;
                }

                cores_s_len = strlen(cores_s);

                if (i != 0 && (cores_s_len + str_len + comma_len) < len) {
                        strncat(cores_s, ",", len - cores_s_len);
                        strncat(cores_s, core, len - cores_s_len - comma_len);
                } else if (i == 0 && (cores_s_len + str_len) < len)
                        strncat(cores_s, core, len - cores_s_len);
                else {
                        result = -1;
                        goto free_memory;
                }
        }

free_memory:
        free(cores);
        return result;
}

/**
 * @brief Allocates memory and initializes new list element and returns ptr
 *        to newly created list-element with given data
 *
 * @param data pointer to data that will be stored in list element
 *
 * @return pointer to allocated and initialized struct slist element. It has to
 *         be freed when no longer needed
 */
static struct slist *
slist_create_elem(void *data)
{
        struct slist *node = (struct slist *)
                malloc(sizeof(struct slist));

        if (node == NULL) {
                printf("Error with memory allocation for slist element!");
                exit(EXIT_FAILURE);
        }

        node->data = data;
        node->next = NULL;

        return node;
}

/**
 * @brief Looks for an element with given pid in slist of proc_stats elements
 *
 * @param pslist pointer to beginning of a slist
 * @param pid pid to be searched in the slist
 *
 * @return ptr to found struct proc_stats or NULL in case element with given PID
 *         has not been found in the slist
 */
static struct proc_stats *
find_proc_stats_by_pid(const struct slist *pslist, const pid_t pid)
{
        const struct slist *it = NULL;

        for (it = pslist; it != NULL; it = it->next) {
                ASSERT(it->data != NULL);
                struct proc_stats *pstats = (struct proc_stats *)it->data;

                if (pstats->pid == pid)
                        return pstats;

        }

        return NULL;
}

/**
 * @brief Counts cpu_avg_ratio for PID and fills it into proc_stats
 *
 * @param pstat[out] proc_stats structure representing stats for process, it
 *                   will be filled with processed cpu_avg_ratio
 *                   and has to be != NULL
 * @param proc_start_time time when process has been started (time from
 *                        beginning of epoch in seconds)
 */
static void
fill_cpu_avg_ratio(struct proc_stats *pstat, const time_t proc_start_time)
{
        time_t curr_time, run_time;

        ASSERT(pstat != NULL);

        curr_time = time(0);
        run_time = curr_time - proc_start_time;
        if (run_time != 0)
                pstat->cpu_avg_ratio = (double)pstat->ticks_delta / run_time;
        else
                pstat->cpu_avg_ratio = 0.0;
}

/**
 * @brief Add statistics for given pid in form of proc_stats struct to slist.
 *        New element-node is allocated and added on beginning of the list.
 *
 * @param pslist pointer to single linked list of proc_stats. It will be
 *               filled with new proc_stats entry. If it equals NULL,
 *               new list will be started with given element
 * @param pid process pid to be added
 * @param cputicks cputicks spent by this process
 * @param proc_start_time time of process creation(seconds since the Epoch)
 *
 * @return pointer to beginning of process statistics slist
 */
static struct slist *
add_proc_cpu_stat(struct slist *pslist, const pid_t pid,
                  const unsigned long cputicks, const time_t proc_start_time)
{
        /* have to allocate new proc_stats struct */
        struct proc_stats *pstat = malloc(sizeof(struct proc_stats));

        if (pstat == NULL) {
                printf("Error with memory allocation for pstat!");
                exit(EXIT_FAILURE);
        }

        pstat->pid = pid;
        pstat->ticks_delta = cputicks;
        pstat->valid = 0;
        fill_cpu_avg_ratio(pstat, proc_start_time);
        struct slist *elem = slist_create_elem(pstat);

        if (pslist == NULL)
                /* starting new list of proc_stats elements*/
                pslist = elem;
        else {
                /* prepending */
                elem->next = pslist;
                pslist = elem;
        }

        return pslist;
}

/**
 * @brief Updates statistics for given pid in in slist
 *
 * @param pslist pointer to slist of proc_stats. Only internal data
 *               will be updated, no new list entries will be created
 *               or removed.
 * @param pid pid number of a process to be updated
 * @param cputicks cputicks spent by this process
 */
static void
update_proc_cpu_stat(const struct slist *pslist, const pid_t pid,
                     const unsigned long cputicks)
{
        /* at first we have to look for previous stats for a given PID*/
        struct proc_stats *ps_updt = find_proc_stats_by_pid(pslist, pid);

        if (ps_updt == NULL)
                /* PID not found, probably new process, can't to fill
                 * ticks_delta so silently returning unmodified list
                 */
                return;

        /* checking if cputicks diff will be valid e.g. won't generate
         * negative diff number in a result
         */
        if (cputicks >= ps_updt->ticks_delta) {
                ps_updt->ticks_delta = cputicks - ps_updt->ticks_delta;

                /* Marking PID statistics as valid - ticks_delta and
                 * cpu_avg_ratio can be used safely during top-pids
                 * selection. This kind of checking is needed to get
                 * rid of dead-pids - processes that existed during
                 * getting first part of statistics and before updated
                 * ticks delta was computed.
                 */
                ps_updt->valid = 1;
        } else {
                /* Probably different process went into previously used PID
                 * number. Zeroing fields for safety and leaving marked as
                 * invalid.
                 */
                ps_updt->ticks_delta = 0;
                ps_updt->cpu_avg_ratio = 0;
                ps_updt->valid = 0;
        }
}

/**
 * @brief Gets start_time value for given PID directory - this can be used as
 * information for how long process lives (we can get time of process creation
 * by checking st_mtime field of /proc/[pid] directory statistics)
 *
 * @param proc_dir DIR structure for '/proc' directory
 * @param pid_dir_name name of PID directory in /proc, e.g. "1234"
 * @param start_time[out] time when process has been started (time from
 *                        beginning of epoch in seconds)
 *
 * @return operation status
 * @retval 0 in case of success
 * @retval -1 in case of error
 */
static int
get_proc_start_time(DIR *proc_dir, const char *pid_dir_name, time_t *start_time)
{
        struct stat p_dir_stat;

        if (start_time == NULL || pid_dir_name == NULL)
                return -1;

        if (fstatat(dirfd(proc_dir), pid_dir_name, &p_dir_stat, 0) != 0)
                return -1;

        *start_time = p_dir_stat.st_mtime;

        return 0;
}

/**
 * @brief Gets pid number for given /proc/pid directory or returns error if
 *        given directory does not hold PID information. It can be used to
 *        filter out non-pid directories in /proc
 *
 * @param proc_dir DIR structure for '/proc' directory
 * @param pid_dir_name name of PID directory in /proc, e.g. "1234"
 * @param pid[out] pid number to be filled
 *
 * @return operation status
 * @retval 0 in case of success
 * @retval -1 in case of error
 */
static int
get_pid_num_from_dir(DIR *proc_dir, const char *pid_dir_name, pid_t *pid)
{
        struct stat p_dir_stat;
        char *tmp_end;/* used for strtoul error check*/
        int ret;

        if (pid == NULL || pid_dir_name == NULL)
                return -1;

        /* trying to get pid number from directory name*/
        *pid = strtoul(pid_dir_name, &tmp_end, 10);
        if (!is_str_conversion_ok(tmp_end))
                return -1; /* conversion failed, not proc-pid */

        ret = fstatat(dirfd(proc_dir), pid_dir_name, &p_dir_stat, 0);
        if (ret)
                /* couldn't get valid stat, can't check pid */
                return -1;

        if (!S_ISDIR(p_dir_stat.st_mode))
                return -1; /* ignoring not-directories */

        /* all checks passed, marking as success */
        return 0;
}

/**
 * @brief Fills slist of proc_stats with process cpu usage stats
 *
 * @param pslist[out] pointer to pointer to slist of proc_stats to be filled
 *                    with new proc_stats entries or entries that will be
 *                    updated. If pslist points to a NULL, then new slist will
 *                    be created and thus memory has to be freed when list
 *                    (and list content) won't be needed anymore.
 *                    If pslist points to not-NULL slist, then content will be
 *                    updated and no additional memory will be mallocated.
 *
 * @return operation status
 * @retval 0 in case of success
 * @retval -1 in case of error
 */
static int
get_proc_pids_stats(struct slist **pslist)
{
        int initialized = 0;
        struct dirent *file;
        DIR *proc_dir = opendir(proc_pids_dir);

        ASSERT(pslist != NULL);
        if (*pslist != NULL)
                /* updating existing entries in list of process stats*/
                initialized = 1;

        if (proc_dir == NULL) {
                perror("Could not open /proc directory:");
                return -1;
        }

        while ((file = readdir(proc_dir)) != NULL) {
                uint64_t cputicks = 0;
                time_t start_time = 0;
                pid_t pid = 0;
                int err;

                err = get_pid_num_from_dir(proc_dir, file->d_name, &pid);
                if (err)
                        continue; /* not a PID directory */

                err = get_pid_cputicks(file->d_name, &cputicks);
                if (err)
                        /* couldn't get cputicks, ignoring this PID-dir*/
                        continue;

                if (!initialized) {
                        err = get_proc_start_time(proc_dir, file->d_name,
                                                  &start_time);
                        if (err)
                        /* start time for given pid is needed for correct CPU
                         * usage statistics - without that we have to ignore
                         * problematic pid entry and move on
                         */
                                continue;

                        *pslist = add_proc_cpu_stat(*pslist, pid, cputicks,
                                                    start_time);
                } else
                        /* only updating proc_stats entries*/
                        update_proc_cpu_stat(*pslist, pid, cputicks);
        }

        closedir(proc_dir);
        return 0;
}

/**
 * @brief Comparator for proc_stats structure - needed for qsort
 *
 * @param a proc_stat data A
 * @param b proc_stat data B
 *
 * @return Comparison status
 * @retval negative number when (a < b)
 * @retval 0 when (a == b)
 * @retval positive number when (a > b)
 */
static int
proc_stats_cmp(const void *a, const void *b)
{
        const struct proc_stats *pa = (const struct proc_stats *)a;
        const struct proc_stats *pb = (const struct proc_stats *)b;

        if (pa->ticks_delta == pb->ticks_delta) {
                /* when tick deltas are equal then comparing cpu_avg*/
                /* NOTE: both ratios are double numbers therefore
                 * comparing here manually to get correct
                 * integer compare-result (during subtracting, if
                 * difference would be between 0 and 1.0, then it would be
                 * wrongly returned as '0' int value (a == b))
                 */
                if (pa->cpu_avg_ratio < pb->cpu_avg_ratio)
                        return -1;
                else if (pa->cpu_avg_ratio > pb->cpu_avg_ratio)
                        return 1;
                else
                        return 0;
        } else
                return (pa->ticks_delta - pb->ticks_delta);
}

/**
 * @brief Fills top processes array - based on CPU usage of all processes
 *        statistics in the system stored in given slist.
 *        From all processes in the system we are choosing 'max_size' amount
 *        of resulting proc stats with highest CPU usage
 *
 * @param pslist list with all processes statistics (holds proc_stats)
 * @param top_procs[out] array to be filled with top-processes data
 * @param max_size max number of elements that top_procs can hold
 *
 * @return number of valid top processes in top_procs filled array (usually
 *         this will equal to max_size)
 */
static int
fill_top_procs(const struct slist *pslist, struct pid_group *top_procs,
               const int max_size)
{
        const struct slist *it = NULL;
        int current_size = 0;
        int i;
        struct proc_stats stats[TOP_PROC_MAX];

        ASSERT(max_size <= TOP_PROC_MAX);
        ASSERT(top_procs != NULL);

        memset(top_procs, 0, sizeof(top_procs[0]) * max_size);
        memset(stats, 0, sizeof(stats[0]) * max_size);

        /* Iterating on CPU usage stats for all of the stored processes in
         * pslist in order to get max_size of 'survivors' - processes
         * with highest CPU usage in the system
         */
        for (it = pslist; it != NULL; it = it->next) {
                struct proc_stats *ps = (struct proc_stats *)it->data;

                ASSERT(ps != NULL);

                if (ps->valid == 0)
                        /* ignore not-fully filled entries (e.g. dead PIDs
                         * or abandoned statistics because of some error)
                         */
                        continue;

                /* If we have free slots in stats array, then things are
                 * simple. We have only to add proc_stats into free slot and
                 * sort entire array afterwards (sorting is done below)
                 */
                if (current_size < max_size) {
                        stats[current_size] = *ps;
                        current_size++;
                } else {
                        /* Handling more pids than can be saved in the top-pids
                         * array. At first we have to check if one of the slots
                         * can be consumed for current proc stats.
                         * Only have to compare smallest element in array,
                         * (list is stored in ascending manner so if it is
                         * smaller from first element, it is smaller than
                         * next element as well)
                         */
                        if (proc_stats_cmp(ps, &stats[0]) <= 0)
                                /* if it is smaller/equal than smallest
                                 * element, ignoring and moving on
                                 */
                                continue;

                        /* adding at place of the smallest element and array
                         * will be sorted below
                         */
                        stats[0] = *ps;
                }

                /* Some change has been made, we have to sort entire array to
                 * make sure that array is always sorted before next element
                 * will be added
                 */
                qsort(stats, current_size, sizeof(stats[0]), proc_stats_cmp);
        }

        /**
         * Fill top_proc table
         */
        for (i = 0; i < current_size; i++) {
                char *desc = uinttostr((unsigned)stats[i].pid);
                uint64_t pid = (uint64_t)stats[i].pid;

                set_pgrp(&top_procs[i], desc, &pid, 1);
        }

        return current_size;
}

/**
 * @brief Looks for processes with highest CPU usage on the system and
 *        starts monitoring for them. Processes are displayed and sorted
 *        afterwards by LLC occupancy
 */
void
selfn_monitor_top_pids(void)
{
        int res = 0, top_size = 0, i;
        struct slist *pslist = NULL, *it = NULL;
        struct pid_group top_procs[TOP_PROC_MAX];

        printf("Monitoring top-pids enabled\n");
        sel_mon_top_like = 1;

        /* getting initial values for CPU usage for processes */
        res = get_proc_pids_stats(&pslist);
        if (res) {
                printf("Getting processor usage statistic failed!");
                goto cleanup_pslist;
        }

        /* Giving here some time for processes for generating cpu activity.
         * In general, there are two ways of calculating CPU usage:
         * -the instantaneous one (checking ticks during last interval)
         * -average one (reported ps)
         *
         * We are using a hybrid approach, at first looking for processes that
         * did some work in last interval (and this is the reason for sleep
         * here) but in case that there is not enough processes which reported
         * cpu ticks, we are checking average ticks ratio for process lifetime.
         * So the more time time we are sleeping here, the more processes will
         * report ticks during this sleep interval and less processes will be
         * found by checking average ticks ratio(it is less accurate method)
         */
        usleep(PID_CPU_TIME_DELAY_USEC);

        /* Getting updated CPU usage statistics*/
        res = get_proc_pids_stats(&pslist);
        if (res) {
                printf("Getting updated processor usage statistic failed!");
                goto cleanup_pslist;
        }

        top_size = fill_top_procs(pslist, top_procs, TOP_PROC_MAX);

        /* finally we can add list of top-pids for LLC/MBM monitoring
         * NOTE: list was sorted in ascending order, so in order to have
         * initially top-cpu processes on top, we are adding in reverse
         * order
         */
        for (i = (top_size - 1); i >= 0; --i)
                add_pids_for_monitoring(&top_procs[i],
                        (enum pqos_mon_event)PQOS_MON_EVENT_ALL);

cleanup_pslist:
        /* cleaning list of all processes stats */
        it = pslist;
        while (it != NULL) {
                struct slist *tmp = it;

                it = it->next;
                free(tmp->data);
                free(tmp);
        }
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
 * @brief CTRL-C handler for infinite monitoring loop
 *
 * @param signo signal number
 */
static void monitoring_ctrlc(int signo)
{
        UNUSED_ARG(signo);
        stop_monitoring_loop = 1;
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
fillin_text_column(const char *format,
                   const double val,
                   char data[],
                   const size_t sz_data,
                   const int is_monitored,
                   const int is_column_present)
{
        static const char blank_column[] = "            ";
        size_t offset = 0;

        if (sz_data <= sizeof(blank_column))
                return 0;

        if (is_monitored) {
                /**
                 * This is monitored and we have the data
                 */
                snprintf(data, sz_data - 1, format, val);
                offset = strlen(data);
        } else if (is_column_present) {
                /**
                 * The column exists though there's no data
                 */
                strncpy(data, blank_column, sz_data - 1);
                offset = strlen(data);
        }

        return offset;
}

/**
 * @brief Fills in single XML column in the monitoring table
 *
 * @param format numerical value format
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
fillin_xml_column(const char *const format,
                  const double val,
                  char data[],
                  const size_t sz_data,
                  const int is_monitored,
                  const int is_column_present,
                  const char node_name[])
{
        size_t offset = 0;

        if (is_monitored) {
                char formatted_val[16];

                snprintf(formatted_val, 15, format, val);

                /**
                 * This is monitored and we have the data
                 */
                snprintf(data, sz_data - 1, "\t<%s>%s</%s>\n",
                         node_name, formatted_val, node_name);
                offset = strlen(data);
        } else if (is_column_present) {
                /**
                 * The column exists though there's no data
                 */
                snprintf(data, sz_data - 1, "\t<%s></%s>\n",
                         node_name, node_name);
                offset = strlen(data);
        }

        return offset;
}

/**
 * @brief Fills in single CSV column in the monitoring table
 *
 * @param format numerical value format
 * @param val numerical value to be put into the column
 * @param data place to put formatted column into
 * @param sz_data available size for the column
 * @param is_monitored if true then \a val holds valid data
 * @param is_column_present if true then corresponding event is
 *        selected for display
 * @return Number of characters added to \a data excluding NULL
 */
static size_t
fillin_csv_column(const char *format,
                  const double val,
                  char data[],
                  const size_t sz_data,
                  const int is_monitored,
                  const int is_column_present)
{
        size_t offset = 0;

        if (is_monitored) {
                /**
                 * This is monitored and we have the data
                 */
                snprintf(data, sz_data - 1, format, val);
                offset = strlen(data);
        } else if (is_column_present) {
                /**
                 * The column exists though there's no data
                 */
                snprintf(data, sz_data - 1, ",");
                offset = strlen(data);
        }

        return offset;
}

/**
 * @brief Prints row of monitoring data in text format
 *
 * @param fp pointer to file to direct output
 * @param mon_data pointer to pqos_mon_data structure
 * @param llc_entry LLC occupancy data structure
 * @param mbr remote memory bandwidth data
 * @param mbl local memory bandwidth data
 * @param mbt total memory bandwidth data
 */
static void
print_text_row(FILE *fp,
               struct pqos_mon_data *mon_data,
               const struct llc_entry_data *llc_entry,
               const double mbr,
               const double mbl,
               const double mbt)
{
        const size_t sz_data = 256;
        char data[sz_data];
        size_t offset = 0;
        char core_list[PQOS_MAX_CORES * 4];

        ASSERT(fp != NULL);
        ASSERT(mon_data != NULL);
        ASSERT(llc_entry != NULL);

        memset(data, 0, sz_data);

#ifdef PQOS_RMID_CUSTOM
        if (sel_interface == PQOS_INTER_MSR) {
                pqos_rmid_t rmid;
                int ret = pqos_mon_assoc_get(mon_data->cores[0], &rmid);

                offset +=
                    fillin_text_column(" %4.0f",
                                       (double)rmid,
                                       data + offset,
                                       sz_data - offset,
                                       ret == PQOS_RETVAL_OK,
                                       sel_interface == PQOS_INTER_MSR);
        }
#endif

        offset += fillin_text_column(" %11.2f", mon_data->values.ipc,
                                     data + offset,
                                     sz_data - offset,
                                     mon_data->event & PQOS_PERF_EVENT_IPC,
                                     sel_events_max & PQOS_PERF_EVENT_IPC);

        offset += fillin_text_column(" %10.0fk", (double)
                                     mon_data->values.llc_misses_delta/1000,
                                     data + offset,
                                     sz_data - offset,
                                     mon_data->event & PQOS_PERF_EVENT_LLC_MISS,
                                     sel_events_max & PQOS_PERF_EVENT_LLC_MISS);

        offset += fillin_text_column(" %11.1f", llc_entry->val, data + offset,
                                     sz_data - offset,
                                     mon_data->event & PQOS_MON_EVENT_L3_OCCUP,
                                     sel_events_max & PQOS_MON_EVENT_L3_OCCUP);

        offset += fillin_text_column(" %11.1f", mbl, data + offset,
                                     sz_data - offset,
                                     mon_data->event & PQOS_MON_EVENT_LMEM_BW,
                                     sel_events_max & PQOS_MON_EVENT_LMEM_BW);

        offset += fillin_text_column(" %11.1f", mbr, data + offset,
                                     sz_data - offset,
                                     mon_data->event & PQOS_MON_EVENT_RMEM_BW,
                                     sel_events_max & PQOS_MON_EVENT_RMEM_BW);

        fillin_text_column(" %11.1f", mbt, data + offset, sz_data - offset,
                           mon_data->event & PQOS_MON_EVENT_TMEM_BW,
                           sel_events_max & PQOS_MON_EVENT_TMEM_BW);

        if (!process_mode())
                fprintf(fp, "\n%8.8s%s", (char *)mon_data->context, data);
        else {
                memset(core_list, 0, sizeof(core_list));

                if (get_pid_cores(mon_data, core_list,
                                  sizeof(core_list)) == -1) {
                        memset(core_list, 0, sizeof(core_list));
                        strncat(core_list, "err", sizeof(core_list) - 1);
                }

                fprintf(fp, "\n%8.8s %8.8s%s", (char *)mon_data->context,
                        core_list, data);
        }
}

/**
 * @brief Prints row of monitoring data in xml format
 *
 * @param fp pointer to file to direct output
 * @param time pointer to string containing time data
 * @param mon_data pointer to pqos_mon_data structure
 * @param llc_entry LLC occupancy data structure
 * @param mbr remote memory bandwidth data
 * @param mbl local memory bandwidth data
 * @param mbt total memory bandwidth data
 */
static void
print_xml_row(FILE *fp,
              char *time,
              struct pqos_mon_data *mon_data,
              const struct llc_entry_data *llc_entry,
              const double mbr,
              const double mbl,
              const double mbt)
{
        const size_t sz_data = 256;
        char data[sz_data];
        char core_list[PQOS_MAX_CORES * 4];
        size_t offset = 0;
        const char *l3_text = NULL;

        ASSERT(fp != NULL);
        ASSERT(time != NULL);
        ASSERT(mon_data != NULL);
        ASSERT(llc_entry != NULL);

        switch (llc_entry->format) {
        case LLC_FORMAT_KILOBYTES:
                l3_text = "l3_occupancy_kB";
                break;
        case LLC_FORMAT_PERCENT:
                l3_text = "l3_occupancy_percent";
                break;
        }

#ifdef PQOS_RMID_CUSTOM
        if (sel_interface == PQOS_INTER_MSR) {
                pqos_rmid_t rmid;
                int ret = pqos_mon_assoc_get(mon_data->cores[0], &rmid);

                offset += fillin_xml_column("%.0f",
                                            rmid,
                                            data + offset,
                                            sz_data - offset,
                                            ret == PQOS_RETVAL_OK,
                                            sel_interface == PQOS_INTER_MSR,
                                            "rmid");
        }
#endif

        offset += fillin_xml_column("%.2f", mon_data->values.ipc, data + offset,
                                    sz_data - offset,
                                    mon_data->event & PQOS_PERF_EVENT_IPC,
                                    sel_events_max & PQOS_PERF_EVENT_IPC,
                                    "ipc");

        offset += fillin_xml_column("%.0f", (double)
                                    mon_data->values.llc_misses_delta,
                                    data + offset,
                                    sz_data - offset,
                                    mon_data->event & PQOS_PERF_EVENT_LLC_MISS,
                                    sel_events_max & PQOS_PERF_EVENT_LLC_MISS,
                                    "llc_misses");

        offset += fillin_xml_column("%.1f", llc_entry->val, data + offset,
                                    sz_data - offset,
                                    mon_data->event & PQOS_MON_EVENT_L3_OCCUP,
                                    sel_events_max & PQOS_MON_EVENT_L3_OCCUP,
                                    l3_text);

        offset += fillin_xml_column("%.1f", mbl, data + offset,
                                    sz_data - offset,
                                    mon_data->event & PQOS_MON_EVENT_LMEM_BW,
                                    sel_events_max & PQOS_MON_EVENT_LMEM_BW,
                                    "mbm_local_MB");

        offset += fillin_xml_column("%.1f", mbr, data + offset,
                                    sz_data - offset,
                                    mon_data->event & PQOS_MON_EVENT_RMEM_BW,
                                    sel_events_max & PQOS_MON_EVENT_RMEM_BW,
                                    "mbm_remote_MB");

        fillin_xml_column("%.1f", mbt, data + offset, sz_data - offset,
                          mon_data->event & PQOS_MON_EVENT_TMEM_BW,
                          sel_events_max & PQOS_MON_EVENT_TMEM_BW,
                          "mbm_total_MB");

        if (!process_mode())
                fprintf(fp,
                        "%s\n"
                        "\t<time>%s</time>\n"
                        "\t<core>%s</core>\n"
                        "%s"
                        "%s\n",
                        xml_child_open,
                        time,
                        (char *)mon_data->context,
                        data,
                        xml_child_close);
        else {
                memset(core_list, 0, sizeof(core_list));

                if (get_pid_cores(mon_data, core_list,
                                  sizeof(core_list)) == -1) {
                        memset(core_list, 0, sizeof(core_list));
                        strncat(core_list, "err", sizeof(core_list) - 1);
                }

                fprintf(fp,
                        "%s\n"
                        "\t<time>%s</time>\n"
                        "\t<pid>%s</pid>\n"
                        "\t<core>%s</core>\n"
                        "%s"
                        "%s\n",
                        xml_child_open,
                        time,
                        (char *)mon_data->context,
                        core_list,
                        data,
                        xml_child_close);
        }
}

/**
 * @brief Prints row of monitoring data in csv format
 *
 * @param fp pointer to file to direct output
 * @param time pointer to string containing time data
 * @param mon_data pointer to pqos_mon_data structure
 * @param llc_entry LLC occupancy data structure
 * @param mbr remote memory bandwidth data
 * @param mbl local memory bandwidth data
 * @param mbt total memory bandwidth data
 */
static void
print_csv_row(FILE *fp, char *time,
              struct pqos_mon_data *mon_data,
              const struct llc_entry_data *llc_entry,
              const double mbr,
              const double mbl,
              const double mbt)
{
        const size_t sz_data = 128;
        char data[sz_data];
        char core_list[PQOS_MAX_CORES];
        size_t offset = 0;

        ASSERT(fp != NULL);
        ASSERT(time != NULL);
        ASSERT(mon_data != NULL);
        ASSERT(llc_entry != NULL);

        memset(data, 0, sz_data);

#ifdef PQOS_RMID_CUSTOM
        if (sel_interface == PQOS_INTER_MSR) {
                pqos_rmid_t rmid;
                int ret = pqos_mon_assoc_get(mon_data->cores[0], &rmid);

                offset += fillin_csv_column(",%.0f",
                                            rmid,
                                            data + offset,
                                            sz_data - offset,
                                            ret == PQOS_RETVAL_OK,
                                            sel_interface == PQOS_INTER_MSR);
        }
#endif

        offset += fillin_csv_column(",%.2f", mon_data->values.ipc,
                                    data + offset,
                                    sz_data - offset,
                                    mon_data->event & PQOS_PERF_EVENT_IPC,
                                    sel_events_max & PQOS_PERF_EVENT_IPC);

        offset += fillin_csv_column(",%.0f", (double)
                                    mon_data->values.llc_misses_delta,
                                    data + offset,
                                    sz_data - offset,
                                    mon_data->event & PQOS_PERF_EVENT_LLC_MISS,
                                    sel_events_max & PQOS_PERF_EVENT_LLC_MISS);

        offset += fillin_csv_column(",%.1f", llc_entry->val, data + offset,
                                    sz_data - offset,
                                    mon_data->event & PQOS_MON_EVENT_L3_OCCUP,
                                    sel_events_max & PQOS_MON_EVENT_L3_OCCUP);

        offset += fillin_csv_column(",%.1f", mbl, data + offset,
                                    sz_data - offset,
                                    mon_data->event & PQOS_MON_EVENT_LMEM_BW,
                                    sel_events_max & PQOS_MON_EVENT_LMEM_BW);

        offset += fillin_csv_column(",%.1f", mbr, data + offset,
                                    sz_data - offset,
                                    mon_data->event & PQOS_MON_EVENT_RMEM_BW,
                                    sel_events_max & PQOS_MON_EVENT_RMEM_BW);

        fillin_csv_column(",%.1f", mbt, data + offset, sz_data - offset,
                          mon_data->event & PQOS_MON_EVENT_TMEM_BW,
                          sel_events_max & PQOS_MON_EVENT_TMEM_BW);

        if (!process_mode())
                fprintf(fp,
                        "%s,\"%s\"%s\n",
                        time, (char *)mon_data->context, data);
        else {
                memset(core_list, 0, sizeof(core_list));

                if (get_pid_cores(mon_data, core_list,
                                  sizeof(core_list)) == -1) {
                        memset(core_list, 0, sizeof(core_list));
                        strncat(core_list, "err", sizeof(core_list) - 1);
                }

                fprintf(fp,
                        "%s,\"%s\",\"%s\"%s\n",
                        time, (char *)mon_data->context, core_list, data);
        }
}

/**
 * @brief Builds monitoring header string
 *
 * @param hdr place to store monitoring header row
 * @param sz_hdr available memory size for the header
 * @param isxml true if XML output selected
 * @param istext true is TEXT output selected
 * @param iscsv true is CSV output selected
 * @param format llc_format representation mode (kilobytes or percent)
 */
static void
build_header_row(char *hdr, const size_t sz_hdr,
                 const int isxml,
                 const int istext,
                 const int iscsv,
                 const enum llc_format format)
{
        ASSERT(hdr != NULL && sz_hdr > 0);
        memset(hdr, 0, sz_hdr);

        if (isxml)
                return;

        if (istext) {
                if (!process_mode()) {
                        strncpy(hdr, "    CORE", sz_hdr - 1);
#ifdef PQOS_RMID_CUSTOM
                        if (sel_interface == PQOS_INTER_MSR)
                                strncat(hdr, " RMID",
                                        sz_hdr - strlen(hdr) - 1);
#endif
                } else
                        strncpy(hdr, "     PID     CORE", sz_hdr - 1);

                if (sel_events_max & PQOS_PERF_EVENT_IPC)
                        strncat(hdr, "         IPC", sz_hdr - strlen(hdr) - 1);
                if (sel_events_max & PQOS_PERF_EVENT_LLC_MISS)
                        strncat(hdr, "      MISSES", sz_hdr - strlen(hdr) - 1);
                if (sel_events_max & PQOS_MON_EVENT_L3_OCCUP) {
                        if (format == LLC_FORMAT_KILOBYTES)
                                strncat(hdr, "     LLC[KB]",
                                        sz_hdr - strlen(hdr) - 1);
                        else
                                strncat(hdr, "      LLC[%]",
                                        sz_hdr - strlen(hdr) - 1);
                }
                if (sel_events_max & PQOS_MON_EVENT_LMEM_BW)
                        strncat(hdr, "   MBL[MB/s]", sz_hdr - strlen(hdr) - 1);
                if (sel_events_max & PQOS_MON_EVENT_RMEM_BW)
                        strncat(hdr, "   MBR[MB/s]", sz_hdr - strlen(hdr) - 1);
                if (sel_events_max & PQOS_MON_EVENT_TMEM_BW)
                        strncat(hdr, "   MBT[MB/s]", sz_hdr - strlen(hdr) - 1);
        }

        if (iscsv) {
                if (!process_mode()) {
                        strncpy(hdr, "Time,Core", sz_hdr - 1);
#ifdef PQOS_RMID_CUSTOM
                        if (sel_interface == PQOS_INTER_MSR)
                                strncat(hdr, ",RMID", sz_hdr - strlen(hdr) - 1);
#endif
                } else
                        strncpy(hdr, "Time,PID,Core", sz_hdr - 1);

                if (sel_events_max & PQOS_PERF_EVENT_IPC)
                        strncat(hdr, ",IPC", sz_hdr - strlen(hdr) - 1);
                if (sel_events_max & PQOS_PERF_EVENT_LLC_MISS)
                        strncat(hdr, ",LLC Misses", sz_hdr - strlen(hdr) - 1);
                if (sel_events_max & PQOS_MON_EVENT_L3_OCCUP) {
                        if (format == LLC_FORMAT_KILOBYTES)
                                strncat(hdr, ",LLC[KB]",
                                        sz_hdr - strlen(hdr) - 1);
                        else
                                strncat(hdr, ",LLC[%]",
                                        sz_hdr - strlen(hdr) - 1);
                }
                if (sel_events_max & PQOS_MON_EVENT_LMEM_BW)
                        strncat(hdr, ",MBL[MB/s]", sz_hdr - strlen(hdr) - 1);
                if (sel_events_max & PQOS_MON_EVENT_RMEM_BW)
                        strncat(hdr, ",MBR[MB/s]", sz_hdr - strlen(hdr) - 1);
                if (sel_events_max & PQOS_MON_EVENT_TMEM_BW)
                        strncat(hdr, ",MBT[MB/s]", sz_hdr - strlen(hdr) - 1);
        }
}

/**
 * @brief Initializes two arrays with pointers to PQoS monitoring structures
 *
 * Function does the following things:
 * - figures size of the array to allocate
 * - allocates memory for the two arrays
 * - initializes allocated arrays with data from core or pid table
 * - saves array pointers in \a parray1 and \a parray2
 * - both arrays have identical content
 *
 * @param parray1 pointer to an array of pointers to PQoS monitoring structures
 * @param parray2 pointer to an array of pointers to PQoS monitoring structures
 *
 * @return Number of elements in each of the tables
 */
static unsigned
get_mon_arrays(struct pqos_mon_data ***parray1,
               struct pqos_mon_data ***parray2)
{
        unsigned mon_number, i;
        struct pqos_mon_data **p1, **p2;

        ASSERT(parray1 != NULL && parray2 != NULL);
        if (!process_mode())
                mon_number = (unsigned) sel_monitor_num;
        else
                mon_number = (unsigned) sel_process_num;
        p1 = malloc(sizeof(p1[0]) * mon_number);
        p2 = malloc(sizeof(p2[0]) * mon_number);
        if (p1 == NULL || p2 == NULL) {
                if (p1)
                        free(p1);
                if (p2)
                        free(p2);
                printf("Error with memory allocation");
                exit(EXIT_FAILURE);
        }

        for (i = 0; i < mon_number; i++) {
                if (!process_mode())
                        p1[i] = sel_monitor_core_tab[i].pgrp;
                else
                        p1[i] = sel_monitor_pid_tab[i].pgrp;
                p2[i] = p1[i];
        }

        *parray1 = p1;
        *parray2 = p2;
        return mon_number;
}

/**
 * @brief Converts microseconds into timeval structure
 *
 * @param tv pointer to timeval structure to be filled in
 * @param usec microseconds to be used to fill in \a tv
 */
static void usec_to_timeval(struct timeval *tv, const long usec)
{
        tv->tv_sec = usec / 1000000L;
        tv->tv_usec = usec % 1000000L;
}

/**
 * @brief Converts timeval structure into microseconds
 *
 * @param tv pointer to timeval structure to be converted
 *
 * @return Number of microseconds corresponding to \a tv
 */
static long timeval_to_usec(const struct timeval *tv)
{
        return ((long)tv->tv_usec) + ((long)tv->tv_sec * 1000000L);
}

/**
 * @brief Gets total l3 cache value
 *
 * @param[in] p_cache_size pointer to cache-size value to be filled.
 *            It must not be NULL.
 *
 * @return PQOS_RETVAL_OK on success or error code in case of failure
 */
static int get_cache_size(unsigned *p_cache_size)
{
        const struct pqos_cpuinfo *p_cpu = NULL;
        int ret;

        if (p_cache_size == NULL)
                return PQOS_RETVAL_PARAM;

        ret = pqos_cap_get(NULL, &p_cpu);
        if (ret != PQOS_RETVAL_OK) {
                printf("Error retrieving PQoS capabilities!\n");
                return ret;
        }

        if (p_cpu == NULL)
                return PQOS_RETVAL_ERROR;

        *p_cache_size = p_cpu->l3.total_size;

        return PQOS_RETVAL_OK;
}

/**
 * @brief Fills llc_entry_data structure with percentage or kilobytes
 *        cache usage representation
 *
 * @param[in] llc_entry pointer to llc_entry_data structure to be filled.
 *            It must not be NULL
 * @param llc_bytes amount of cache(in bytes) used by core
 * @param cache_total total l3 cache size
 * @param format enum representing percentage or kilobyte format used
 *        for showing cache occupancy for core
 *
 * @return PQOS_RETVAL_OK on success or error code otherwise
 */
static int get_llc_entry(struct llc_entry_data *llc_entry,
                         uint64_t llc_bytes,
                         unsigned cache_total,
                         enum llc_format format)
{
        if (llc_entry == NULL)
                return PQOS_RETVAL_PARAM;

        if (cache_total == 0)
                return PQOS_RETVAL_PARAM;

        switch (format) {
        case LLC_FORMAT_KILOBYTES:
                llc_entry->val = bytes_to_kb(llc_bytes);
                break;
        case LLC_FORMAT_PERCENT:
                llc_entry->val = llc_bytes*100/cache_total;
                break;
        default:
                printf("Incorrect llc_format: %i\n", format);
                return PQOS_RETVAL_PARAM;
        }

        llc_entry->format = format;

        return PQOS_RETVAL_OK;
}

void monitor_loop(void)
{
#define TERM_MIN_NUM_LINES 3

        const long interval =
                (long)sel_mon_interval * 100000LL; /* interval in [us] units */
        struct timeval tv_start, tv_s;
        const int istty = isatty(fileno(fp_monitor));
        const int istext = !strcasecmp(sel_output_type, "text");
        const int isxml = !strcasecmp(sel_output_type, "xml");
        const int iscsv = !strcasecmp(sel_output_type, "csv");
        const size_t sz_header = 128;
        unsigned cache_size;
        char header[sz_header];
        unsigned mon_number = 0, display_num = 0;
        struct pqos_mon_data **mon_data = NULL, **mon_grps = NULL;

        if ((!istext)  && (!isxml) && (!iscsv)) {
                printf("Invalid selection of output file type '%s'!\n",
                       sel_output_type);
                return;
        }

        if (get_cache_size(&cache_size) != PQOS_RETVAL_OK) {
                printf("Error during getting L3 cache size\n");
                return;
        }

        mon_number = get_mon_arrays(&mon_grps, &mon_data);
        display_num = mon_number;

        /**
         * Capture ctrl-c to gracefully stop the loop
         */
        if (signal(SIGINT, monitoring_ctrlc) == SIG_ERR)
                printf("Failed to catch SIGINT!\n");
        if (signal(SIGHUP, monitoring_ctrlc) == SIG_ERR)
                printf("Failed to catch SIGHUP!\n");

        if (istty) {
                struct winsize w;
                unsigned max_lines = 0;

                if (ioctl(fileno(fp_monitor), TIOCGWINSZ, &w) != -1) {
                        max_lines = w.ws_row;
                        if (max_lines < TERM_MIN_NUM_LINES)
                                max_lines = TERM_MIN_NUM_LINES;

                }
                if ((display_num + TERM_MIN_NUM_LINES - 1) > max_lines)
                        display_num = max_lines - TERM_MIN_NUM_LINES + 1;
        }

        /**
         * Coefficient to display the data as MB / s
         */
        const double coeff = 10.0 / (double)sel_mon_interval;

        /**
         * Build the header
         */
        build_header_row(header, sz_header, isxml, istext, iscsv,
                         sel_llc_format);

        if (iscsv)
                fprintf(fp_monitor, "%s\n", header);

        gettimeofday(&tv_start, NULL);
        tv_s = tv_start;

        while (!stop_monitoring_loop) {
                struct timeval tv_e;
                struct tm *ptm = NULL;
                unsigned i = 0;
                long usec_start = 0, usec_end = 0, usec_diff = 0;
                char cb_time[64];
                int ret;

                ret = pqos_mon_poll(mon_grps, mon_number);
                if (ret != PQOS_RETVAL_OK) {
                        printf("Failed to poll monitoring data!\n");
                        free(mon_grps);
                        free(mon_data);
                        return;
                }
                memcpy(mon_data, mon_grps, mon_number * sizeof(mon_grps[0]));

                if (sel_mon_top_like)
                        qsort(mon_data, mon_number, sizeof(mon_data[0]),
                              mon_qsort_llc_cmp_desc);
                else if (!process_mode())
                        qsort(mon_data, mon_number, sizeof(mon_data[0]),
                              mon_qsort_coreid_cmp_asc);

                /**
                 * Get time string
                 */
                ptm = localtime(&tv_s.tv_sec);
                if (ptm != NULL)
                        strftime(cb_time, sizeof(cb_time) - 1,
                                 "%Y-%m-%d %H:%M:%S", ptm);
                else
                        strncpy(cb_time, "error", sizeof(cb_time) - 1);

                if (istty && istext)
                        fprintf(fp_monitor,
                                "\033[2J"      /* Clear screen */
                                "\033[0;0H");  /* move to position 0:0 */

                if (istext)
                        fprintf(fp_monitor, "TIME %s\n%s", cb_time, header);

                for (i = 0; i < display_num; i++) {
                        const struct pqos_event_values *pv =
                                 &mon_data[i]->values;

                        struct llc_entry_data llc_entry;
                        int ret = get_llc_entry(&llc_entry, pv->llc,
                                                cache_size,
                                                sel_llc_format);
                        if (ret != PQOS_RETVAL_OK) {
                                printf("Could not fill llc_entry data!\n");
                                stop_monitoring_loop = 1;
                                break;
                        }

                        double mbr = bytes_to_mb(pv->mbm_remote_delta) * coeff;
                        double mbl = bytes_to_mb(pv->mbm_local_delta) * coeff;
                        double mbt = bytes_to_mb(pv->mbm_total_delta) * coeff;

                        if (istext)
                                print_text_row(fp_monitor, mon_data[i],
                                               &llc_entry, mbr, mbl, mbt);
                        if (isxml)
                                print_xml_row(fp_monitor, cb_time, mon_data[i],
                                              &llc_entry, mbr, mbl, mbt);
                        if (iscsv)
                                print_csv_row(fp_monitor, cb_time, mon_data[i],
                                              &llc_entry, mbr, mbl, mbt);
                }
                if (!istty && istext)
                        fputs("\n", fp_monitor);

                fflush(fp_monitor);

                gettimeofday(&tv_e, NULL);

                if (stop_monitoring_loop)
                        break;

                /**
                 * Calculate microseconds to the nearest measurement interval
                 */
                usec_start = timeval_to_usec(&tv_s);
                usec_end = timeval_to_usec(&tv_e);
                usec_diff = usec_end - usec_start;

                if (usec_diff < interval && usec_diff > 0) {
                        struct timespec req, rem;

                        memset(&rem, 0, sizeof(rem));
                        memset(&req, 0, sizeof(req));

                        req.tv_sec = (interval - usec_diff) / 1000000L;
                        req.tv_nsec =
                                ((interval - usec_diff) % 1000000L) * 1000L;
                        if (nanosleep(&req, &rem) == -1) {
                                /**
                                 * nanosleep() interrupted by a signal
                                 */
                                if (stop_monitoring_loop)
                                        break;
                                req = rem;
                                memset(&rem, 0, sizeof(rem));
                                nanosleep(&req, &rem);
                        }
                }

                /* move tv_s to the next interval */
                usec_to_timeval(&tv_s, usec_start + interval);

                if (sel_timeout >= 0) {
                        gettimeofday(&tv_e, NULL);
                        if ((tv_e.tv_sec - tv_start.tv_sec) > sel_timeout)
                                break;
                }
        }
        if (isxml)
                fprintf(fp_monitor, "%s\n", xml_root_close);

        if (istty)
                fputs("\n\n", fp_monitor);

        free(mon_grps);
        free(mon_data);
}

void monitor_cleanup(void)
{
        /**
         * Close file descriptor for monitoring output
         */
        if (fp_monitor != NULL && fp_monitor != stdout)
                fclose(fp_monitor);
        fp_monitor = NULL;

        /**
         * Free allocated memory
         */
        if (sel_output_file != NULL)
                free(sel_output_file);
        sel_output_file = NULL;
        if (sel_output_type != NULL)
                free(sel_output_type);
        sel_output_type = NULL;
}
