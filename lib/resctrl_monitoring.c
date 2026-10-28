/*
 * BSD LICENSE
 *
 * Copyright(c) 2018-2026 Intel Corporation. All rights reserved.
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

#include "resctrl_monitoring.h"

#include "cap.h"
#include "common.h"
#include "log.h"
#include "monitoring.h"
#include "resctrl.h"
#include "resctrl_alloc.h"

#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define GROUP_NAME_PREFIX "pqos-"

/**
 * ---------------------------------------
 * Local data structures
 * ---------------------------------------
 */
static int supported_events = 0;

static unsigned resctrl_mon_counter = 0;

/**
 * The core groups this process has started. A directory lists only the cores
 * that are online in it, so what the library tells from the cores a directory
 * lists - the L3s it is in use on, and whether another group is in it - it
 * tells from these as well: a group whose cores are offline is in its
 * directory all the same. An entry is a copy, and \a group only tells it
 * apart, since a group can be freed without its stop being reached
 */
static struct resctrl_mon_started {
        const struct pqos_mon_data *group; /**< the group, never read */
        char name[32];                     /**< its directory's name */
        uint64_t l3ids;                    /**< the L3s of its cores */
        int l3ids_valid; /**< 0 for an L3 ID above RESCTRL_CORE_MAX_L3ID */
} *mon_started = NULL;
static unsigned mon_started_num = 0;
static unsigned mon_started_size = 0;

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
 * @brief Copy a monitoring group name to a caller-provided buffer
 *
 * @param [out] dst destination buffer
 * @param [in] dst_size destination buffer size
 * @param [in] src monitoring group name
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK on success
 * @retval PQOS_RETVAL_PARAM when the destination buffer is too small
 */
static int
resctrl_mon_name_copy(char *dst, const unsigned dst_size, const char *src)
{
        const size_t src_size = strlen(src) + 1;

        if (src_size > dst_size)
                return PQOS_RETVAL_PARAM;

        memcpy(dst, src, src_size);

        return PQOS_RETVAL_OK;
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
        int have_monitoring_info = 0;
        char buf[64];
        FILE *fd;

        UNUSED_PARAM(cap);
        UNUSED_PARAM(cpu);

        supported_events = 0;

        /**
         * Resctrl monitoring not supported
         */
        if (!pqos_dir_exists(RESCTRL_PATH_INFO_L3_MON) &&
            !pqos_dir_exists(RESCTRL_PATH_INFO_PERF_PKG_MON))
                return PQOS_RETVAL_OK;

        /**
         * Discover classic L3_MON supported events
         */
        if (pqos_dir_exists(RESCTRL_PATH_INFO_L3_MON)) {
                fd = pqos_fopen(RESCTRL_PATH_INFO_L3_MON "/mon_features", "r");
                if (fd == NULL) {
                        LOG_ERROR("Failed to obtain resctrl monitoring "
                                  "features\n");
                        return PQOS_RETVAL_ERROR;
                }

                have_monitoring_info = 1;

                while (fgets(buf, sizeof(buf), fd) != NULL) {
                        if (strcmp(buf, "llc_occupancy\n") == 0) {
                                LOG_INFO("Detected resctrl support for "
                                         "LLC Occupancy\n");
                                supported_events |= PQOS_MON_EVENT_L3_OCCUP;
                                continue;
                        }

                        if (strcmp(buf, "mbm_local_bytes\n") == 0) {
                                LOG_INFO("Detected resctrl support for "
                                         "Local Memory B/W\n");
                                supported_events |= PQOS_MON_EVENT_LMEM_BW;
                                continue;
                        }

                        if (strcmp(buf, "mbm_total_bytes\n") == 0) {
                                LOG_INFO("Detected resctrl support for "
                                         "Total Memory B/W\n");
                                supported_events |= PQOS_MON_EVENT_TMEM_BW;
                        }
                }

                if ((supported_events & PQOS_MON_EVENT_LMEM_BW) &&
                    (supported_events & PQOS_MON_EVENT_TMEM_BW))
                        supported_events |= PQOS_MON_EVENT_RMEM_BW;

                pqos_fclose(fd);
        }

        /**
         * Discover PERF_PKG_MON telemetry events
         */
        if (pqos_dir_exists(RESCTRL_PATH_INFO_PERF_PKG_MON)) {
                fd = pqos_fopen(RESCTRL_PATH_INFO_PERF_PKG_MON "/mon_features",
                                "r");
                if (fd == NULL) {
                        LOG_ERROR("Failed to obtain PERF_PKG monitoring "
                                  "features\n");
                        return PQOS_RETVAL_ERROR;
                }

                have_monitoring_info = 1;

                while (fgets(buf, sizeof(buf), fd) != NULL) {
                        if (strcmp(buf, "core_energy\n") == 0) {
                                LOG_INFO("Detected resctrl support for "
                                         "Core Energy telemetry\n");
                                supported_events |= PQOS_MON_EVENT_CORE_ENERGY;
                                supported_events |= PQOS_MON_EVENT_POWER;
                                continue;
                        }

                        if (strcmp(buf, "activity\n") == 0) {
                                LOG_INFO("Detected resctrl support for "
                                         "Activity telemetry\n");
                                supported_events |= PQOS_MON_EVENT_ACTIVITY;
                        }
                }

                pqos_fclose(fd);
        }

        if (!have_monitoring_info)
                return PQOS_RETVAL_OK;
        return ret;
}

int
resctrl_mon_fini(void)
{
        supported_events = 0;

        free(mon_started);
        mon_started = NULL;
        mon_started_num = 0;
        mon_started_size = 0;

        return PQOS_RETVAL_OK;
}

/**
 * @brief Get core association with ctrl group
 *
 * @param [in] lcore core id
 * @param [out] class_id CLOS id
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK on success
 * @retval PQOS_RETVAL_RESOURCE if no class holds the core, as none holds an
 *         offline one, which is not said here: the callers say what it is to
 *         them
 */
static int
alloc_assoc_get(const unsigned lcore, unsigned *class_id)
{
        int ret;
        unsigned max_clos;
        const struct pqos_cap *cap = _pqos_get_cap();

        ASSERT(class_id != NULL);

        ret = resctrl_alloc_get_grps_num(cap, &max_clos);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        /* with no class but the default group, a core is in it, unless it
         * is offline: the kernel takes an offline CPU out of every group
         */
        if (max_clos == 0) {
                struct resctrl_cpumask mask;

                ret = resctrl_alloc_cpumask_read(0, &mask);
                if (ret != PQOS_RETVAL_OK) {
                        LOG_ERROR("Failed to retrieve core %u association\n",
                                  lcore);
                        return ret;
                }
                if (!resctrl_cpumask_get(lcore, &mask))
                        return PQOS_RETVAL_RESOURCE;
                *class_id = 0;
                return PQOS_RETVAL_OK;
        }

        ret = resctrl_alloc_assoc_get(lcore, class_id);
        if (ret != PQOS_RETVAL_OK && ret != PQOS_RETVAL_RESOURCE)
                LOG_ERROR("Failed to retrieve core %u association\n", lcore);

        return ret;
}

/**
 * @brief Get task association with ctrl group
 *
 * @param [in] tid task id
 * @param [out] class_id CLOS id
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK on success
 */
static int
alloc_assoc_get_pid(const pid_t tid, unsigned *class_id)
{
        int ret;
        unsigned max_clos;
        const struct pqos_cap *cap = _pqos_get_cap();

        ASSERT(class_id != NULL);

        ret = resctrl_alloc_get_grps_num(cap, &max_clos);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        if (max_clos == 0) {
                *class_id = 0;
                return PQOS_RETVAL_OK;
        }

        ret = resctrl_alloc_assoc_get_pid(tid, class_id);
        if (ret == PQOS_RETVAL_PARAM)
                LOG_DEBUG("Cannot retrieve association for task %d: "
                          "task no longer exists (benign race with "
                          "concurrent exit, e.g. during a global "
                          "allocation reset).\n",
                          tid);
        else if (ret != PQOS_RETVAL_OK)
                LOG_ERROR("Failed to retrieve task %d association\n", tid);

        return ret;
}

/**
 * @brief Obtain path to monitoring group
 *
 * @param [in] class_id CLOS id
 * @param [in] resctrl_group mon group name
 * @param [in] file file name inside group, or NULL for the group itself
 * @param [out] buf Buffer to store path
 * @param [in] buf_size buffer size
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK on success
 */
