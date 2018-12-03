/*
 * BSD LICENSE
 *
 * Copyright(c) 2018 Intel Corporation. All rights reserved.
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
#include "resctrl.h"
#include "resctrl_monitoring.h"
#include "resctrl_alloc.h"

#define RESCTRL_PATH_INFO_L3_MON RESCTRL_PATH_INFO"/L3_MON"

/**
 * ---------------------------------------
 * Local data structures
 * ---------------------------------------
 */
static const struct pqos_cap *m_cap = NULL;
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
 * @brief Update monitoring capability structure with supported events
 *
 * @param cap pqos capability structure
 *
 * @return Operational Status
 * @retval PQOS_RETVAL_OK on success
 */
static void
set_mon_caps(const struct pqos_cap *cap)
{
        int ret;
        unsigned i;
        const struct pqos_capability *p_cap = NULL;

        ASSERT(cap != NULL);

        /* find monitoring capability */
        ret = pqos_cap_get_type(cap, PQOS_CAP_TYPE_MON, &p_cap);
        if (ret != PQOS_RETVAL_OK)
                return;

        /* update capabilities structure */
        for (i = 0; i < p_cap->u.mon->num_events; i++) {
                struct pqos_monitor *mon = &p_cap->u.mon->events[i];

                if (supported_events & mon->type)
                        mon->os_support = PQOS_OS_MON_RESCTRL;
        }
}

int
resctrl_mon_init(const struct pqos_cpuinfo *cpu, const struct pqos_cap *cap)
{
        int ret = PQOS_RETVAL_OK;
        char buf[64];
        FILE *fd;
        struct stat st;

        ASSERT(cpu != NULL);
        ASSERT(cap != NULL);

        /**
         * Check if resctrl is mounted
         */
        if (stat(RESCTRL_PATH_INFO, &st) != 0) {
                ret = resctrl_mount(PQOS_REQUIRE_CDP_OFF, PQOS_REQUIRE_CDP_OFF,
                                    PQOS_MBA_DEFAULT);
                if (ret != PQOS_RETVAL_OK) {
                        LOG_INFO("Unable to mount resctrl\n");
                        return PQOS_RETVAL_RESOURCE;
                }
        }

        /**
         * Resctrl monitiring not supported
         */
        if (stat(RESCTRL_PATH_INFO_L3_MON, &st) != 0)
                return PQOS_RETVAL_OK;

        /**
         * Discover supported events
         */
        fd = fopen(RESCTRL_PATH_INFO_L3_MON"/mon_features", "r");
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
                        LOG_INFO("Detected resctrl support "
                                "Total Memory B/W\n");
                        supported_events |= PQOS_MON_EVENT_TMEM_BW;
                }
        }

        if ((supported_events & PQOS_MON_EVENT_LMEM_BW) &&
                (supported_events & PQOS_MON_EVENT_TMEM_BW))
                supported_events |= PQOS_MON_EVENT_RMEM_BW;

        fclose(fd);

        /**
         * Update mon capabilities
         */
        set_mon_caps(cap);

        m_cap = cap;
        m_cpu = cpu;

        return ret;
}

