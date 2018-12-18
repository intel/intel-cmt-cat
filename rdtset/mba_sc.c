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

struct mba_sc_state {
        struct pqos_mon_data group;
        cpu_set_t cpumask;
        unsigned prev_rate;
        uint64_t max_bw;
        uint64_t prev_bw;
        unsigned delta_comp;
        uint64_t delta_bw;
};

static struct mba_sc_state state;
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
mba_sc_mon_poll(struct pqos_mon_data *group)
{
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
        if (ret != PQOS_RETVAL_OK || !m_cap_mba->u.mba->is_linear) {
                DBG("MBA SC: MBA not supported or not linear.\n");
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


        memset(&state, 0, sizeof(state));
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
        mba_sc_mon_stop(&state.group);
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

        if (g_cfg.interface != PQOS_INTER_MSR)
                return 0;

        for (i = 0; i < g_cfg.config_count; i++)
                if (g_cfg.config[i].mba.ctrl == 1)
                        return 1;

        return 0;
}

/**
 * @brief Sets MBA class of service defined by \a mba_cfg on cores in \a cpumask
 *
 * @param [in] cpumask set of lcores
 * @param [in] mba_cfg mba configuration to be set
 *
 * @return status
 */
static int
mba_sc_mba_set(const cpu_set_t cpumask, struct pqos_mba *mba_cfg)
{
        int ret;
        int lcore;
        unsigned cluster_id = 0;
        unsigned cluster_array[CPU_SETSIZE] = {0};

        for (lcore = 0; lcore < CPU_SETSIZE; lcore++) {
                if (CPU_ISSET(lcore, &cpumask) != 1)
                        continue;

                ret = pqos_cpu_get_clusterid(m_cpu, lcore, &cluster_id);
                if (ret != PQOS_RETVAL_OK) {
                        DBG("MBA SC: error while reading cluster id "
                            "for lcore %d\n", lcore);
                        return -EFAULT;
                }

                if (cluster_array[cluster_id])
                        continue;

                ret = pqos_alloc_assoc_get(lcore, &mba_cfg->class_id);
                if (ret != PQOS_RETVAL_OK) {
                        DBG("MBA SC: error while reading assoc for lcore %d\n",
                            lcore);
                        return -EFAULT;
                }

                ret = pqos_mba_set(cluster_id, 1, mba_cfg, NULL);
                if (ret != PQOS_RETVAL_OK) {
                        DBG("MBA SC: error while setting mba for cluster %u\n",
                            cluster_id);
                        return -EFAULT;
                }

                cluster_array[cluster_id]++;
        }

        return 0;
}

int
mba_sc_main(pid_t pid)
{
        int ret;
        uint64_t prev_time;
        uint64_t reg_start_time = 0;
        unsigned i;

        if (!supported)
                return PQOS_RETVAL_RESOURCE;

        const unsigned min_rate = m_cap_mba->u.mba->throttle_step;
        const unsigned step_rate = m_cap_mba->u.mba->throttle_step;
        const unsigned max_rate = 100;

        for (i = 0; i < g_cfg.config_count; i++) {
                const struct rdt_config *config = &g_cfg.config[i];

                if (config->mba.ctrl == 1) {
                        ret = mba_sc_mon_start(config->cpumask, &state.group);
                        if (ret != 0) {
                                DBG("MBA SC: failed to start monitoring\n");
                                goto err;
                        }

                        state.max_bw = mb_to_bytes(config->mba.mb_max);
                        state.prev_rate = MBA_SC_DEF_INIT_MBA;
                        state.cpumask = config->cpumask;

                        break;
                }
        }

        prev_time = get_time_usec();

        while (mba_sc_running(pid)) {
                uint64_t cur_time;
                uint64_t delta_time;
                struct pqos_mba mba_cfg;
                uint64_t prev_bw = state.prev_bw;
                uint64_t cur_bw;
                const struct pqos_event_values *pv = &state.group.values;

		mba_cfg.ctrl = 0;

                usleep(MBA_SC_SAMPLING_INTERVAL * 1000);

                ret = mba_sc_mon_poll(&state.group);
                if (ret != 0)
                        continue;

                cur_time = get_time_usec();
                delta_time = cur_time - prev_time;
                prev_time = cur_time;

                cur_bw = pv->mbm_local_delta * 1000000 / delta_time;
                state.prev_bw = cur_bw;

                if (state.delta_comp) {
                        state.delta_comp = 0;
                        if (cur_bw >= prev_bw)
                                state.delta_bw = cur_bw - prev_bw;
                        else
                                state.delta_bw = prev_bw - cur_bw;
                }

                DBG("MBA SC: Current BW %lluMBps",
                    (unsigned long long)bytes_to_mb(cur_bw));
                if (state.prev_rate > min_rate &&
                    cur_bw > state.max_bw) {
                        DBG(" > %lluMBps",
                            (unsigned long long)bytes_to_mb(state.max_bw));
                        mba_cfg.mb_max = state.prev_rate - step_rate;
                } else if (state.prev_rate < max_rate &&
                           (cur_bw + state.delta_bw) < state.max_bw) {
                        DBG(" < %lluMBps",
                            (unsigned long long)bytes_to_mb(state.max_bw));
                        mba_cfg.mb_max = state.prev_rate + step_rate;
                } else {
                        if (reg_start_time) {
                                DBG(" Max BW %lluMBps, regulation took %.1fs\n",
                                    (unsigned long long)
                                    bytes_to_mb(state.max_bw),
                                    (cur_time - reg_start_time) / 1000000.0);
                                reg_start_time = 0;
                        } else
                                DBG("\n");
                        continue;
                }

                DBG(", setting MBA to %u%%\n", mba_cfg.mb_max);
                ret = mba_sc_mba_set(state.cpumask, &mba_cfg);
                if (ret != 0) {
                        DBG(" Failed to update mba rate!\n");
                        continue;
                }

                state.prev_rate = mba_cfg.mb_max;
                state.delta_comp = 1;

                if (!reg_start_time)
                        reg_start_time = get_time_usec();
        }

 err:
        ret = mba_sc_mon_stop(&state.group);

        return ret;
}