static int
resctrl_mon_group_path(const unsigned class_id,
                       const char *resctrl_group,
                       const char *file,
                       char *buf,
                       const unsigned buf_size)
{
        const char *suffix = (file != NULL) ? file : "";
        int len;

        ASSERT(buf != NULL);

        /* Group name not set - get path to mon_groups directory */
        if (resctrl_group == NULL && class_id == 0)
                len = snprintf(buf, buf_size, RESCTRL_PATH "%s", suffix);
        else if (resctrl_group == NULL)
                len = snprintf(buf, buf_size, RESCTRL_PATH "/COS%u%s", class_id,
                               suffix);

        /* mon group for CLOS 0 */
        else if (class_id == 0)
                len = snprintf(buf, buf_size, RESCTRL_PATH "/mon_groups/%s%s",
                               resctrl_group, suffix);
        /* mon group for the other classes */
        else
                len = snprintf(buf, buf_size,
                               RESCTRL_PATH "/COS%u/mon_groups/%s%s", class_id,
                               resctrl_group, suffix);

        if (len < 0 || len >= (int)buf_size)
                return PQOS_RETVAL_ERROR;

        return PQOS_RETVAL_OK;
}

/**
 * @brief Write CPU mask to file
 *
 * @param [in] class_id CLOS id
 * @param [in] resctrl_group mon group name
 * @param [in] mask CPU mask to write
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK on success
 */
PQOS_STATIC int
resctrl_mon_cpumask_write(const unsigned class_id,
                          const char *resctrl_group,
                          const struct resctrl_cpumask *mask)
{
        FILE *fd;
        char path[128];
        int ret;

        ASSERT(mask != NULL);

        ret = resctrl_mon_group_path(class_id, resctrl_group, "/cpus", path,
                                     sizeof(path));
        if (ret != PQOS_RETVAL_OK)
                return ret;

        fd = pqos_fopen(path, "w");
        if (fd == NULL)
                return PQOS_RETVAL_ERROR;

        ret = resctrl_cpumask_write(fd, mask);

        if (pqos_fclose(fd) != 0)
                return PQOS_RETVAL_ERROR;

        return ret;
}

/**
 * @brief Read CPU mask from file
 *
 * @param [in] class_id CLOS id
 * @param [in] resctrl_group mon group name
 * @param [out] mask CPU mask to write
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK on success
 */
PQOS_STATIC int
resctrl_mon_cpumask_read(const unsigned class_id,
                         const char *resctrl_group,
                         struct resctrl_cpumask *mask)
{
        FILE *fd;
        char path[128];
        int ret;

        ASSERT(mask != NULL);

        ret = resctrl_mon_group_path(class_id, resctrl_group, "/cpus", path,
                                     sizeof(path));
        if (ret != PQOS_RETVAL_OK)
                return ret;

        fd = pqos_fopen(path, "r");
        if (fd == NULL)
                return PQOS_RETVAL_ERROR;

        ret = resctrl_cpumask_read(fd, mask);

        pqos_fclose(fd);

        return ret;
}

/**
 * @brief Read counter value
 *
 * Parses the complete contents of a resctrl monitoring counter file.
 * Distinguishes valid numeric values (including zero) from the kernel's
 * non-numeric status strings:
 *   - "Unassigned": no hardware counter assigned -> PQOS_RETVAL_UNAVAILABLE
 *   - "Unavailable": counter temporarily unavailable -> PQOS_RETVAL_UNAVAILABLE
 *   - "Error": hardware read error -> PQOS_RETVAL_ERROR
 *   - empty, malformed, overflowing, or trailing-junk -> PQOS_RETVAL_ERROR
 *
 * @param [in] class_id CLOS id
 * @param [in] resctrl_group mon group name
 * @param [in] l3id l3id to read from
 * @param [in] event resctrl mon event
 * @param [out] value counter value (only valid when PQOS_RETVAL_OK)
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK on success (valid numeric value in *value)
 * @retval PQOS_RETVAL_UNAVAILABLE counter is Unassigned or Unavailable
 * @retval PQOS_RETVAL_ERROR I/O failure, Error state, or malformed content
 * @retval PQOS_RETVAL_PARAM unknown event
 */
PQOS_STATIC int
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
        int len;
        char line[64];
        char *endptr;
        unsigned long long parsed;

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

        if (resctrl_mon_group_path(class_id, resctrl_group, NULL, buf,
                                   sizeof(buf)) != PQOS_RETVAL_OK)
                return PQOS_RETVAL_ERROR;
        len = snprintf(path, sizeof(path), "%s/mon_data/mon_L3_%02u/%s", buf,
                       l3id, name);
        if (len < 0 || len >= (int)sizeof(path))
                return PQOS_RETVAL_ERROR;
        fd = pqos_fopen(path, "r");
        if (fd == NULL)
                return PQOS_RETVAL_ERROR;

        if (fgets(line, sizeof(line), fd) == NULL) {
                pqos_fclose(fd);
                LOG_ERROR("Failed to read resctrl counter: %s\n", path);
                return PQOS_RETVAL_ERROR;
        }
        pqos_fclose(fd);

        /* Check for kernel non-numeric status strings */
        if (strcmp(line, "Unassigned\n") == 0 ||
            strcmp(line, "Unassigned") == 0) {
                LOG_DEBUG("resctrl counter Unassigned: %s\n", path);
                return PQOS_RETVAL_UNAVAILABLE;
        }
        if (strcmp(line, "Unavailable\n") == 0 ||
            strcmp(line, "Unavailable") == 0) {
                LOG_DEBUG("resctrl counter Unavailable: %s\n", path);
                return PQOS_RETVAL_UNAVAILABLE;
        }
        if (strcmp(line, "Error\n") == 0 || strcmp(line, "Error") == 0) {
                LOG_ERROR("resctrl counter Error: %s\n", path);
                return PQOS_RETVAL_ERROR;
        }

        /* Parse complete line as an unsigned integer */
        /* Reject leading sign characters that strtoull() would silently accept
         */
        {
                const char *p = line;

                while (*p == ' ' || *p == '\t')
                        p++;
                if (*p == '+' || *p == '-') {
                        LOG_ERROR("Failed to parse resctrl counter: %s\n",
                                  path);
                        return PQOS_RETVAL_ERROR;
                }
        }
        errno = 0;
        parsed = strtoull(line, &endptr, 10);
        if (errno != 0 || endptr == line ||
            (*endptr != '\n' && *endptr != '\0')) {
                LOG_ERROR("Failed to parse resctrl counter: %s\n", path);
                return PQOS_RETVAL_ERROR;
        }
        /* Guard against unsigned long long being wider than uint64_t */
        if (parsed > UINT64_MAX) {
                LOG_ERROR("Failed to parse resctrl counter: %s\n", path);
                return PQOS_RETVAL_ERROR;
        }

        *value = (uint64_t)parsed;
        return PQOS_RETVAL_OK;
}

/**
 * @brief Build PERF_PKG domain path for a telemetry file
 */
static int
resctrl_mon_perf_pkg_path(const unsigned class_id,
                          const char *resctrl_group,
                          const unsigned pkgid,
                          const char *filename,
                          char *path,
                          const unsigned path_size)
{
        char buf[128];
        int ret;
        int len;

        ret = resctrl_mon_group_path(class_id, resctrl_group, NULL, buf,
                                     sizeof(buf));
        if (ret != PQOS_RETVAL_OK)
                return ret;
        len = snprintf(path, path_size, "%s/mon_data/mon_PERF_PKG_%02u/%s", buf,
                       pkgid, filename);
        if (len < 0 || len >= (int)path_size)
                return PQOS_RETVAL_ERROR;

        return PQOS_RETVAL_OK;
}

/**
 * @brief Read a floating-point value from a telemetry file
 */
static int
resctrl_mon_read_double(const char *path, double *value)
{
        FILE *fd;
        int ret = PQOS_RETVAL_OK;

        ASSERT(path != NULL);
        ASSERT(value != NULL);

        *value = 0.0;

        fd = pqos_fopen(path, "r");
        if (fd == NULL)
                return PQOS_RETVAL_ERROR;
        if (fscanf(fd, "%lf", value) != 1) {
                *value = 0.0;
                ret = PQOS_RETVAL_ERROR;
        }
        pqos_fclose(fd);

        return ret;
}

/**
 * @brief Read and aggregate telemetry event across package domains
 */
static int
resctrl_mon_read_tel_pkgs(const unsigned class_id,
                          const char *resctrl_group,
                          const char *filename,
                          const unsigned *pkgids,
                          const unsigned num_pkgids,
                          double *value)
{
        unsigned i;
        int ret = PQOS_RETVAL_OK;

        ASSERT(resctrl_group != NULL);
        ASSERT(pkgids != NULL);
        ASSERT(value != NULL);

        *value = 0.0;

        for (i = 0; i < num_pkgids; i++) {
                char path[PATH_MAX];
                double pkg_val = 0.0;

                ret = resctrl_mon_perf_pkg_path(class_id, resctrl_group,
                                                pkgids[i], filename, path,
                                                sizeof(path));
                if (ret != PQOS_RETVAL_OK)
                        return ret;

                ret = resctrl_mon_read_double(path, &pkg_val);
                if (ret != PQOS_RETVAL_OK)
                        return ret;

                *value += pkg_val;
        }

        return ret;
}