int
resctrl_mon_fini(void)
{
        m_cap = NULL;
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

        ASSERT(class_id != NULL);

        ret = resctrl_alloc_get_grps_num(m_cap, &max_cos);
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

        ASSERT(class_id != NULL);

        ret = resctrl_alloc_get_grps_num(m_cap, &max_cos);
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
        if (resctrl_group == NULL) {
                if (class_id == 0)
                        snprintf(buf, buf_size, RESCTRL_PATH);
                else
                        snprintf(buf, buf_size, RESCTRL_PATH"/COS%u",
                                 class_id);
        /* mon group for COS 0 */
        } else if (class_id == 0)
                snprintf(buf, buf_size, RESCTRL_PATH"/mon_groups/%s",
                         resctrl_group);
        /* mon group for the other classes */
        else
                snprintf(buf, buf_size, RESCTRL_PATH"/COS%u/mon_groups/%s",
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

        fd = fopen(path, "w");
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

        fd = fopen(path, "r");
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
 * @param [in] event resctrl mon event
 * @param [out] value counter value
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK on success
 */
static int
resctrl_mon_read_counters(const unsigned class_id,
                 const char *resctrl_group,
                 const enum pqos_mon_event event,
                 uint64_t *value)
{
        int ret = PQOS_RETVAL_OK;
        unsigned *sockets = NULL;
        unsigned sockets_num;
        unsigned socket;
        char buf[128];
        const char *name;

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

        sockets = pqos_cpu_get_sockets(m_cpu, &sockets_num);
        if (sockets == NULL) {
                ret = PQOS_RETVAL_ERROR;
                goto resctrl_mon_read_exit;
        }

        for (socket = 0; socket < sockets_num; socket++) {
                char path[PATH_MAX];
                FILE *fd;
                unsigned long long counter;

                snprintf(path, sizeof(path), "%s/mon_data/mon_L3_%02u/%s", buf,
                         sockets[socket], name);
                fd = fopen(path, "r");
                if (fd == NULL) {
                        ret = PQOS_RETVAL_ERROR;
                        goto resctrl_mon_read_exit;
                }
                if (fscanf(fd, "%llu", &counter) == 1)
                        *value += counter;
                fclose(fd);
        }

 resctrl_mon_read_exit:
        if (sockets != NULL)
                free(sockets);

        return ret;
}

/**
 * @brief Check if mon group is empty (no cores/tasks assigned)
 *
 * @param [in] class_id COS id
 * @param [in] resctrl_group mon group name
 * @param [out] empty 1 when group is empty
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK on success
 */
static int
resctrl_mon_empty(const unsigned class_id,
                  const char *resctrl_group,
                  int *empty)
{
        int ret;
        FILE *fd;
        char path[128];
        char buf[128];
        struct resctrl_cpumask mask;
        unsigned i;
        unsigned max_threshold_occupancy;
        uint64_t value;

        ASSERT(resctrl_group != NULL);
        ASSERT(empty != NULL);

        *empty = 1;

        /*
         * Some cores are assigned to group?
         */
        ret = resctrl_mon_cpumask_read(class_id, resctrl_group, &mask);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        for (i = 0; i < sizeof(mask.tab); i++)
                if (mask.tab[i] > 0) {
                        *empty = 0;
                        return PQOS_RETVAL_OK;
                }

        /*
         *Some tasks are assigned to group?
         */
        resctrl_mon_group_path(class_id, resctrl_group, "/tasks", path,
                               sizeof(path));
        fd = fopen(path, "r");
        if (fd == NULL)
                return PQOS_RETVAL_ERROR;
        /* Search tasks file for any task ID */
        memset(buf, 0, sizeof(buf));
        if (fgets(buf, sizeof(buf), fd) != NULL) {
                *empty = 0;
                fclose(fd);
                return PQOS_RETVAL_OK;
        }
        fclose(fd);

        /*
         * Check if llc occupancy is lower than max_occupancy_threshold
         */
        if ((supported_events & PQOS_MON_EVENT_L3_OCCUP) == 0)
                return PQOS_RETVAL_OK;

        ret = resctrl_mon_read_counters(class_id, resctrl_group,
                                        PQOS_MON_EVENT_L3_OCCUP, &value);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        fd = fopen(RESCTRL_PATH"/info/L3_MON/max_threshold_occupancy", "r");
        if (fd == NULL)
                return PQOS_RETVAL_ERROR;
        if (fscanf(fd, "%u", &max_threshold_occupancy) != 1)
                ret = PQOS_RETVAL_ERROR;
        else if (value > max_threshold_occupancy)
                *empty = 0;
        fclose(fd);

        return PQOS_RETVAL_OK;
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
        free(namelist);

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
                          "group\n", lcore);

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
                         "group %s does not exists\n", buf);
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

                fd = fopen(path, "r");
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
        free(namelist);

        return ret;
}

int
resctrl_mon_assoc_set_pid(const pid_t task, const char *name)
{
        unsigned class_id;
        int ret;
        char path[128];
        FILE *fd;

        ASSERT(name != NULL);

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
        fd = fopen(path, "w");
        if (fd == NULL)
                return PQOS_RETVAL_ERROR;

        fprintf(fd, "%d\n", task);

        if (fclose(fd) != 0) {
                LOG_ERROR("Could not assign TID %d to resctrl monitoring "
                          "group\n", task);
                return PQOS_RETVAL_ERROR;
        }

        return PQOS_RETVAL_OK;
}

