/*
 * BSD LICENSE
 *
 * Copyright(c) 2018-2020 Intel Corporation. All rights reserved.
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
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#include "pqos.h"
#include "log.h"
#include "types.h"
#include "cap.h"
#include "common.h"
#include "monitoring.h"
#include "resctrl.h"
#include "resctrl_monitoring.h"
#include "resctrl_alloc.h"

#define GROUP_NAME_PREFIX "pqos-"

/**
 * ---------------------------------------
 * Local data structures
 * ---------------------------------------
 */
static const struct pqos_cpuinfo *m_cpu = NULL;

static int supported_events = 0;

static unsigned resctrl_mon_counter = 0;

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
 * @brief Free memory allocated by scandir
 *
 * @param namelist structure allocated by scandir
 * @param count number of files
 */
static void
free_scandir(struct dirent **namelist, int count)
{
        int i;

        if (count < 0)
                return;

        for (i = 0; i < count; i++)
                free(namelist[i]);
        free(namelist);
}

int
resctrl_mon_init(const struct pqos_cpuinfo *cpu, const struct pqos_cap *cap)
{
        int ret = PQOS_RETVAL_OK;
        char buf[64];
        FILE *fd;
        struct stat st;

        ASSERT(cpu != NULL);

        UNUSED_PARAM(cap);

        /**
         * Resctrl monitoring not supported
         */
        if (stat(RESCTRL_PATH_INFO_L3_MON, &st) != 0)
                return PQOS_RETVAL_OK;

        /**
         * Discover supported events
         */
        fd = pqos_fopen(RESCTRL_PATH_INFO_L3_MON "/mon_features", "r");
        if (fd == NULL) {
                LOG_ERROR("Failed to obtain resctrl monitoring features\n");
                return PQOS_RETVAL_ERROR;
        }

        while (fgets(buf, sizeof(buf), fd) != NULL) {
                if (strncmp(buf, "llc_occupancy\n", sizeof(buf)) == 0) {
                        LOG_INFO("Detected resctrl support for "
                                 "LLC Occupancy\n");
                        supported_events |= PQOS_MON_EVENT_L3_OCCUP;
                        continue;
                }

                if (strncmp(buf, "mbm_local_bytes\n", sizeof(buf)) == 0) {
                        LOG_INFO("Detected resctrl support for "
                                 "Local Memory B/W\n");
                        supported_events |= PQOS_MON_EVENT_LMEM_BW;
                        continue;
                }

                if (strncmp(buf, "mbm_total_bytes\n", sizeof(buf)) == 0) {
                        LOG_INFO("Detected resctrl support for "
                                 "Total Memory B/W\n");
                        supported_events |= PQOS_MON_EVENT_TMEM_BW;
                }
        }

        if ((supported_events & PQOS_MON_EVENT_LMEM_BW) &&
            (supported_events & PQOS_MON_EVENT_TMEM_BW))
                supported_events |= PQOS_MON_EVENT_RMEM_BW;

        fclose(fd);

        m_cpu = cpu;

        return ret;
}

int
resctrl_mon_fini(void)
{
        m_cpu = NULL;

        return PQOS_RETVAL_OK;
}

/**
 * @brief Get core association with ctrl group
 *
 * @param [in] lcore core id
 * @param [out] class_id COS id
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK on success
 */
static int
alloc_assoc_get(const unsigned lcore, unsigned *class_id)
{
        int ret;
        unsigned max_cos;
        const struct pqos_cap *cap;

        ASSERT(class_id != NULL);

        _pqos_cap_get(&cap, NULL);

        ret = resctrl_alloc_get_grps_num(cap, &max_cos);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        if (max_cos == 0) {
                *class_id = 0;
                return PQOS_RETVAL_OK;
        }

        ret = resctrl_alloc_assoc_get(lcore, class_id);
        if (ret != PQOS_RETVAL_OK)
                LOG_ERROR("Failed to retrieve core %u association\n", lcore);

        return ret;
}

/**
 * @brief Get task association with ctrl group
 *
 * @param [in] tid task id
 * @param [out] class_id COS id
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK on success
 */
static int
alloc_assoc_get_pid(const pid_t tid, unsigned *class_id)
{
        int ret;
        unsigned max_cos;
        const struct pqos_cap *cap;

        ASSERT(class_id != NULL);

        _pqos_cap_get(&cap, NULL);

        ret = resctrl_alloc_get_grps_num(cap, &max_cos);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        if (max_cos == 0) {
                *class_id = 0;
                return PQOS_RETVAL_OK;
        }

        ret = resctrl_alloc_assoc_get_pid(tid, class_id);
        if (ret != PQOS_RETVAL_OK)
                LOG_ERROR("Failed to retrieve task %d association\n", tid);

        return ret;
}

/**
 * @brief Obtain path to monitoring group
 *
 * @param [in] class_id COS id
 * @param [in] resctrl_group mon group name
 * @param [in] file file name insige group
 * @param [out] buf Buffer to store path
 * @param [in] buf_size buffer size
 */
static void
resctrl_mon_group_path(const unsigned class_id,
                       const char *resctrl_group,
                       const char *file,
                       char *buf,
                       const unsigned buf_size)
{
        ASSERT(buf != NULL);

        /* Group name not set - get path to mon_groups directory */
        if (resctrl_group == NULL && class_id == 0)
                snprintf(buf, buf_size, RESCTRL_PATH);
        else if (resctrl_group == NULL)
                snprintf(buf, buf_size, RESCTRL_PATH "/COS%u", class_id);

        /* mon group for COS 0 */
        else if (class_id == 0)
                snprintf(buf, buf_size, RESCTRL_PATH "/mon_groups/%s",
                         resctrl_group);
        /* mon group for the other classes */
        else
                snprintf(buf, buf_size, RESCTRL_PATH "/COS%u/mon_groups/%s",
                         class_id, resctrl_group);

        /* Append file name */
        if (file != NULL)
                strncat(buf, file, buf_size - strlen(buf));
}