/**
 * @brief Read counter value from requested \a l3ids
 *
 * Aggregates the counter value across all l3ids.  If any domain returns
 * PQOS_RETVAL_UNAVAILABLE the partial sum is discarded and
 * PQOS_RETVAL_UNAVAILABLE is returned.  Hard errors propagate immediately.
 *
 * @param [in] class_id CLOS id
 * @param [in] resctrl_group mon group name
 * @param [in] l3ids l3ids to read from
 * @param [in] l3ids_num number of l3ids
 * @param [in] event resctrl monitoring event
 * @param [out] value counter value (only valid when PQOS_RETVAL_OK)
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK on success
 * @retval PQOS_RETVAL_UNAVAILABLE any domain is Unassigned or Unavailable
 * @retval PQOS_RETVAL_ERROR I/O or parse failure
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
        const struct pqos_cpuinfo *cpu = _pqos_get_cpu();

        ASSERT(resctrl_group != NULL);
        ASSERT(value != NULL);

        *value = 0;

        if (l3ids == NULL) {
                l3cat_ids = pqos_cpu_get_l3cat_ids(cpu, &l3cat_id_num);
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
                if (ret == PQOS_RETVAL_UNAVAILABLE) {
                        /* Discard partial sum; propagate unavailability */
                        *value = 0;
                        goto resctrl_mon_read_exit;
                }
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
        pqos_fclose(fd);

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
 * @param [in] class_id CLOS id
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
        ret = resctrl_mon_group_path(class_id, resctrl_group, "/cpus_list",
                                     path, sizeof(path));
        if (ret != PQOS_RETVAL_OK)
                return ret;
        retval = resctrl_mon_file_empty(path);
        if (retval < 0)
                return PQOS_RETVAL_ERROR;
        else if (!retval) {
                *empty = 0;
                return PQOS_RETVAL_OK;
        }

        /*
         * Some tasks are assigned to group?
         */
        ret = resctrl_mon_group_path(class_id, resctrl_group, "/tasks", path,
                                     sizeof(path));
        if (ret != PQOS_RETVAL_OK)
                return ret;
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
        if (!resctrl_mon_is_event_supported(PQOS_MON_EVENT_L3_OCCUP))
                return PQOS_RETVAL_OK;

        ret = resctrl_mon_read_counters(class_id, resctrl_group, l3id, l3id_num,
                                        PQOS_MON_EVENT_L3_OCCUP, &value);
        if (ret == PQOS_RETVAL_UNAVAILABLE) {
                /*
                 * Cannot determine LLC occupancy; treat group as non-empty to
                 * avoid deleting a group whose occupancy is simply unavailable.
                 */
                *empty = 0;
                return PQOS_RETVAL_OK;
        }
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
 * @brief Create monitoring group directory if not exists
 *
 * @param [in] class_id CLOS id
 * @param[in] name mon group name
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 */
PQOS_STATIC int
resctrl_mon_mkdir(const unsigned class_id, const char *name)
{
        char path[128];
        int mkdir_ret;

        ASSERT(name != NULL);

        if (resctrl_mon_group_path(class_id, name, NULL, path, sizeof(path)) !=
            PQOS_RETVAL_OK)
                return PQOS_RETVAL_ERROR;

        mkdir_ret = mkdir(path, 0755);
        if (mkdir_ret == -1 && errno != EEXIST) {
                const int err = errno;

                LOG_ERROR("Failed to create resctrl monitoring group %s: "
                          "errno %d (%s)\n",
                          path, err, strerror(err));
                return PQOS_RETVAL_BUSY;
        }

        return PQOS_RETVAL_OK;
}

/**
 * @brief Remove monitoring group directory if exists
 *
 * @param [in] class_id CLOS id
 * @param[in] name mon group name
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 */
PQOS_STATIC int
resctrl_mon_rmdir(const unsigned class_id, const char *name)
{
        char path[128];

        ASSERT(name != NULL);

        if (resctrl_mon_group_path(class_id, name, NULL, path, sizeof(path)) !=
            PQOS_RETVAL_OK)
                return PQOS_RETVAL_ERROR;

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
        unsigned class_id = 0;
        char dir[256];
        struct dirent **namelist = NULL;
        int num_groups;
        int i;

        ASSERT(name != NULL);

        if (!resctrl_mon_is_supported())
                return PQOS_RETVAL_RESOURCE;

        /* a core in no class is in no group either, which the RESOURCE
         * returned says, as it does below for a core in a class
         */
        ret = alloc_assoc_get(lcore, &class_id);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        ret = resctrl_mon_group_path(class_id, "", NULL, dir, sizeof(dir));
        if (ret != PQOS_RETVAL_OK)
                return ret;
        num_groups = scandir(dir, &namelist, filter, NULL);
        if (num_groups < 0) {
                LOG_ERROR("Failed to read monitoring groups for CLOS %u\n",
                          class_id);
                return PQOS_RETVAL_ERROR;
        }

        for (i = 0; i < num_groups; i++) {
                struct resctrl_cpumask mask;
                const char *d_name = namelist[i]->d_name;

                if (d_name == NULL)
                        continue;

                ret = resctrl_mon_cpumask_read(class_id, d_name, &mask);
                if (ret != PQOS_RETVAL_OK)
                        goto resctrl_mon_assoc_get_exit;

                if (resctrl_cpumask_get(lcore, &mask)) {
                        ret = resctrl_mon_name_copy(name, name_size, d_name);
                        goto resctrl_mon_assoc_get_exit;
                }
        }

        /* Core not associated with mon group */
        ret = PQOS_RETVAL_RESOURCE;

resctrl_mon_assoc_get_exit:
        free_scandir(namelist, num_groups);

        return ret;
}

/**
 * @brief Puts \a lcore in monitoring group \a name of class \a class_id,
 *        making the group's directory there if it has none
 *
 * @param [in] class_id class of service the core is in
 * @param [in] lcore core to put in the group
 * @param [in] name name of the monitoring group
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK on success
 */
static int
mon_assoc_add(const unsigned class_id, const unsigned lcore, const char *name)
{
        int ret;
        struct resctrl_cpumask cpumask;

        ret = resctrl_mon_mkdir(class_id, name);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        ret = resctrl_mon_cpumask_read(class_id, name, &cpumask);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        resctrl_cpumask_set(lcore, &cpumask);

        return resctrl_mon_cpumask_write(class_id, name, &cpumask);
}

int
resctrl_mon_assoc_set(const unsigned lcore, const char *name)
{
        unsigned class_id = 0;
        int ret;

        ASSERT(name != NULL);

        if (!resctrl_mon_is_supported())
                return PQOS_RETVAL_RESOURCE;

        ret = alloc_assoc_get(lcore, &class_id);
        if (ret == PQOS_RETVAL_RESOURCE)
                LOG_ERROR("Could not assign core %u to resctrl monitoring "
                          "group: it is in no class, as an offline core is\n",
                          lcore);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        ret = mon_assoc_add(class_id, lcore, name);
        if (ret != PQOS_RETVAL_OK)
                LOG_ERROR("Could not assign core %u to resctrl monitoring "
                          "group\n",
                          lcore);

        return ret;
}

/**
 * @brief Whether monitoring group \a name has a directory in a class other
 *        than \a class_id
 *
 * @param [in] class_id the class not looked in
 * @param [in] name name of the monitoring group
 * @param [out] elsewhere 1 for a directory in another class
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK on success
 */
static int
mon_dir_elsewhere(const unsigned class_id, const char *name, int *elsewhere)
{
        const struct pqos_cap *cap = _pqos_get_cap();
        unsigned max_clos;
        unsigned clos;
        int ret;

        *elsewhere = 0;

        ret = resctrl_alloc_get_grps_num(cap, &max_clos);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        for (clos = 0; clos < max_clos; clos++) {
                char path[128];

                if (clos == class_id)
                        continue;
                ret = resctrl_mon_group_path(clos, name, NULL, path,
                                             sizeof(path));
                if (ret != PQOS_RETVAL_OK)
                        return ret;
                if (pqos_dir_exists(path)) {
                        *elsewhere = 1;
                        break;
                }
        }

        return PQOS_RETVAL_OK;
}

