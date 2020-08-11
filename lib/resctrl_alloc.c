/*
 * BSD LICENSE
 *
 * Copyright(c) 2017-2020 Intel Corporation. All rights reserved.
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

#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>

#include "common.h"
#include "allocation.h"
#include "log.h"
#include "types.h"
#include "cap.h"
#include "resctrl_alloc.h"
#include "resctrl_utils.h"
#include "resctrl_monitoring.h"

/**
 * ---------------------------------------
 * Local data structures
 * ---------------------------------------
 */
static const struct pqos_cpuinfo *m_cpu = NULL;

/*
 * COS file names on resctrl file system
 */
static const char *rctl_cpus = "cpus";
static const char *rctl_schemata = "schemata";
static const char *rctl_tasks = "tasks";

int
resctrl_alloc_init(const struct pqos_cpuinfo *cpu, const struct pqos_cap *cap)
{
        if (cpu == NULL || cap == NULL)
                return PQOS_RETVAL_PARAM;

        m_cpu = cpu;

        return PQOS_RETVAL_OK;
}

int
resctrl_alloc_fini(void)
{
        m_cpu = NULL;
        return PQOS_RETVAL_OK;
}

int
resctrl_alloc_get_grps_num(const struct pqos_cap *cap, unsigned *grps_num)
{
        unsigned i;
        unsigned max_rctl_grps = 0;
        int ret = PQOS_RETVAL_OK;

        ASSERT(cap != NULL);
        ASSERT(grps_num != NULL);

        /*
         * Loop through all caps that have OS support
         * Find max COS supported by all
         */
        for (i = 0; i < cap->num_cap; i++) {
                unsigned num_cos = 0;
                const struct pqos_capability *p_cap = &cap->capabilities[i];

                /* get L3 CAT COS num */
                if (p_cap->type == PQOS_CAP_TYPE_L3CA) {
                        ret = pqos_l3ca_get_cos_num(cap, &num_cos);
                        if (ret != PQOS_RETVAL_OK)
                                return ret;

                        if (max_rctl_grps == 0)
                                max_rctl_grps = num_cos;
                        else if (num_cos < max_rctl_grps)
                                max_rctl_grps = num_cos;
                }
                /* get L2 CAT COS num */
                if (p_cap->type == PQOS_CAP_TYPE_L2CA) {
                        ret = pqos_l2ca_get_cos_num(cap, &num_cos);
                        if (ret != PQOS_RETVAL_OK)
                                return ret;

                        if (max_rctl_grps == 0)
                                max_rctl_grps = num_cos;
                        else if (num_cos < max_rctl_grps)
                                max_rctl_grps = num_cos;
                }
                /* get MBA COS num */
                if (p_cap->type == PQOS_CAP_TYPE_MBA) {
                        ret = pqos_mba_get_cos_num(cap, &num_cos);
                        if (ret != PQOS_RETVAL_OK)
                                return ret;

                        if (max_rctl_grps == 0)
                                max_rctl_grps = num_cos;
                        else if (num_cos < max_rctl_grps)
                                max_rctl_grps = num_cos;
                }
        }
        *grps_num = max_rctl_grps;
        return PQOS_RETVAL_OK;
}

FILE *
resctrl_alloc_fopen(const unsigned class_id, const char *name, const char *mode)
{
        FILE *fd;
        char buf[128];
        int result;

        ASSERT(name != NULL);
        ASSERT(mode != NULL);

        memset(buf, 0, sizeof(buf));
        if (class_id == 0)
                result =
                    snprintf(buf, sizeof(buf) - 1, "%s/%s", RESCTRL_PATH, name);
        else
                result = snprintf(buf, sizeof(buf) - 1, "%s/COS%u/%s",
                                  RESCTRL_PATH, class_id, name);

        if (result < 0)
                return NULL;

        fd = pqos_fopen(buf, mode);
        if (fd == NULL)
                LOG_ERROR("Could not open %s file %s for COS %u\n", name, buf,
                          class_id);

        return fd;
}

/**
 * @brief Closes COS file in resctl filesystem
 *
 * @param[in] fd File descriptor
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK on success
 */
static int
resctrl_alloc_fclose(FILE *fd)
{
        if (fd == NULL)
                return PQOS_RETVAL_PARAM;

        if (fclose(fd) == 0)
                return PQOS_RETVAL_OK;

        switch (errno) {
        case EBADF:
                LOG_ERROR("Invalid file descriptor!\n");
                break;
        case EINVAL:
                LOG_ERROR("Invalid file arguments!\n");
                break;
        default:
                LOG_ERROR("Error closing file!\n");
        }

        return PQOS_RETVAL_ERROR;
}