/**
 * @brief Write CPU mask to file
 *
 * @param [in] class_id COS id
 * @param [in] resctrl_group mon group name
 * @param [in] mask CPU mask to write
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK on success
 */
static int
resctrl_mon_cpumask_write(const unsigned class_id,
                          const char *resctrl_group,
                          const struct resctrl_cpumask *mask)
{
        FILE *fd;
        char path[128];
        int ret;

        ASSERT(mask != NULL);

        resctrl_mon_group_path(class_id, resctrl_group, "/cpus", path,
                               sizeof(path));

        fd = pqos_fopen(path, "w");
        if (fd == NULL)
                return PQOS_RETVAL_ERROR;

        ret = resctrl_cpumask_write(fd, mask);

        if (fclose(fd) != 0)
                return PQOS_RETVAL_ERROR;

        return ret;
}

/**
 * @brief Read CPU mask from file
 *
 * @param [in] class_id COS id
 * @param [in] resctrl_group mon group name
 * @param [out] mask CPU mask to write
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK on success
 */
static int
resctrl_mon_cpumask_read(const unsigned class_id,
                         const char *resctrl_group,
                         struct resctrl_cpumask *mask)
{
        FILE *fd;
        char path[128];
        int ret;

        ASSERT(mask != NULL);

        resctrl_mon_group_path(class_id, resctrl_group, "/cpus", path,
                               sizeof(path));

        fd = pqos_fopen(path, "r");
        if (fd == NULL)
                return PQOS_RETVAL_ERROR;

        ret = resctrl_cpumask_read(fd, mask);

        fclose(fd);

        return ret;
}

/**
 * @brief Read counter value
 *
 * @param [in] class_id COS id
 * @param [in] resctrl_group mon group name
 * @param [in] l3id l3id to read from
 * @param [in] event resctrl mon event
 * @param [out] value counter value
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK on success
 */
static int
resctrl_mon_read_counter(const unsigned class_id,
                         const char *resctrl_group,
                         const unsigned l3id,
                         const enum pqos_mon_event event,
                         uint64_t *value)
{
        char buf[128];
        const char *name;
        char path[PATH_MAX];
        FILE *fd;
        unsigned long long counter;

        ASSERT(resctrl_group != NULL);
        ASSERT(value != NULL);

        switch (event) {
        case PQOS_MON_EVENT_L3_OCCUP:
                name = "llc_occupancy";
                break;
        case PQOS_MON_EVENT_LMEM_BW:
                name = "mbm_local_bytes";
                break;
        case PQOS_MON_EVENT_TMEM_BW:
                name = "mbm_total_bytes";
                break;
        default:
                LOG_ERROR("Unknown resctrl event\n");
                return PQOS_RETVAL_PARAM;
                break;
        }

        *value = 0;

        resctrl_mon_group_path(class_id, resctrl_group, NULL, buf, sizeof(buf));
        snprintf(path, sizeof(path), "%s/mon_data/mon_L3_%02u/%s", buf, l3id,
                 name);
        fd = pqos_fopen(path, "r");
        if (fd == NULL)
                return PQOS_RETVAL_ERROR;
        if (fscanf(fd, "%llu", &counter) == 1)
                *value = counter;
        fclose(fd);

        return PQOS_RETVAL_OK;
}

/**
 * @brief Read counter value from requested \a l3ids
 *
 * @param [in] class_id COS id
 * @param [in] resctrl_group mon group name
 * @param [in] event resctrl mon event
 * @param [in] l3ids l3ids to read from
 * @param [in] l3ids_num number of l3ids
 * @param [in] event monitoring event
 * @param [out] value counter value
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK on success
 */
static int
resctrl_mon_read_counters(const unsigned class_id,
                          const char *resctrl_group,
                          unsigned *l3ids,
                          unsigned l3ids_num,
                          const enum pqos_mon_event event,
                          uint64_t *value)
{
        int ret = PQOS_RETVAL_OK;
        unsigned *l3cat_ids = NULL;
        unsigned l3cat_id_num;
        unsigned l3cat_id;

        ASSERT(resctrl_group != NULL);
        ASSERT(value != NULL);

        *value = 0;

        if (l3ids == NULL) {
                l3cat_ids = pqos_cpu_get_l3cat_ids(m_cpu, &l3cat_id_num);
                if (l3cat_ids == NULL) {
                        ret = PQOS_RETVAL_ERROR;
                        goto resctrl_mon_read_exit;
                }
        } else {
                l3cat_ids = l3ids;
                l3cat_id_num = l3ids_num;
        }

        for (l3cat_id = 0; l3cat_id < l3cat_id_num; l3cat_id++) {
                const unsigned l3id = l3cat_ids[l3cat_id];
                uint64_t counter;

                ret = resctrl_mon_read_counter(class_id, resctrl_group, l3id,
                                               event, &counter);
                if (ret != PQOS_RETVAL_OK)
                        goto resctrl_mon_read_exit;

                *value += counter;
        }

resctrl_mon_read_exit:
        if (l3ids == NULL && l3cat_ids != NULL)
                free(l3cat_ids);

        return ret;
}

/**
 * @brief Obtain max threshold occupancy
 *
 * @param [out] value max threshold occupancy value
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK on success
 */
static int
restrl_mon_get_max_llc_tresh(unsigned *value)
{
        int ret = PQOS_RETVAL_OK;
        FILE *fd;

        fd = pqos_fopen(RESCTRL_PATH "/info/L3_MON/max_threshold_occupancy",
                        "r");
        if (fd == NULL)
                return PQOS_RETVAL_ERROR;
        if (fscanf(fd, "%u", value) != 1)
                ret = PQOS_RETVAL_ERROR;
        fclose(fd);

        return ret;
}

