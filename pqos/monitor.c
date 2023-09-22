/*
 * BSD LICENSE
 *
 * Copyright(c) 2014-2023 Intel Corporation. All rights reserved.
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
#include "monitor.h"

#include "common.h"
#include "main.h"
#include "monitor_csv.h"
#include "monitor_text.h"
#include "monitor_utils.h"
#include "monitor_xml.h"
#include "pqos.h"
#ifdef PQOS_RMID_CUSTOM
#include "pqos_internal.h"
#endif

#include <ctype.h>  /**< isspace() */
#include <dirent.h> /**< for dir list*/
#include <fcntl.h>
#include <inttypes.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h> /**< terminal ioctl */
#include <sys/stat.h>
#ifdef __linux__
#include <sys/timerfd.h> /**< timerfd_create() */
#endif
#include <sys/types.h> /**< open() */
#include <time.h>      /**< localtime() */
#include <unistd.h>

#define PQOS_MON_EVENT_ALL                                                     \
        ((enum pqos_mon_event) ~(PQOS_MON_EVENT_TMEM_BW |                      \
                                 PQOS_PERF_EVENT_LLC_REF))
#define PQOS_MON_EVENT_UNCORE                                                  \
        ((enum pqos_mon_event)(PQOS_PERF_EVENT_LLC_MISS_PCIE_READ |            \
                               PQOS_PERF_EVENT_LLC_MISS_PCIE_WRITE |           \
                               PQOS_PERF_EVENT_LLC_REF_PCIE_READ |             \
                               PQOS_PERF_EVENT_LLC_REF_PCIE_WRITE))
#define PID_CPU_TIME_DELAY_USEC (1200000) /**< delay for cpu stats */

#define TOP_PROC_MAX (10)  /**< maximum number of top-pids to be handled */
#define NUM_TIDS_MAX (128) /**< maximum number of TIDs */

#define PID_COL_STATUS (3)  /**< col for process status letter */
#define PID_COL_UTIME  (14) /**< col for cpu-user time in /proc/pid/stat */
#define PID_COL_STIME  (15) /**< col for cpu-kernel time in /proc/pid/stat */

#define TIMEOUT_INFINITE ((unsigned)-1)

#define REALLOC_ALLOWED    1
#define REALLOC_DISALLOWED 0
#define CORE_LIST_SIZE     128

/**
 * Local data structures
 *
 */

/**
 * Location of directory with PID's in the system
 */
static const char *proc_pids_dir = "/proc";

/**
 * White-list of process status fields that can go into top-pids list
 */
static const char *proc_stat_whitelist = "RSD";

/** Trigger for disabling ipc monitoring */
static int sel_disable_ipc = 0;
/** Trigger for disabling llc_miss monitoring */
static int sel_disable_llc_miss = 0;

/**
 * The mask to tell which events to display
 */
static enum pqos_mon_event sel_events_max = (enum pqos_mon_event)0;

/**
 * Monitoring group type
 */
enum mon_group_type {
        MON_GROUP_TYPE_CORE = 0x1,
        MON_GROUP_TYPE_PID = 0x2,
        MON_GROUP_TYPE_UNCORE = 0x4,
        MON_GROUP_TYPE_CHANNEL = 0x8,
        MON_GROUP_TYPE_DEVICE = 0x10,
};

union pqos_device {
        uint64_t raw;
        struct __attribute__((__packed__)) {
                uint16_t segment;
                uint16_t bdf;
                uint32_t vc;
        };
};

/**
 * Maintains a table of core, pid, event, number of events that are selected in
 * config string for monitoring
 */
static struct mon_group {
        enum mon_group_type type;
        char *desc;
        enum pqos_mon_event events;
        struct pqos_mon_data *data;
        unsigned started;

        union {
                unsigned *cores;
                pid_t *pids;
                pqos_channel_t *channels;
                union pqos_device *devices;
                unsigned *sockets;
                void *generic_res;
        };
        unsigned num_res;

#ifdef PQOS_RMID_CUSTOM
        struct pqos_mon_options opt;
#endif
} *sel_monitor_group = NULL;

/**
 * Maintains the number of monitoring groups
 */
static unsigned sel_monitor_num = 0;

/**
 * Selected monitoring type
 */
static int sel_monitor_type = 0;

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
static unsigned sel_timeout = TIMEOUT_INFINITE;

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
        pid_t pid;                 /**< process pid */
        unsigned long ticks_delta; /**< current cpu_time - previous ticks */
        double cpu_avg_ratio;      /**< cpu usage/running time ratio*/
        int valid; /**< marks if statistics are fully processed */
};

/**
 * Maintains single linked list implementation
 */
struct slist {
        void *data;         /**< abstract data that is hold by a list element */
        struct slist *next; /**< ptr to next list element */
};

/**
 * Stores display format for LLC (kilobytes/percent)
 */
static enum monitor_llc_format sel_llc_format = LLC_FORMAT_KILOBYTES;

/**
 * @brief Check to determine if processes are monitored
 *
 * @return Process monitoring mode status
 * @retval 0 not monitoring processes
 * @retval 1 monitoring processes
 */
int
monitor_process_mode(void)
{
        return (sel_monitor_type == MON_GROUP_TYPE_PID);
}

int
monitor_core_mode(void)
{
        return (sel_monitor_type == MON_GROUP_TYPE_CORE);
}

int
monitor_iordt_mode(void)
{
        return (sel_monitor_type == MON_GROUP_TYPE_CHANNEL ||
                sel_monitor_type == MON_GROUP_TYPE_DEVICE);
}

int
monitor_uncore_mode(void)
{
        return (sel_monitor_type == MON_GROUP_TYPE_UNCORE);
}

/**
 * @brief Function to safely translate an unsigned int
 *        value to a string
 *
 * @param val value to be translated
 *
 * @return Pointer to allocated string
 */
static char *
uinttostr(const unsigned val)
{
        char buf[16], *str = NULL;

        (void)monitor_utils_uinttostr(buf, sizeof(buf), val);
        selfn_strdup(&str, buf);

        return str;
}

/**
 * @brief Function to safely translate an unsigned int
 *        value to a hex string
 *
 * @param val value to be translated
 *
 * @return Pointer to allocated string
 */
