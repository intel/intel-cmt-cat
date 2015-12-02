/*
 * BSD LICENSE
 *
 * Copyright(c) 2014-2015 Intel Corporation. All rights reserved.
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

#include "log.h"
#include "cpuinfo.h"
#include "types.h"
#include "list.h"

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

#ifndef PROC_CPUINFO_FILE_NAME
#define PROC_CPUINFO_FILE_NAME "/proc/cpuinfo"
#endif

/**
 * @brief Converts \a str to numeric value
 *
 * \a str is assumed to be in the follwing format:
 * "<some text>: <number_base_10>"
 *
 * @param str string to be converted
 * @param val place to store converted value
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
 *        converst the number subsequent to the string.
 *
 * @param buf string to be matched and converted
 * @param lcore_id place to store converted number
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
 *        converst the number subsequent to the string.
 *
 * @param buf string to be matched and converted
 * @param socket_id place to store converted number
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
 * Parse /proc/cpuinfo to get the number of logical
 * processors on the machine.
 */
int
cpuinfo_init(const struct cpuinfo_topology **topology)
{
        FILE *f = NULL;
        unsigned lcore_id = 0, socket_id = 0,
                core_count = 0, sockets = 0,
                di = 0;
        LIST_HEAD_DECLARE(core_list);
        struct list_item *p = NULL, *aux = NULL;
        char buf[160];
        int retval = CPUINFO_RETVAL_OK;

        ASSERT(m_cpu == NULL);
        if (m_cpu != NULL)
                return CPUINFO_RETVAL_ERROR;

        /**
         * Open cpuinfo file in proc file system
         */
        f = fopen(PROC_CPUINFO_FILE_NAME, "r");
        if (f == NULL) {
                LOG_ERROR("Cannot open " PROC_CPUINFO_FILE_NAME "\n");
                return CPUINFO_RETVAL_ERROR;
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
                        if (inf == NULL) {
                                retval = CPUINFO_RETVAL_ERROR;
                                goto cpuinfo_init_error;
                        }

                        LOG_INFO("Detected core %u on socket %u, cluster %u\n",
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
         * Fill in m_cpu structure that will be made
         * visible to other modules
         */
        m_cpu = (struct cpuinfo_topology *)
                malloc(sizeof(*m_cpu) +
                       (core_count * sizeof(struct cpuinfo_core)));

        ASSERT(m_cpu != NULL);

        if (m_cpu == NULL) {
                retval = CPUINFO_RETVAL_ERROR;
                goto cpuinfo_init_error;
        }

        m_cpu->num_cores = core_count;
        di = 0;

        list_for_each(p, aux, &core_list) {
                struct core_info *inf =
                        list_get_container(p, struct core_info, list);

                m_cpu->cores[di].lcore = inf->lcore;
                m_cpu->cores[di].socket = inf->socket;
                m_cpu->cores[di].cluster = inf->cluster;
                di++;
        }
        ASSERT(di == core_count);

 cpuinfo_init_error:
        fclose(f);

        list_for_each(p, aux, &core_list) {
                struct core_info *inf =
                        list_get_container(p, struct core_info, list);

                list_del(p);
                free(inf);
        }

        ASSERT(list_empty(&core_list));

        if (retval == CPUINFO_RETVAL_OK &&
            topology != NULL) {
                *topology = m_cpu;
        }

        return retval;
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
