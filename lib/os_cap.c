/*
 * BSD LICENSE
 *
 * Copyright(c) 2019-2020 Intel Corporation. All rights reserved.
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

#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "pqos.h"
#include "allocation.h"
#include "common.h"
#include "os_cap.h"
#include "types.h"
#include "log.h"
#include "resctrl.h"
#include "resctrl_alloc.h"
#include "perf_monitoring.h"

#define PROC_CPUINFO     "/proc/cpuinfo"
#define PROC_FILESYSTEMS "/proc/filesystems"
#define PROC_MOUNTS      "/proc/mounts"

static int mba_ctrl = -1; /**< mba ctrl support */

/**
 * @brief Checks file fname to detect str and sets a flag
 *
 * @param [in] fname name of the file to be searched
 * @param [in] str string being searched for
 * @param [in] check_symlink a flag indicating if a path must be checked
 *             against symlinks
 * @param [out] supported pointer to os_supported flag
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK success
 */
static int
detect_os_support(const char *fname,
                  const char *str,
                  int check_symlink,
                  int *supported)
{
        FILE *fd;
        char temp[1024];

        if (fname == NULL || str == NULL || supported == NULL)
                return PQOS_RETVAL_PARAM;

        if (check_symlink)
                fd = pqos_fopen(fname, "r");
        else
                fd = fopen(fname, "r");

        if (fd == NULL) {
                LOG_DEBUG("%s not found.\n", fname);
                *supported = 0;
                return PQOS_RETVAL_OK;
        }

        while (fgets(temp, sizeof(temp), fd) != NULL) {
                if (strstr(temp, str) != NULL) {
                        *supported = 1;
                        fclose(fd);
                        return PQOS_RETVAL_OK;
                }
        }

        fclose(fd);
        return PQOS_RETVAL_OK;
}

/**
 * @brief Read uint64 from file
 *
 * @param [in] fname name of the file
 * @param [in] base numerical base
 * @param [out] value UINT value
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK success
 */
static int
readuint64(const char *fname, unsigned base, uint64_t *value)
{
        FILE *fd;
        char buf[16] = "\0";
        char *s = buf;
        char *endptr = NULL;
        size_t bytes;

        ASSERT(fname != NULL);
        ASSERT(value != NULL);

        fd = pqos_fopen(fname, "r");
        if (fd == NULL)
                return PQOS_RETVAL_ERROR;

        bytes = fread(buf, sizeof(buf) - 1, 1, fd);
        if (bytes == 0 && !feof(fd)) {
                fclose(fd);
                return PQOS_RETVAL_ERROR;
        }
        fclose(fd);

        *value = strtoull(s, &endptr, base);

        if (!((*s != '\0' && *s != '\n') &&
              (*endptr == '\0' || *endptr == '\n'))) {
                LOG_ERROR("Error converting '%s' to unsigned number!\n", buf);
                return PQOS_RETVAL_ERROR;
        }

        return PQOS_RETVAL_OK;
}

/**
 * @brief Retrieves number of closids
 *
 * @param [in] dir path to info directory
 * @param [out] num_closids place to store retrieved value
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 */
static int
get_num_closids(const char *dir, unsigned *num_closids)
{
        char path[128];
        int ret;
        uint64_t val;

        snprintf(path, sizeof(path) - 1, "%s/num_closids", dir);

        ret = readuint64(path, 10, &val);
        if (ret == PQOS_RETVAL_OK)
                *num_closids = val;

        return ret;
}

/**
 * @brief Retrieves number of ways
 *
 * @param [in] dir path to info directory
 * @param [out] num_ways place to store retrieved value
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 */
static int
get_num_ways(const char *dir, unsigned *num_ways)
{
        char path[128];
        int ret;
        uint64_t val;

        snprintf(path, sizeof(path) - 1, "%s/cbm_mask", dir);

        ret = readuint64(path, 16, &val);
        if (ret == PQOS_RETVAL_OK) {
                *num_ways = 0;
                while (val > 0) {
                        (*num_ways)++;
                        val = val >> 1;
                }
        }

        return ret;
}

