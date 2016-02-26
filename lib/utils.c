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
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.O
 *
 */

/**
 * @brief  Set of utility functions to operate on Platform QoS (pqos) data structures.
 *
 * These functions need no synchronization mechanisms.
 *
 */
#include <stdlib.h>
#include <string.h>
#ifdef __linux__
#include <alloca.h>       /* alloca() */
#endif /* __linux__ */

#include "pqos.h"
#include "types.h"

/**
 * @brief Calculates number of sockets in CPU topology
 *
 * @param [in] cpu CPU topology
 *
 * @return Number of CPU sockets
 * @retval 0 on error
 */
static unsigned
__get_num_sockets(const struct pqos_cpuinfo *cpu)
{
        unsigned scount = 0, i = 0;

        ASSERT(cpu != NULL);
        if (cpu == NULL)
                return 0;

        ASSERT(cpu->num_cores > 0);
        if (cpu->num_cores == 0)
                return 0;

        for (i = 1, scount = 1; i < cpu->num_cores; i++) {
                /**
                 * Check if this socket id was already counted in
                 */
                unsigned j = 0;

                for (j = 0; j < i; j++)
                        if (cpu->cores[i].socket == cpu->cores[j].socket)
                                break;

                if (j >= i)
                        scount++; /**< socket not reported yet */
        }

        return scount;
}

int
pqos_cpu_get_num_sockets(const struct pqos_cpuinfo *cpu,
			 unsigned *count)
{
        ASSERT(cpu != NULL);
        ASSERT(count != NULL);

        if (cpu == NULL || count == NULL)
		return PQOS_RETVAL_PARAM;
	*count = __get_num_sockets(cpu);
	if (*count == 0)
		return PQOS_RETVAL_ERROR;
	return PQOS_RETVAL_OK;
}

int
pqos_cpu_get_sockets(const struct pqos_cpuinfo *cpu,
                     const unsigned max_count,
                     unsigned *count,
                     unsigned *sockets)
{
        unsigned scount = 0, i = 0;

        ASSERT(cpu != NULL);
        ASSERT(count != NULL);
        ASSERT(sockets != NULL);
        ASSERT(max_count > 0);
        if (cpu == NULL || count == NULL ||
            sockets == NULL || max_count == 0)
                return PQOS_RETVAL_PARAM;

        for (i = 1; i < cpu->num_cores; i++) {
                unsigned j = 0;

                /**
                 * Check if this socket id is already on the \a sockets list
                 */
                for (j = 0; j < scount && scount > 0; j++)
                        if (cpu->cores[i].socket == sockets[j])
                                break;

                if (j >= scount || scount == 0) {
                        /**
                         * This socket wasn't reported before
                         */
                        if (scount >= max_count)
                                return PQOS_RETVAL_ERROR;
                        sockets[scount] = cpu->cores[i].socket;
                        scount++;
                }
        }

        *count = scount;
        return PQOS_RETVAL_OK;
}

int
pqos_cpu_get_cores(const struct pqos_cpuinfo *cpu,
                   const unsigned socket,
                   const unsigned max_count,
                   unsigned *count,
                   unsigned *cores)
{
        unsigned i = 0, cnt = 0;

        ASSERT(cpu != NULL);
        ASSERT(count != NULL);
        ASSERT(cores != NULL);
        ASSERT(max_count > 0);

        if (cpu == NULL || count == NULL ||
            cores == NULL || max_count == 0)
                return PQOS_RETVAL_PARAM;

        for (i = 0; i < cpu->num_cores; i++)
                if (cpu->cores[i].socket == socket) {
                        if (max_count == 1) {
                                /**
                                 * Special case when app wants to get
                                 * just one core for the socket
                                 */
                                *cores = cpu->cores[i].lcore;
                                *count = 1;
                                return PQOS_RETVAL_OK;
                        }

                        /**
                         * Is there is more cores than \a cores can accommodate?
                         */
                        if (cnt >= max_count)
                                return PQOS_RETVAL_ERROR;
                        cores[cnt] = cpu->cores[i].lcore;
                        cnt++;
                }

        if (!cnt)
                return PQOS_RETVAL_ERROR;               /**< no core found */

        *count = cnt;
        return PQOS_RETVAL_OK;
}

