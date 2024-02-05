/*
 * BSD LICENSE
 *
 * Copyright(c) 2021-2023 Intel Corporation. All rights reserved.
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

#include "os_cpuinfo.h"

#include "assert.h"
#include "common.h"
#include "log.h"

#include <dirent.h> /**< scandir() */
#include <errno.h>
#include <fnmatch.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#define SYSTEM_CPU  "/sys/devices/system/cpu"
#define SYSTEM_NODE "/sys/devices/system/node"

/**
 * @brief Filter directory filenames
 *
 * This function is used by the scandir function to filter directories
 *
 * @param dir dirent structure containing directory info
 *
 * @return if directory entry should be included in scandir() output list
 * @retval 0 means don't include the entry
 * @retval 1 means include the entry
 */
#define FILTER(PREFIX)                                                         \
        static int filter_##PREFIX(const struct dirent *dir)                   \
        {                                                                      \
                return fnmatch(#PREFIX "[0-9]*", dir->d_name, 0) == 0;         \
        }

FILTER(cpu)
FILTER(node)
FILTER(index)

/**
 * @brief Converts string into unsigned number.
 *
 * @param [in] str string to be converted into unsigned number
 * @param [out] value Numeric value of the string representing the number
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK on success
 */
static int
parse_uint(const char *str, unsigned *value)
{
        unsigned long val;
        char *endptr = NULL;

        ASSERT(str != NULL);
        ASSERT(value != NULL);

        errno = 0;
        val = strtoul(str, &endptr, 0);
        if (!(*str != '\0' && (*endptr == '\0' || *endptr == '\n')))
                return PQOS_RETVAL_ERROR;

        if (val <= UINT_MAX) {
                *value = val;
                return PQOS_RETVAL_OK;
        }

        return PQOS_RETVAL_ERROR;
}

/**
 * @brief Sort directory filenames
 *
 * @param dir1 dirent structure containing directory info
 * @param dir2 dirent structure containing directory info
 *
 * @return directory names comparison result
 */
static int
cpu_sort(const struct dirent **dir1, const struct dirent **dir2)
{
        unsigned cpu1 = 0;
        unsigned cpu2 = 0;

        parse_uint((*dir1)->d_name + 3, &cpu1);
        parse_uint((*dir2)->d_name + 3, &cpu2);

        return cpu1 - cpu2;
}

/**
 * @brief Detects if \a lcore is online
 *
 * @param [in] lcore Logical core id
 *
 * @return if logical core is online
 */
PQOS_STATIC int
os_cpuinfo_cpu_online(unsigned lcore)
{
        char buf[256];
        unsigned online = 0;
        int retval;

        snprintf(buf, sizeof(buf) - 1, SYSTEM_CPU "/cpu%u/online", lcore);
        retval = pqos_fread_uint(buf, &online);
        /* online file does not exists for cpu 0 */
        if (retval == PQOS_RETVAL_RESOURCE)
                online = 1;

        return online;
}

/**
 * @brief Detects node of \a lcore
 *
 * @param [in] lcore Logical core id
 * @param [out] node Detected node
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 */
PQOS_STATIC int
os_cpuinfo_cpu_node(unsigned lcore, unsigned *node)
{
        char buf[256];
        struct dirent **namelist = NULL;
        int i;
        int count;
        int ret = PQOS_RETVAL_ERROR;

        snprintf(buf, sizeof(buf) - 1, SYSTEM_CPU "/cpu%u", lcore);
        count = scandir(buf, &namelist, filter_node, NULL);
        if (count == 1)
                ret = parse_uint(namelist[0]->d_name + 4, node);

        for (i = 0; i < count; i++)
                free(namelist[i]);
        free(namelist);

        return ret;
}

/**
 * @brief Detects socket of \a lcore
 *
 * @param [in] lcore Logical core id
 * @param [out] socket Detected socket
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 */
PQOS_STATIC int
os_cpuinfo_cpu_socket(unsigned lcore, unsigned *socket)
{
        char buf[256];

        snprintf(buf, sizeof(buf) - 1,
                 SYSTEM_CPU "/cpu%u/topology/physical_package_id", lcore);
        return pqos_fread_uint(buf, socket);
}

/**
 * @brief Detects cache topology of \a lcore
 *
 * @param [in] lcore Logical core id
 * @param [out] l3id L3 cache id
 * @param [out] l2id L2 cache id
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 */
PQOS_STATIC int
os_cpuinfo_cpu_cache(unsigned lcore, unsigned *l3, unsigned *l2)
{
        char buf[512];
        struct dirent **namelist = NULL;
        int i;
        int count;
        int ret = PQOS_RETVAL_ERROR;

        snprintf(buf, sizeof(buf) - 1, SYSTEM_CPU "/cpu%u/cache", lcore);
        count = scandir(buf, &namelist, filter_index, NULL);
        for (i = 0; i < count; ++i) {
                unsigned level;
                unsigned id;

                snprintf(buf, sizeof(buf) - 1,
                         SYSTEM_CPU "/cpu%u/cache/%s/level", lcore,
                         namelist[i]->d_name);
                ret = pqos_fread_uint(buf, &level);
                if (ret != PQOS_RETVAL_OK)
                        break;

                snprintf(buf, sizeof(buf) - 1, SYSTEM_CPU "/cpu%u/cache/%s/id",
                         lcore, namelist[i]->d_name);
                ret = pqos_fread_uint(buf, &id);
                if (ret != PQOS_RETVAL_OK)
                        break;

                if (level == 2)
                        *l2 = id;
                else if (level == 3)
                        *l3 = id;
        }

        for (i = 0; i < count; i++)
                free(namelist[i]);
        free(namelist);

        return ret;
}

/**
 * @brief Builds CPU topology structure
 *
 * @return Pointer to CPU topology structure
 * @retval NULL on error
 */
struct pqos_cpuinfo *
os_cpuinfo_topology(void)
{
        struct pqos_cpuinfo *cpu = NULL;
        struct dirent **namelist = NULL;
        int max_core_count;
        int num_cpus;
        int i;
        int retval = PQOS_RETVAL_OK;

        max_core_count = sysconf(_SC_NPROCESSORS_CONF);
        if (max_core_count < 0) {
                LOG_ERROR("Failed to get number of processors!\n");
                return NULL;
        } else if (max_core_count == 0) {
                LOG_ERROR("Zero processors in the system!\n");
                return NULL;
        }

        if (pqos_set_no_files_limit(max_core_count)) {
                LOG_ERROR("Open files limit not sufficient!\n");
                return NULL;
        }

        const size_t mem_sz =
            sizeof(*cpu) + (max_core_count * sizeof(struct pqos_coreinfo));

        cpu = (struct pqos_cpuinfo *)malloc(mem_sz);
        if (cpu == NULL) {
                LOG_ERROR("Couldn't allocate CPU topology structure!\n");
                return NULL;
        }
        cpu->mem_size = (unsigned)mem_sz;
        memset(cpu, 0, mem_sz);

        num_cpus = scandir(SYSTEM_CPU, &namelist, filter_cpu, cpu_sort);
        if (num_cpus <= 0 || max_core_count < num_cpus) {
                LOG_ERROR("Failed to read proc cpus!\n");
                free(cpu);
                return NULL;
        }

        for (i = 0; i < num_cpus; i++) {
                unsigned lcore = atoi(namelist[i]->d_name + 3);
                struct pqos_coreinfo *info = &cpu->cores[cpu->num_cores];

                if (!os_cpuinfo_cpu_online(lcore))
                        continue;

                retval = os_cpuinfo_cpu_socket(lcore, &info->socket);
                if (retval != PQOS_RETVAL_OK)
                        break;

                retval = os_cpuinfo_cpu_node(lcore, &info->numa);
                if (retval != PQOS_RETVAL_OK)
                        break;

                retval =
                    os_cpuinfo_cpu_cache(lcore, &info->l3_id, &info->l2_id);
                if (retval != PQOS_RETVAL_OK)
                        break;

                info->lcore = lcore;

                LOG_DEBUG("Detected core %u, socket %u, "
                          "NUMAnode %u, "
                          "L2 ID %u, L3 ID %u\n",
                          info->lcore, info->socket, info->numa, info->l2_id,
                          info->l3_id);

                cpu->num_cores++;
        }

        for (i = 0; i < num_cpus; i++)
                free(namelist[i]);
        free(namelist);

        if (retval != PQOS_RETVAL_OK) {
                free(cpu);
                return NULL;
        }

        return cpu;
}

/**
 * @brief Provides total NUMA nodes count
 *
 * @return Total NUMA nodes count
 */

int
os_cpuinfo_get_numa_num(void)
{
        int count;
        struct dirent **namelist = NULL;
        int i;

        count = scandir(SYSTEM_NODE, &namelist, filter_node, NULL);
        for (i = 0; i < count; ++i)
                free(namelist[i]);
        free(namelist);
        return count;
}

/**
 * @brief Provides total sockets count
 *
 * @return Total sockets count
 */
int
os_cpuinfo_get_socket_num(void)
{
        struct dirent **namelist = NULL;
        int max_core_count;
        int num_cpus;
        unsigned num_sockets = 0;
        unsigned *sockets;
        int i;
        int retval = PQOS_RETVAL_OK;

        max_core_count = sysconf(_SC_NPROCESSORS_CONF);
        if (max_core_count < 0) {
                LOG_ERROR("Failed to get number of processors!\n");
                return -1;
        } else if (max_core_count == 0) {
                LOG_ERROR("Zero processors in the system!\n");
                return -1;
        }

        num_cpus = scandir(SYSTEM_CPU, &namelist, filter_cpu, cpu_sort);
        if (num_cpus <= 0 || max_core_count < num_cpus) {
                LOG_ERROR("Failed to read proc cpus!\n");
                return -1;
        }

        sockets = malloc(sizeof(*sockets) * num_cpus);

        for (i = 0; i < num_cpus; i++) {
                unsigned lcore = atoi(namelist[i]->d_name + 3);
                unsigned socket;
                unsigned s;

                if (!os_cpuinfo_cpu_online(lcore))
                        continue;

                retval = os_cpuinfo_cpu_socket(lcore, &socket);
                if (retval != PQOS_RETVAL_OK)
                        break;

                /* Check if this socket id is already on the list */
                for (s = 0; s < num_sockets; ++s)
                        if (socket == sockets[s])
                                break;

                if (s >= num_sockets || num_sockets == 0) {
                        /* This socket wasn't reported before */
                        sockets[num_sockets++] = socket;
                }
        }

        for (i = 0; i < num_cpus; i++)
                free(namelist[i]);
        free(namelist);
        free(sockets);

        if (retval != PQOS_RETVAL_OK)
                return -1;

        return num_sockets;
}
