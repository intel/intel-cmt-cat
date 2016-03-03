/*
 * BSD LICENSE
 *
 * Copyright(c) 2014-2016 Intel Corporation. All rights reserved.
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

/**
 * @brief CPU sockets and cores enumeration module.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>

#ifdef __FreeBSD__
#include <sys/param.h>   /* sched affinity */
#include <sys/cpuset.h>  /* sched affinity */
#include <unistd.h>      /* sysconf() */
#endif

#include "log.h"
#include "cpuinfo.h"
#include "types.h"
#include "list.h"
#ifdef __FreeBSD__
#include "machine.h"
#endif

/**
 * This structure will be made externally available
 *
 * If not NULL then module is initialized.
 */
static struct cpuinfo_topology *m_cpu = NULL;

/**
 * Core info structure needed internally
 * to build list of cores in the system
 */
struct core_info {
        struct list_item list;
        unsigned lcore;
        unsigned socket;
        unsigned cluster;
};

#ifdef __linux__
#ifndef PROC_CPUINFO_FILE_NAME
#define PROC_CPUINFO_FILE_NAME "/proc/cpuinfo"
#endif

/**
 * @brief Converts \a str to numeric value
 *
 * \a str is assumed to be in the following format:
 * "<some text>: <number_base_10>"
 *
 * @param [in] str string to be converted
 * @param [out] val place to store converted value
 *
 * @return Operation status
 * @retval 0 OK
 * @retval -1 conversion error
 */
static int
get_str_value(const char *str, unsigned *val)
{
        ASSERT(str != NULL);
        ASSERT(val != NULL);

        char *endptr = NULL;
        const char *s = strchr(str, ':');

        if (s == NULL)
                return -1;

        *val = (unsigned) strtoul(s+1, &endptr, 10);
        if (endptr != NULL && *endptr != '\0' && !isspace(*endptr))
                return -1;

        return 0;
}

/**
 * @brief Matches \a buf against "processor" and
 *        converts the number subsequent to the string.
 *
 * @param [in] buf string to be matched and converted
 * @param [out] lcore_id place to store converted number
 *
 * @return Operation status
 * @retval 0 OK
 * @retval -1 conversion error
 */
static int
parse_processor_id(const char *buf, unsigned *lcore_id)
{
        static const char *_processor = "processor";

        ASSERT(buf != NULL);
        ASSERT(lcore_id != NULL);

        if (strncmp(buf, _processor, strlen(_processor)) != 0)
                return -1;

        return get_str_value(buf, lcore_id);
}

/**
 * @brief Matches \a buf against "physical id" and
 *        converts the number subsequent to the string.
 *
 * @param [in] buf string to be matched and converted
 * @param [out] socket_id place to store converted number
 *
 * @return Operation status
 * @retval 0 OK
 * @retval -1 conversion error
 */
static int
parse_socket_id(const char *buf, unsigned *socket_id)
{
        static const char *_physical_id = "physical id";

        ASSERT(buf != NULL);
        ASSERT(socket_id != NULL);

        if (strncmp(buf, _physical_id, strlen(_physical_id)) != 0)
                return -1;

        return get_str_value(buf, socket_id);
}

/**
 * @brief Builds CPU topology structure (Linux)
 *
 * It scans /proc/cpuinfo to discover
 * CPU sockets and logical cores in the system.
 * Based on this data it builds structure with
 * system CPU information.
 *
 * @return Pointer to CPU topology structure
 * @retval NULL on error
 */
static struct cpuinfo_topology *cpuinfo_build_topo(void)
{
        FILE *f = NULL;
        unsigned lcore_id = 0, socket_id = 0,
                core_count = 0, sockets = 0,
                di = 0;
        LIST_HEAD_DECLARE(core_list);
        struct list_item *p = NULL, *aux = NULL;
        char buf[160];
        struct cpuinfo_topology *l_cpu = NULL;

        /**
         * Open cpuinfo file in proc file system
         */
        f = fopen(PROC_CPUINFO_FILE_NAME, "r");
        if (f == NULL) {
                LOG_ERROR("Cannot open " PROC_CPUINFO_FILE_NAME "\n");
                return NULL;
        }