/**
 * @brief Check if file is empty
 *
 * @param [in] path file path
 *
 * @return If file is empty
 * @retval 1 if file is empty
 */
static int
resctrl_mon_file_empty(const char *path)
{
        int empty = 1;
        char buf[128];

        FILE *fd = pqos_fopen(path, "r");

        if (fd == NULL)
                return -1;
        memset(buf, 0, sizeof(buf));
        /* Try to read from file */
        while (fgets(buf, sizeof(buf), fd) != NULL)
                if (buf[0] != '\n') {
                        empty = 0;
                        break;
                }
        fclose(fd);

        return empty;
}

/**
 * @brief Check if monitored group is junk (no cores/tasks assigned/low llc)
 *
 * @param [in] class_id COS id
 * @param [in] resctrl_group mon group name
 * @param [in] l3id  l3ids to read from
 * @param [in] l3id_num number of l3ids
 * @param [out] empty 1 when group is empty
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK on success
 */
static int
resctrl_mon_empty(const unsigned class_id,
                  const char *resctrl_group,
                  unsigned *l3id,
                  unsigned l3id_num,
                  int *empty)
{
        int ret;
        int retval;
        char path[128];
        unsigned max_threshold_occupancy;
        uint64_t value;

        ASSERT(resctrl_group != NULL);
        ASSERT(empty != NULL);

        *empty = 1;

        /*
         * Some cores are assigned to group?
         */
        resctrl_mon_group_path(class_id, resctrl_group, "/cpus_list", path,
                               sizeof(path));
        retval = resctrl_mon_file_empty(path);
        if (retval < 0)
                return PQOS_RETVAL_ERROR;
        else if (!retval) {
                *empty = 0;
                return PQOS_RETVAL_OK;
        }

        /*
         *Some tasks are assigned to group?
         */
        resctrl_mon_group_path(class_id, resctrl_group, "/tasks", path,
                               sizeof(path));
        retval = resctrl_mon_file_empty(path);
        if (retval < 0)
                return PQOS_RETVAL_ERROR;
        else if (!retval) {
                *empty = 0;
                return PQOS_RETVAL_OK;
        }

        /*
         * Check if llc occupancy is lower than max_occupancy_threshold
         */
        if ((supported_events & PQOS_MON_EVENT_L3_OCCUP) == 0)
                return PQOS_RETVAL_OK;

        ret = resctrl_mon_read_counters(class_id, resctrl_group, l3id, l3id_num,
                                        PQOS_MON_EVENT_L3_OCCUP, &value);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        ret = restrl_mon_get_max_llc_tresh(&max_threshold_occupancy);
        if (ret != PQOS_RETVAL_OK)
                return ret;
        if (value > max_threshold_occupancy)
                *empty = 0;

        return ret;
}

/**
 * @brief Create directory if not exists
 *
 * @param[in] path directory path
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 */
static int
resctrl_mon_mkdir(const char *path)
{
        ASSERT(path != NULL);

        if (mkdir(path, 0755) == -1 && errno != EEXIST)
                return PQOS_RETVAL_BUSY;

        return PQOS_RETVAL_OK;
}

/**
 * @brief Remove directory if exists
 *
 * @param[in] path directory path
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 */
static int
resctrl_mon_rmdir(const char *path)
{
        ASSERT(path != NULL);

        if (rmdir(path) == -1 && errno != ENOENT)
                return PQOS_RETVAL_ERROR;

        return PQOS_RETVAL_OK;
}

int
resctrl_mon_assoc_get(const unsigned lcore,
                      char *name,
                      const unsigned name_size)
{
        int ret;
        unsigned class_id;
        char dir[256];
        struct dirent **namelist = NULL;
        int num_groups;
        int i;

        ASSERT(name != NULL);

        if (supported_events == 0)
                return PQOS_RETVAL_RESOURCE;

        ret = alloc_assoc_get(lcore, &class_id);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        resctrl_mon_group_path(class_id, "", NULL, dir, sizeof(dir));
        num_groups = scandir(dir, &namelist, filter, NULL);
        if (num_groups < 0) {
                LOG_ERROR("Failed to read monitoring groups for COS %u\n",
                          class_id);
                return PQOS_RETVAL_ERROR;
        }

        for (i = 0; i < num_groups; i++) {
                struct resctrl_cpumask mask;

                ret = resctrl_mon_cpumask_read(class_id, namelist[i]->d_name,
                                               &mask);
                if (ret != PQOS_RETVAL_OK)
                        goto resctrl_mon_assoc_get_exit;

                if (resctrl_cpumask_get(lcore, &mask)) {
                        strncpy(name, namelist[i]->d_name, name_size);
                        ret = PQOS_RETVAL_OK;
                        goto resctrl_mon_assoc_get_exit;
                }
        }

        /* Core not associated with mon group */
        ret = PQOS_RETVAL_RESOURCE;

resctrl_mon_assoc_get_exit:
        free_scandir(namelist, num_groups);

        return ret;
}

int
resctrl_mon_assoc_set(const unsigned lcore, const char *name)
{
        unsigned class_id = 0;
        int ret;
        char path[128];
        struct resctrl_cpumask cpumask;

        ASSERT(name != NULL);

        ret = alloc_assoc_get(lcore, &class_id);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        resctrl_mon_group_path(class_id, name, NULL, path, sizeof(path));
        ret = resctrl_mon_mkdir(path);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        ret = resctrl_mon_cpumask_read(class_id, name, &cpumask);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        resctrl_cpumask_set(lcore, &cpumask);

        ret = resctrl_mon_cpumask_write(class_id, name, &cpumask);
        if (ret != PQOS_RETVAL_OK)
                LOG_ERROR("Could not assign core %u to resctrl monitoring "
                          "group\n",
                          lcore);

        return ret;
}

