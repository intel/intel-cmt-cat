/*
 * BSD LICENSE
 *
 * Copyright(c) 2022-2026 Intel Corporation. All rights reserved.
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

#include "monitor_utils.h"

#include "common.h"
#include "monitor.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#define PID_COL_CORE (39) /**< col for core number in /proc/pid/stat */

#define PROC_DIR "/proc"

int
monitor_utils_uinttostr(char *buf, const int buf_len, const unsigned val)
{
        int ret;

        ASSERT(buf != NULL);

        memset(buf, 0, buf_len);
        ret = snprintf(buf, buf_len, "%u", val);

        /* Return -1 when output was truncated */
        if (ret >= buf_len)
                ret = -1;

        return ret;
}

int
monitor_utils_uinttohexstr(char *buf, const int buf_len, const unsigned val)
{
        int ret;

        ASSERT(buf != NULL);

        memset(buf, 0, buf_len);
        ret = snprintf(buf, buf_len, "0x%x", val);

        /* Return -1 when output was truncated */
        if (ret >= buf_len)
                ret = -1;

        return ret;
}

/**
 * @brief Scale byte value up to KB
 *
 * @param bytes value to be scaled up
 * @return scaled up value in KB's
 */
static inline double
bytes_to_kb(const double bytes)
{
        return bytes / 1024.0;
}

/**
 * @brief Scale byte value up to MB
 *
 * @param bytes value to be scaled up
 * @return scaled up value in MB's
 */
static inline double
bytes_to_mb(const double bytes)
{
        return bytes / (1024.0 * 1024.0);
}

double
monitor_utils_get_value(const struct pqos_mon_data *const group,
                        const enum pqos_mon_event event)
{
        int ret;
        uint64_t delta;
        double value;

        /** Coefficient to display the data as MB/s */
        const double coeff = 10.0 / (double)monitor_get_interval();

        if ((group->event & event) == 0)
                return 0.0;

        switch (event) {
        case PQOS_MON_EVENT_L3_OCCUP:
                ret = pqos_mon_get_value(group, event, &delta, NULL);
                if (ret == PQOS_RETVAL_OK) {
                        enum monitor_llc_format format =
                            monitor_get_llc_format();
                        unsigned cache_total;

                        ret = monitor_utils_get_cache_size(&cache_total);
                        if (ret != PQOS_RETVAL_OK)
                                break;

                        switch (format) {
                        case LLC_FORMAT_KILOBYTES:
                                value = bytes_to_kb(delta);
                                break;
                        case LLC_FORMAT_PERCENT:
                                value = delta * 100 / cache_total;
                                break;
                        default:
                                printf("Incorrect llc_format: %i\n", format);
                                ret = PQOS_RETVAL_PARAM;
                        }
                }
                break;
        case PQOS_MON_EVENT_LMEM_BW:
        case PQOS_MON_EVENT_TMEM_BW:
        case PQOS_MON_EVENT_RMEM_BW:
                ret = pqos_mon_get_value(group, event, NULL, &delta);
                if (ret == PQOS_RETVAL_OK)
                        value = bytes_to_mb(delta) * coeff;

                break;
        case PQOS_PERF_EVENT_LLC_MISS:
        case PQOS_PERF_EVENT_LLC_REF:
        case PQOS_PERF_EVENT_LLC_MISS_PCIE_READ:
        case PQOS_PERF_EVENT_LLC_MISS_PCIE_WRITE:
        case PQOS_PERF_EVENT_LLC_REF_PCIE_READ:
        case PQOS_PERF_EVENT_LLC_REF_PCIE_WRITE:
                ret = pqos_mon_get_value(group, event, NULL, &delta);
                value = (double)delta;
                break;
        case PQOS_PERF_EVENT_IPC:
                ret = pqos_mon_get_ipc(group, &value);
                break;
        case PQOS_MON_EVENT_CORE_ENERGY:
        case PQOS_MON_EVENT_ACTIVITY:
        case PQOS_MON_EVENT_POWER: {
                double tel_val = 0.0;
                int tel_ret = pqos_mon_get_tel_value(group, event, &tel_val);

                if (tel_ret == PQOS_RETVAL_OK) {
                        value = tel_val;
                        ret = PQOS_RETVAL_OK;
                } else {
                        ret = tel_ret;
                }
                break;
        }
        default:
                ret = PQOS_RETVAL_PARAM;
        }

        if (ret != PQOS_RETVAL_OK)
                return 0;

        return value;
}