        /**
         * Parse lines of /proc/cpuinfo and fill in core_table
         */
        while (fgets(buf, sizeof(buf), f) != NULL) {
                if (parse_processor_id(buf, &lcore_id) == 0)
                        continue;

                if (parse_socket_id(buf, &socket_id) == 0) {
                        sockets++;
                        continue;
                }

                if (buf[0] == '\n') {
                        struct core_info *inf =
                                (struct core_info *) malloc(sizeof(*inf));

                        ASSERT(inf != NULL);
                        if (inf == NULL)
                                goto cpuinfo_build_topo_error;

                        LOG_DEBUG("Detected core %u on socket %u, cluster %u\n",
                                  lcore_id, socket_id, socket_id);

                        /**
                         * Fill in logical core data
                         */
                        inf->lcore = lcore_id;
                        inf->socket = socket_id;
                        inf->cluster = socket_id;
                        list_add_tail(&inf->list, &core_list);

                        core_count++;
                }
        }

        if (!sockets) {
                LOG_INFO("No physical id detected in "
                         PROC_CPUINFO_FILE_NAME ".\n"
                         "Most likely running on a VM.\n");
        }

        /**
         * Fill in l_cpu structure that will be made
         * visible to other modules
         */
        l_cpu = (struct cpuinfo_topology *)
                malloc(sizeof(*l_cpu) +
                       (core_count * sizeof(struct cpuinfo_core)));

        if (l_cpu == NULL)
                goto cpuinfo_build_topo_error;

        l_cpu->num_cores = core_count;
        di = 0;

        list_for_each(p, aux, &core_list) {
                struct core_info *inf =
                        list_get_container(p, struct core_info, list);

                l_cpu->cores[di].lcore = inf->lcore;
                l_cpu->cores[di].socket = inf->socket;
                l_cpu->cores[di].cluster = inf->cluster;
                di++;
        }
        ASSERT(di == core_count);

 cpuinfo_build_topo_error:
        fclose(f);

        list_for_each(p, aux, &core_list) {
                struct core_info *inf =
                        list_get_container(p, struct core_info, list);

                list_del(p);
                free(inf);
        }

        return l_cpu;
}
#endif /* __linux__ */

#ifdef __FreeBSD__
struct apic_info {
        uint32_t smt_mask;      /**< mask to get SMT ID */
        uint32_t smt_size;      /**< size of SMT ID mask */
        uint32_t core_mask;     /**< mask to get CORE ID */
        uint32_t core_smt_mask; /**< mask to get CORE+SMT ID */
        uint32_t pkg_mask;      /**< mask to get PACKAGE ID */
        uint32_t pkg_shift;     /**< bits to shift to get PACKAGE ID */
};

/**
 * @brief Discovers APICID structure information
 *
 * Uses CPUID leaf 0xB to find SMT and CORE APICID information.
 *
 * @param [out] apic structure to be filled in with details
 *
 * @return Operation status
 * @retval 0 OK
 * @retval -1 error
 */
static int detect_apic_masks(struct apic_info *apic)
{
        int core_reported = 0;
        int thread_reported = 0;
        unsigned subleaf = 0;

        if (apic == NULL)
                return -1;

        memset(apic, 0, sizeof(*apic));

        for (subleaf = 0; ; subleaf++) {
                struct cpuid_out leafB;
                unsigned level_type, level_shift;
                uint32_t mask;

                lcpuid(0xb, subleaf, &leafB);
                if (leafB.ebx == 0) /* invalid subleaf */
                        break;

                level_type = (leafB.ecx >> 8) & 0xff;     /* ECX bits 15:8 */
                level_shift = leafB.eax & 0x1f;           /* EAX bits 4:0 */
                mask = ~(((uint32_t)-1) << level_shift);

                if (level_type == 1) {
                        /* level_type 1 is for SMT  */
                        apic->smt_mask = mask;
                        apic->smt_size = level_shift;
                        thread_reported = 1;
                }
                if (level_type == 2) {
                        /* level_type 2 is for CORE */
                        apic->core_smt_mask = mask;
                        apic->pkg_shift = level_shift;
                        apic->pkg_mask = ~mask;
                        core_reported = 1;
                }
        }

        if (!thread_reported)
                return -1;

        if (core_reported) {
                apic->core_mask = apic->core_smt_mask ^ apic->smt_mask;
        } else {
                apic->core_mask = 0;
                apic->pkg_shift = apic->smt_size;
                apic->pkg_mask = ~apic->smt_mask;
        }

        return 0;
}

/**
 * @brief Detects CPU information
 *
 * - schedules current task to run on \a cpu
 * - runs CPUID leaf 0xB to get cpu's APICID
 * - uses \a apic information to retrieve socket id from the APICID
 *
 * @param [in] cpu logical cpu id used by OS
 * @param [in] apic information about APICID structure
 * @param [out] info cpu information structure to be filled by the function
 *
 * @return Operation status
 * @retval 0 OK
 * @retval -1 error
 */