/* @brief Restore association of \a lcore to monitoring group
 *
 * @param [in] lcore CPU logical core id
 * @param [in] name name of monitoring group
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK on success
 */
static int
resctrl_mon_assoc_restore(const unsigned lcore, const char *name)
{
        int ret;
        char buf[128];
        unsigned class_id;
        struct stat st;

        ASSERT(name != NULL);

        ret = resctrl_mon_assoc_get(lcore, buf, sizeof(buf));
        /* core already associated with mon group */
        if (ret == PQOS_RETVAL_OK)
                return PQOS_RETVAL_OK;

        ret = alloc_assoc_get(lcore, &class_id);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        resctrl_mon_group_path(class_id, name, NULL, buf, sizeof(buf));
        if (stat(buf, &st) != 0) {
                LOG_WARN("Could not restore core association with mon "
                         "group %s does not exists\n",
                         buf);
                return PQOS_RETVAL_RESOURCE;
        }

        ret = resctrl_mon_assoc_set(lcore, name);

        return ret;
}

int
resctrl_mon_assoc_get_pid(const pid_t task,
                          char *name,
                          const unsigned name_size)
{
        int ret;
        unsigned class_id;
        char dir[256];
        struct dirent **namelist = NULL;
        int num_groups;
        int i;

        ASSERT(name != NULL);

        if (supported_events == 0)
                return PQOS_RETVAL_RESOURCE;

        ret = alloc_assoc_get_pid(task, &class_id);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        resctrl_mon_group_path(class_id, "", NULL, dir, sizeof(dir));
        num_groups = scandir(dir, &namelist, filter, NULL);
        if (num_groups < 0) {
                LOG_ERROR("Failed to read monitoring groups for COS %u\n",
                          class_id);
                return PQOS_RETVAL_ERROR;
        }

        for (i = 0; i < num_groups; i++) {
                char path[256];
                char buf[128];
                FILE *fd;

                resctrl_mon_group_path(class_id, namelist[i]->d_name, "/tasks",
                                       path, sizeof(path));

                fd = pqos_fopen(path, "r");
                if (fd == NULL)
                        goto resctrl_mon_assoc_get_pid_exit;

                while (fgets(buf, sizeof(buf), fd) != NULL) {
                        char *endptr = NULL;
                        pid_t value = strtol(buf, &endptr, 10);

                        if (!(*buf != '\0' &&
                              (*endptr == '\0' || *endptr == '\n'))) {
                                ret = PQOS_RETVAL_ERROR;
                                fclose(fd);
                                goto resctrl_mon_assoc_get_pid_exit;
                        }

                        if (value == task) {
                                strncpy(name, namelist[i]->d_name, name_size);
                                ret = PQOS_RETVAL_OK;
                                fclose(fd);
                                goto resctrl_mon_assoc_get_pid_exit;
                        }
                }

                fclose(fd);
        }

        /* Task not associated with mon group */
        ret = PQOS_RETVAL_RESOURCE;

resctrl_mon_assoc_get_pid_exit:
        free_scandir(namelist, num_groups);

        return ret;
}

int
resctrl_mon_assoc_set_pid(const pid_t task, const char *name)
{
        unsigned class_id;
        int ret;
        char path[128];
        FILE *fd;

        ret = alloc_assoc_get_pid(task, &class_id);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        resctrl_mon_group_path(class_id, name, NULL, path, sizeof(path));
        ret = resctrl_mon_mkdir(path);
        if (ret != PQOS_RETVAL_OK) {
                LOG_ERROR("Failed to create resctrl monitoring group!\n");
                return ret;
        }

        strncat(path, "/tasks", sizeof(path) - strlen(path) - 1);
        fd = pqos_fopen(path, "w");
        if (fd == NULL)
                return PQOS_RETVAL_ERROR;

        fprintf(fd, "%d\n", task);

        if (fclose(fd) != 0) {
                LOG_ERROR("Could not assign TID %d to resctrl monitoring "
                          "group\n",
                          task);
                return PQOS_RETVAL_ERROR;
        }

        return PQOS_RETVAL_OK;
}

#define RESCTRL_CORE_MAX_L3ID 63
struct resctrl_core_group {
        char name[32];
        int valid;
        uint64_t llc;
        uint64_t l3ids;
        time_t mtime;
};

/**
 * @brief Parse resctrl core monitoring groups
 *
 * @param [out] cgrp Parser monitoring groups
 * @param [out] cgrp_num Number of groups
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK on success
 */