/**
 * @brief Retrieves shareable bit mask
 *
 * @param [in] dir path to info directory
 * @param [out] shareable_bits place to store retrieved value
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 */
static int
get_shareable_bits(const char *dir, uint64_t *shareable_bits)
{
        char path[128];

        ASSERT(dir != NULL);

        snprintf(path, sizeof(path) - 1, "%s/shareable_bits", dir);

        /* Information not present in info dir */
        if (access(path, F_OK) != 0) {
                LOG_DEBUG("Unable to obtain ways contention bit-mask, %s file "
                          "does not exist\n",
                          path);
                *shareable_bits = 0;
                return PQOS_RETVAL_OK;
        }

        return readuint64(path, 16, shareable_bits);
}

int
os_cap_init(const enum pqos_interface inter)
{
        int ret;
        int res_flag = 0;
        struct stat st;

        /**
         * resctrl detection
         */
        ret = detect_os_support(PROC_FILESYSTEMS, "resctrl", 0, &res_flag);
        if (ret != PQOS_RETVAL_OK) {
                LOG_ERROR("Fatal error encountered in resctrl detection!\n");
                return ret;
        }
        LOG_INFO("%s\n", res_flag ? "resctrl detected"
                                  : "resctrl not detected. "
                                    "Kernel version 4.10 or higher required");

        if (res_flag == 0) {
                LOG_ERROR("OS interface selected but not supported\n");
                return PQOS_RETVAL_INTER;
        }

        /**
         * Mount resctrl with default parameters
         */
        if (access(RESCTRL_PATH "/cpus", F_OK) != 0) {
                LOG_INFO("resctrl not mounted\n");
                /**
                 * Check if it is possible to enable MBA CTRL
                 */
                ret = resctrl_mount(PQOS_REQUIRE_CDP_OFF, PQOS_REQUIRE_CDP_OFF,
                                    PQOS_MBA_CTRL);
                if (ret == PQOS_RETVAL_OK) {
                        FILE *fd;

                        /* Verify possibility of setting mba value above 100 */
                        fd = resctrl_alloc_fopen(0, "schemata", "w");
                        if (fd != NULL) {
                                fprintf(fd, "MB:0=200\n");
                                if (fclose(fd) == 0)
                                        mba_ctrl = 1;
                                else
                                        mba_ctrl = 0;
                        }
                        resctrl_umount();
                } else
                        mba_ctrl = 0;

                ret = resctrl_mount(PQOS_REQUIRE_CDP_OFF, PQOS_REQUIRE_CDP_OFF,
                                    PQOS_MBA_DEFAULT);
                if (ret != PQOS_RETVAL_OK) {
                        LOG_INFO("Unable to mount resctrl\n");
                        return PQOS_RETVAL_RESOURCE;
                }
        }

        if (inter == PQOS_INTER_OS_RESCTRL_MON &&
            stat(RESCTRL_PATH_INFO_L3_MON, &st) != 0) {
                LOG_ERROR("Resctrl monitoring selected but not supported\n");
                return PQOS_RETVAL_INTER;
        }

        return ret;
}

/**
 * @brief Checks if event is supported by resctrl monitoring
 *
 * @param [in] event monitoring event type
 * @param [out] supported set to 1 if resctrl support is present
 * @param [out] scale scale factor
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK success
 */
static int
detect_mon_resctrl_support(const enum pqos_mon_event event,
                           int *supported,
                           uint32_t *scale)
{
        struct stat st;
        const char *event_name = NULL;
        int ret;

        ASSERT(supported != NULL);

        *supported = 0;

        /* resctrl monitoring is not supported */
        if (stat(RESCTRL_PATH_INFO_L3_MON, &st) != 0)
                return PQOS_RETVAL_OK;

        switch (event) {
        case PQOS_MON_EVENT_L3_OCCUP:
                event_name = "llc_occupancy";
                break;
        case PQOS_MON_EVENT_TMEM_BW:
                event_name = "mbm_total_bytes";
                break;
        case PQOS_MON_EVENT_LMEM_BW:
                event_name = "mbm_local_bytes";
                break;
        default:
                return PQOS_RETVAL_OK;
                break;
        }

        ret = detect_os_support(RESCTRL_PATH_INFO_L3_MON "/mon_features",
                                event_name, 1, supported);

        if (scale != NULL)
                *scale = 1;

        return ret;
}

/**
 * @brief Reads scale factor of perf monitoring event
 *
 * @param [in] event_name perf monitoring event name
 * @param [out] scale scale factor
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK success
 */