/* @brief Restore association of \a lcore to monitoring group
 *
 * @param [in] lcore CPU logical core id
 * @param [in] name name of monitoring group
 * @param [in,out] purged whether the purge removed the group's last
 *                        directory, which the restore makes again; cleared
 *                        once the group has a directory in the core's class
 *                        again, whoever made it
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK on success
 * @retval PQOS_RETVAL_RESOURCE if the group's directories were removed by
 *         other than the purge
 */
PQOS_STATIC int
resctrl_mon_assoc_restore(const unsigned lcore, const char *name, int *purged)
{
        int ret;
        char buf[RESCTRL_MON_GROUP_NAME_SIZE];
        unsigned class_id = 0;
        struct resctrl_cpumask mask;

        ASSERT(name != NULL);

        /* a core in no class, as an offline one is, has no group to be put
         * back in, and the poll goes on without it. When it comes back
         * online, in the default class, the restore puts it back in its
         * group, making the group's directory there where the purge removed
         * it
         */
        ret = alloc_assoc_get(lcore, &class_id);
        if (ret == PQOS_RETVAL_RESOURCE)
                return PQOS_RETVAL_OK;
        if (ret != PQOS_RETVAL_OK)
                return ret;

        /* the group's own directory first: a core still where the start put
         * it, as it is at nearly every poll, needs no search through the
         * class's groups, which reads a file of each and so grew with the
         * groups started. The directory there, whoever made it again, ends
         * the purge's mark: the group's last directory is not gone any more
         */
        if (resctrl_mon_cpumask_read(class_id, name, &mask) == PQOS_RETVAL_OK) {
                *purged = 0;
                if (resctrl_cpumask_get(lcore, &mask))
                        return PQOS_RETVAL_OK;
        }

        ret = resctrl_mon_assoc_get(lcore, buf, sizeof(buf));
        /* core already associated with mon group */
        if (ret == PQOS_RETVAL_OK) {
                /* its own, made again since the read above: the purge's mark
                 * ends as it does there
                 */
                if (strcmp(buf, name) == 0)
                        *purged = 0;
                return PQOS_RETVAL_OK;
        }

        /* the class again, which the search looked the core up in too: a
         * writer to resctrl, which the library's lock does not hold off, may
         * have moved the core to another since the lookup above, and the core
         * may have gone offline, which leaves nothing to restore either
         */
        ret = alloc_assoc_get(lcore, &class_id);
        if (ret == PQOS_RETVAL_RESOURCE)
                return PQOS_RETVAL_OK;
        if (ret != PQOS_RETVAL_OK)
                return ret;

        /* put back in the group. Where the group has no directory in that
         * class, one is made if the purge removed the group's last, as it
         * does once a core gone offline has left the directory empty: the
         * core comes back online in the default class. The purge saved the
         * counts of what it removed, and the group's count goes on from them.
         * One is made too if the group still has a directory in another
         * class, as when a writer to resctrl moved the core. Directories
         * removed by anything else, as by a pqos_mon_reset(), are not made
         * again: the group has gone, and the poll fails
         */
        ret = resctrl_mon_group_path(class_id, name, NULL, buf, sizeof(buf));
        if (ret != PQOS_RETVAL_OK)
                return ret;
        if (!*purged && !pqos_dir_exists(buf)) {
                int elsewhere;

                ret = mon_dir_elsewhere(class_id, name, &elsewhere);
                if (ret != PQOS_RETVAL_OK)
                        return ret;
                if (!elsewhere) {
                        LOG_WARN("Could not restore core association with mon "
                                 "group %s does not exists\n",
                                 buf);
                        return PQOS_RETVAL_RESOURCE;
                }
        }

        ret = mon_assoc_add(class_id, lcore, name);
        if (ret == PQOS_RETVAL_OK) {
                *purged = 0;
                return PQOS_RETVAL_OK;
        }

        /* a core that went offline since the lookup is refused by the
         * kernel's cpus file, and is in no class now: nothing to restore
         */
        if (alloc_assoc_get(lcore, &class_id) == PQOS_RETVAL_RESOURCE)
                return PQOS_RETVAL_OK;

        /* the kernel refuses the whole mask written for any CPU in it that is
         * offline, or no longer in the class: another core of the directory
         * may have gone since the add read the mask. The add reads it again,
         * once, and that read lists no such core
         */
        ret = mon_assoc_add(class_id, lcore, name);
        if (ret == PQOS_RETVAL_OK) {
                *purged = 0;
                return PQOS_RETVAL_OK;
        }

        LOG_ERROR("Could not assign core %u to resctrl monitoring group\n",
                  lcore);
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

        if (!resctrl_mon_is_supported())
                return PQOS_RETVAL_RESOURCE;

        ret = alloc_assoc_get_pid(task, &class_id);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        ret = resctrl_mon_group_path(class_id, "", NULL, dir, sizeof(dir));
        if (ret != PQOS_RETVAL_OK)
                return ret;
        num_groups = scandir(dir, &namelist, filter, NULL);
        if (num_groups < 0) {
                LOG_ERROR("Failed to read monitoring groups for CLOS %u\n",
                          class_id);
                return PQOS_RETVAL_ERROR;
        }

        for (i = 0; i < num_groups; i++) {
                char path[256];
                char buf[128];
                FILE *fd;
                const char *d_name = namelist[i]->d_name;

                if (d_name == NULL)
                        continue;

                ret = resctrl_mon_group_path(class_id, d_name, "/tasks", path,
                                             sizeof(path));
                if (ret != PQOS_RETVAL_OK)
                        goto resctrl_mon_assoc_get_pid_exit;

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
                                ret = resctrl_mon_name_copy(name, name_size,
                                                            d_name);
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

        if (!resctrl_mon_is_supported())
                return PQOS_RETVAL_RESOURCE;

        ret = alloc_assoc_get_pid(task, &class_id);
        if (ret != PQOS_RETVAL_OK)
                return ret;
        if (name != NULL) {
                ret = resctrl_mon_mkdir(class_id, name);
                if (ret != PQOS_RETVAL_OK) {
                        LOG_ERROR(
                            "Failed to create resctrl monitoring group!\n");
                        return ret;
                }
        }

        ret = resctrl_mon_group_path(class_id, name, "/tasks", path,
                                     sizeof(path));
        if (ret != PQOS_RETVAL_OK)
                return ret;
        fd = pqos_fopen(path, "w");
        if (fd == NULL)
                return PQOS_RETVAL_ERROR;

        fprintf(fd, "%d\n", task);

        /* the write is made by the close, and the kernel refuses to move a
         * task that has ended since the caller found it, with ESRCH: the
         * answer resctrl_alloc_task_write() gives for its own write
         */
        errno = 0;
        if (pqos_fclose(fd) != 0) {
                if (errno == ESRCH) {
                        LOG_DEBUG("TID %d ended before it could be assigned "
                                  "to resctrl monitoring group\n",
                                  task);
                        return PQOS_RETVAL_PARAM;
                }
                LOG_ERROR("Could not assign TID %d to resctrl monitoring "
                          "group\n",
                          task);
                return PQOS_RETVAL_ERROR;
        }

        return PQOS_RETVAL_OK;
}

/* the highest L3 ID that the 64-bit l3ids masks below hold */
#define RESCTRL_CORE_MAX_L3ID 63
struct resctrl_core_group {
        char name[32];
        int valid;
        uint64_t llc;
        uint64_t l3ids;
        time_t mtime;
};

/**
 * @brief Makes room to list one more core group started
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK on success
 * @retval PQOS_RETVAL_RESOURCE on a memory allocation failure
 */
static int
mon_started_reserve(void)
{
        struct resctrl_mon_started *grown;
        const unsigned size = mon_started_size + 8;

        if (mon_started_num < mon_started_size)
                return PQOS_RETVAL_OK;

        grown = realloc(mon_started, size * sizeof(*grown));
        if (grown == NULL)
                return PQOS_RETVAL_RESOURCE;
        mon_started = grown;
        mon_started_size = size;

        return PQOS_RETVAL_OK;
}

/**
 * @brief Lists core group \a group as started, in the room
 *        mon_started_reserve() made
 *
 * @param [in] group a core group whose start has succeeded
 */
static void
mon_started_add(const struct pqos_mon_data *group)
{
        struct resctrl_mon_started *entry;
        unsigned i;

        ASSERT(mon_started_num < mon_started_size);
        entry = &mon_started[mon_started_num++];

        entry->group = group;
        strncpy(entry->name, group->intl->resctrl.mon_group,
                sizeof(entry->name));
        entry->name[sizeof(entry->name) - 1] = '\0';
        entry->l3ids = 0;
        entry->l3ids_valid = 1;
        for (i = 0; i < group->intl->resctrl.num_l3id; i++) {
                const unsigned l3id = group->intl->resctrl.l3id[i];

                if (l3id > RESCTRL_CORE_MAX_L3ID)
                        entry->l3ids_valid = 0;
                else
                        entry->l3ids |= 1LLU << l3id;
        }
}

/**
 * @brief Takes core group \a group off the list of those started
 *
 * @param [in] group the group, which is not read
 */
static void
mon_started_remove(const struct pqos_mon_data *group)
{
        unsigned i = 0;

        while (i < mon_started_num)
                if (mon_started[i].group == group)
                        mon_started[i] = mon_started[--mon_started_num];
                else
                        i++;
}

void
resctrl_mon_unlist(const struct pqos_mon_data *group)
{
        mon_started_remove(group);
}

/**
 * @brief Adds to \a grp the L3s of the core groups started in its directory
 *
 * A core gone offline is in the cpus file of no directory, so the L3 it is on
 * would look free in a directory that is still in use on it
 *
 * @param [in,out] grp a directory, as resctrl_mon_parse() lists it
 */
static void
mon_started_l3ids(struct resctrl_core_group *grp)
{
        unsigned i;

        for (i = 0; i < mon_started_num; i++) {
                if (strncmp(mon_started[i].name, grp->name,
                            sizeof(grp->name)) != 0)
                        continue;
                if (!mon_started[i].l3ids_valid)
                        grp->valid = 0;
                grp->l3ids |= mon_started[i].l3ids;
        }
}

/**
 * @brief Parse resctrl monitoring groups of the library
 *
 * Lists the groups with the L3 IDs of the cores assigned to them. Whether a
 * group has tasks is left to resctrl_mon_tasks_assigned(), asked only of a
 * group that could take the cores otherwise: the kernel lists a group's tasks
 * by going through every task in the system, and a parse that read the tasks
 * of each group made a start take time with the groups and the tasks there
 * are, so that starting a group per core grew as their square
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
        unsigned clos = 0;
        unsigned ctrl_grps;
        const struct pqos_cap *cap = _pqos_get_cap();
        const struct pqos_cpuinfo *cpu = _pqos_get_cpu();
        const struct pqos_capability *cap_mon;
        struct resctrl_core_group *grps = NULL;
        unsigned grps_num = 0;
        unsigned grps_size;

        ret = pqos_cap_get_type(cap, PQOS_CAP_TYPE_MON, &cap_mon);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        ret = resctrl_alloc_get_grps_num(cap, &ctrl_grps);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        grps_size = cap_mon->u.mon->max_rmid;
        if (grps_size == 0) {
                /*
                 * No RMIDs available for building a reusable core monitoring
                 * group list. Treat as "no existing groups parsed".
                 */
                *cgrp = NULL;
                *cgrp_num = 0;
                return PQOS_RETVAL_OK;
        }

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

                ret = resctrl_mon_group_path(clos, "", NULL, dir, sizeof(dir));
                if (ret != PQOS_RETVAL_OK)
                        goto resctrl_mon_parse_exit;
                num_groups = scandir(dir, &namelist, filter, NULL);
                if (num_groups < 0) {
                        LOG_ERROR(
                            "Failed to read monitoring groups for CLOS %u\n",
                            clos);
                        ret = PQOS_RETVAL_ERROR;
                        goto resctrl_mon_parse_exit;
                }

                for (i = 0; i < num_groups; i++) {
                        const char *grp_name = namelist[i]->d_name;
                        struct resctrl_core_group *grp = NULL;
                        unsigned j;

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
                                if (grps_num >= grps_size) {
                                        ret = PQOS_RETVAL_ERROR;
                                        break;
                                }
                                grp = &grps[grps_num++];
                                strncpy(grp->name, grp_name, sizeof(grp->name));
                                grp->name[sizeof(grp->name) - 1] = '\0';
                                grp->valid = 1;
                                /* and the L3s of the groups started in it,
                                 * whose cores may be offline
                                 */
                                mon_started_l3ids(grp);
                        }
                        if (!grp->valid)
                                continue;

                        /* Cores assigned to group */
                        ret =
                            resctrl_mon_cpumask_read(clos, grp->name, &cpumask);
                        if (ret != PQOS_RETVAL_OK)
                                break;

                        for (j = 0; j < cpu->num_cores; j++) {
                                const struct pqos_coreinfo *coreinfo =
                                    &(cpu->cores[j]);

                                if (resctrl_cpumask_get(coreinfo->lcore,
                                                        &cpumask)) {
                                        if (coreinfo->l3cat_id >
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
        } while (++clos < ctrl_grps);

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
 * @brief Generate unique monitoring group name
 *
 * @return New monitoring group name
 */
PQOS_STATIC char *
resctrl_mon_new_group(void)
{
        char buf[32];

        snprintf(buf, sizeof(buf), GROUP_NAME_PREFIX "%d-%u", (int)getpid(),
                 resctrl_mon_counter++);

        return strdup(buf);
}

/**
 * @brief Which classes a monitoring group has a directory in
 *
 * @param [in] name group name
 * @param [out] present for each class, whether the group's directory is
 *                      there, to be freed by the caller; NULL for no class
 * @param [out] num_classes number of classes
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 */
static int
resctrl_mon_dirs_present(const char *name,
                         unsigned char **present,
                         unsigned *num_classes)
{
        const struct pqos_cap *cap = _pqos_get_cap();
        unsigned max_clos;
        unsigned clos;
        int ret;

        *present = NULL;
        *num_classes = 0;

        ret = resctrl_alloc_get_grps_num(cap, &max_clos);
        if (ret != PQOS_RETVAL_OK)
                return ret;
        if (max_clos == 0)
                return PQOS_RETVAL_OK;

        *present = calloc(max_clos, sizeof(**present));
        if (*present == NULL)
                return PQOS_RETVAL_RESOURCE;

        for (clos = 0; clos < max_clos; clos++) {
                char path[128];

                ret = resctrl_mon_group_path(clos, name, NULL, path,
                                             sizeof(path));
                if (ret != PQOS_RETVAL_OK) {
                        free(*present);
                        *present = NULL;
                        return ret;
                }
                (*present)[clos] = pqos_dir_exists(path) ? 1 : 0;
        }
        *num_classes = max_clos;

        return PQOS_RETVAL_OK;
}

/**
 * @brief Check if tasks are assigned to a monitoring group in any class
 *
 * Class 0 is looked in whatever max_clos is. Where there is no allocation
 * feature, max_clos is 0, and the default group, class 0, still holds the
 * monitoring groups, which resctrl_mon_parse() lists from it.
 *
 * @param [in] name mon group name
 * @param [in] max_clos number of classes, 0 where there is no allocation
 *                      feature
 * @param [out] assigned 1 when the group's directory in some class lists a
 *                       task
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK on success
 */
static int
resctrl_mon_tasks_assigned(const char *name,
                           const unsigned max_clos,
                           int *assigned)
{
        unsigned clos = 0;

        *assigned = 0;

        do {
                char path[256];
                int empty;
                int ret;

                ret = resctrl_mon_group_path(clos, name, NULL, path,
                                             sizeof(path));
                if (ret != PQOS_RETVAL_OK)
                        return ret;
                if (!pqos_dir_exists(path))
                        continue;

                ret = resctrl_mon_group_path(clos, name, "/tasks", path,
                                             sizeof(path));
                if (ret != PQOS_RETVAL_OK)
                        return ret;
                empty = resctrl_mon_file_empty(path);
                if (empty < 0)
                        return PQOS_RETVAL_ERROR;
                if (!empty) {
                        *assigned = 1;
                        break;
                }
        } while (++clos < max_clos);

        return PQOS_RETVAL_OK;
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
                uint64_t l3ids = 0;
                unsigned cgrp_num;
                const struct pqos_cpuinfo *cpu = _pqos_get_cpu();
                const struct pqos_cap *cap = _pqos_get_cap();
                unsigned max_threshold_occupancy;
                unsigned max_clos;

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

                if (resctrl_mon_is_event_supported(PQOS_MON_EVENT_L3_OCCUP)) {
                        ret = restrl_mon_get_max_llc_tresh(
                            &max_threshold_occupancy);
                        if (ret != PQOS_RETVAL_OK)
                                return NULL;
                }

                ret = resctrl_alloc_get_grps_num(cap, &max_clos);
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
                        int tasks;

                        if (!grp->valid)
                                continue;

                        /* L3Ids overlap */
                        if ((l3ids & grp->l3ids) != 0)
                                continue;

                        /* a group that monitors tasks is not given cores */
                        ret = resctrl_mon_tasks_assigned(grp->name, max_clos,
                                                         &tasks);
                        if (ret != PQOS_RETVAL_OK)
                                goto mon_assign_exit;
                        if (tasks)
                                continue;

                        /* check if llc doesn't exceed occupancy threshold */
                        if (resctrl_mon_is_event_supported(
                                PQOS_MON_EVENT_L3_OCCUP)) {
                                for (l3id = 0; l3id <= RESCTRL_CORE_MAX_L3ID;
                                     l3id++) {
                                        unsigned clos = 0;

                                        if ((l3ids & (1LLU << l3id)) == 0)
                                                continue;

                                        do {
                                                uint64_t val;
                                                char buf[128];

                                                ret = resctrl_mon_group_path(
                                                    clos, grp->name, NULL, buf,
                                                    sizeof(buf));
                                                if (ret != PQOS_RETVAL_OK)
                                                        goto mon_assign_exit;
                                                if (!pqos_dir_exists(buf))
                                                        continue;

                                                ret = resctrl_mon_read_counter(
                                                    clos, grp->name, l3id,
                                                    PQOS_MON_EVENT_L3_OCCUP,
                                                    &val);
                                                if (ret != PQOS_RETVAL_OK)
                                                        goto mon_assign_exit;

                                                llc += val;
                                        } while (++clos < max_clos);
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
        if (resctrl_group == NULL)
                resctrl_group = resctrl_mon_new_group();

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
        unsigned num_tids = 0;
        unsigned reached = 0;
        int new_group;
        unsigned char *present = NULL;
        unsigned num_classes = 0;
        const struct pqos_cpuinfo *cpu = _pqos_get_cpu();

        ASSERT(group != NULL);
        ASSERT(group->tid_nr == 0 || group->tid_map != NULL);
        ASSERT(group->num_cores == 0 || group->cores != NULL);

        /* room to list a core group in once started, made before the start
         * changes anything, so that listing it cannot fail the start after
         */
        if (group->num_cores > 0) {
                ret = mon_started_reserve();
                if (ret != PQOS_RETVAL_OK)
                        return ret;
        }

        /* a group the caller names already monitors other tasks -
         * os_mon_add_pids() adds to one - so a failure here removes only what
         * this start made for it
         */
        new_group = group->intl->resctrl.mon_group == NULL;

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
        if (resctrl_group == NULL) {
                ret = PQOS_RETVAL_ERROR;
                goto resctrl_mon_start_exit;
        }
        group->intl->resctrl.mon_group = resctrl_group;

        /* the classes a group being added to has a directory in as the add
         * begins: a thread in another class gets a directory made for it,
         * which a failed add takes away again
         */
        if (!new_group && group->tid_nr > 0) {
                ret = resctrl_mon_dirs_present(resctrl_group, &present,
                                               &num_classes);
                if (ret != PQOS_RETVAL_OK)
                        goto resctrl_mon_start_exit;
        }

        /**
         * Add pids to the resctrl group
         *
         * A thread that ended after the process's threads were listed has
         * nothing left to monitor. It is skipped, and marked so that the stop
         * passes over it as well, and the start fails only when none of the
         * listed threads is left.
         */
        for (i = 0; i < group->tid_nr; i++) {
                const pid_t tid = group->tid_map[i];
                struct pqos_mon_perf_ctx *ctx = group->intl->perf.ctx != NULL
                                                    ? &group->intl->perf.ctx[i]
                                                    : NULL;

                reached = i;
                /* a perf start before this one found it ended: its ID may be
                 * another task's by now, which is not to be moved in here
                 */
                if (ctx != NULL && ctx->untracked)
                        continue;

                ret = resctrl_mon_assoc_set_pid(tid, resctrl_group);
                if (ret == PQOS_RETVAL_OK) {
                        num_tids++;
                        continue;
                }
                /* PARAM is a task that no longer exists, the kernel's ESRCH
                 * included. A kernel that moves tasks through task_work gives
                 * that for a leader that has exited beside running threads,
                 * which kill() still finds, so only another failure is checked
                 */
                if (ret != PQOS_RETVAL_PARAM &&
                    resctrl_alloc_task_validate(tid) == PQOS_RETVAL_OK)
                        goto resctrl_mon_start_exit;
                LOG_DEBUG("resctrl_mon_start: Skipping ended TID: %d\n", tid);
                if (ctx != NULL)
                        ctx->untracked = 1;
                ret = PQOS_RETVAL_OK;
        }
        reached = group->tid_nr;
        if (group->tid_nr > 0 && num_tids == 0) {
                /* none of the threads listed for the group is left */
                ret = PQOS_RETVAL_PARAM;
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
        if (group->num_cores > 0)
                mon_started_add(group);

resctrl_mon_start_exit:
        if (ret != PQOS_RETVAL_OK) {
                /* The stop that rolls a failed start back takes out what the
                 * start moved in, and only that: a thread it did not reach, or
                 * failed to move, is in another group or none, and stays there
                 */
                if (group->intl->perf.ctx != NULL)
                        for (i = reached; i < group->tid_nr; i++)
                                group->intl->perf.ctx[i].untracked = 1;

                if (group->num_pids > 0 &&
                    group->intl->resctrl.mon_group != NULL) {
                        const struct pqos_cap *cap = _pqos_get_cap();
                        unsigned max_clos;

                        if (new_group &&
                            resctrl_alloc_get_grps_num(cap, &max_clos) ==
                                PQOS_RETVAL_OK) {
                                unsigned clos = 0;

                                do {
                                        (void)resctrl_mon_rmdir(
                                            clos,
                                            group->intl->resctrl.mon_group);
                                } while (++clos < max_clos);
                        } else if (present != NULL) {
                                unsigned clos;

                                for (clos = 0; clos < num_classes; clos++)
                                        if (!present[clos])
                                                (void)resctrl_mon_rmdir(
                                                    clos, group->intl->resctrl
                                                              .mon_group);
                        }

                        free(group->intl->resctrl.mon_group);
                        group->intl->resctrl.mon_group = NULL;
                        resctrl_group = NULL;
                }

                /* cleared as well as freed: the stop that follows frees it */
                free(group->intl->resctrl.l3id);
                group->intl->resctrl.l3id = NULL;
                group->intl->resctrl.num_l3id = 0;
                if (group->intl->resctrl.mon_group != resctrl_group)
                        free(resctrl_group);
        }
        free(present);

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
        unsigned max_clos;
        unsigned clos = 0;
        const struct pqos_cap *cap = _pqos_get_cap();

        ret = resctrl_alloc_get_grps_num(cap, &max_clos);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        do {
                ret = resctrl_mon_rmdir(clos, resctrl_group);
                if (ret != PQOS_RETVAL_OK) {
                        LOG_ERROR("Failed to remove resctrl "
                                  "monitoring group\n");
                        return PQOS_RETVAL_ERROR;
                }
        } while (++clos < max_clos);

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
        unsigned max_clos;
        unsigned clos;
        int ret;
        const struct pqos_cap *cap = _pqos_get_cap();
        const struct pqos_cpuinfo *cpu = _pqos_get_cpu();
        const char *mon_group = group->intl->resctrl.mon_group;
        unsigned started;

        *shared = 0;

        if (group->num_pids > 0)
                return PQOS_RETVAL_OK;

        /* another group this process started is in the directory, whether its
         * cores are in the cpus files or offline
         */
        for (started = 0; started < mon_started_num; started++)
                if (mon_started[started].group != group &&
                    strncmp(mon_started[started].name, mon_group,
                            sizeof(mon_started[started].name)) == 0) {
                        *shared = 1;
                        return PQOS_RETVAL_OK;
                }

        ret = resctrl_alloc_get_grps_num(cap, &max_clos);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        clos = 0;
        do {
                char path[128];
                struct resctrl_cpumask mask;
                unsigned i;

                /* check if group exists */
                ret = resctrl_mon_group_path(clos, mon_group, NULL, path,
                                             sizeof(path));
                if (ret != PQOS_RETVAL_OK)
                        return ret;
                if (!pqos_dir_exists(path))
                        continue;

                /* read assigned cores */
                ret = resctrl_mon_cpumask_read(clos, mon_group, &mask);
                if (ret != PQOS_RETVAL_OK)
                        return ret;

                /* check if cores overlap */
                for (i = 0; i < cpu->num_cores; i++) {
                        const unsigned lcore = cpu->cores[i].lcore;

                        if (resctrl_cpumask_get(lcore, &mask)) {
                                unsigned j;

                                *shared = 1;
                                for (j = 0; j < group->num_cores; j++)
                                        if (group->cores[j] == lcore) {
                                                *shared = 0;
                                                break;
                                        }
                        }

                        if (*shared)
                                return PQOS_RETVAL_OK;
                }

        } while (++clos < max_clos);

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

/**
 * @brief Collect unique package IDs for a monitoring group
 */
int
resctrl_mon_collect_pkgids(struct pqos_mon_data *group)
{
        const struct pqos_cpuinfo *cpu = _pqos_get_cpu();
        unsigned i;
        unsigned *pkgids = NULL;
        unsigned num_pkgids = 0;

        ASSERT(group != NULL);

        if (group->num_cores > 0) {
                pkgids = calloc(group->num_cores, sizeof(*pkgids));
                if (pkgids == NULL)
                        return PQOS_RETVAL_RESOURCE;

                for (i = 0; i < group->num_cores; i++) {
                        const struct pqos_coreinfo *ci =
                            pqos_cpu_get_core_info(cpu, group->cores[i]);
                        unsigned j;
                        int found = 0;

                        if (ci == NULL) {
                                free(pkgids);
                                return PQOS_RETVAL_ERROR;
                        }
                        for (j = 0; j < num_pkgids; j++) {
                                if (pkgids[j] == ci->socket) {
                                        found = 1;
                                        break;
                                }
                        }
                        if (!found)
                                pkgids[num_pkgids++] = ci->socket;
                }
        } else {
                unsigned num_sockets = 0;
                unsigned *sockets = pqos_cpu_get_sockets(cpu, &num_sockets);

                if (sockets == NULL)
                        return PQOS_RETVAL_RESOURCE;

                pkgids = sockets;
                num_pkgids = num_sockets;
        }

        group->intl->resctrl.pkgids = pkgids;
        group->intl->resctrl.num_pkgids = num_pkgids;
        return PQOS_RETVAL_OK;
}

int
resctrl_mon_stop(struct pqos_mon_data *group)
{
        int ret;
        unsigned max_clos;
        unsigned i;
        const struct pqos_cap *cap = _pqos_get_cap();

        ASSERT(group != NULL);

        /* off the list first: pqos_mon_stop() frees the group whatever the
         * stop returns, and no restore puts a core of it back after. A core
         * of it that is offline comes back in the default group, so a
         * directory that a failed stop leaves cannot merge another group's
         * counts with it
         */
        mon_started_remove(group);

        ret = resctrl_alloc_get_grps_num(cap, &max_clos);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        /*
         * Add pids back to the default group
         */
        if (group->num_pids > 0)
                for (i = 0; i < group->tid_nr; i++) {
                        const pid_t tid = group->tid_map[i];

                        /* never added to the group: its ID may be another
                         * task's by now, which is not to be moved out of its
                         * own
                         */
                        if (group->intl->perf.ctx != NULL &&
                            group->intl->perf.ctx[i].untracked)
                                continue;
                        if (resctrl_alloc_task_validate(tid) !=
                            PQOS_RETVAL_OK) {
                                LOG_DEBUG("resctrl_mon_stop: Skipping "
                                          "non-existent PID: %d\n",
                                          tid);
                                continue;
                        }
                        ret = resctrl_mon_assoc_set_pid(tid, NULL);
                        if (ret == PQOS_RETVAL_PARAM) {
                                /* ended after the check above */
                                ret = PQOS_RETVAL_OK;
                                continue;
                        }
                        if (ret != PQOS_RETVAL_OK)
                                goto resctrl_mon_stop_exit;
                }

        /*
         * Remove cores from mon group. A group the start could not name has
         * no directory to take its cores out of: with no name, the paths
         * below are the class's own, whose cores would be taken out of it
         */
        if (group->num_cores > 0 && group->intl->resctrl.mon_group != NULL) {
                const char *mon_group = group->intl->resctrl.mon_group;
                char path[128];
                unsigned clos = 0;

                do {
                        struct resctrl_cpumask cpumask;

                        ret = resctrl_mon_group_path(clos, mon_group, NULL,
                                                     path, sizeof(path));
                        if (ret != PQOS_RETVAL_OK)
                                goto resctrl_mon_stop_exit;
                        if (!pqos_dir_exists(path))
                                continue;

                        ret =
                            resctrl_mon_cpumask_read(clos, mon_group, &cpumask);
                        if (ret != PQOS_RETVAL_OK)
                                goto resctrl_mon_stop_exit;

                        for (i = 0; i < group->num_cores; i++) {
                                const unsigned core = group->cores[i];

                                resctrl_cpumask_unset(core, &cpumask);
                        }

                        ret = resctrl_mon_cpumask_write(clos, mon_group,
                                                        &cpumask);
                        if (ret != PQOS_RETVAL_OK)
                                goto resctrl_mon_stop_exit;
                } while (++clos < max_clos);
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

        if (group->intl->resctrl.l3id != NULL) {
                free(group->intl->resctrl.l3id);
                group->intl->resctrl.l3id = NULL;
        }
        group->intl->resctrl.num_l3id = 0;

        if (group->intl->resctrl.pkgids != NULL) {
                free(group->intl->resctrl.pkgids);
                group->intl->resctrl.pkgids = NULL;
        }

        group->intl->resctrl.num_pkgids = 0;

resctrl_mon_stop_exit:
        return ret;
}

/**
 * @brief The growth of a resctrl MBM count from \a old_value to \a new_value
 *
 * The kernel keeps these counts in 64 bits, from a hardware counter however
 * narrow, so they do not wrap. A group's count is the sum of its directories'
 * and of what the purge saved from those it removed, and so falls only where a
 * directory went without its counts saved: one removed by a writer to resctrl
 * outside the library, or one the purge removed while the kernel gave its
 * counts as Unavailable or Unassigned. The count has then started again, and
 * the reading starts again from it too, rather than the fall being taken for
 * a wrap.
 *
 * @param [in] old_value the previous reading
 * @param [in] new_value this reading
 * @param [in,out] valid 0 before the group's first valid reading, and after
 *                       one the kernel gave as Unavailable or Unassigned; 1
 *                       after a valid one
 *
 * @return the growth: 0 for the first valid reading, for the first after an
 *         unavailable one, and for one that fell
 */
PQOS_STATIC uint64_t
resctrl_mon_count_delta(const uint64_t old_value,
                        const uint64_t new_value,
                        int *valid)
{
        if (*valid && new_value >= old_value)
                return new_value - old_value;

        *valid = 1;
        return 0;
}

/**
 * @brief This function removes all empty monitoring
 *        groups associated with \a group
 *
 * A directory another group is in is left. Where the group's last directory
 * is removed, the group is marked purged, for the restore to make one again
 * when a core of the group is back.
 *
 * @param group monitoring structure
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 * @retval PQOS_RETVAL_ERROR if error occurs
 */
PQOS_STATIC int
resctrl_mon_purge(struct pqos_mon_data *group)
{
        unsigned max_clos;
        unsigned clos;
        int ret;
        unsigned shared;
        unsigned kept = 0;
        unsigned removed = 0;
        const struct pqos_cap *cap = _pqos_get_cap();

        ASSERT(group != NULL);

        ret = resctrl_mon_shared(group, &shared);
        if (ret != PQOS_RETVAL_OK)
                return ret;
        if (shared)
                return PQOS_RETVAL_OK;

        ret = resctrl_alloc_get_grps_num(cap, &max_clos);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        clos = 0;
        do {
                int empty;
                uint64_t value;
                char buf[128];
                const char *name = group->intl->resctrl.mon_group;

                ret =
                    resctrl_mon_group_path(clos, name, NULL, buf, sizeof(buf));
                if (ret != PQOS_RETVAL_OK)
                        return ret;
                if (!pqos_dir_exists(buf))
                        continue;

                ret = resctrl_mon_empty(clos, group->intl->resctrl.mon_group,
                                        group->intl->resctrl.l3id,
                                        group->intl->resctrl.num_l3id, &empty);
                if (ret != PQOS_RETVAL_OK)
                        return ret;

                if (!empty) {
                        kept++;
                        continue;
                }

                /* store counter values */
                if (resctrl_mon_is_event_supported(PQOS_MON_EVENT_LMEM_BW)) {
                        ret = resctrl_mon_read_counters(
                            clos, group->intl->resctrl.mon_group,
                            group->intl->resctrl.l3id,
                            group->intl->resctrl.num_l3id,
                            PQOS_MON_EVENT_LMEM_BW, &value);
                        if (ret == PQOS_RETVAL_OK)
                                group->intl->resctrl.values_storage.mbm_local +=
                                    value;
                        else if (ret != PQOS_RETVAL_UNAVAILABLE)
                                return ret;
                        /* UNAVAILABLE: skip saving partial/stale MBM data */
                }
                if (resctrl_mon_is_event_supported(PQOS_MON_EVENT_TMEM_BW)) {
                        ret = resctrl_mon_read_counters(
                            clos, group->intl->resctrl.mon_group,
                            group->intl->resctrl.l3id,
                            group->intl->resctrl.num_l3id,
                            PQOS_MON_EVENT_TMEM_BW, &value);
                        if (ret == PQOS_RETVAL_OK)
                                group->intl->resctrl.values_storage.mbm_total +=
                                    value;
                        else if (ret != PQOS_RETVAL_UNAVAILABLE)
                                return ret;
                        /* UNAVAILABLE: skip saving partial/stale MBM data */
                }

                ret = resctrl_mon_rmdir(clos, name);
                if (ret != PQOS_RETVAL_OK) {
                        LOG_WARN("Failed to remove empty mon group %s: %m\n",
                                 buf);
                        return ret;
                }

                LOG_INFO("Deleted empty mon group %s\n", buf);
                removed++;

        } while (++clos < max_clos);

        if (removed > 0 && kept == 0)
                group->intl->resctrl.purged = 1;

        return ret;
}

/**
 * @brief This function polls all resctrl counters
 *
 * Reads counters for all events and stores values
 *
 * @param group monitoring structure
 * @param [in] event monitoring event
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
        unsigned max_clos;
        unsigned clos;
        unsigned i;
        uint64_t old_value;
        const struct pqos_cap *cap = _pqos_get_cap();

        ASSERT(group != NULL);

        ret = resctrl_alloc_get_grps_num(cap, &max_clos);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        /*
         * When core CLOS assoc changes then kernel resets monitoring group
         * assoc. We need to restore monitoring assoc for cores
         */
        for (i = 0; i < group->num_cores; i++) {
                ret = resctrl_mon_assoc_restore(group->cores[i],
                                                group->intl->resctrl.mon_group,
                                                &group->intl->resctrl.purged);
                if (ret != PQOS_RETVAL_OK)
                        goto resctrl_mon_poll_exit;
        }

        /* PERF_PKG telemetry events use a different read path */
        if (event == PQOS_MON_EVENT_CORE_ENERGY ||
            event == PQOS_MON_EVENT_ACTIVITY) {
                const char *filename = (event == PQOS_MON_EVENT_CORE_ENERGY)
                                           ? "core_energy"
                                           : "activity";
                const int slot = (event == PQOS_MON_EVENT_CORE_ENERGY)
                                     ? PQOS_TEL_SLOT_CORE_ENERGY
                                     : PQOS_TEL_SLOT_ACTIVITY;
                double total = 0.0;
                unsigned clos2 = 0;
                unsigned max_clos2;
                const struct pqos_cap *cap2 = _pqos_get_cap();

                ret = resctrl_alloc_get_grps_num(cap2, &max_clos2);
                if (ret != PQOS_RETVAL_OK)
                        goto resctrl_mon_poll_exit;

                do {
                        char mon_path[128];
                        double pval = 0.0;

                        ret = resctrl_mon_group_path(
                            clos2, group->intl->resctrl.mon_group, NULL,
                            mon_path, sizeof(mon_path));
                        if (ret != PQOS_RETVAL_OK)
                                goto resctrl_mon_poll_exit;
                        if (!pqos_dir_exists(mon_path))
                                continue;
                        ret = resctrl_mon_read_tel_pkgs(
                            clos2, group->intl->resctrl.mon_group, filename,
                            group->intl->resctrl.pkgids,
                            group->intl->resctrl.num_pkgids, &pval);
                        if (ret != PQOS_RETVAL_OK)
                                goto resctrl_mon_poll_exit;
                        total += pval;
                } while (++clos2 < max_clos2);

                group->intl->resctrl.tel[slot].previous =
                    group->intl->resctrl.tel[slot].current;
                group->intl->resctrl.tel[slot].current = total;
                group->intl->resctrl.tel[slot].valid = 1;

                if (event == PQOS_MON_EVENT_CORE_ENERGY) {
                        group->intl->resctrl.prev_tel_ts =
                            group->intl->resctrl.tel_ts;
                        clock_gettime(CLOCK_MONOTONIC,
                                      &group->intl->resctrl.tel_ts);
                }

                /*
                 * Keep telemetry-only polling consistent with classic resctrl
                 * events and purge empty monitoring groups before exiting.
                 */
                {
                        const int purge_ret = resctrl_mon_purge(group);

                        if (ret == PQOS_RETVAL_OK &&
                            purge_ret != PQOS_RETVAL_OK)
                                ret = purge_ret;
                }

                goto resctrl_mon_poll_exit;
        }

        /* Search CLOSes for given resctrl mon group */
        clos = 0;
        do {
                uint64_t val;
                char buf[128];

                ret =
                    resctrl_mon_group_path(clos, group->intl->resctrl.mon_group,
                                           NULL, buf, sizeof(buf));
                if (ret != PQOS_RETVAL_OK)
                        goto resctrl_mon_poll_exit;
                if (!pqos_dir_exists(buf))
                        continue;

                ret = resctrl_mon_read_counters(
                    clos, group->intl->resctrl.mon_group,
                    group->intl->resctrl.l3id, group->intl->resctrl.num_l3id,
                    event, &val);
                if (ret == PQOS_RETVAL_UNAVAILABLE) {
                        /*
                         * Mark this event invalid and return UNAVAILABLE so
                         * the caller can continue polling other events without
                         * updating baselines or calculating a delta.
                         */
                        switch (event) {
                        case PQOS_MON_EVENT_L3_OCCUP:
                                group->intl->resctrl.valid_llc = 0;
                                break;
                        case PQOS_MON_EVENT_LMEM_BW:
                                group->intl->resctrl.valid_mbm_local = 0;
                                break;
                        case PQOS_MON_EVENT_TMEM_BW:
                                group->intl->resctrl.valid_mbm_total = 0;
                                break;
                        default:
                                break;
                        }
                        goto resctrl_mon_poll_exit;
                }
                if (ret != PQOS_RETVAL_OK)
                        goto resctrl_mon_poll_exit;

                value += val;

        } while (++clos < max_clos);

        /**
         * Set value.
         * For MBM events, apply a per-event validity state machine:
         *   initial/invalid -> valid: establish baseline, delta = 0.
         *   valid -> valid: calculate normal delta, or 0 and a new baseline
         *   for a count that fell, which has started again.
         */
        switch (event) {
        case PQOS_MON_EVENT_L3_OCCUP:
                group->values.llc = value;
                group->intl->resctrl.valid_llc = 1;
                break;
        case PQOS_MON_EVENT_LMEM_BW:
                old_value = group->values.mbm_local;
                group->values.mbm_local =
                    value + group->intl->resctrl.values_storage.mbm_local;
                group->values.mbm_local_delta = resctrl_mon_count_delta(
                    old_value, group->values.mbm_local,
                    &group->intl->resctrl.valid_mbm_local);
                break;
        case PQOS_MON_EVENT_TMEM_BW:
                old_value = group->values.mbm_total;
                group->values.mbm_total =
                    value + group->intl->resctrl.values_storage.mbm_total;
                group->values.mbm_total_delta = resctrl_mon_count_delta(
                    old_value, group->values.mbm_total,
                    &group->intl->resctrl.valid_mbm_total);
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
        unsigned clos = 0;
        const struct pqos_cap *cap = _pqos_get_cap();

        if (!resctrl_mon_is_supported())
                return PQOS_RETVAL_RESOURCE;

        ret = resctrl_alloc_get_grps_num(cap, &grps);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        do {
                struct dirent **namelist = NULL;
                char dir[256];
                int i;

                ret = resctrl_mon_group_path(clos, "", NULL, dir, sizeof(dir));
                if (ret != PQOS_RETVAL_OK)
                        return ret;
                num_groups = scandir(dir, &namelist, filter, NULL);
                if (num_groups < 0) {
                        LOG_ERROR("Failed to read monitoring groups for "
                                  "CLOS %u\n",
                                  clos);
                        return PQOS_RETVAL_ERROR;
                }

                for (i = 0; i < num_groups; i++) {
                        ret = resctrl_mon_rmdir(clos, namelist[i]->d_name);
                        if (ret != PQOS_RETVAL_OK) {
                                free_scandir(namelist, num_groups);
                                return ret;
                        }
                }

                free_scandir(namelist, num_groups);
        } while (++clos < grps);

        return ret;
}

int
resctrl_mon_is_supported(void)
{
        return supported_events != 0;
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
        const struct pqos_cap *cap = _pqos_get_cap();

        if (!resctrl_mon_is_supported()) {
                *monitoring_status = 0;
                return PQOS_RETVAL_OK;
        }

        ret = resctrl_alloc_get_grps_num(cap, &resctrl_group_count);
        if (ret != PQOS_RETVAL_OK) {
                LOG_ERROR("Failed to count resctrl groups");
                return ret;
        }

        do {
                int files_count;
                char path[256];
                struct dirent **mon_group_files = NULL;

                ret = resctrl_mon_group_path(group_idx, "", NULL, path,
                                             DIM(path));
                if (ret != PQOS_RETVAL_OK)
                        return ret;

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