/*
 * ---------------------------------------
 * CPU mask structures and utility functions
 * ---------------------------------------
 */

int
resctrl_alloc_cpumask_write(const unsigned class_id,
                            const struct resctrl_cpumask *mask)
{
        int ret = PQOS_RETVAL_OK;
        FILE *fd;

        fd = resctrl_alloc_fopen(class_id, rctl_cpus, "w");
        if (fd == NULL)
                return PQOS_RETVAL_ERROR;

        ret = resctrl_cpumask_write(fd, mask);
        if (ret == PQOS_RETVAL_OK)
                ret = resctrl_alloc_fclose(fd);
        else
                resctrl_alloc_fclose(fd);

        return ret;
}

int
resctrl_alloc_cpumask_read(const unsigned class_id,
                           struct resctrl_cpumask *mask)
{
        int ret;
        FILE *fd;

        fd = resctrl_alloc_fopen(class_id, rctl_cpus, "r");
        if (fd == NULL)
                return PQOS_RETVAL_ERROR;

        ret = resctrl_cpumask_read(fd, mask);

        if (resctrl_alloc_fclose(fd) != PQOS_RETVAL_OK)
                return PQOS_RETVAL_ERROR;

        return ret;
}

int
resctrl_alloc_schemata_read(const unsigned class_id,
                            struct resctrl_schemata *schemata)
{
        int ret = PQOS_RETVAL_OK;
        FILE *fd = NULL;

        ASSERT(schemata != NULL);

        fd = resctrl_alloc_fopen(class_id, rctl_schemata, "r");
        if (fd == NULL) {
                ret = PQOS_RETVAL_ERROR;
                goto resctrl_alloc_schemata_read_exit;
        }

        ret = resctrl_schemata_read(fd, schemata);

resctrl_alloc_schemata_read_exit:
        /* check if error occurred */
        if (ret == PQOS_RETVAL_OK)
                ret = resctrl_alloc_fclose(fd);
        else if (fd)
                resctrl_alloc_fclose(fd);

        return ret;
}

int
resctrl_alloc_schemata_write(const unsigned class_id,
                             const unsigned technology,
                             const struct resctrl_schemata *schemata)
{
        int ret = PQOS_RETVAL_OK;
        FILE *fd = NULL;
        const size_t buf_size = 16 * 1024;
        char *buf = calloc(buf_size, sizeof(*buf));

        if (buf == NULL) {
                ret = PQOS_RETVAL_ERROR;
                goto resctrl_alloc_schemata_write_exit;
        }

        ASSERT(schemata != NULL);

        fd = resctrl_alloc_fopen(class_id, rctl_schemata, "w");
        if (fd == NULL) {
                ret = PQOS_RETVAL_ERROR;
                goto resctrl_alloc_schemata_write_exit;
        }

        /* Enable fully buffered output. File won't be flushed until 16kB
         * buffer is full */
        if (setvbuf(fd, buf, _IOFBF, buf_size) != 0) {
                ret = PQOS_RETVAL_ERROR;
                goto resctrl_alloc_schemata_write_exit;
        }

        if ((technology & PQOS_TECHNOLOGY_L3CA) == PQOS_TECHNOLOGY_L3CA) {
                ret = resctrl_schemata_l3ca_write(fd, schemata);
                if (ret != PQOS_RETVAL_OK)
                        goto resctrl_alloc_schemata_write_exit;
        }
        if ((technology & PQOS_TECHNOLOGY_L2CA) == PQOS_TECHNOLOGY_L2CA) {
                ret = resctrl_schemata_l2ca_write(fd, schemata);
                if (ret != PQOS_RETVAL_OK)
                        goto resctrl_alloc_schemata_write_exit;
        }
        if ((technology & PQOS_TECHNOLOGY_MBA) == PQOS_TECHNOLOGY_MBA)
                ret = resctrl_schemata_mba_write(fd, schemata);

resctrl_alloc_schemata_write_exit:

        /* check if error occurred */
        if (ret == PQOS_RETVAL_OK)
                ret = resctrl_alloc_fclose(fd);
        else if (fd)
                resctrl_alloc_fclose(fd);

        /* setvbuf buffer should be freed after fclose */
        if (buf != NULL)
                free(buf);

        return ret;
}