static int
get_mon_perf_scale_factor(const char *event_name, uint32_t *scale)
{
        char path[128];
        char buf[16];
        double scale_factor;
        unsigned unit = 1;
        int ret;
        FILE *fd;

        ASSERT(scale != NULL);
        ASSERT(event_name != NULL);

        /* read scale factor value */
        snprintf(path, sizeof(path) - 1, PERF_MON_PATH "/events/%s.scale",
                 event_name);

        fd = pqos_fopen(path, "r");
        if (fd == NULL) {
                LOG_ERROR("Failed to open %s perf monitoring event scale "
                          "file!\n",
                          event_name);
                return PQOS_RETVAL_ERROR;
        }
        ret = fscanf(fd, "%10lf", &scale_factor);
        fclose(fd);
        if (ret < 1) {
                LOG_ERROR("Failed to read %s perf monitoring event scale "
                          "factor!\n",
                          event_name);
                return PQOS_RETVAL_ERROR;
        }

        /* read scale factor unit */
        snprintf(path, sizeof(path) - 1, PERF_MON_PATH "/events/%s.unit",
                 event_name);

        fd = pqos_fopen(path, "r");
        if (fd == NULL) {
                LOG_ERROR("Failed to open %s perf monitoring event unit "
                          "file!\n",
                          event_name);
                return PQOS_RETVAL_ERROR;
        }

        if (fgets(buf, sizeof(buf), fd) != NULL) {
                if (strncmp(buf, "Bytes", sizeof(buf)) == 0)
                        unit = 1;
                else if (strncmp(buf, "MB", sizeof(buf)) == 0)
                        unit = 1000000;
                else {
                        LOG_ERROR("Unknown \"%s\" scale factor unit", buf);
                        fclose(fd);
                        return PQOS_RETVAL_ERROR;
                }
        } else {
                LOG_ERROR("Failed to read %s perf monitoring event unit!\n",
                          event_name);
                fclose(fd);
                return PQOS_RETVAL_ERROR;
        }
        fclose(fd);

        *scale = (uint32_t)(scale_factor * unit);

        return PQOS_RETVAL_OK;
}

/**
 * @brief Checks if event is supported by perf
 *
 * @param [in] event monitoring event type
 * @param [out] supported set to 1 if perf support is present
 * @param [out] scale scale factor
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK success
 */
static int
detect_mon_perf_support(const enum pqos_mon_event event,
                        int *supported,
                        uint32_t *scale)
{
        char path[128];
        struct stat st;
        const char *event_name = NULL;
        int ret;
        static int warn = 1;

        ASSERT(supported != NULL);

        *supported = 0;
        *scale = 1;

        switch (event) {
        case PQOS_MON_EVENT_L3_OCCUP:
                event_name = "llc_occupancy";
                break;
        case PQOS_MON_EVENT_LMEM_BW:
                event_name = "local_bytes";
                break;
        case PQOS_MON_EVENT_TMEM_BW:
                event_name = "total_bytes";
                break;
        /**
         * Assume support of perf events
         */
        case PQOS_PERF_EVENT_LLC_MISS:
        case PQOS_PERF_EVENT_IPC:
                *supported = 1;
                return PQOS_RETVAL_OK;

        default:
                return PQOS_RETVAL_OK;
                break;
        }

        snprintf(path, sizeof(path) - 1, PERF_MON_PATH "/events/%s",
                 event_name);
        if (stat(path, &st) != 0)
                return PQOS_RETVAL_OK;

        *supported = 1;

        ret = get_mon_perf_scale_factor(event_name, scale);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        if (*supported && warn) {
                LOG_WARN("As of Kernel 4.10, Intel(R) RDT perf results per "
                         "core are found to be incorrect.\n");
                warn = 0;
        }

        return PQOS_RETVAL_OK;
}

/**
 * @brief Checks if event is supported
 *
 * @param [in] event monitoring event type
 * @param [out] supported set to 1 if OS support is present
 * @param [out] scale scale factor
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK success
 */