static int
resctrl_mon_parse(struct resctrl_core_group **cgrp, unsigned *cgrp_num)
{
        int ret = PQOS_RETVAL_OK;
        unsigned cos = 0;
        unsigned ctrl_grps;
        const struct pqos_cap *cap;
        const struct pqos_cpuinfo *cpu;
        const struct pqos_capability *cap_mon;
        struct resctrl_core_group *grps = NULL;
        unsigned grps_num = 0;
        unsigned grps_size;

        _pqos_cap_get(&cap, &cpu);

        ret = pqos_cap_get_type(cap, PQOS_CAP_TYPE_MON, &cap_mon);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        ret = resctrl_alloc_get_grps_num(cap, &ctrl_grps);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        grps_size = cap_mon->u.mon->max_rmid;
        grps = calloc(grps_size, sizeof(*grps));
        if (grps == NULL)
                return PQOS_RETVAL_RESOURCE;

        /* Build list of core monitroing groups */
        do {
                struct dirent **namelist = NULL;
                struct resctrl_cpumask cpumask;
                int num_groups;
                char dir[256];
                int i;

                resctrl_mon_group_path(cos, "", NULL, dir, sizeof(dir));
                num_groups = scandir(dir, &namelist, filter, NULL);
                if (num_groups < 0) {
                        LOG_ERROR(
                            "Failed to read monitoring groups for COS %u\n",
                            cos);
                        ret = PQOS_RETVAL_ERROR;
                        goto resctrl_mon_parse_exit;
                }

                for (i = 0; i < num_groups; i++) {
                        char path[256];
                        const char *grp_name = namelist[i]->d_name;
                        struct resctrl_core_group *grp = NULL;
                        unsigned j;
                        int retval;

                        /* group not managed by pqos library */
                        if (strncmp(grp_name, GROUP_NAME_PREFIX, 5) != 0)
                                continue;

                        /* Check if group already exists */
                        for (j = 0; j < grps_num; j++)
                                if (strncmp(grp_name, grps[j].name,
                                            DIM(grps[j].name)) == 0) {
                                        grp = &grps[j];
                                        break;
                                }
                        /* new group */
                        if (grp == NULL) {
                                if (grps_num + 1 == grps_size) {
                                        ret = PQOS_RETVAL_ERROR;
                                        break;
                                }
                                grp = &grps[grps_num++];
                                strncpy(grp->name, grp_name, sizeof(grp->name));
                                grp->name[sizeof(grp->name) - 1] = '\0';
                                grp->valid = 1;
                        }
                        if (!grp->valid)
                                continue;

                        /* Some tasks are assigned to group? */
                        resctrl_mon_group_path(cos, grp->name, "/tasks", path,
                                               sizeof(path));
                        retval = resctrl_mon_file_empty(path);
                        if (retval < 0) {
                                ret = PQOS_RETVAL_ERROR;
                                break;
                        } else if (!retval) {
                                grp->valid = 0;
                                continue;
                        }

                        /* Cores assigned to group */
                        ret =
                            resctrl_mon_cpumask_read(cos, grp->name, &cpumask);
                        if (ret != PQOS_RETVAL_OK)
                                break;

                        for (j = 0; j < cpu->num_cores; j++) {
                                const struct pqos_coreinfo *coreinfo =
                                    &(cpu->cores[j]);

                                if (resctrl_cpumask_get(coreinfo->lcore,
                                                        &cpumask)) {
                                        if (coreinfo->l3cat_id >=
                                            RESCTRL_CORE_MAX_L3ID) {
                                                grp->valid = 0;
                                                break;
                                        }
                                        grp->l3ids |=
                                            (1LLU << coreinfo->l3cat_id);
                                }
                        }
                }
                free_scandir(namelist, num_groups);

                if (ret != PQOS_RETVAL_OK)
                        break;
        } while (++cos < ctrl_grps);

resctrl_mon_parse_exit:
        if (ret != PQOS_RETVAL_OK)
                free(grps);
        else {
                *cgrp = grps;
                *cgrp_num = grps_num;
        }

        return ret;
}

/**
 * @brief Assigns resctrl monitoring group
 *
 * @param group monitoring structure
 *
 * @return Resctrl monitoring group
 */
static char *
resctrl_mon_assign(struct pqos_mon_data *group)
{
        char *resctrl_group = group->intl->resctrl.mon_group;
        struct resctrl_core_group *cgrp = NULL;

        /* search for available core group */
        if (resctrl_group == NULL && group->tid_nr == 0 &&
            group->num_cores > 0) {
                int ret;
                unsigned i;
                unsigned l3ids = 0;
                unsigned cgrp_num;
                const struct pqos_cpuinfo *cpu;
                const struct pqos_cap *cap;
                unsigned max_threshold_occupancy;
                unsigned max_cos;

                _pqos_cap_get(&cap, &cpu);

                /* list L3IDs for requested cores */
                for (i = 0; i < group->num_cores; i++) {
                        const struct pqos_coreinfo *coreinfo =
                            pqos_cpu_get_core_info(cpu, group->cores[i]);

                        if (coreinfo == NULL)
                                return NULL;
                        if (coreinfo->l3cat_id > RESCTRL_CORE_MAX_L3ID)
                                goto mon_assign_new;

                        l3ids |= (1LLU << coreinfo->l3cat_id);
                }

                if ((supported_events & PQOS_MON_EVENT_L3_OCCUP) != 0) {
                        ret = restrl_mon_get_max_llc_tresh(
                            &max_threshold_occupancy);
                        if (ret != PQOS_RETVAL_OK)
                                return NULL;
                }

                ret = resctrl_alloc_get_grps_num(cap, &max_cos);
                if (ret != PQOS_RETVAL_OK)
                        return NULL;

                /* parse existing core monitoring groups */
                ret = resctrl_mon_parse(&cgrp, &cgrp_num);
                if (ret != PQOS_RETVAL_OK)
                        return NULL;

                for (i = 0; i < cgrp_num; i++) {
                        struct resctrl_core_group *grp = &cgrp[i];
                        uint64_t llc = 0;
                        unsigned l3id;

                        if (!grp->valid)
                                continue;

                        /* L3Ids overlap */
                        if ((l3ids & grp->l3ids) != 0)
                                continue;

                        /* check if llc doesn't occupancy threshold */
                        if ((supported_events & PQOS_MON_EVENT_L3_OCCUP) != 0) {
                                for (l3id = 0; l3id < RESCTRL_CORE_MAX_L3ID;
                                     l3id++) {
                                        unsigned cos = 0;

                                        if ((l3ids & (1LLU << l3id)) == 0)
                                                continue;

                                        do {
                                                uint64_t val;
                                                struct stat st;
                                                char buf[128];

                                                resctrl_mon_group_path(
                                                    cos, grp->name, NULL, buf,
                                                    sizeof(buf));
                                                if (stat(buf, &st) != 0)
                                                        continue;

                                                ret = resctrl_mon_read_counter(
                                                    cos, grp->name, l3id,
                                                    PQOS_MON_EVENT_L3_OCCUP,
                                                    &val);
                                                if (ret != PQOS_RETVAL_OK)
                                                        goto mon_assign_exit;

                                                llc += val;
                                        } while (++cos < max_cos);
                                }
                                if (llc > max_threshold_occupancy)
                                        continue;
                        }

                        resctrl_group = strdup(grp->name);
                        break;
                }
        }

mon_assign_new:
        /* Create new monitoring group */
        if (resctrl_group == NULL) {
                char buf[32];

                snprintf(buf, sizeof(buf), GROUP_NAME_PREFIX "%d-%u",
                         (int)getpid(), resctrl_mon_counter++);

                resctrl_group = strdup(buf);
                if (resctrl_group == NULL)
                        LOG_DEBUG("Memory allocation failed\n");
        }

mon_assign_exit:
        if (cgrp != NULL)
                free(cgrp);
        return resctrl_group;
}