/**
 * ---------------------------------------
 * Task utility functions
 * ---------------------------------------
 */

int
resctrl_alloc_task_validate(const pid_t task)
{
        if (kill(task, 0) == 0)
                return PQOS_RETVAL_OK;

        return PQOS_RETVAL_ERROR;
}

int
resctrl_alloc_task_write(const unsigned class_id, const pid_t task)
{
        FILE *fd;
        int ret;

        /* Check if task exists */
        ret = resctrl_alloc_task_validate(task);
        if (ret != PQOS_RETVAL_OK) {
                LOG_ERROR("Task %d does not exist!\n", (int)task);
                return PQOS_RETVAL_PARAM;
        }

        /* Open resctrl tasks file */
        fd = resctrl_alloc_fopen(class_id, rctl_tasks, "w");
        if (fd == NULL)
                return PQOS_RETVAL_ERROR;

        /* Write task ID to file */
        if (fprintf(fd, "%d\n", task) < 0) {
                LOG_ERROR("Failed to write to task %d to file!\n", (int)task);
                resctrl_alloc_fclose(fd);
                return PQOS_RETVAL_ERROR;
        }

        errno = 0;
        ret = resctrl_alloc_fclose(fd);
        if (ret != PQOS_RETVAL_OK && errno == ESRCH) {
                LOG_ERROR("Task %d does not exist! fclose\n", (int)task);
                ret = PQOS_RETVAL_PARAM;
        }

        return ret;
}

unsigned *
resctrl_alloc_task_read(unsigned class_id, unsigned *count)
{
        FILE *fd;
        unsigned *tasks = NULL, idx = 0;
        int ret;
        char buf[128];
        struct linked_list {
                uint64_t task_id;
                struct linked_list *next;
        } head, *current = NULL;

        /* Open resctrl tasks file */
        fd = resctrl_alloc_fopen(class_id, rctl_tasks, "r");
        if (fd == NULL)
                return NULL;

        head.next = NULL;
        current = &head;
        memset(buf, 0, sizeof(buf));
        while (fgets(buf, sizeof(buf), fd) != NULL) {
                uint64_t tmp;
                struct linked_list *p = NULL;

                ret = resctrl_utils_strtouint64(buf, 10, &tmp);
                if (ret != PQOS_RETVAL_OK)
                        goto resctrl_alloc_task_read_exit_clean;
                p = malloc(sizeof(head));
                if (p == NULL)
                        goto resctrl_alloc_task_read_exit_clean;
                p->task_id = tmp;
                p->next = NULL;
                current->next = p;
                current = p;
                idx++;
        }

        /* if no pids found then allocate empty buffer to be returned */
        if (idx == 0)
                tasks = (unsigned *)calloc(1, sizeof(tasks[0]));
        else
                tasks = (unsigned *)malloc(idx * sizeof(tasks[0]));
        if (tasks == NULL)
                goto resctrl_alloc_task_read_exit_clean;

        *count = idx;
        current = head.next;
        idx = 0;
        while (current != NULL) {
                tasks[idx++] = current->task_id;
                current = current->next;
        }

resctrl_alloc_task_read_exit_clean:
        resctrl_alloc_fclose(fd);
        current = head.next;
        while (current != NULL) {
                struct linked_list *tmp = current->next;

                free(current);
                current = tmp;
        }
        return tasks;
}

int
resctrl_alloc_task_search(unsigned *class_id,
                          const struct pqos_cap *cap,
                          const pid_t task)
{
        FILE *fd;
        unsigned i, max_cos = 0;
        int ret;

        /* Check if task exists */
        ret = resctrl_alloc_task_validate(task);
        if (ret != PQOS_RETVAL_OK) {
                LOG_ERROR("Task %d does not exist!\n", (int)task);
                return PQOS_RETVAL_PARAM;
        }

        /* Get number of COS */
        ret = resctrl_alloc_get_grps_num(cap, &max_cos);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        /**
         * Starting at highest COS - search all COS tasks files for task ID
         */
        for (i = (max_cos - 1); (int)i >= 0; i--) {
                uint64_t tid = 0;
                char buf[128];

                /* Open resctrl tasks file */
                fd = resctrl_alloc_fopen(i, rctl_tasks, "r");
                if (fd == NULL)
                        return PQOS_RETVAL_ERROR;

                /* Search tasks file for specified task ID */
                memset(buf, 0, sizeof(buf));
                while (fgets(buf, sizeof(buf), fd) != NULL) {
                        ret = resctrl_utils_strtouint64(buf, 10, &tid);
                        if (ret != PQOS_RETVAL_OK)
                                continue;

                        if (task == (pid_t)tid) {
                                *class_id = i;
                                if (resctrl_alloc_fclose(fd) != PQOS_RETVAL_OK)
                                        return PQOS_RETVAL_ERROR;

                                return PQOS_RETVAL_OK;
                        }
                }
                if (resctrl_alloc_fclose(fd) != PQOS_RETVAL_OK)
                        return PQOS_RETVAL_ERROR;
        }
        /* If not found in any COS group - return error */
        LOG_ERROR("Failed to get association for task %d!\n", (int)task);
        return PQOS_RETVAL_ERROR;
}