static int
detect_mon_support(const enum pqos_mon_event event,
                   int *supported,
                   uint32_t *scale)
{
        int ret;

        *supported = 0;

        if (event == PQOS_MON_EVENT_RMEM_BW) {
                int lmem;
                int tmem;

                ret = detect_mon_support(PQOS_MON_EVENT_LMEM_BW, &lmem, scale);
                if (ret != PQOS_RETVAL_OK)
                        return ret;

                ret = detect_mon_support(PQOS_MON_EVENT_TMEM_BW, &tmem, scale);
                if (ret != PQOS_RETVAL_OK)
                        return ret;

                *supported = (lmem && tmem);

                return PQOS_RETVAL_OK;
        }

        ret = detect_mon_resctrl_support(event, supported, scale);
        if (ret != PQOS_RETVAL_OK) {
                LOG_ERROR("Fatal error encountered while checking for resctrl "
                          "monitoring support\n");
                return ret;
        }
        if (*supported)
                return ret;

        ret = detect_mon_perf_support(event, supported, scale);
        if (ret != PQOS_RETVAL_OK)
                LOG_ERROR("Fatal error encountered while checking for perf "
                          "monitoring support\n");

        return ret;
}

int
os_cap_mon_discover(struct pqos_cap_mon **r_cap, const struct pqos_cpuinfo *cpu)
{
        struct pqos_cap_mon *cap = NULL;
        int supported;
        int ret = PQOS_RETVAL_OK;
        uint64_t num_rmids = 0;
        unsigned i;

        enum pqos_mon_event events[] = {
            /* clang-format off */
                PQOS_MON_EVENT_L3_OCCUP,
                PQOS_MON_EVENT_LMEM_BW,
                PQOS_MON_EVENT_TMEM_BW,
                PQOS_MON_EVENT_RMEM_BW,
                PQOS_PERF_EVENT_LLC_MISS,
                PQOS_PERF_EVENT_IPC
            /* clang-format on */
        };

        ret = detect_os_support(PROC_CPUINFO, "cqm", 0, &supported);
        if (ret != PQOS_RETVAL_OK) {
                LOG_ERROR("Fatal error encountered in"
                          " OS detection!\n");
                return ret;
        }
        if (!supported)
                return PQOS_RETVAL_RESOURCE;

        if (access(RESCTRL_PATH_INFO_L3_MON "/num_rmids", F_OK) == 0) {
                ret = readuint64(RESCTRL_PATH_INFO_L3_MON "/num_rmids", 10,
                                 &num_rmids);
                if (ret != PQOS_RETVAL_OK)
                        return ret;
        }

        cap = (struct pqos_cap_mon *)malloc(sizeof(*cap));
        if (cap == NULL)
                return PQOS_RETVAL_RESOURCE;
        memset(cap, 0, sizeof(*cap));
        cap->mem_size = sizeof(*cap);
        cap->max_rmid = num_rmids;
        cap->l3_size = cpu->l3.total_size;

        for (i = 0; i < DIM(events); i++) {
                int supported;
                uint32_t scale;
                struct pqos_cap_mon *mon;
                struct pqos_monitor *monitor;

                ret = detect_mon_support(events[i], &supported, &scale);
                if (ret != PQOS_RETVAL_OK)
                        break;

                if (!supported)
                        continue;

                mon = realloc(cap, cap->mem_size + sizeof(struct pqos_monitor));
                if (mon == NULL) {
                        ret = PQOS_RETVAL_RESOURCE;
                        break;
                }

                monitor = &(mon->events[mon->num_events]);
                memset(monitor, 0, sizeof(*monitor));
                monitor->type = events[i];
                monitor->max_rmid = num_rmids;
                monitor->scale_factor = scale;

                mon->mem_size += sizeof(struct pqos_monitor);
                mon->num_events++;

                cap = mon;
        }

        if (ret == PQOS_RETVAL_OK)
                *r_cap = cap;
        else
                free(cap);

        return ret;
}