double
monitor_utils_get_region_value(const struct pqos_mon_data *const group,
                               const enum pqos_mon_event event,
                               int region_num)
{
        int ret;
        uint64_t delta;
        double value;

        /** Coefficient to display the data as MB/s */
        const double coeff = 10.0 / (double)monitor_get_interval();

        if ((group->event & event) == 0)
                return 0.0;

        switch (event) {
        case PQOS_MON_EVENT_L3_OCCUP:
        case PQOS_MON_EVENT_IO_L3_OCCUP:
                ret = pqos_mon_get_region_value(group, event, region_num,
                                                &delta, NULL);
                if (ret == PQOS_RETVAL_OK) {
                        enum monitor_llc_format format =
                            monitor_get_llc_format();
                        unsigned cache_total;

                        ret = monitor_utils_get_cache_size(&cache_total);
                        if (ret != PQOS_RETVAL_OK)
                                break;

                        switch (format) {
                        case LLC_FORMAT_KILOBYTES:
                                value = bytes_to_kb(delta);
                                break;
                        case LLC_FORMAT_PERCENT:
                                value = delta * 100 / cache_total;
                                break;
                        default:
                                printf("Incorrect llc_format: %i\n", format);
                                ret = PQOS_RETVAL_PARAM;
                        }
                }
                break;
        case PQOS_MON_EVENT_TMEM_BW:
        case PQOS_MON_EVENT_IO_TOTAL_MEM_BW:
        case PQOS_MON_EVENT_IO_MISS_MEM_BW:
                ret = pqos_mon_get_region_value(group, event, region_num, NULL,
                                                &delta);
                if (ret == PQOS_RETVAL_OK)
                        value = bytes_to_mb(delta) * coeff;
                break;
        case PQOS_PERF_EVENT_LLC_MISS:
        case PQOS_PERF_EVENT_LLC_REF:
        case PQOS_PERF_EVENT_LLC_MISS_PCIE_READ:
        case PQOS_PERF_EVENT_LLC_MISS_PCIE_WRITE:
        case PQOS_PERF_EVENT_LLC_REF_PCIE_READ:
        case PQOS_PERF_EVENT_LLC_REF_PCIE_WRITE:
                ret = pqos_mon_get_value(group, event, NULL, &delta);
                if (ret == PQOS_RETVAL_OK)
                        value = (double)delta;
                break;
        case PQOS_PERF_EVENT_IPC:
                ret = pqos_mon_get_ipc(group, &value);
                break;
        default:
                ret = PQOS_RETVAL_PARAM;
        }

        if (ret != PQOS_RETVAL_OK)
                return 0;

        return value;
}

int
monitor_utils_get_cache_size(unsigned *p_cache_size)
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
        long core_val;
        int ret;

        if (core == NULL || pid < 0)
                return -1;

        memset(core_s, 0, sizeof(core_s));
        ret = monitor_utils_uinttostr(pid_s, sizeof(pid_s), pid);
        if (ret < 0)
                return -1;

        ret = monitor_utils_get_pid_stat(pid_s, PID_COL_CORE, sizeof(core_s),
                                         core_s);
        if (ret != 0)
                return -1;

        errno = 0;
        core_val = strtol(core_s, &tmp, 10);

        if (tmp == core_s || *tmp != '\0' || errno == ERANGE || core_val < 0 ||
            core_val > UINT_MAX)
                return -1;

        *core = (unsigned)core_val;

        return 0;
}

