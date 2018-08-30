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
 */

#include "mba_sc.h"
#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <sched.h>
#include <errno.h>
#include <string.h>
#include <sys/wait.h>
#include <signal.h>

static const struct pqos_cap *m_cap;
static const struct pqos_cpuinfo *m_cpu;
static const struct pqos_capability *m_cap_mba;
static const struct pqos_capability *m_cap_mon;

static struct pqos_mon_data group;
static int supported = 0;


/**
 * @brief Start LMBM monitoring
 *
 * @param[in] cpumask cores to monitor
 * @param[out] group monitoring group pointer
 * @return status
 * @retval 0 on success
 * @retval negative on error (-errno)
 */
static int
mba_sc_mon_start(const cpu_set_t cpumask, struct pqos_mon_data *group)
{
        unsigned *cores = NULL;
        unsigned num_cores = 0;
        int i;
        int ret;

        cores = malloc(CPU_SETSIZE * sizeof(unsigned));
        if (cores == NULL) {
                DBG("MBA SC: memory allocation failed\n");
                return -EFAULT;
        }

        for (i = 0; i < CPU_SETSIZE; i++) {
                if (CPU_ISSET(i, &cpumask) != 1)
                        continue;
                cores[num_cores++] = i;
        }

        ret = pqos_mon_start(num_cores, cores, PQOS_MON_EVENT_LMEM_BW,
                             NULL, group);
        if (ret != PQOS_RETVAL_OK)
                ret = -EFAULT;

        if (cores != NULL)
                free(cores);

        return ret;
}

/**
 * @brief Stop LMBM monitoring
 *
 * @param group monitoring group pointer
 *
 * @return status
 * @retval 0 on success
 * @retval negative on error (-errno)
 */
static int
mba_sc_mon_stop(struct pqos_mon_data *group)
{
        int ret;

        ret = pqos_mon_stop(group);
        if (ret != PQOS_RETVAL_OK)
                return -EFAULT;

        return 0;
}

/**
 * @brief Poll mon values
 *
 * @param group monitoring group
 *
 * @return status
 * @retval 0 on success
 * @retval negative on error (-errno)
 */
static int
mba_sc_mon_poll(struct pqos_mon_data *group) {
        int ret;

        ret = pqos_mon_poll(&group, 1);
        if (ret != PQOS_RETVAL_OK)
                return -EFAULT;

        return 0;
}

int
mba_sc_init(void)
{
        int ret = 0;
        const struct pqos_monitor *cap_lmbm;

        if (m_cap != NULL || m_cpu != NULL) {
                DBG("MBA SC: module already initialized!\n");
                ret = -EEXIST;
                goto err;
        }

        if (g_cfg.interface != PQOS_INTER_MSR) {
                DBG("MBA SC: Supported only for MSR interface\n");
                ret = -EFAULT;
                goto err;
        }

        /* Get capability and CPU info pointer */
        ret = pqos_cap_get(&m_cap, &m_cpu);
        if (ret != PQOS_RETVAL_OK) {
                DBG("MBA SC: Error retrieving PQoS capabilities!\n");
                ret = -EFAULT;
                goto err;
        }

        /* Get MBA capabilities */
        ret = pqos_cap_get_type(m_cap, PQOS_CAP_TYPE_MBA, &m_cap_mba);
        if (ret != PQOS_RETVAL_OK) {
                DBG("MBA SC: MBA not supported.\n");
                ret = -EFAULT;
                goto err;
        }

        /* Get mon capabilities */
        ret = pqos_cap_get_type(m_cap, PQOS_CAP_TYPE_MON, &m_cap_mon);
        if  (ret != PQOS_RETVAL_OK) {
                DBG("MBA SC: monitoring not supported.\n");
                ret = -EFAULT;
                goto err;
        }

        /* Check if LMBM monitoring is supported */
        ret = pqos_cap_get_event(m_cap, PQOS_MON_EVENT_LMEM_BW, &cap_lmbm);
        if  (ret == PQOS_RETVAL_OK && cap_lmbm != NULL)
                supported = 1;
        else {
                DBG("MBA SC: local BW monitoring not supported.\n");
                ret = -EFAULT;
                goto err;
        }


        memset(&group, 0, sizeof(group));
        supported = 1;

        return 0;
 err:
        /* deallocate all the resources */
        mba_sc_fini();
        return ret;
}

void
mba_sc_fini(void)
{
        if (m_cap == NULL && m_cpu == NULL)
                return;

        m_cap = NULL;
        m_cpu = NULL;
        m_cap_mba = NULL;
}

void
mba_sc_exit(void)
{
        mba_sc_mon_stop(&group);
}

/**
 * @brief Check if child process is still running
 *
 * @param[in] pid Child pid
 *
 * @return status
 */
static int
mba_sc_running(const pid_t pid)
{
        int status;
        int ret;

        if (pid != -1) {
                ret = waitpid(pid, &status, WNOHANG);
                if (ret == 0)
                        return 1;
                if (ret == pid &&
                    (!WIFEXITED(status) && WEXITSTATUS(status) != EXIT_SUCCESS))
                        exit(EXIT_FAILURE);

        } else if (!g_cfg.command) {
                unsigned i;

                /* Send sig-null to check if pid is still running */
                for (i = 0; i < g_cfg.pid_count; i++) {
                        ret = kill(g_cfg.pids[i], 0);
                        if (ret == 0)
                                return 1;
                }
        }

        return 0;
}

int
mba_sc_mode(void)
{
        unsigned i;

        for (i = 0; i < g_cfg.config_count; i++)
                if (g_cfg.config[i].mba_max > 0)
                        return 1;

        return 0;
}

int
mba_sc_main(pid_t pid)
{
        int ret;
        uint64_t start_time;
        unsigned i;

        if (!supported)
                return PQOS_RETVAL_RESOURCE;

        for (i = 0; i < g_cfg.config_count; i++) {
                const struct rdt_config *config = &g_cfg.config[i];

                if (config->mba_max > 0) {
                        ret = mba_sc_mon_start(config->cpumask, &group);
                        if (ret != PQOS_RETVAL_OK) {
                                DBG("MBA SC: failed to start monitoring\n");
                                return ret;
                        }

                        break;
                }
        }

        start_time = get_time_usec();

        while (mba_sc_running(pid)) {
                uint64_t current_time;
                const struct pqos_event_values *pv = &group.values;

                ret = mba_sc_mon_poll(&group);
                if (ret == PQOS_RETVAL_OK) {
                        current_time = get_time_usec();

                        DBG("time=%10lu interval=%10lu LMBM=%lu\n",
                            current_time,
                            current_time - start_time, pv->mbm_local_delta);

                        start_time = current_time;
                }

                sleep(1);
        }

        ret = mba_sc_mon_stop(&group);

        return ret;
}