int
os_cap_l3ca_discover(struct pqos_cap_l3ca *cap, const struct pqos_cpuinfo *cpu)
{
        struct stat st;
        const char *info;
        int cdp_on;
        int ret = PQOS_RETVAL_OK;

        ASSERT(cap != NULL);

        if (stat(RESCTRL_PATH_INFO_L3, &st) == 0) {
                info = RESCTRL_PATH_INFO_L3;
                cdp_on = 0;
        } else if (stat(RESCTRL_PATH_INFO_L3CODE, &st) == 0 &&
                   stat(RESCTRL_PATH_INFO_L3DATA, &st) == 0) {
                info = RESCTRL_PATH_INFO_L3CODE;
                cdp_on = 1;
        } else
                return PQOS_RETVAL_RESOURCE;

        memset(cap, 0, sizeof(*cap));
        cap->mem_size = sizeof(*cap);
        cap->cdp = cdp_on;
        cap->cdp_on = cdp_on;
        cap->way_size = cpu->l3.way_size;

        ret = get_num_closids(info, &cap->num_classes);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        ret = get_num_ways(info, &cap->num_ways);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        ret = get_shareable_bits(info, &cap->way_contention);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        if (!cdp_on)
                ret = detect_os_support(PROC_CPUINFO, "cdp_l3", 0, &cap->cdp);

        return ret;
}

int
os_cap_l2ca_discover(struct pqos_cap_l2ca *cap, const struct pqos_cpuinfo *cpu)
{
        struct stat st;
        const char *info;
        int cdp_on;
        int ret = PQOS_RETVAL_OK;

        ASSERT(cap != NULL);

        if (stat(RESCTRL_PATH_INFO_L2, &st) == 0) {
                info = RESCTRL_PATH_INFO_L2;
                cdp_on = 0;
        } else if (stat(RESCTRL_PATH_INFO_L2CODE, &st) == 0 &&
                   stat(RESCTRL_PATH_INFO_L2DATA, &st) == 0) {
                info = RESCTRL_PATH_INFO_L2CODE;
                cdp_on = 1;
        } else
                return PQOS_RETVAL_RESOURCE;

        memset(cap, 0, sizeof(*cap));
        cap->mem_size = sizeof(*cap);
        cap->cdp = cdp_on;
        cap->cdp_on = cdp_on;
        cap->way_size = cpu->l2.way_size;

        ret = get_num_closids(info, &cap->num_classes);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        ret = get_num_ways(info, &cap->num_ways);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        ret = get_shareable_bits(info, &cap->way_contention);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        if (!cdp_on)
                ret = detect_os_support(PROC_CPUINFO, "cdp_l2", 0, &cap->cdp);

        return ret;
}