int
pqos_cpu_check_core(const struct pqos_cpuinfo *cpu,
                    const unsigned lcore)
{
        unsigned i = 0;

        ASSERT(cpu != NULL);
        if (cpu == NULL)
                return PQOS_RETVAL_PARAM;

        for (i = 0; i < cpu->num_cores; i++)
                if (cpu->cores[i].lcore == lcore)
                        return PQOS_RETVAL_OK;

        return PQOS_RETVAL_ERROR;
}

int
pqos_cpu_get_socketid(const struct pqos_cpuinfo *cpu,
                      const unsigned lcore,
                      unsigned *socket)
{
        unsigned i = 0;

        if (cpu == NULL || socket == NULL)
                return PQOS_RETVAL_PARAM;

        for (i = 0; i < cpu->num_cores; i++)
                if (cpu->cores[i].lcore == lcore) {
                        *socket = cpu->cores[i].socket;
                        return PQOS_RETVAL_OK;
                }

        return PQOS_RETVAL_ERROR;
}

int
pqos_cpu_get_clusterid(const struct pqos_cpuinfo *cpu,
                       const unsigned lcore,
                       unsigned *cluster)
{
        unsigned i = 0;

        if (cpu == NULL || cluster == NULL)
                return PQOS_RETVAL_PARAM;

        for (i = 0; i < cpu->num_cores; i++)
                if (cpu->cores[i].lcore == lcore) {
                        *cluster = cpu->cores[i].cluster;
                        return PQOS_RETVAL_OK;
                }

        return PQOS_RETVAL_ERROR;
}

int
pqos_cap_get_type(const struct pqos_cap *cap,
                  const enum pqos_cap_type type,
                  const struct pqos_capability **cap_item)
{
        int ret = PQOS_RETVAL_ERROR;
        unsigned i;

        ASSERT(cap != NULL && cap_item != NULL);
        if (cap == NULL || cap_item == NULL)
                return PQOS_RETVAL_PARAM;

        ASSERT(type < PQOS_CAP_TYPE_NUMOF);
        if (type >= PQOS_CAP_TYPE_NUMOF)
                return PQOS_RETVAL_PARAM;

        for (i = 0; i < cap->num_cap; i++) {
                if (cap->capabilities[i].type != type)
                        continue;
                *cap_item = &cap->capabilities[i];
                ret = PQOS_RETVAL_OK;
                break;
        }

        return ret;
}

int
pqos_cap_get_event(const struct pqos_cap *cap,
                   const enum pqos_mon_event event,
                   const struct pqos_monitor **p_mon)
{
        const struct pqos_capability *cap_item = NULL;
        const struct pqos_cap_mon *mon = NULL;
        int ret = PQOS_RETVAL_OK;
        unsigned i;

        if (cap == NULL || p_mon == NULL)
                return PQOS_RETVAL_PARAM;

        ret = pqos_cap_get_type(cap, PQOS_CAP_TYPE_MON, &cap_item);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        ASSERT(cap_item != NULL);
        mon = cap_item->u.mon;

        ret = PQOS_RETVAL_ERROR;

        for (i = 0; i < mon->num_events; i++) {
                if (mon->events[i].type == event) {
                        *p_mon = &mon->events[i];
                        ret = PQOS_RETVAL_OK;
                        break;
                }
        }

        return ret;
}