int
monitor_utils_get_pid_cores(const struct pqos_mon_data *mon_data,
                            char *cores_s,
                            const int len)
{
        char core[16];
        unsigned num_cores = 0;
        unsigned i;
        int str_len = 0;
        int cores_s_len = 0;
        int comma_len = 1;
        int has_written_core = 0;
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

        memset(cores_s, 0, len);

        for (i = 0; i < num_tids; i++)
                if (get_pid_core_num(tids[i], &cores[num_cores]) == 0)
                        num_cores++;

        if (num_cores == 0) {
                snprintf(cores_s, len, "-");
                goto free_memory;
        }

        qsort(cores, num_cores, sizeof(*cores), unsigned_cmp);

        for (i = 0; i < num_cores; i++) {

                /* check for duplicate cores and skips them*/
                if (i != 0 && cores[i] == cores[i - 1])
                        continue;

                str_len = monitor_utils_uinttostr(core, sizeof(core), cores[i]);
                if (str_len < 0) {
                        result = -1;
                        goto free_memory;
                }

                cores_s_len = strlen(cores_s);

                if (has_written_core &&
                    (cores_s_len + str_len + comma_len) < len) {
                        strncat(cores_s, ",", len - cores_s_len);
                        strncat(cores_s, core, len - cores_s_len - comma_len);
                } else if (!has_written_core && (cores_s_len + str_len) < len) {
                        strncat(cores_s, core, len - cores_s_len);
                        has_written_core = 1;
                } else
                        break;
        }

free_memory:
        free(cores);
        return result;
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
        char path_buf[512];
        const char *proc_stat_path_fmt = "%s/%s/stat";

        ASSERT(proc_pid_dir_name != NULL);

        snprintf(path_buf, sizeof(path_buf) - 1, proc_stat_path_fmt, PROC_DIR,
                 proc_pid_dir_name);

        return pqos_fopen(path_buf, "r");
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
int
monitor_utils_get_pid_stat(const char *proc_pid_dir_name,
                           const int column,
                           const unsigned len_val,
                           char *val)
{
        FILE *fproc_pid_stats;
        char buf[512];
        char *left_paren;
        char *right_paren;
        char *saveptr = NULL;
        char *token;
        size_t n_read;
        int col_idx = 3;

        if (proc_pid_dir_name == NULL || val == NULL || column < 1 ||
            len_val == 0)
                return -1;

        fproc_pid_stats = open_proc_stat_file(proc_pid_dir_name);
        if (fproc_pid_stats == NULL)
                return -1;

        n_read = fread(buf, sizeof(char), sizeof(buf) - 1, fproc_pid_stats);
        if (ferror(fproc_pid_stats)) {
                pqos_fclose(fproc_pid_stats);
                return -1;
        }
        pqos_fclose(fproc_pid_stats);

        if (n_read == 0)
                return -1;
        if (n_read == sizeof(buf) - 1 && buf[n_read - 1] != '\n')
                return -1;
        buf[n_read] = '\0';

        left_paren = strchr(buf, '(');
        right_paren = strrchr(buf, ')');
        if (left_paren == NULL || right_paren == NULL || left_paren == buf ||
            right_paren <= left_paren || left_paren[-1] != ' ' ||
            right_paren[1] != ' ' || right_paren[2] == '\0')
                return -1;

        if (column == 1) {
                left_paren[-1] = '\0';
                token = buf;
        } else if (column == 2) {
                right_paren[1] = '\0';
                token = left_paren;
        } else {
                token = strtok_r(right_paren + 2, "\n ", &saveptr);
                while (token != NULL && col_idx < column) {
                        token = strtok_r(NULL, "\n ", &saveptr);
                        col_idx++;
                }
        }

        if (token == NULL)
                return -1;
        if (len_val < strlen(token) + 1)
                return -1;

        memcpy(val, token, strlen(token) + 1);
        return 0;
}

char *
trim(char *str)
{
        char *end;

        while (isspace((unsigned char)*str))
                str++;

        if (*str == 0)
                return str;

        end = str + strlen(str) - 1;
        while (end > str && isspace((unsigned char)*end))
                end--;

        end[1] = '\0';

        return str;
}