int
os_cap_get_mba_ctrl(const struct pqos_cap *cap,
                    const struct pqos_cpuinfo *cpu,
                    int *supported,
                    int *enabled)
{
        int ret;

        ret = pqos_mba_ctrl_enabled(cap, supported, enabled);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        /* resctrl is mounted with default options */
        if (mba_ctrl != -1) {
                *enabled = 0;
                *supported = mba_ctrl;
                return PQOS_RETVAL_OK;
        }

        if (access(RESCTRL_PATH "/cpus", F_OK) != 0)
                *enabled = 0;

        /* check mount flags */
        if (*enabled == -1) {
                ret = detect_os_support(PROC_MOUNTS, "mba_MBps", 0, enabled);
                if (ret != PQOS_RETVAL_OK)
                        return ret;
        }

        /* check for values above 100 */
        if (*enabled == -1) {
                unsigned grp;
                unsigned count = 0;
                unsigned i;
                unsigned *mba_ids, mba_id_num;
                struct resctrl_schemata *schmt;

                ret = resctrl_alloc_get_grps_num(cap, &count);
                if (ret != PQOS_RETVAL_OK)
                        return ret;

                mba_ids = pqos_cpu_get_mba_ids(cpu, &mba_id_num);
                if (mba_ids == NULL)
                        return PQOS_RETVAL_ERROR;

                schmt = resctrl_schemata_alloc(cap, cpu);
                if (schmt == NULL) {
                        free(mba_ids);
                        return PQOS_RETVAL_ERROR;
                }

                for (grp = 0; grp < count && *enabled == -1; grp++) {
                        ret = resctrl_alloc_schemata_read(grp, schmt);
                        if (ret != PQOS_RETVAL_OK)
                                continue;

                        for (i = 0; i < mba_id_num; i++) {
                                struct pqos_mba mba;

                                ret = resctrl_schemata_mba_get(
                                    schmt, mba_ids[i], &mba);
                                if (ret == PQOS_RETVAL_OK && mba.mb_max > 100) {
                                        *enabled = 1;
                                        break;
                                }
                        }
                }

                resctrl_schemata_free(schmt);
                free(mba_ids);
        }

        /* get free COS and try to write value above 100 */
        if (*enabled == -1) {
                unsigned grp;
                unsigned count = 0;
                struct resctrl_schemata *schmt;
                FILE *fd;

                ret = resctrl_alloc_get_grps_num(cap, &count);
                if (ret != PQOS_RETVAL_OK)
                        return ret;

                ret = resctrl_alloc_get_unused_group(count, &grp);
                if (ret != PQOS_RETVAL_OK) {
                        LOG_WARN("Unable to check if MBA CTRL is enabled - "
                                 "No free group\n");
                        goto ctrl_support;
                }

                schmt = resctrl_schemata_alloc(cap, cpu);
                if (schmt == NULL)
                        goto ctrl_support;

                ret = resctrl_alloc_schemata_read(grp, schmt);
                if (ret == PQOS_RETVAL_OK) {
                        fd = resctrl_alloc_fopen(grp, "schemata", "w");
                        if (fd != NULL) {
                                fprintf(fd, "MB:0=2000\n");
                                if (fclose(fd) == 0)
                                        *enabled = 1;
                                else
                                        *enabled = 0;
                        }
                        /* restore MBA configuration */
                        if (*enabled == 1) {
                                ret = resctrl_alloc_schemata_write(
                                    grp, PQOS_TECHNOLOGY_MBA, schmt);
                                if (ret != PQOS_RETVAL_OK)
                                        LOG_WARN("Unable to restore MBA "
                                                 "settings\n");
                        }
                }

                resctrl_schemata_free(schmt);
        }

ctrl_support:
        if (*supported != -1)
                goto ctrl_exit;

        if (*enabled == 1)
                *supported = 1;

        /* Check if MBL monitoring is supported */
        else {
                int mbl = 0;

                ret = detect_mon_resctrl_support(PQOS_MON_EVENT_LMEM_BW, &mbl,
                                                 NULL);
                if (ret != PQOS_RETVAL_OK)
                        return ret;
                if (!mbl)
                        *supported = 0;
        }

ctrl_exit:
        if (*supported == 0)
                *enabled = 0;

        if (*supported == 1)
                LOG_INFO("OS support for MBA CTRL detected\n");
        else if (*supported == 0)
                LOG_INFO("OS support for MBA CTRL not detected\n");
        else
                LOG_INFO("OS support for MBA CTRL unknown\n");

        return PQOS_RETVAL_OK;
}

int
os_cap_mba_discover(struct pqos_cap_mba *cap, const struct pqos_cpuinfo *cpu)
{
        struct stat st;
        uint64_t val;
        const char *info = RESCTRL_PATH_INFO_MB;
        int ret = PQOS_RETVAL_OK;

        UNUSED_PARAM(cpu);
        ASSERT(cap != NULL);

        if (stat(RESCTRL_PATH_INFO_MB, &st) != 0)
                return PQOS_RETVAL_RESOURCE;

        memset(cap, 0, sizeof(*cap));
        cap->mem_size = sizeof(*cap);
        cap->ctrl = -1;
        cap->ctrl_on = -1;

        ret = get_num_closids(info, &cap->num_classes);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        /* Detect MBA CTRL status */
        ret = detect_os_support(PROC_MOUNTS, "mba_MBps", 0, &(cap->ctrl_on));
        if (ret != PQOS_RETVAL_OK)
                return ret;
        if (cap->ctrl_on == 1)
                cap->ctrl = 1;
        else
                cap->ctrl = mba_ctrl;

        ret = readuint64(RESCTRL_PATH_INFO_MB "/min_bandwidth", 10, &val);
        if (ret != PQOS_RETVAL_OK)
                return ret;
        else
                cap->throttle_max = 100 - val;

        ret = readuint64(RESCTRL_PATH_INFO_MB "/bandwidth_gran", 10, &val);
        if (ret != PQOS_RETVAL_OK)
                return ret;
        else
                cap->throttle_step = val;

        ret = readuint64(RESCTRL_PATH_INFO_MB "/delay_linear", 10, &val);
        if (ret != PQOS_RETVAL_OK)
                return ret;
        else
                cap->is_linear = (val == 1);

        return ret;
}