int
resctrl_mon_start(struct pqos_mon_data *group)
{
        char *resctrl_group = NULL;
        int ret = PQOS_RETVAL_OK;
        unsigned i;
        const struct pqos_cpuinfo *cpu;

        ASSERT(group != NULL);
        ASSERT(group->tid_nr == 0 || group->tid_map != NULL);
        ASSERT(group->num_cores == 0 || group->cores != NULL);

        _pqos_cap_get(NULL, &cpu);

        /* List l3ids */
        for (i = 0; i < group->num_cores; i++) {
                struct pqos_mon_data_internal *intl = group->intl;
                unsigned j;
                const struct pqos_coreinfo *coreinfo =
                    pqos_cpu_get_core_info(cpu, group->cores[i]);

                if (coreinfo == NULL) {
                        ret = PQOS_RETVAL_ERROR;
                        goto resctrl_mon_start_exit;
                }

                for (j = 0; j < intl->resctrl.num_l3id; j++)
                        if (coreinfo->l3cat_id == intl->resctrl.l3id[j])
                                break;

                if (j == intl->resctrl.num_l3id) {
                        intl->resctrl.l3id =
                            realloc(intl->resctrl.l3id,
                                    sizeof(*intl->resctrl.l3id) *
                                        (intl->resctrl.num_l3id + 1));
                        if (intl->resctrl.l3id == NULL) {
                                ret = PQOS_RETVAL_ERROR;
                                goto resctrl_mon_start_exit;
                        }
                        intl->resctrl.l3id[intl->resctrl.num_l3id++] =
                            coreinfo->l3cat_id;
                }
        }

        /**
         * Get resctrl monitoring group
         */
        resctrl_group = resctrl_mon_assign(group);
        if (resctrl_group == NULL)
                return PQOS_RETVAL_ERROR;

        /**
         * Add pids to the resctrl group
         */
        for (i = 0; i < group->tid_nr; i++) {
                ret =
                    resctrl_mon_assoc_set_pid(group->tid_map[i], resctrl_group);
                if (ret != PQOS_RETVAL_OK)
                        goto resctrl_mon_start_exit;
        }

        /**
         * Add cores to the resctrl group
         */
        for (i = 0; i < group->num_cores; i++) {
                ret = resctrl_mon_assoc_set(group->cores[i], resctrl_group);
                if (ret != PQOS_RETVAL_OK)
                        goto resctrl_mon_start_exit;
        }

        group->intl->resctrl.mon_group = resctrl_group;

resctrl_mon_start_exit:
        if (ret != PQOS_RETVAL_OK) {
                if (group->intl->resctrl.l3id != NULL)
                        free(group->intl->resctrl.l3id);
                if (group->intl->resctrl.mon_group != resctrl_group)
                        free(resctrl_group);
        }

        return ret;
}

/**
 * @brief Remove resctrl monitoring group
 *
 * @param [in] resctrl_group group name
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 */
static int
resctrl_mon_delete(const char *resctrl_group)
{
        int ret;
        unsigned max_cos;
        unsigned cos = 0;
        const struct pqos_cap *cap;

        _pqos_cap_get(&cap, NULL);

        ret = resctrl_alloc_get_grps_num(cap, &max_cos);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        do {
                char buf[128];

                resctrl_mon_group_path(cos, resctrl_group, NULL, buf,
                                       sizeof(buf));

                ret = resctrl_mon_rmdir(buf);
                if (ret != PQOS_RETVAL_OK) {
                        LOG_ERROR("Failed to remove resctrl "
                                  "monitoring group\n");
                        return PQOS_RETVAL_ERROR;
                }
        } while (++cos < max_cos);

        return PQOS_RETVAL_OK;
}

/**
 * @brief Check if resctrl group is shared between two mon groups
 *
 * @param [in] group monitoring structure
 * @param [out] shared 1 if resctrl is shared
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 * @retval PQOS_RETVAL_ERROR if error occurs
 */
static int
resctrl_mon_shared(struct pqos_mon_data *group, unsigned *shared)
{
        unsigned max_cos;
        unsigned cos;
        int ret;
        const struct pqos_cap *cap;
        const struct pqos_cpuinfo *cpu;
        const char *mon_group = group->intl->resctrl.mon_group;

        *shared = 0;

        if (group->num_pids > 0)
                return PQOS_RETVAL_OK;

        _pqos_cap_get(&cap, &cpu);

        ret = resctrl_alloc_get_grps_num(cap, &max_cos);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        cos = 0;
        do {
                char path[128];
                struct stat st;
                struct resctrl_cpumask mask;
                unsigned i;

                /* check if group exists */
                resctrl_mon_group_path(cos, mon_group, NULL, path,
                                       sizeof(path));
                if (stat(path, &st) != 0)
                        continue;

                /* read assigned cores */
                ret = resctrl_mon_cpumask_read(cos, mon_group, &mask);
                if (ret != PQOS_RETVAL_OK)
                        return ret;

                /* check if cores overlap */
                for (i = 0; i < cpu->num_cores; i++) {
                        const unsigned lcore = cpu->cores[i].lcore;

                        if (resctrl_cpumask_get(lcore, &mask)) {
                                unsigned j;

                                *shared = 1;
                                for (j = 0; j < group->num_cores; j++)
                                        if (group->cores[i] == lcore) {
                                                *shared = 0;
                                                break;
                                        }
                        }

                        if (*shared)
                                return PQOS_RETVAL_OK;
                }

        } while (++cos < max_cos);

        return ret;
}