int
resctrl_mon_start(struct pqos_mon_data *group)
{
        char *resctrl_group = NULL;
        char buf[128];
        int ret = PQOS_RETVAL_OK;
        unsigned i;

        ASSERT(group != NULL);
        ASSERT(group->tid_nr == 0 || group->tid_map != NULL);
        ASSERT(group->num_cores == 0 || group->cores != NULL);

        /**
         * Create new monitoring gorup
         */
        if (group->resctrl_mon_group == NULL) {
                snprintf(buf, sizeof(buf), "%d-%u",
                        (int)getpid(), resctrl_mon_counter++);

                resctrl_group = strdup(buf);
                if (resctrl_group == NULL) {
                        LOG_DEBUG("Memory allocation failed\n");
                        ret = PQOS_RETVAL_ERROR;
                        goto resctrl_mon_start_exit;
                }
        /**
         * Reuse group
         */
        } else
                resctrl_group = group->resctrl_mon_group;

        /**
         * Add pids to the resctrl group
         */
        for (i = 0; i < group->tid_nr; i++) {
                ret = resctrl_mon_assoc_set_pid(group->tid_map[i],
                                                resctrl_group);
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

        group->resctrl_mon_group = resctrl_group;

 resctrl_mon_start_exit:
        if (ret != PQOS_RETVAL_OK && group->resctrl_mon_group != resctrl_group)
                free(resctrl_group);

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
        unsigned cos;
        unsigned i;

        ASSERT(group != NULL);

        ret = resctrl_alloc_get_grps_num(m_cap, &max_cos);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        if (group->resctrl_mon_group != NULL) {
                cos = 0;
                do {
                        char buf[128];

                        resctrl_mon_group_path(cos, group->resctrl_mon_group,
                                               NULL, buf, sizeof(buf));

                        ret = resctrl_mon_rmdir(buf);
                        if (ret != PQOS_RETVAL_OK) {
                                LOG_ERROR("Failed to remove resctrl "
                                          "monitoring group\n");
                                goto resctrl_mon_stop_exit;
                        }
                } while (++cos < max_cos);

                free(group->resctrl_mon_group);
                group->resctrl_mon_group = NULL;

        } else if (group->num_pids > 0) {
                /**
                 * Add pids to the default group
                 */
                for (i = 0; i < group->tid_nr; i++) {
                        const pid_t tid = group->tid_map[i];

                        if (resctrl_alloc_task_validate(tid) !=
                            PQOS_RETVAL_OK) {
                                LOG_DEBUG("resctrl_mon_stop: Skipping "
                                          "non-existent PID: %d\n", tid);
                                continue;
                        }
                        ret = resctrl_mon_assoc_set_pid(tid, NULL);
                        if (ret != PQOS_RETVAL_OK)
                                goto resctrl_mon_stop_exit;
                }

        } else
                return PQOS_RETVAL_PARAM;

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

        ASSERT(group != NULL);
        ASSERT(m_cap != NULL);

        ret = resctrl_alloc_get_grps_num(m_cap, &max_cos);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        cos = 0;
        do {
                int empty;
                struct stat st;
                uint64_t value;
                char buf[128];

                resctrl_mon_group_path(cos, group->resctrl_mon_group, NULL, buf,
                                       sizeof(buf));
                if (stat(buf, &st) != 0)
                        continue;

                ret = resctrl_mon_empty(cos, group->resctrl_mon_group, &empty);
                if (ret != PQOS_RETVAL_OK)
                        return ret;

                if (!empty)
                        continue;

                /* store counter values */
                if (supported_events & PQOS_MON_EVENT_LMEM_BW) {
                        ret = resctrl_mon_read_counters(cos,
                                group->resctrl_mon_group,
                                PQOS_MON_EVENT_LMEM_BW, &value);
                        if (ret != PQOS_RETVAL_OK)
                                return ret;
                        group->resctrl_values_storage.mbm_local += value;
                }
                if (supported_events & PQOS_MON_EVENT_TMEM_BW) {
                        ret = resctrl_mon_read_counters(cos,
                                group->resctrl_mon_group,
                                PQOS_MON_EVENT_TMEM_BW, &value);
                        if (ret != PQOS_RETVAL_OK)
                                return ret;

                        group->resctrl_values_storage.mbm_total += value;
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

        ASSERT(group != NULL);
        ASSERT(m_cap != NULL);

        ret = resctrl_alloc_get_grps_num(m_cap, &max_cos);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        /*
         * When core COS assoc changes then kernel resets monitoring group
         * assoc. We need to restore monitoring assoc for cores
         */
        for (i = 0; i < group->num_cores; i++) {
                ret = resctrl_mon_assoc_restore(group->cores[i],
                                                group->resctrl_mon_group);
                if (ret != PQOS_RETVAL_OK)
                        goto resctrl_mon_poll_exit;
        }

        /* Search COSes for given resctrl mon group */
        cos = 0;
        do {
                uint64_t val;
                struct stat st;
                char buf[128];

                resctrl_mon_group_path(cos, group->resctrl_mon_group, NULL, buf,
                                       sizeof(buf));
                if (stat(buf, &st) != 0)
                        continue;

                ret = resctrl_mon_read_counters(cos, group->resctrl_mon_group,
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
                        value + group->resctrl_values_storage.mbm_local;
                group->values.mbm_local_delta =
                        get_delta(old_value, group->values.mbm_local);
                break;
        case PQOS_MON_EVENT_TMEM_BW:
                old_value = group->values.mbm_total;
                group->values.mbm_total =
                        value + group->resctrl_values_storage.mbm_total;
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

        if (supported_events == 0)
                return PQOS_RETVAL_RESOURCE;

        ret = resctrl_alloc_get_grps_num(m_cap, &grps);
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
                                  "COS %u\n", cos);
                        return PQOS_RETVAL_ERROR;
                }

                for (i = 0; i < num_groups; i++) {
                        char path[256];

                        resctrl_mon_group_path(cos, namelist[i]->d_name, NULL,
                                               path, sizeof(path));

                        ret = resctrl_mon_rmdir(path);
                        if (ret != PQOS_RETVAL_OK) {
                                free(namelist);
                                return ret;
                        }
                }

                free(namelist);
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

	if (supported_events == 0) {
		*monitoring_status = 0;
		return PQOS_RETVAL_OK;
	}

	ret = resctrl_alloc_get_grps_num(m_cap, &resctrl_group_count);
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
		free(mon_group_files);

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