static char *
uinttohexstr(const unsigned val)
{
        char buf[16], *str = NULL;

        (void)monitor_utils_uinttohexstr(buf, sizeof(buf), val);
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
grp_set_core(struct mon_group *cg,
             char *desc,
             const uint64_t *cores,
             const int num_cores)
{
        int i;

        ASSERT(cg != NULL);
        ASSERT(desc != NULL);
        ASSERT(cores != NULL);
        ASSERT(num_cores > 0);

        cg->type = MON_GROUP_TYPE_CORE;
        cg->desc = desc;
        cg->cores = malloc(sizeof(unsigned) * num_cores);
        if (cg->cores == NULL) {
                printf("Error allocating core group table\n");
                return -1;
        }
        cg->num_res = num_cores;

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
grp_set_pid(struct mon_group *pg,
            char *desc,
            const uint64_t *pids,
            const int num_pids)
{
        int i;

        ASSERT(pg != NULL);
        ASSERT(desc != NULL);
        ASSERT(pids != NULL);
        ASSERT(num_pids > 0);

        pg->type = MON_GROUP_TYPE_PID;
        pg->desc = desc;
        pg->pids = malloc(sizeof(*pg->pids) * num_pids);
        if (pg->pids == NULL) {
                printf("Error allocating pid group table\n");
                return -1;
        }
        pg->num_res = num_pids;

        /**
         * Transfer pids from buffer to table
         */
        for (i = 0; i < num_pids; i++)
                pg->pids[i] = (pid_t)pids[i];

        return 0;
}
/**
 * @brief Function to set uncore group values
 *
 * @param pg pointer to pid_group structure
 * @param desc string containing pid group description
 * @param sockets pointer to table of socket values
 * @param num_socktets number of sockets contained in the table
 *
 * @return Operational status
 * @retval 0 on success
 * @retval -1 on error
 */

static int
grp_set_uncore(struct mon_group *group,
               char *desc,
               const uint64_t *sockets,
               const int num_socktets)
{
        int i;

        ASSERT(group != NULL);
        ASSERT(desc != NULL);
        ASSERT(sockets != NULL);
        ASSERT(num_socktets > 0);

        group->type = MON_GROUP_TYPE_UNCORE;
        group->desc = desc;
        group->sockets = malloc(sizeof(*group->sockets) * num_socktets);
        if (group->sockets == NULL) {
                printf("Error allocating uncore group table\n");
                return -1;
        }
        group->num_res = num_socktets;

        /**
         * Transfer pids from buffer to table
         */
        for (i = 0; i < num_socktets; i++)
                group->sockets[i] = (unsigned)sockets[i];

        return 0;
}

/**
 * @brief Function to set channel group values
 *
 * @param chg pointer to channel_group structure
 * @param desc string containing channel group description
 * @param channels pointer to table of channel values
 * @param num_channels number of channels contained in the table
 *
 * @return Operational status
 * @retval 0 on success
 * @retval -1 on error
 */
static int
grp_set_channel(struct mon_group *chp,
                char *desc,
                const uint64_t *channels,
                const int num_channels)
{
        int i;

        ASSERT(chp != NULL);
        ASSERT(desc != NULL);
        ASSERT(channels != NULL);
        ASSERT(num_channels > 0);

        chp->type = MON_GROUP_TYPE_CHANNEL;
        chp->desc = desc;
        chp->channels = malloc(sizeof(pqos_channel_t) * num_channels);
        if (chp->channels == NULL) {
                printf("Error allocating channel group table\n");
                return -1;
        }
        chp->num_res = num_channels;

        /**
         * Transfer channels from buffer to table
         */
        for (i = 0; i < num_channels; i++)
                chp->channels[i] = (pqos_channel_t)channels[i];

        return 0;
}

/**
 * @brief Function to set device group values
 *
 * @param chg pointer to channel_group structure
 * @param desc string containing channel group description
 * @param [in] devices Device Id
 * @param [in] num_devices Number of devices
 *
 * @return Operational status
 * @retval 0 on success
 * @retval -1 on error
 */
static int
grp_set_device(struct mon_group *grp,
               char *desc,
               const uint64_t *devices,
               const int num_devices)
{
        int i;

        ASSERT(grp != NULL);
        ASSERT(desc != NULL);
        ASSERT(devices != NULL);
        ASSERT(num_devices > 0);

        grp->type = MON_GROUP_TYPE_DEVICE;
        grp->desc = desc;
        grp->devices = malloc(sizeof(union pqos_device) * num_devices);
        if (grp->channels == NULL) {
                printf("Error allocating device group table\n");
                return -1;
        }
        grp->num_res = num_devices;

        for (i = 0; i < num_devices; i++)
                grp->devices[i].raw = devices[i];

        return 0;
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
grp_cmp_core(const struct mon_group *cg_a, const struct mon_group *cg_b)
{
        ASSERT(cg_a != NULL);
        ASSERT(cg_b != NULL);

        int i, found = 0;
        const int sz_a = cg_a->num_res;
        const int sz_b = cg_b->num_res;
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
grp_cmp_pid(const struct mon_group *pg_a, const struct mon_group *pg_b)
{
        int i, found = 0;

        ASSERT(pg_a != NULL);
        ASSERT(pg_b != NULL);

        const int sz_a = pg_a->num_res;
        const int sz_b = pg_b->num_res;
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
 * @brief Function to compare sockets in 2 uncore groups
 *
 * This function takes 2 core groups and compares their core values
 *
 * @param cg_a pointer to uncore group a
 * @param cg_b pointer to uncore group b
 *
 * @return Whether both groups contain some/none/all of the same cores
 * @retval 1 if both groups contain the same cores
 * @retval 0 if none of their cores match
 * @retval -1 if some but not all cores match
 */
static int
grp_cmp_uncore(const struct mon_group *cg_a, const struct mon_group *cg_b)
{
        int i, found = 0;

        ASSERT(cg_a != NULL);
        ASSERT(cg_b != NULL);

        const int sz_a = cg_a->num_res;
        const int sz_b = cg_b->num_res;
        const unsigned *tab_a = cg_a->sockets;
        const unsigned *tab_b = cg_b->sockets;

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
 * @brief Function to compare channels in 2 channel groups
 *
 * This function takes 2 channel groups and compares their channel values
 *
 * @param cg_a pointer to channel group a
 * @param cg_b pointer to channel group b
 *
 * @return Whether both groups contain some/none/all of the same channel
 * @retval 1 if both groups contain the same channels
 * @retval 0 if none of their channel match
 * @retval -1 if some but not all channels match
 */
static int
grp_cmp_channel(const struct mon_group *cg_a, const struct mon_group *cg_b)
{
        int i, found = 0;

        ASSERT(cg_a != NULL);
        ASSERT(cg_b != NULL);

        const int sz_a = cg_a->num_res;
        const int sz_b = cg_b->num_res;
        const pqos_channel_t *tab_a = cg_a->channels;
        const pqos_channel_t *tab_b = cg_b->channels;

        for (i = 0; i < sz_a; i++) {
                int j;

                for (j = 0; j < sz_b; j++)
                        if (tab_a[i] == tab_b[j])
                                found++;
        }
        /* if no channels are the same */
        if (!found)
                return 0;
        /* if group contains same channels */
        if (sz_a == sz_b && sz_b == found)
                return 1;
        /* if not all channels are the same */
        return -1;
}

/**
 * @brief Function to compare device in 2 device groups
 *
 * This function takes 2 device groups and compares their device values
 *
 * @param dg_a pointer to device group a
 * @param dg_b pointer to device group b
 *
 * @return Whether both groups contain some/none/all of the same device and vc
 * @retval 1 if both groups contain the same device and vc
 * @retval 0 if none of their device match
 * @retval -1 if some but not all devices' vc match
 */
static int
grp_cmp_device(const struct mon_group *dg_a, const struct mon_group *dg_b)
{
        int i, found = 0;

        ASSERT(dg_a != NULL);
        ASSERT(dg_b != NULL);

        const int sz_a = dg_a->num_res;
        const int sz_b = dg_b->num_res;
        const union pqos_device *tab_a = dg_a->devices;
        const union pqos_device *tab_b = dg_b->devices;

        for (i = 0; i < sz_a; i++) {
                int j;

                for (j = 0; j < sz_b; j++) {
                        if (tab_a[i].segment != tab_b[j].segment)
                                continue;
                        if (tab_a[i].bdf != tab_b[j].bdf)
                                continue;

                        if (tab_a[i].vc == tab_b[j].vc)
                                found++;
                        else if (tab_a[i].vc == DEV_ALL_VCS ||
                                 tab_b[j].vc == DEV_ALL_VCS)
                                return -1;
                }
        }

        /* if no devices are the same */
        if (!found)
                return 0;
        /* if group contains same devices */
        if (sz_a == sz_b && sz_b == found)
                return 1;
        /* if not all devices are the same */
        return -1;
}

/*
 * @brief Function to compare resources in 2 groups
 *
 * @param grp_a pointer to group a
 * @param grp_b pointer to group b
 *
 * @return Whether both groups contain some/none/all of the same resources
 * @retval 1 if both groups contain the same pids
 * @retval 0 if none of their pids match
 * @retval -1 if some but not all pids match
 * @retval -2 error
 */
static int
grp_cmp(const struct mon_group *grp_a, const struct mon_group *grp_b)
{
        if (grp_a->type != grp_b->type)
                return -2;

        switch (grp_a->type) {
        case MON_GROUP_TYPE_CORE:
                return grp_cmp_core(grp_a, grp_b);
                break;
        case MON_GROUP_TYPE_PID:
                return grp_cmp_pid(grp_a, grp_b);
                break;
        case MON_GROUP_TYPE_UNCORE:
                return grp_cmp_uncore(grp_a, grp_b);
                break;
        case MON_GROUP_TYPE_CHANNEL:
                return grp_cmp_channel(grp_a, grp_b);
                break;
        case MON_GROUP_TYPE_DEVICE:
                return grp_cmp_device(grp_a, grp_b);
                break;
        }

        return -2;
}

/**
 * @brief Deallocates group memory
 *
 * @param grp monitoring group;
 */
static void
grp_free(struct mon_group *grp)
{
        free(grp->desc);
#ifndef __clang_analyzer__
        free(grp->generic_res);
#else
        if (grp->type == MON_GROUP_TYPE_PID)
                free(grp->pids);
        else if (grp->type == MON_GROUP_TYPE_CORE)
                free(grp->cores);
        else if (grp->type == MON_GROUP_TYPE_CHANNEL)
                free(grp->channels);
        else if (grp->type == MON_GROUP_TYPE_DEVICE)
                free(grp->devices);
        else if (grp->type == MON_GROUP_TYPE_UNCORE)
                free(grp->sockets);
#endif
}

/**
 * @brief Adds monitoring group
 *
 * @param type monitoring group type
 * @param event monitoring event
 * @param desc string containing group description
 * @param res pointer to table of resource values
 * @param num_res number of resources contained in the table
 *
 * @return Pointer to added group
 */
static struct mon_group *
grp_add(enum mon_group_type type,
        enum pqos_mon_event event,
        char *desc,
        const uint64_t *res,
        const int num_res)
{
        int ret = 0;
        int dup = 0;
        unsigned i;
        struct mon_group new_grp;

        memset(&new_grp, 0, sizeof(new_grp));

        switch (type) {
        case MON_GROUP_TYPE_CORE:
                sel_monitor_type |= MON_GROUP_TYPE_CORE;
                ret = grp_set_core(&new_grp, desc, res, num_res);
                break;
        case MON_GROUP_TYPE_PID:
                sel_monitor_type |= MON_GROUP_TYPE_PID;
                ret = grp_set_pid(&new_grp, desc, res, num_res);
                break;
        case MON_GROUP_TYPE_CHANNEL:
                sel_monitor_type |= MON_GROUP_TYPE_CHANNEL;
                ret = grp_set_channel(&new_grp, desc, res, num_res);
                break;
        case MON_GROUP_TYPE_DEVICE:
                sel_monitor_type |= MON_GROUP_TYPE_CHANNEL;
                ret = grp_set_device(&new_grp, desc, res, num_res);
                break;
        case MON_GROUP_TYPE_UNCORE:
                sel_monitor_type |= MON_GROUP_TYPE_UNCORE;
                ret = grp_set_uncore(&new_grp, desc, res, num_res);
                break;
        default:
                return NULL;
        }
        if (ret < 0) {
                grp_free(&new_grp);
                return NULL;
        }
        new_grp.events = event;

        /**
         *  For each core group we are processing:
         *  - if it's already in the sel_monitor_group
         *    =>  update the entry
         *  - else
         *    => add it to the sel_monitor_group
         */
        for (i = 0; i < sel_monitor_num; i++) {
                struct mon_group *grp = &sel_monitor_group[i];

                if (grp->type != type)
                        continue;
                dup = grp_cmp(&new_grp, grp);
                if (dup == 1) {
                        grp->events |= event;
                        grp_free(&new_grp);
                        return grp;
                } else if (dup < 0) {
                        switch (type) {
                        case MON_GROUP_TYPE_CORE:
                                fprintf(stderr, "Error: cannot monitor same "
                                                "cores in different groups\n");
                                break;
                        case MON_GROUP_TYPE_PID:
                                fprintf(stderr, "Error: cannot monitor same "
                                                "pids in different groups\n");
                                break;
                        case MON_GROUP_TYPE_CHANNEL:
                                fprintf(stderr,
                                        "Error: cannot monitor same "
                                        "channels in different groups\n");
                                break;
                        case MON_GROUP_TYPE_DEVICE:
                                fprintf(stderr,
                                        "Error: cannot monitor same "
                                        "devices in different groups\n");
                                break;
                        case MON_GROUP_TYPE_UNCORE:
                                fprintf(stderr,
                                        "Error: cannot monitor same "
                                        "sockets in different groups\n");
                                break;
                        }
                        grp_free(&new_grp);
                        return NULL;
                }
        }

        /* allocate memory for monitoring context */
        {
                struct mon_group *grps =
                    realloc(sel_monitor_group,
                            sizeof(*sel_monitor_group) * (sel_monitor_num + 1));

                if (grps == NULL) {
                        fprintf(stderr, "Error with memory allocation\n");
                        grp_free(&new_grp);
                        return NULL;
                } else
                        sel_monitor_group = grps;
        }

        sel_monitor_group[sel_monitor_num++] = new_grp;

        return &sel_monitor_group[sel_monitor_num - 1];
}

/**
 * @brief Converts device monitoring group to channel
 *
 * @param type monitoring group type
 * @param event monitoring event
 * @param desc string containing group description
 * @param res pointer to table of resource values
 * @param num_res number of resources contained in the table
 *
 * @return Operational status
 * @retval 0 on success
 * @retval -1 on error
 */
static int
grp_device_to_channel(struct mon_group *grp, const struct pqos_devinfo *devinfo)
{
        union pqos_device *devices = grp->devices;
        pqos_channel_t *channels = NULL;
        unsigned num_channels;

        /* currently parser supports single device */
        if (grp->num_res != 1)
                return PQOS_RETVAL_PARAM;

        if (devices->vc != DEV_ALL_VCS) {
                channels = calloc(1, sizeof(*channels));
                if (!channels) {
                        printf("Error allocating memory\n");
                        exit(EXIT_FAILURE);
                }

                num_channels = 1;
                channels[0] = pqos_devinfo_get_channel_id(
                    devinfo, devices->segment, devices->bdf, devices->vc);
                if (channels[0] == 0) {
                        free(channels);
                        channels = NULL;
                }

        } else
                channels = pqos_devinfo_get_channel_ids(
                    devinfo, devices->segment, devices->bdf, &num_channels);

        if (channels == NULL) {
                printf("Failed to get channels for %s\n", grp->desc);
                return PQOS_RETVAL_PARAM;
        }

        grp->type = MON_GROUP_TYPE_CHANNEL;
        grp->channels = channels;
        grp->num_res = num_channels;

        return PQOS_RETVAL_OK;
}

/**
 * @brief Common function to parse selected events
 *
 * @param str string of the event
 * @param evt pointer to the selected events so far
 */
static void
parse_event(const char *str, enum pqos_mon_event *evt)
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
                *evt = (enum pqos_mon_event)PQOS_MON_EVENT_ALL;
        else if (strncasecmp(str, "llc_ref:", 8) == 0)
                *evt = PQOS_PERF_EVENT_LLC_REF;
        else
                parse_error(str, "Unrecognized monitoring event type");
}

#define PARSE_MON_GRP_BUFF_SIZE 1250

/**
 * @brief Function to set the descriptions and cores/pids for each monitoring
 * group
 *
 * @param str string passed to -m command line option
 * @param type monitoring group type
 *
 * @retval -1 on error
 */
static int
parse_monitor_group(char *str, enum mon_group_type type)
{
        enum pqos_mon_event evt = (enum pqos_mon_event)0;
        unsigned group_count = 0;
        unsigned i;
        uint64_t cbuf[PARSE_MON_GRP_BUFF_SIZE];
        char *non_grp = NULL;

        parse_event(str, &evt);
        str = strchr(str, ':') + 1;

        while ((non_grp = strsep(&str, "[")) != NULL) {
                /**
                 * Ungrouped cores/pids
                 */
                if (*non_grp != '\0') {
                        /* for separate cores/pids - each will get his own
                         * group so strlisttotab result is treated as the
                         * number of new groups
                         */
                        unsigned new_groups_count =
                            strlisttotab(non_grp, cbuf, DIM(cbuf));

                        /* set group info */
                        for (i = 0; i < new_groups_count; i++) {
                                char *desc;
                                const struct mon_group *grp;

                                if (type != MON_GROUP_TYPE_CHANNEL)
                                        desc = uinttostr((unsigned)cbuf[i]);
                                else
                                        desc = uinttohexstr((unsigned)cbuf[i]);

                                grp = grp_add(type, evt, desc, &cbuf[i], 1);
                                if (grp == NULL) {
                                        free(desc);
                                        return -1;
                                }

                                group_count++;
                        }
                }
                /**
                 * If group contains multiple cores/pids
                 */
                char *grp = strsep(&str, "]");

                if (grp != NULL) {
                        char *desc = NULL;
                        unsigned element_count;
                        const struct mon_group *group;

                        selfn_strdup(&desc, grp);

                        /* for grouped pids/cores, all elements are in
                         * one group so strlisttotab result is the number
                         * of elements in that one group
                         */
                        element_count = strlisttotab(grp, cbuf, DIM(cbuf));

                        /* set group info */
                        group = grp_add(type, evt, desc, cbuf, element_count);
                        if (group == NULL)
                                return -1;
                        group_count++;
                }
        }

        return group_count;
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
        int ret = parse_monitor_group(str, MON_GROUP_TYPE_CORE);

        if (ret < 0)
                exit(EXIT_FAILURE);
}

void
selfn_monitor_file_type(const char *arg)
{
        selfn_strdup(&sel_output_type, arg);
}

void
selfn_monitor_file(const char *arg)
{
        selfn_strdup(&sel_output_file, arg);
}

void
selfn_monitor_set_llc_percent(void)
{
        sel_llc_format = LLC_FORMAT_PERCENT;
}

void
selfn_monitor_disable_ipc(const char *arg)
{
        UNUSED_ARG(arg);
        sel_disable_ipc = 1;
}

void
selfn_monitor_disable_llc_miss(const char *arg)
{
        UNUSED_ARG(arg);
        sel_disable_llc_miss = 1;
}

void
selfn_monitor_cores(const char *arg)
{
        char *cp = NULL, *str = NULL;
        char *saveptr = NULL;

        if (arg == NULL)
                parse_error(arg, "NULL pointer!");

        if (*arg == '\0')
                parse_error(arg, "Empty string!");

        selfn_strdup(&cp, arg);

        for (str = cp;; str = NULL) {
                char *token = NULL;

                token = strtok_r(str, ";", &saveptr);
                if (token == NULL)
                        break;
                parse_monitor_cores(token);
        }

        free(cp);
}

#ifdef PQOS_RMID_CUSTOM
#define DEFAULT_TABLE_SIZE 128

void
selfn_monitor_rmid_cores(const char *arg)
{
        char *cp = NULL, *str = NULL;
        char *saveptr = NULL;
        uint64_t *cores = NULL;

        if (arg == NULL)
                parse_error(arg, "NULL pointer!");

        if (*arg == '\0')
                parse_error(arg, "Empty string!");

        selfn_strdup(&cp, arg);

        for (str = cp;; str = NULL) {
                char *token = NULL;
                char *p = NULL;
                pqos_rmid_t rmid;
                unsigned count;
                unsigned core_list_size = DEFAULT_TABLE_SIZE;
                char *desc = NULL;
                struct mon_group *grp;

                token = strtok_r(str, ";", &saveptr);
                if (token == NULL)
                        break;
                if (cores == NULL) {
                        cores = calloc(core_list_size, sizeof(*cores));
                        if (cores == NULL) {
                                printf("Error with memory allocation!\n");
                                goto error_exit;
                        }
                }
                p = strchr(token, '=');
                if (p == NULL)
                        parse_error(str, "Invalid RMID association format");
                *p = '\0';

                rmid = (pqos_rmid_t)strtouint64(token);
                selfn_strdup(&desc, p + 1);
                count = strlisttotabrealloc(p + 1, &cores, &core_list_size);

                grp = grp_add(MON_GROUP_TYPE_CORE, 0, desc, cores, count);
                if (grp == NULL)
                        goto error_exit;

                grp->opt.rmid.type = PQOS_RMID_TYPE_MAP;
                grp->opt.rmid.rmid = rmid;

                free(cores);
                cores = NULL;
        }
        free(cp);
        return;
error_exit:
        if (cores != NULL)
                free(cores);
        free(cp);
        exit(EXIT_FAILURE);
}
#endif /* PQOS_RMID_CUSTOM */

void
selfn_monitor_uncore(const char *arg)
{
        char *cp = NULL, *str = NULL;
        char *saveptr = NULL;

        if (arg == NULL) {
                sel_monitor_type |= MON_GROUP_TYPE_UNCORE;
                return;
        }

        if (*arg == '\0')
                parse_error(arg, "Empty string!");

        selfn_strdup(&cp, arg);

        for (str = cp;; str = NULL) {
                char *token = NULL;
                int ret;

                token = strtok_r(str, ";", &saveptr);
                if (token == NULL)
                        break;

                ret = parse_monitor_group(token, MON_GROUP_TYPE_UNCORE);
                if (ret < 0)
                        exit(EXIT_FAILURE);
        }

        free(cp);
}

/**
 * Update list of events to be monitored
 *
 * @param [in] type monitoring group type
 * @param [in,out] events List of monitoring events
 * @param [in] cap_mon monitoring capability
 * @param [in] iordt I/O rdt support required
 */
static void
monitor_setup_events(enum mon_group_type type,
                     enum pqos_mon_event *events,
                     const struct pqos_capability *const cap_mon,
                     int iordt)
{
        unsigned i;
        enum pqos_mon_event all_evts = (enum pqos_mon_event)0;

        /**
         * get all available events on this platform
         */
        for (i = 0; i < cap_mon->u.mon->num_events; i++) {
                struct pqos_monitor *mon = &cap_mon->u.mon->events[i];

                if (iordt && !mon->iordt)
                        continue;

                all_evts |= mon->type;
        }
        if (type == MON_GROUP_TYPE_UNCORE)
                all_evts &= PQOS_MON_EVENT_UNCORE;
        else
                all_evts &= ~PQOS_MON_EVENT_UNCORE;

        /* Disable IPC monitoring */
        if (sel_disable_ipc)
                all_evts &= (enum pqos_mon_event)(~PQOS_PERF_EVENT_IPC);
        /* Disable LLC miss monitoring */
        if (sel_disable_llc_miss)
                all_evts &= (enum pqos_mon_event)(~PQOS_PERF_EVENT_LLC_MISS);

        /* check if all available events were selected */
        if ((*events & PQOS_MON_EVENT_ALL) == PQOS_MON_EVENT_ALL) {
                *events = (enum pqos_mon_event)(all_evts & *events);

        } else {
                /* Start IPC and LLC miss monitoring if available */
                if (all_evts & PQOS_PERF_EVENT_IPC)
                        *events |= (enum pqos_mon_event)PQOS_PERF_EVENT_IPC;
                if (all_evts & PQOS_PERF_EVENT_LLC_MISS)
                        *events |=
                            (enum pqos_mon_event)PQOS_PERF_EVENT_LLC_MISS;
        }
        sel_events_max |= *events;
}

int
monitor_setup(const struct pqos_cpuinfo *cpu_info,
              const struct pqos_capability *const cap_mon,
              const struct pqos_devinfo *dev_info)
{
        unsigned i;
        int ret = PQOS_RETVAL_OK;

        ASSERT(cpu_info != NULL);
        ASSERT(cap_mon != NULL);

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

        /**
         * If no monitoring mode selected through command line
         * by default let's monitor all cores
         */
        if (sel_monitor_num == 0 && sel_monitor_type == 0) {
                for (i = 0; i < cpu_info->num_cores; i++) {
                        unsigned lcore = cpu_info->cores[i].lcore;
                        uint64_t core = (uint64_t)lcore;
                        const struct mon_group *grp;

                        grp = grp_add(MON_GROUP_TYPE_CORE,
                                      (enum pqos_mon_event)PQOS_MON_EVENT_ALL,
                                      uinttostr(lcore), &core, 1);
                        if (grp == NULL) {
                                printf("Core group setup error!\n");
                                exit(EXIT_FAILURE);
                        }
                }

        } else if (sel_monitor_num == 0 &&
                   sel_monitor_type == MON_GROUP_TYPE_UNCORE) {
                for (i = 0; i < cpu_info->num_cores; i++) {
                        uint64_t socket = (uint64_t)cpu_info->cores[i].socket;
                        const struct mon_group *grp;

                        grp = grp_add(MON_GROUP_TYPE_UNCORE,
                                      (enum pqos_mon_event)PQOS_MON_EVENT_ALL,
                                      uinttostr(socket), &socket, 1);
                        if (grp == NULL) {
                                printf("Uncore group setup error!\n");
                                exit(EXIT_FAILURE);
                        }
                }
        }
        if (sel_monitor_type != MON_GROUP_TYPE_CORE &&
            sel_monitor_type != MON_GROUP_TYPE_PID &&
            sel_monitor_type != MON_GROUP_TYPE_CHANNEL &&
            sel_monitor_type != MON_GROUP_TYPE_DEVICE &&
            sel_monitor_type != MON_GROUP_TYPE_UNCORE) {
                printf("Monitoring start error, process, core, channel/device"
                       " tracking can not be done simultaneously\n");
                return -1;
        }

        for (i = 0; i < sel_monitor_num; i++) {
                struct mon_group *grp = &sel_monitor_group[i];

                /* Convert device group to channel group */
                if (grp->type == MON_GROUP_TYPE_DEVICE) {
                        ret = grp_device_to_channel(grp, dev_info);
                        if (ret != PQOS_RETVAL_OK)
                                break;
                }

                monitor_setup_events(grp->type, &grp->events, cap_mon,
                                     monitor_iordt_mode());

                if (grp->type == MON_GROUP_TYPE_CORE) {
                        /**
                         * Make calls to pqos_mon_start - track cores
                         */
#ifdef PQOS_RMID_CUSTOM
                        ret = pqos_mon_start_cores_ext(
                            grp->num_res, grp->cores, grp->events,
                            (void *)grp->desc, &grp->data, &grp->opt);
#else
                        ret = pqos_mon_start_cores(
                            grp->num_res, grp->cores, grp->events,
                            (void *)grp->desc, &grp->data);
#endif

                        if (ret == PQOS_RETVAL_PERF_CTR)
                                printf("Use -r option to start monitoring "
                                       "anyway.\n");
                        /**
                         * The error raised also if two instances of PQoS
                         * attempt to use the same core id.
                         */
                        if (ret != PQOS_RETVAL_OK) {
                                printf("Monitoring start error on core(s) "
                                       "%s, status %d\n",
                                       grp->desc, ret);
                                break;
                        } else
                                grp->started = 1;

                } else if (grp->type == MON_GROUP_TYPE_PID) {
                        /**
                         * Make calls to pqos_mon_start_pid - track PIDs
                         */
                        ret = pqos_mon_start_pids2(
                            grp->num_res, grp->pids, grp->events,
                            (void *)grp->desc, &grp->data);
                        /**
                         * Any problem with monitoring the process?
                         */
                        if (ret != PQOS_RETVAL_OK) {
                                printf("PID %s monitoring start error,"
                                       "status %d\n",
                                       grp->desc, ret);
                                break;
                        } else
                                grp->started = 1;

                } else if (grp->type == MON_GROUP_TYPE_CHANNEL) {
                        /*
                         * Make calls to pqos_mon_start_channels
                         *  - track channels
                         */
#ifdef PQOS_RMID_CUSTOM
                        ret = pqos_mon_start_channels_ext(
                            grp->num_res, grp->channels, grp->events,
                            (void *)grp->desc, &grp->data, &grp->opt);
#else
                        ret = pqos_mon_start_channels(
                            grp->num_res, grp->channels, grp->events,
                            (void *)grp->desc, &grp->data);
#endif

                        /*
                         * Any problem with monitoring channels?
                         */
                        if (ret != PQOS_RETVAL_OK) {
                                printf("Channel %s monitoring start error,"
                                       "status %d\n",
                                       grp->desc, ret);
                                break;
                        } else
                                grp->started = 1;

                } else if (grp->type == MON_GROUP_TYPE_UNCORE) {
                        ret = pqos_mon_start_uncore(
                            grp->num_res, grp->sockets, grp->events,
                            (void *)grp->desc, &grp->data);
                        if (ret != PQOS_RETVAL_OK) {
                                printf("Uncore monitoring start error on socket"
                                       " %s, status %d\n",
                                       grp->desc, ret);
                                break;
                        } else
                                grp->started = 1;
                }
        }
        if (ret != PQOS_RETVAL_OK) {
                /**
                 * Stop mon groups that are already started
                 */
                for (i = 0; i < sel_monitor_num; i++) {
                        struct mon_group *grp = &sel_monitor_group[i];

                        if (!grp->started)
                                continue;
                        pqos_mon_stop(grp->data);
                }

                return -1;
        }
        return 0;
}

void
monitor_stop(void)
{
        unsigned i;

        for (i = 0; i < sel_monitor_num; i++) {
                struct mon_group *grp = &sel_monitor_group[i];

                int ret = pqos_mon_stop(grp->data);

                if (ret != PQOS_RETVAL_OK)
                        printf("Monitoring stop error!\n");

                /* coverity[double_free] */
                grp_free(grp);
        }

        free(sel_monitor_group);
}

void
selfn_monitor_time(const char *arg)
{
        if (arg == NULL)
                parse_error(arg, "NULL monitor time argument!");

        if (!strcasecmp(arg, "inf") || !strcasecmp(arg, "infinite"))
                sel_timeout = TIMEOUT_INFINITE;
        else
                sel_timeout = (int)strtouint64(arg);
}

void
selfn_monitor_interval(const char *arg)
{
        sel_mon_interval = (int)strtouint64(arg);
        if (sel_mon_interval < 1)
                parse_error(arg, "Invalid interval value!\n");
}

void
selfn_monitor_top_like(const char *arg)
{
        UNUSED_ARG(arg);
        sel_mon_top_like = 1;
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
        int ret = parse_monitor_group(str, MON_GROUP_TYPE_PID);

        if (ret > 0)
                return;
        if (ret == 0)
                parse_error(str, "No process id selected for monitoring");

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

        for (str = cp;; str = NULL) {
                char *token = NULL;

                token = strtok_r(str, ";", &saveptr);
                if (token == NULL)
                        break;

                parse_monitor_pids(token);
        }

        free(cp);
}

/**
 * @brief Verifies and translates monitoring config string into
 *        internal channel monitoring configuration.
 *
 * @param str single channel string passed to --mon-channel command line option
 */
static void
parse_monitor_channel(char *str)
{
        int ret = parse_monitor_group(str, MON_GROUP_TYPE_CHANNEL);

        if (ret > 0)
                return;
        if (ret == 0)
                parse_error(str, "No channel id selected for monitoring");

        exit(EXIT_FAILURE);

        return;
}

void
selfn_monitor_channels(const char *arg)
{
        char *cp = NULL, *str = NULL;
        char *saveptr = NULL;

        if (arg == NULL)
                parse_error(arg, "NULL pointer!");

        if (*arg == '\0')
                parse_error(arg, "Empty string!");

        selfn_strdup(&cp, arg);

        for (str = cp;; str = NULL) {
                char *token = NULL;

                token = strtok_r(str, ";", &saveptr);
                if (token == NULL)
                        break;
                parse_monitor_channel(token);
        }

        free(cp);
}

#ifdef PQOS_RMID_CUSTOM
#define DEFAULT_TABLE_SIZE 128

void
selfn_monitor_rmid_channels(const char *arg)
{
        char *cp = NULL;
        char *str = NULL;
        char *saveptr = NULL;
        uint64_t *channels = NULL;

        if (arg == NULL)
                parse_error(arg, "NULL pointer!");

        if (*arg == '\0')
                parse_error(arg, "Empty string!");

        selfn_strdup(&cp, arg);

        for (str = cp;; str = NULL) {
                char *token = NULL;
                char *p = NULL;
                pqos_rmid_t rmid;
                unsigned count;
                unsigned channel_list_size = DEFAULT_TABLE_SIZE;
                char *desc = NULL;
                struct mon_group *grp;

                token = strtok_r(str, ";", &saveptr);
                if (token == NULL)
                        break;
                if (channels == NULL) {
                        channels = calloc(channel_list_size, sizeof(*channels));
                        if (channels == NULL) {
                                printf("Error with memory allocation!\n");
                                goto error_exit;
                        }
                }
                p = strchr(token, '=');
                if (p == NULL)
                        parse_error(str, "Invalid RMID association format");
                *p = '\0';

                rmid = (pqos_rmid_t)strtouint64(token);

                selfn_strdup(&desc, p + 1);
                count =
                    strlisttotabrealloc(p + 1, &channels, &channel_list_size);
                grp = grp_add(MON_GROUP_TYPE_CHANNEL, 0, desc, channels, count);
                if (grp == NULL) {
                        free(desc);
                        goto error_exit;
                }

                grp->opt.rmid.type = PQOS_RMID_TYPE_MAP;
                grp->opt.rmid.rmid = rmid;

                free(channels);
                channels = NULL;
        }
        free(cp);
        return;
error_exit:
        if (channels != NULL)
                free(channels);
        free(cp);
        exit(EXIT_FAILURE);
}
#endif

/**
 * @brief Verifies and translates monitoring config string into
 *        internal device monitoring configuration.
 *
 * @param str single dev string passed to --mon-dev command line option
 */
static void
parse_monitor_dev(char *str)
{
        uint16_t segment = 0;
        uint16_t bus, device, function;
        uint16_t bdf = 0;
        unsigned vc = DEV_ALL_VCS; /* All channels by default */
        enum pqos_mon_event evt = (enum pqos_mon_event)0;
        char *desc = NULL;
        char *p;

        parse_event(str, &evt);

        p = strchr(str, ':');
        if (p == NULL)
                parse_error(str, "Invalid device format");
        str = p + 1;
        selfn_strdup(&desc, str);
        p = str;

        /* Rough PCI ID validation */
        size_t colon_count = 0;
        size_t point_count = 0;

        while (*p) {
                if (*p == ':')
                        ++colon_count;
                if (*p == '.')
                        ++point_count;
                ++p;
        }

        if (!colon_count || (colon_count > 2) || (point_count != 1))
                parse_error(str, "Invalid PCI ID format.");

        /* PCI segment */
        if (colon_count > 1) {
                p = strchr(str, ':');
                if (p == NULL)
                        parse_error(str, "Invalid PCI ID format.");
                *p = '\0';
                segment = (uint16_t)strhextouint64(str);
                str = p + 1;
        }

        /* PCI bus */
        p = strchr(str, ':');
        if (p == NULL)
                parse_error(str, "Invalid PCI ID format.");
        *p = '\0';
        bus = (uint16_t)strhextouint64(str);
        str = p + 1;

        /* PCI device */
        p = strchr(str, '.');
        if (p == NULL)
                parse_error(str, "Invalid PCI ID format.");
        *p = '\0';
        device = (uint16_t)strhextouint64(str);
        str = p + 1;

        /* PCI virtual channel */
        p = strchr(str, '@');
        if (p) {
                *p = '\0';
                vc = (unsigned)strtouint64(p + 1);
        }

        /* PCI function */
        function = (uint16_t)strhextouint64(str);

        /* PCI BDF */
        bdf |= bus << 8;
        bdf |= (device & 0x1F) << 3;
        bdf |= function & 0x7;

        printf("Setting up monitoring for dev %.4x:%.4x:%.2x.%x@", segment,
               BDF_BUS(bdf), BDF_DEV(bdf), BDF_FUNC(bdf));
        if (vc != DEV_ALL_VCS)
                printf("%u", vc);
        else
                printf("ALL");
        printf("\n");

        union pqos_device dev;

        dev.segment = segment;
        dev.bdf = bdf;
        dev.vc = vc;

        {
                const struct mon_group *grp;

                grp = grp_add(MON_GROUP_TYPE_DEVICE, evt, desc, &dev.raw, 1);
                if (grp == NULL) {
                        printf("Device group setup error!\n");
                        exit(EXIT_FAILURE);
                }
        }
}

void
selfn_monitor_devs(const char *arg)
{
        char *cp = NULL, *str = NULL;
        char *saveptr = NULL;

        if (arg == NULL)
                parse_error(arg, "NULL pointer!");

        if (*arg == '\0')
                parse_error(arg, "Empty string!");

        selfn_strdup(&cp, arg);

        for (str = cp;; str = NULL) {
                char *token = NULL;

                token = strtok_r(str, ";", &saveptr);
                if (token == NULL)
                        break;
                parse_monitor_dev(token);
        }

        free(cp);
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
                unsigned long long time;
                uint64_t time_int = 0;
                int time_success = 0;

                memset(time_str, 0, sizeof(time_str)); /*set time buffer to 0*/

                time_success = monitor_utils_get_pid_stat(
                    proc_pid_dir_name, col_val[i], sizeof(time_str), time_str);
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

                time = strtoull(time_str, &tmp, 10);
                if (time > UINT64_MAX)
                        return -1;
                time_int = (uint64_t)time;
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
        struct slist *node = (struct slist *)malloc(sizeof(struct slist));

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
 * @param pid process identifier to be searched in the slist
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
 * @param cputicks cpu ticks spent by this process
 * @param proc_start_time time of process creation(seconds since the Epoch)
 *
 * @return pointer to beginning of process statistics slist
 */
static struct slist *
add_proc_cpu_stat(struct slist *pslist,
                  const pid_t pid,
                  const unsigned long cputicks,
                  const time_t proc_start_time)
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
 * @brief Updates statistics for given pid in slist
 *
 * @param pslist pointer to slist of proc_stats. Only internal data
 *               will be updated, no new list entries will be created
 *               or removed.
 * @param pid process identifier of a process to be updated
 * @param cputicks cpu ticks spent by this process
 */
static void
update_proc_cpu_stat(const struct slist *pslist,
                     const pid_t pid,
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
        char *tmp_end; /* used for strtoul error check*/
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
                                /* start time for given pid is needed for
                                 * correct CPU usage statistics - without that
                                 * we have to ignore problematic pid entry and
                                 * move on
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
 *
 * @return number of valid top processes in top_procs filled array (usually
 *         this will equal to max_size)
 */
static int
fill_top_procs(const struct slist *pslist)
{
        const struct slist *it = NULL;
        int current_size = 0;
        int i;
        struct proc_stats stats[TOP_PROC_MAX];

        memset(stats, 0, sizeof(stats[0]) * TOP_PROC_MAX);

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
                if (current_size < TOP_PROC_MAX) {
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
                const struct mon_group *grp;

                /* finally we can add list of top-pids for LLC/MBM monitoring
                 * NOTE: list was sorted in ascending order, so in order to have
                 * initially top-cpu processes on top, we are adding in reverse
                 * order
                 */
                grp = grp_add(MON_GROUP_TYPE_PID,
                              (enum pqos_mon_event)PQOS_MON_EVENT_ALL, desc,
                              &pid, 1);
                if (grp == NULL)
                        exit(EXIT_FAILURE);
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
        int res = 0;
        struct slist *pslist = NULL, *it = NULL;

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
         * So the more time we are sleeping here, the more processes will
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

        fill_top_procs(pslist);

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
            (const struct pqos_mon_data *const *)a;
        const struct pqos_mon_data *const *bpp =
            (const struct pqos_mon_data *const *)b;
        const struct pqos_mon_data *ap = *app;
        const struct pqos_mon_data *bp = *bpp;
        /**
         * This (b-a) is to get descending order
         * otherwise it would be (a-b)
         */
        return (int)(((int64_t)bp->values.llc) - ((int64_t)ap->values.llc));
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
        const struct pqos_mon_data *const *app =
            (const struct pqos_mon_data *const *)a;
        const struct pqos_mon_data *const *bpp =
            (const struct pqos_mon_data *const *)b;
        const struct pqos_mon_data *ap = *app;
        const struct pqos_mon_data *bp = *bpp;
        /**
         * This (a-b) is to get ascending order
         * otherwise it would be (b-a)
         */
        return (int)(((unsigned)ap->cores[0]) - ((unsigned)bp->cores[0]));
}

/**
 * @brief CTRL-C handler for infinite monitoring loop
 *
 * @param signo signal number
 */
static void
monitoring_ctrlc(int signo)
{
        UNUSED_ARG(signo);
        stop_monitoring_loop = 1;
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
get_mon_arrays(struct pqos_mon_data ***parray1, struct pqos_mon_data ***parray2)
{
        unsigned i;
        struct pqos_mon_data **p1, **p2;

        ASSERT(parray1 != NULL && parray2 != NULL);

        p1 = malloc(sizeof(p1[0]) * sel_monitor_num);
        p2 = malloc(sizeof(p2[0]) * sel_monitor_num);
        if (p1 == NULL || p2 == NULL) {
                if (p1)
                        free(p1);
                if (p2)
                        free(p2);
                printf("Error with memory allocation");
                exit(EXIT_FAILURE);
        }

        for (i = 0; i < sel_monitor_num; i++) {
                p1[i] = sel_monitor_group[i].data;
                p2[i] = p1[i];
        }

        *parray1 = p1;
        *parray2 = p2;
        return sel_monitor_num;
}

void
monitor_loop(void)
{
#define TERM_MIN_NUM_LINES 3

        const int istty = isatty(fileno(fp_monitor));
        unsigned cache_size;
        unsigned mon_number = 0, display_num = 0;
        struct pqos_mon_data **mon_data = NULL, **mon_grps = NULL;
        long runtime = 0;
#ifdef __linux__
        int tfd;
#else
        timer_t timerid;
        struct sigevent sev;
        sigset_t sigset;
#endif
        int retval;
        struct itimerspec timer_spec;

        struct {
                void (*begin)(FILE *fp);
                void (*header)(FILE *fp, const char *timestamp);
                void (*row)(FILE *fp,
                            const char *timestamp,
                            const struct pqos_mon_data *data);
                void (*footer)(FILE *fp);
                void (*end)(FILE *fp);
        } output;

        if (strcasecmp(sel_output_type, "text") == 0) {
                output.begin = monitor_text_begin;
                output.header = monitor_text_header;
                output.row = monitor_text_row;
                output.footer = monitor_text_footer;
                output.end = monitor_text_end;
        } else if (strcasecmp(sel_output_type, "csv") == 0) {
                output.begin = monitor_csv_begin;
                output.header = monitor_csv_header;
                output.row = monitor_csv_row;
                output.footer = monitor_csv_footer;
                output.end = monitor_csv_end;
        } else if (strcasecmp(sel_output_type, "xml") == 0) {
                output.begin = monitor_xml_begin;
                output.header = monitor_xml_header;
                output.row = monitor_xml_row;
                output.footer = monitor_xml_footer;
                output.end = monitor_xml_end;
        } else {
                printf("Invalid selection of output file type '%s'!\n",
                       sel_output_type);
                return;
        }

        if (monitor_utils_get_cache_size(&cache_size) != PQOS_RETVAL_OK) {
                printf("Error during getting L3 cache size\n");
                return;
        }

#ifdef __linux__
        tfd = timerfd_create(CLOCK_MONOTONIC, 0);
        if (tfd == -1) {
#else
        sigemptyset(&sigset);
        sigaddset(&sigset, SIGUSR1);
        sigprocmask(SIG_BLOCK, &sigset, NULL);

        sev.sigev_notify = SIGEV_SIGNAL;
        sev.sigev_signo = SIGUSR1;
        sev.sigev_value.sigval_ptr = &timerid;
        if (timer_create(CLOCK_MONOTONIC, &sev, &timerid) == -1) {
#endif
                fprintf(stderr, "Failed to create timer\n");
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
        if (signal(SIGTERM, monitoring_ctrlc) == SIG_ERR)
                printf("Failed to catch SIGTERM!\n");

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

        timer_spec.it_interval.tv_sec = sel_mon_interval / 10l;
        timer_spec.it_interval.tv_nsec =
            sel_mon_interval % 10l * 100l * 1000000l;
        timer_spec.it_value.tv_sec = timer_spec.it_interval.tv_sec;
        timer_spec.it_value.tv_nsec = timer_spec.it_interval.tv_nsec;
#ifdef __linux__
        retval = timerfd_settime(tfd, 0, &timer_spec, NULL);
#else
        retval = timer_settime(timerid, 0, &timer_spec, NULL);
#endif

        if (retval == -1) {
                fprintf(stderr, "Failed to setup timer\n");
                stop_monitoring_loop = 1;
        }

        output.begin(fp_monitor);
        while (!stop_monitoring_loop) {
                struct tm *ptm = NULL;
                unsigned i = 0;
                char cb_time[64];
                int ret;
                uint64_t timer_count = 0;
                time_t curr_time;

                ret = pqos_mon_poll(mon_grps, mon_number);
                if (ret == PQOS_RETVAL_OVERFLOW) {
                        printf("MBM counter overflow\n");
                        continue;
                } else if (ret != PQOS_RETVAL_OK) {
                        printf("Failed to poll monitoring data!\n");
                        break;
                }

                memcpy(mon_data, mon_grps, mon_number * sizeof(mon_grps[0]));

                if (sel_mon_top_like)
                        qsort(mon_data, mon_number, sizeof(mon_data[0]),
                              mon_qsort_llc_cmp_desc);
                else if (monitor_core_mode())
                        qsort(mon_data, mon_number, sizeof(mon_data[0]),
                              mon_qsort_coreid_cmp_asc);

                /**
                 * Get time string
                 */
                curr_time = time(0);
                ptm = localtime(&curr_time);
                if (ptm != NULL)
                        strftime(cb_time, sizeof(cb_time) - 1,
                                 "%Y-%m-%d %H:%M:%S", ptm);
                else
                        strncpy(cb_time, "error", sizeof(cb_time) - 1);

                output.header(fp_monitor, cb_time);
                for (i = 0; i < display_num; i++) {
#ifndef __clang_analyzer__
                        const struct pqos_mon_data *data = mon_data[i];
#else
                        const struct pqos_mon_data *data = NULL;
#endif

                        output.row(fp_monitor, cb_time, data);
                }
                output.footer(fp_monitor);

                fflush(fp_monitor);

                if (stop_monitoring_loop)
                        break;

                /* timeout */
                if (sel_timeout != TIMEOUT_INFINITE &&
                    runtime / 1000l >= sel_timeout)
                        break;

#ifdef __linux__
                retval = read(tfd, &timer_count, sizeof(timer_count));
#else
                sigwaitinfo(&sigset, NULL);
                retval = timer_getoverrun(timerid);
                timer_count = retval + 1;
#endif
                if (retval < 0 || timer_count < 1 || timer_count > 100) {
                        fprintf(stderr, "Failed to read timer\n");
                        break;
                }
                runtime += timer_count * sel_mon_interval * 100l;
        }
        output.end(fp_monitor);

        free(mon_grps);
        free(mon_data);
}

void
monitor_cleanup(void)
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

int
monitor_get_interval(void)
{
        return sel_mon_interval;
}

enum pqos_mon_event
monitor_get_events(void)
{
        return sel_events_max;
}

enum monitor_llc_format
monitor_get_llc_format(void)
{
        return sel_llc_format;
}