static int
detect_cpu(const int cpu,
           const struct apic_info *apic,
           struct cpuinfo_core *info)
{
        cpuset_t new_mask;
        struct cpuid_out leafB;
        uint32_t apicid;

        CPU_ZERO(&new_mask);
        CPU_SET(cpu, &new_mask);
        if (cpuset_setaffinity(CPU_LEVEL_WHICH, CPU_WHICH_TID, -1,
                               sizeof(new_mask), &new_mask) != 0)
                return -1;

        lcpuid(0xb, 0, &leafB);
        apicid = leafB.edx;        /* x2APIC ID */

        info->lcore = cpu;
        info->socket = (apicid & apic->pkg_mask) >> apic->pkg_shift;
        info->cluster = info->socket;

        LOG_DEBUG("Detected core %u, socket %u, cluster %u, APICID core %u\n",
                  info->lcore, info->socket, info->cluster,
                  apicid & apic->core_smt_mask);

        return 0;
}

/**
 * @brief Builds CPU topology structure (FreeBSD)
 *
 * - saves current task CPU affinity
 * - retrieves number of processors in the system
 * - detects information on APICID structure
 * - for each processor in the system:
 *      - change affinity to run only on the processor
 *      - read APICID of current processor with CPUID
 *      - retrieve package & cluster data from the APICID
 * - restores initial task CPU affinity
 *
 * @return Pointer to cpu topology structure
 * @retval NULL on error
 */
static struct cpuinfo_topology *cpuinfo_build_topo(void)
{
        unsigned i, max_core_count, core_count = 0;
        struct cpuinfo_topology *l_cpu = NULL;
        cpuset_t current_mask;
        struct apic_info apic;

        if (cpuset_getaffinity(CPU_LEVEL_WHICH, CPU_WHICH_TID, -1,
                               sizeof(current_mask), &current_mask) == -1) {
                LOG_ERROR("Error retrieving CPU affinity mask!");
                return NULL;
        }

        max_core_count = sysconf(_SC_NPROCESSORS_CONF);
        if (max_core_count == 0) {
                LOG_ERROR("Zero processors in the system!");
                return NULL;
        }

        if (detect_apic_masks(&apic) != 0) {
                LOG_ERROR("Couldn't retrieve APICID structure information!");
                return NULL;
        }

        l_cpu = (struct cpuinfo_topology *)
                malloc(sizeof(*l_cpu) +
                       (max_core_count * sizeof(struct cpuinfo_core)));

        for (i = 0; i < max_core_count; i++)
                if (detect_cpu(i, &apic, &l_cpu->cores[core_count]) == 0)
                        core_count++;

        if (cpuset_setaffinity(CPU_LEVEL_WHICH, CPU_WHICH_TID, -1,
                               sizeof(current_mask), &current_mask) != 0) {
                LOG_ERROR("Couldn't restore original CPU affinity mask!");
                free(l_cpu);
                return NULL;
        }

        l_cpu->num_cores = core_count;

        if (core_count == 0) {
                free(l_cpu);
                l_cpu = NULL;
        }

        return l_cpu;
}
#endif /* __FreeBSD__ */

/**
 * Detect number of logical processors on the machine
 * and their location.
 */
int
cpuinfo_init(const struct cpuinfo_topology **topology)
{
        ASSERT(m_cpu == NULL);
        if (m_cpu != NULL)
                return CPUINFO_RETVAL_ERROR;

        m_cpu = cpuinfo_build_topo();
        if (m_cpu == NULL) {
                LOG_ERROR("CPU topology detection error!");
                return CPUINFO_RETVAL_ERROR;
        }

        if (topology != NULL)
                *topology = m_cpu;

        return CPUINFO_RETVAL_OK;
}

int
cpuinfo_fini(void)
{
        ASSERT(m_cpu != NULL);
        if (m_cpu == NULL)
                return CPUINFO_RETVAL_ERROR;
        free(m_cpu);
        m_cpu = NULL;
        return CPUINFO_RETVAL_OK;
}

int
cpuinfo_get(const struct cpuinfo_topology **topology)
{
        ASSERT(topology != NULL);
        if (topology == NULL)
                return CPUINFO_RETVAL_PARAM;

        ASSERT(m_cpu != NULL);
        if (m_cpu == NULL)
                return CPUINFO_RETVAL_ERROR;

        *topology = m_cpu;
        return CPUINFO_RETVAL_OK;
}