int
pqos_l3ca_get_cos_num(const struct pqos_cap *cap,
                       unsigned *cos_num)
{
        const struct pqos_capability *item = NULL;
        int ret = PQOS_RETVAL_OK;

        ASSERT(cap != NULL && cos_num != NULL);
        if (cap == NULL || cos_num == NULL)
                return PQOS_RETVAL_PARAM;

        ret = pqos_cap_get_type(cap, PQOS_CAP_TYPE_L3CA, &item);
        if (ret != PQOS_RETVAL_OK)
                return ret;                           /**< no L3CA capability */

        ASSERT(item != NULL);
        *cos_num = item->u.l3ca->num_classes;
        return ret;
}

int
pqos_l3ca_cdp_enabled(const struct pqos_cap *cap,
                      int *cdp_supported,
                      int *cdp_enabled)
{
        const struct pqos_capability *item = NULL;
        int ret = PQOS_RETVAL_OK;

        ASSERT(cap != NULL && (cdp_enabled != NULL || cdp_supported != NULL));
        if (cap == NULL || (cdp_enabled == NULL && cdp_supported == NULL))
                return PQOS_RETVAL_PARAM;

        ret = pqos_cap_get_type(cap, PQOS_CAP_TYPE_L3CA, &item);
        if (ret != PQOS_RETVAL_OK)
                return ret;                           /**< no L3CA capability */

        ASSERT(item != NULL);
        if (cdp_supported != NULL)
                *cdp_supported = item->u.l3ca->cdp;
        if (cdp_enabled != NULL)
                *cdp_enabled = item->u.l3ca->cdp_on;
        return ret;
}

int
pqos_l3ca_reset(const struct pqos_cap *cap,
                const struct pqos_cpuinfo *cpu)
{
        unsigned *sockets = NULL;
        unsigned sockets_num = 0, sockets_num_ret = 0;
        const struct pqos_capability *item = NULL;
        int ret = PQOS_RETVAL_OK;
        unsigned j;

        ASSERT(cap != NULL && cpu != NULL);
        if (cap == NULL || cpu == NULL)
                return PQOS_RETVAL_PARAM;

        ret = pqos_cap_get_type(cap, PQOS_CAP_TYPE_L3CA, &item);
        if (ret != PQOS_RETVAL_OK)
                return ret;                           /**< no L3CA capability */
        ASSERT(item != NULL);

        /**
         * Figure out number of sockets in the system
         * - allocate enough memory to accommodate all socket id's
         * - use stack frame allocator for it (not heap)
         * - get list of socket id's through another API
         * - validate that number of socket id's obtained in
         *   two different ways match
         */
        sockets_num = __get_num_sockets(cpu);
        if (sockets_num == 0)
                return PQOS_RETVAL_RESOURCE;

        sockets = alloca(sockets_num * sizeof(sockets[0]));
        if (sockets == NULL)
                return PQOS_RETVAL_RESOURCE;

        ret = pqos_cpu_get_sockets(cpu, sockets_num, &sockets_num_ret, sockets);
        if (ret != PQOS_RETVAL_OK)
                return ret;
        ASSERT(sockets_num_ret == sockets_num);
        if (sockets_num != sockets_num_ret)
                return PQOS_RETVAL_ERROR;

        /**
         * Change COS definition on all sockets
         * so that each COS allows for access to all cache ways
         */
        for (j = 0; j < sockets_num; j++) {
                const uint64_t ways_mask = (1ULL<<item->u.l3ca->num_ways)-1ULL;
                unsigned i;

                for (i = 0; i < item->u.l3ca->num_classes; i++) {
                        struct pqos_l3ca cos;

                        cos.cdp = 0;
                        cos.class_id = i;
                        cos.ways_mask = ways_mask;
                        ret = pqos_l3ca_set(sockets[j], 1, &cos);
                        if (ret != PQOS_RETVAL_OK)
                                return ret;
                }
        }

        /**
         * Associate all cores with COS0
         */
        for (j = 0; j < cpu->num_cores; j++) {
                ret = pqos_l3ca_assoc_set(cpu->cores[j].lcore, 0);
                if (ret != PQOS_RETVAL_OK)
                        return ret;
        }

        return ret;
}