int
resctrl_alloc_task_file_check(const unsigned class_id, unsigned *found)
{
        FILE *fd;
        char buf[128];

        /* Open resctrl tasks file */
        fd = resctrl_alloc_fopen(class_id, rctl_tasks, "r");
        if (fd == NULL)
                return PQOS_RETVAL_ERROR;

        /* Search tasks file for any task ID */
        memset(buf, 0, sizeof(buf));
        if (fgets(buf, sizeof(buf), fd) != NULL)
                *found = 1;

        if (resctrl_alloc_fclose(fd) != PQOS_RETVAL_OK)
                return PQOS_RETVAL_ERROR;

        return PQOS_RETVAL_OK;
}

int
resctrl_alloc_assoc_set(const unsigned lcore, const unsigned class_id)
{
        int ret, ret_mon;
        struct resctrl_cpumask mask;
        char name[32];

        /* check if core is assigned to monitoring group */
        ret_mon = resctrl_mon_assoc_get(lcore, name, DIM(name));

        ret = resctrl_alloc_cpumask_read(class_id, &mask);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        resctrl_cpumask_set(lcore, &mask);

        ret = resctrl_alloc_cpumask_write(class_id, &mask);

        /* assign core back to monitoring group */
        if (ret_mon == PQOS_RETVAL_OK)
                resctrl_mon_assoc_set(lcore, name);

        return ret;
}

int
resctrl_alloc_assoc_get(const unsigned lcore, unsigned *class_id)
{
        int ret;
        unsigned grps;
        unsigned i;
        struct resctrl_cpumask mask;
        const struct pqos_cap *cap;

        _pqos_cap_get(&cap, NULL);

        ret = resctrl_alloc_get_grps_num(cap, &grps);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        for (i = 0; i < grps; i++) {
                ret = resctrl_alloc_cpumask_read(i, &mask);
                if (ret != PQOS_RETVAL_OK)
                        return ret;

                if (resctrl_cpumask_get(lcore, &mask)) {
                        *class_id = i;
                        return PQOS_RETVAL_OK;
                }
        }

        return ret;
}

int
resctrl_alloc_assoc_set_pid(const pid_t task, const unsigned class_id)
{
        /* Write to tasks file */
        return resctrl_alloc_task_write(class_id, task);
}

int
resctrl_alloc_assoc_get_pid(const pid_t task, unsigned *class_id)
{
        const struct pqos_cap *cap;

        _pqos_cap_get(&cap, NULL);

        /* Search tasks files */
        return resctrl_alloc_task_search(class_id, cap, task);
}

int
resctrl_alloc_get_unused_group(const unsigned grps_num, unsigned *group_id)
{
        unsigned used_groups[grps_num];
        unsigned i;
        int ret;

        if (group_id == NULL || grps_num == 0)
                return PQOS_RETVAL_PARAM;

        memset(used_groups, 0, sizeof(used_groups));

        for (i = grps_num - 1; i > 0; i--) {
                struct resctrl_cpumask mask;
                unsigned j;

                ret = resctrl_alloc_cpumask_read(i, &mask);
                if (ret != PQOS_RETVAL_OK)
                        return ret;

                for (j = 0; j < sizeof(mask.tab); j++)
                        if (mask.tab[j] > 0) {
                                used_groups[i] = 1;
                                break;
                        }

                if (used_groups[i] == 1)
                        continue;

                ret = resctrl_alloc_task_file_check(i, &used_groups[i]);
                if (ret != PQOS_RETVAL_OK)
                        return ret;
        }

        /* Find unused COS */
        for (i = grps_num - 1; i > 0; i--) {
                if (used_groups[i] == 0) {
                        *group_id = i;
                        return PQOS_RETVAL_OK;
                }
        }

        return PQOS_RETVAL_RESOURCE;
}