/**
 * @brief Function to stop resctrl event counters
 *
 * @param group monitoring structure
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 */
int
resctrl_mon_stop(struct pqos_mon_data *group)
{
        int ret;
        unsigned max_cos;
        unsigned i;
        const struct pqos_cap *cap;

        ASSERT(group != NULL);

        _pqos_cap_get(&cap, NULL);

        ret = resctrl_alloc_get_grps_num(cap, &max_cos);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        /*
         * Add pids back to the default group
         */
        if (group->num_pids > 0)
                for (i = 0; i < group->tid_nr; i++) {
                        const pid_t tid = group->tid_map[i];

                        if (resctrl_alloc_task_validate(tid) !=
                            PQOS_RETVAL_OK) {
                                LOG_DEBUG("resctrl_mon_stop: Skipping "
                                          "non-existent PID: %d\n",
                                          tid);
                                continue;
                        }
                        ret = resctrl_mon_assoc_set_pid(tid, NULL);
                        if (ret != PQOS_RETVAL_OK)
                                goto resctrl_mon_stop_exit;
                }

        /*
         * Remove cores from mon group
         */
        if (group->num_cores > 0) {
                const char *mon_group = group->intl->resctrl.mon_group;
                char path[128];
                struct stat st;
                unsigned cos = 0;

                do {
                        struct resctrl_cpumask cpumask;

                        resctrl_mon_group_path(cos, mon_group, NULL, path,
                                               sizeof(path));
                        if (stat(path, &st) != 0)
                                continue;

                        ret =
                            resctrl_mon_cpumask_read(cos, mon_group, &cpumask);
                        if (ret != PQOS_RETVAL_OK)
                                goto resctrl_mon_stop_exit;

                        for (i = 0; i < group->num_cores; i++) {
                                const unsigned core = group->cores[i];

                                resctrl_cpumask_unset(core, &cpumask);
                        }

                        ret =
                            resctrl_mon_cpumask_write(cos, mon_group, &cpumask);
                        if (ret != PQOS_RETVAL_OK)
                                goto resctrl_mon_stop_exit;
                } while (++cos < max_cos);
        }

        if (group->intl->resctrl.mon_group != NULL) {
                unsigned shared;

                ret = resctrl_mon_shared(group, &shared);
                if (ret != PQOS_RETVAL_OK)
                        goto resctrl_mon_stop_exit;

                if (!shared) {
                        ret =
                            resctrl_mon_delete(group->intl->resctrl.mon_group);
                        if (ret != PQOS_RETVAL_OK)
                                goto resctrl_mon_stop_exit;
                }

                free(group->intl->resctrl.mon_group);
                group->intl->resctrl.mon_group = NULL;
        }

resctrl_mon_stop_exit:
        return ret;
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

/**
 * @brief This function removes all empty monitoring
 *        groups associated with \a group
 *
 * @param group monitoring structure
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 * @retval PQOS_RETVAL_ERROR if error occurs
 */
static int
resctrl_mon_purge(struct pqos_mon_data *group)
{
        unsigned max_cos;
        unsigned cos;
        int ret;
        unsigned shared;
        const struct pqos_cap *cap;

        ASSERT(group != NULL);

        _pqos_cap_get(&cap, NULL);

        ret = resctrl_mon_shared(group, &shared);
        if (ret != PQOS_RETVAL_OK)
                return ret;
        if (shared)
                return PQOS_RETVAL_OK;

        ret = resctrl_alloc_get_grps_num(cap, &max_cos);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        cos = 0;
        do {
                int empty;
                struct stat st;
                uint64_t value;
                char buf[128];

                resctrl_mon_group_path(cos, group->intl->resctrl.mon_group,
                                       NULL, buf, sizeof(buf));
                if (stat(buf, &st) != 0)
                        continue;

                ret = resctrl_mon_empty(cos, group->intl->resctrl.mon_group,
                                        group->intl->resctrl.l3id,
                                        group->intl->resctrl.num_l3id, &empty);
                if (ret != PQOS_RETVAL_OK)
                        return ret;

                if (!empty)
                        continue;

                /* store counter values */
                if (supported_events & PQOS_MON_EVENT_LMEM_BW) {
                        ret = resctrl_mon_read_counters(
                            cos, group->intl->resctrl.mon_group,
                            group->intl->resctrl.l3id,
                            group->intl->resctrl.num_l3id,
                            PQOS_MON_EVENT_LMEM_BW, &value);
                        if (ret != PQOS_RETVAL_OK)
                                return ret;
                        group->intl->resctrl.values_storage.mbm_local += value;
                }
                if (supported_events & PQOS_MON_EVENT_TMEM_BW) {
                        ret = resctrl_mon_read_counters(
                            cos, group->intl->resctrl.mon_group,
                            group->intl->resctrl.l3id,
                            group->intl->resctrl.num_l3id,
                            PQOS_MON_EVENT_TMEM_BW, &value);
                        if (ret != PQOS_RETVAL_OK)
                                return ret;

                        group->intl->resctrl.values_storage.mbm_total += value;
                }

                ret = resctrl_mon_rmdir(buf);
                if (ret != PQOS_RETVAL_OK) {
                        LOG_WARN("Failed to remove empty mon group %s: %m\n",
                                 buf);
                        return ret;
                }

                LOG_INFO("Deleted empty mon group %s\n", buf);

        } while (++cos < max_cos);

        return ret;
}

/**
 * @brief This function polls all resctrl counters
 *
 * Reads counters for all events and stores values
 *
 * @param group monitoring structure
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 * @retval PQOS_RETVAL_ERROR if error occurs
 */
int
resctrl_mon_poll(struct pqos_mon_data *group, const enum pqos_mon_event event)
{
        int ret;
        uint64_t value = 0;
        unsigned max_cos;
        unsigned cos;
        unsigned i;
        uint64_t old_value;
        const struct pqos_cap *cap;

        ASSERT(group != NULL);

        _pqos_cap_get(&cap, NULL);

        ret = resctrl_alloc_get_grps_num(cap, &max_cos);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        /*
         * When core COS assoc changes then kernel resets monitoring group
         * assoc. We need to restore monitoring assoc for cores
         */
        for (i = 0; i < group->num_cores; i++) {
                ret = resctrl_mon_assoc_restore(group->cores[i],
                                                group->intl->resctrl.mon_group);
                if (ret != PQOS_RETVAL_OK)
                        goto resctrl_mon_poll_exit;
        }

        /* Search COSes for given resctrl mon group */
        cos = 0;
        do {
                uint64_t val;
                struct stat st;
                char buf[128];

                resctrl_mon_group_path(cos, group->intl->resctrl.mon_group,
                                       NULL, buf, sizeof(buf));
                if (stat(buf, &st) != 0)
                        continue;

                ret = resctrl_mon_read_counters(
                    cos, group->intl->resctrl.mon_group,
                    group->intl->resctrl.l3id, group->intl->resctrl.num_l3id,
                    event, &val);
                if (ret != PQOS_RETVAL_OK)
                        goto resctrl_mon_poll_exit;

                value += val;

        } while (++cos < max_cos);

        /**
         * Set value
         */
        switch (event) {
        case PQOS_MON_EVENT_L3_OCCUP:
                group->values.llc = value;
                break;
        case PQOS_MON_EVENT_LMEM_BW:
                old_value = group->values.mbm_local;
                group->values.mbm_local =
                    value + group->intl->resctrl.values_storage.mbm_local;
                group->values.mbm_local_delta =
                    get_delta(old_value, group->values.mbm_local);
                break;
        case PQOS_MON_EVENT_TMEM_BW:
                old_value = group->values.mbm_total;
                group->values.mbm_total =
                    value + group->intl->resctrl.values_storage.mbm_total;
                group->values.mbm_total_delta =
                    get_delta(old_value, group->values.mbm_total);
                break;
        default:
                return PQOS_RETVAL_ERROR;
        }

        /*
         * If this group is empty, save the values for
         * next poll and clear the group.
         */
        ret = resctrl_mon_purge(group);
        if (ret != PQOS_RETVAL_OK)
                goto resctrl_mon_poll_exit;

resctrl_mon_poll_exit:
        return ret;
}

int
resctrl_mon_reset(void)
{
        int ret;
        unsigned grps;
        int num_groups;
        unsigned cos = 0;
        const struct pqos_cap *cap;

        if (supported_events == 0)
                return PQOS_RETVAL_RESOURCE;

        _pqos_cap_get(&cap, NULL);

        ret = resctrl_alloc_get_grps_num(cap, &grps);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        do {
                struct dirent **namelist = NULL;
                char dir[256];
                int i;

                resctrl_mon_group_path(cos, "", NULL, dir, sizeof(dir));
                num_groups = scandir(dir, &namelist, filter, NULL);
                if (num_groups < 0) {
                        LOG_ERROR("Failed to read monitoring groups for "
                                  "COS %u\n",
                                  cos);
                        return PQOS_RETVAL_ERROR;
                }

                for (i = 0; i < num_groups; i++) {
                        char path[256];

                        resctrl_mon_group_path(cos, namelist[i]->d_name, NULL,
                                               path, sizeof(path));

                        ret = resctrl_mon_rmdir(path);
                        if (ret != PQOS_RETVAL_OK) {
                                free_scandir(namelist, num_groups);
                                return ret;
                        }
                }

                free_scandir(namelist, num_groups);
        } while (++cos < grps);

        return ret;
}

int
resctrl_mon_is_event_supported(const enum pqos_mon_event event)
{
        return (supported_events & event) == event;
}

int
resctrl_mon_active(unsigned *monitoring_status)
{
        unsigned group_idx = 0;
        unsigned resctrl_group_count = 0;
        int ret;
        const struct pqos_cap *cap;

        if (supported_events == 0) {
                *monitoring_status = 0;
                return PQOS_RETVAL_OK;
        }

        _pqos_cap_get(&cap, NULL);

        ret = resctrl_alloc_get_grps_num(cap, &resctrl_group_count);
        if (ret != PQOS_RETVAL_OK) {
                LOG_ERROR("Failed to count resctrl groups");
                return ret;
        }

        do {
                int files_count;
                char path[256];
                struct dirent **mon_group_files = NULL;

                resctrl_mon_group_path(group_idx, "", NULL, path, DIM(path));

                /* check content of mon_groups directory */
                files_count = scandir(path, &mon_group_files, filter, NULL);
                free_scandir(mon_group_files, files_count);

                if (files_count < 0) {
                        LOG_ERROR("Could not scan %s directory!\n", path);
                        return PQOS_RETVAL_ERROR;
                } else if (files_count > 0) {
                        /* directory is not empty - monitoring is active */
                        *monitoring_status = 1;
                        return PQOS_RETVAL_OK;
                }
        } while (++group_idx < resctrl_group_count);

        *monitoring_status = 0;
        return PQOS_RETVAL_OK;
}
