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
 */

/**
 * @brief Implementation of PQoS monitoring API.
 *
 * CPUID and MSR operations are done on 'local' system.
 *
 */

#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <dirent.h>

#include "pqos.h"
#include "host_pidapi.h"
#include "host_cap.h"
#include "host_monitoring.h"

#include "machine.h"
#include "types.h"
#include "log.h"

/**
 * ---------------------------------------
 * Local macros
 * ---------------------------------------
 */

/**
 * Allocation & Monitoring association MSR register
 *
 * [63..<QE COS>..32][31..<RESERVED>..10][9..<RMID>..0]
 */
#define PQOS_MSR_ASSOC             0xC8F
#define PQOS_MSR_ASSOC_QECOS_SHIFT 32
#define PQOS_MSR_ASSOC_QECOS_MASK  0xffffffff00000000ULL
#define PQOS_MSR_ASSOC_RMID_MASK   ((1ULL<<10)-1ULL)

/**
 * Monitoring data read MSR register
 */
#define PQOS_MSR_MON_QMC             0xC8E
#define PQOS_MSR_MON_QMC_DATA_MASK   ((1ULL<<62)-1ULL)
#define PQOS_MSR_MON_QMC_ERROR       (1ULL<<63)
#define PQOS_MSR_MON_QMC_UNAVAILABLE (1ULL<<62)

/**
 * Monitoring event selection MSR register
 * [63..<RESERVED>..42][41..<RMID>..32][31..<RESERVED>..8][7..<EVENTID>..0]
 */
#define PQOS_MSR_MON_EVTSEL            0xC8D
#define PQOS_MSR_MON_EVTSEL_RMID_SHIFT 32
#define PQOS_MSR_MON_EVTSEL_RMID_MASK  ((1ULL<<10)-1ULL)
#define PQOS_MSR_MON_EVTSEL_EVTID_MASK ((1ULL<<8)-1ULL)

/**
 * Allocation class of service (COS) MSR registers
 */
#define PQOS_MSR_L3CA_MASK_START 0xC90
#define PQOS_MSR_L3CA_MASK_END   0xD8F
#define PQOS_MSR_L3CA_MASK_NUMOF \
        (PQOS_MSR_L3CA_MASK_END-PQOS_MSR_L3CA_MASK_START+1)

/**
 * MSR's to read instructions retired, unhalted cycles,
 * LLC references and LLC misses.
 * These MSR's are needed to calculate IPC (instructions per clock) and
 * LLC miss ratio.
 */
#define IA32_MSR_INST_RETIRED_ANY     0x309
#define IA32_MSR_CPU_UNHALTED_THREAD  0x30A
#define IA32_MSR_FIXED_CTR_CTRL       0x38D
#define IA32_MSR_PERF_GLOBAL_CTRL     0x38F
#define IA32_MSR_PMC0                 0x0C1
#define IA32_MSR_PERFEVTSEL0          0x186

#define IA32_EVENT_LLC_MISS_MASK      0x2EULL
#define IA32_EVENT_LLC_MISS_UMASK     0x41ULL

/**
 * Special RMID - after reset all cores are associated with it.
 *
 * The assumption is that if core is not assigned to it
 * then it is subject of monitoring activity by a different process.
 */
#define RMID0 (0)

/**
 * Max value of the memory bandwidth data = 2^24
 * assuming there is 24 bit space available
 */
#define MBM_MAX_VALUE (1<<24)

/**
 * Value marking monitoring group structure as "valid".
 * Group becomes "valid" after successful pqos_mon_start() or
 * pqos_mon_start_pid() call.
 */
#define GROUP_VALID_MARKER (0x00DEAD00)

/**
 * ---------------------------------------
 * Local data types
 * ---------------------------------------
 */

/**
 * List of RMID states
 */
enum rmid_state {
        RMID_STATE_FREE = 0,            /**< RMID is currently unused and can
                                           be used by the library */
        RMID_STATE_ALLOCATED,           /**< RMID was free at start but now it
                                           is used for monitoring */
        RMID_STATE_UNAVAILABLE          /**< RMID was associated to some core
                                           at start-up. It may be used by
                                           another process for monitoring */
};

/**
 * Per logical core entry to track monitoring processes
 */
struct mon_entry {
        pqos_rmid_t rmid;               /**< current RMID association */
        int unavailable;                /**< if true then core is subject of
                                           monitoring by another process */
        struct pqos_mon_data *grp;      /**< monitoring group the core
                                           belongs to */
};

/**
 * ---------------------------------------
 * Local data structures
 * ---------------------------------------
 */
static const struct pqos_cap *m_cap = NULL; /**< capabilities structure
                                               passed from host_cap */
static const struct pqos_cpuinfo *m_cpu = NULL; /**< cpu topology passed
                                                   from host_cap */
static enum rmid_state **m_rmid_cluster_map = NULL; /**< 32 is max supported
                                                       monitoring clusters */
static unsigned m_num_clusters = 0;     /**< number of clusters
                                           in the topology */
static unsigned m_rmid_max = 0;         /**< max RMID */
static unsigned m_dim_cores = 0;        /**< max coreid in the topology */
static struct mon_entry *m_core_map = NULL; /**< map of core states */
static int m_force_mon = 0;
/**
 * ---------------------------------------
 * Local Functions
 * ---------------------------------------
 */

static unsigned
cpu_get_num_clusters(const struct pqos_cpuinfo *cpu);

static unsigned
cpu_get_num_cores(const struct pqos_cpuinfo *cpu);

static int
mon_assoc_set_nocheck(const unsigned lcore,
                      const pqos_rmid_t rmid);

static int
mon_assoc_set(const unsigned lcore,
              const unsigned cluster,
              const pqos_rmid_t rmid);

static int
mon_assoc_get(const unsigned lcore,
              pqos_rmid_t *rmid);

static int
mon_read(const unsigned lcore,
         const pqos_rmid_t rmid,
         const enum pqos_mon_event event,
         uint64_t *value);

static int
pqos_core_poll(struct pqos_mon_data *group);

static int
rmid_alloc(const unsigned cluster,
           const enum pqos_mon_event event,
           pqos_rmid_t *rmid);

static int
rmid_free(const unsigned cluster,
          const pqos_rmid_t rmid);

static unsigned
get_event_id(const enum pqos_mon_event event);

static uint64_t
get_delta(const uint64_t old_value, const uint64_t new_value);

/**
 * =======================================
 * =======================================
 *
 * initialize and shutdown
 *
 * =======================================
 * =======================================
 */


int
pqos_mon_init(const struct pqos_cpuinfo *cpu,
              const struct pqos_cap *cap,
              const struct pqos_config *cfg)
{
        const struct pqos_capability *item = NULL;
        unsigned i = 0, fails = 0;
        int ret = PQOS_RETVAL_OK;

        m_cpu = cpu;
        m_cap = cap;

#ifndef PQOS_NO_PID_API
        /**
         * Init monitoring processes
         */
        ret = pqos_pid_init(m_cap);
        if (ret == PQOS_RETVAL_ERROR)
                return ret;
#endif /* PQOS_NO_PID_API */

        /**
         * If monitoring capability has been discovered
         * then get max RMID supported by a CPU socket
         * and allocate memory for RMID table
         */
        ret = pqos_cap_get_type(cap, PQOS_CAP_TYPE_MON, &item);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        m_num_clusters = cpu_get_num_clusters(m_cpu);
        ASSERT(m_num_clusters >= 1);

        ASSERT(item != NULL);
        m_rmid_max = item->u.mon->max_rmid;
        if (m_rmid_max == 0) {
                pqos_mon_fini();
                return PQOS_RETVAL_PARAM;
        }

        LOG_DEBUG("Max RMID per monitoring cluster is %u\n", m_rmid_max);

        ASSERT(m_cpu != NULL);

        m_dim_cores = cpu_get_num_cores(m_cpu);
        m_core_map =
                (struct mon_entry *)malloc(m_dim_cores * sizeof(m_core_map[0]));
        ASSERT(m_core_map != NULL);
        if (m_core_map == NULL) {
                pqos_mon_fini();
                return PQOS_RETVAL_ERROR;
        }
        memset(m_core_map, 0, m_dim_cores*sizeof(m_core_map[0]));

        m_rmid_cluster_map =
                (enum rmid_state **)malloc(m_num_clusters *
                                           sizeof(m_rmid_cluster_map[0]));
        ASSERT(m_rmid_cluster_map != NULL);
        if (m_rmid_cluster_map == NULL) {
                pqos_mon_fini();
                return PQOS_RETVAL_ERROR;
        }

        for (i = 0 ; i < m_num_clusters; i++) {
                const size_t size = m_rmid_max * sizeof(enum rmid_state);
                enum rmid_state *st = (enum rmid_state *)malloc(size);

                if (st == NULL) {
                        pqos_mon_fini();
                        return PQOS_RETVAL_RESOURCE;
                }
                m_rmid_cluster_map[i] = st;
                memset(st, 0, size);
                st[RMID0] = RMID_STATE_UNAVAILABLE; /**< RMID0 has a special
                                                       meaning */
        }

        LOG_DEBUG("RMID internal tables allocated\n");
        m_force_mon = cfg->free_in_use_rmid;

        /**
         * Read current core<=>RMID associations
         */
        for (i = 0; i < m_cpu->num_cores; i++) {
                pqos_rmid_t rmid = 0;
                unsigned coreid = m_cpu->cores[i].lcore;
                unsigned clusterid = m_cpu->cores[i].cluster;

                ret = mon_assoc_get(coreid, &rmid);
                if (ret != PQOS_RETVAL_OK) {
                        LOG_ERROR("Failed to read RMID association of "
                                  "lcore %u!\n", coreid);
                        fails++;
                } else {
                        ASSERT(rmid < m_rmid_max);

                        m_core_map[coreid].rmid = rmid;
                        m_core_map[coreid].unavailable = 0;
                        m_core_map[coreid].grp = NULL;

                        if (rmid == RMID0)
                                continue;

                        /**
                         * At this stage we know core is assigned to non-zero RMID
                         * This means it may be used by another instance of
                         * the program for monitoring.
                         * The other option is that previously ran program
                         * died and it didn't revert RMID association.
                         */

                        if (!cfg->free_in_use_rmid) {
                                enum rmid_state *pstate = NULL;

                                LOG_DEBUG("Detected RMID%u is associated with "
                                          "core %u. "
                                          "Marking RMID & core unavailable.\n",
                                          rmid, coreid);

                                ASSERT(clusterid < m_num_clusters);
                                pstate = m_rmid_cluster_map[clusterid];
                                pstate[rmid] = RMID_STATE_UNAVAILABLE;

                                m_core_map[coreid].unavailable = 1;
                        } else {
                                LOG_DEBUG("Detected RMID%u is associated with "
                                          "core %u. "
                                          "Freeing the RMID and associating "
                                          "core with RMID0.\n",
                                         rmid, coreid);
                                ret = mon_assoc_set_nocheck(coreid, RMID0);
                                if (ret != PQOS_RETVAL_OK) {
                                        LOG_ERROR("Failed to associate core %u "
                                                  "with RMID0!\n", coreid);
                                        fails++;
                                }
                        }
                }
        }

        ret = (fails == 0) ? PQOS_RETVAL_OK : PQOS_RETVAL_ERROR;

        if (ret != PQOS_RETVAL_OK)
                pqos_mon_fini();

        return ret;
}

int
pqos_mon_fini(void)
{
        int retval = PQOS_RETVAL_OK;

#ifndef PQOS_NO_PID_API
        /**
         * Shut down monitoring processes
         */
        if (pqos_pid_fini() != PQOS_RETVAL_OK)
                LOG_ERROR("Failed to finalize PID monitoring API\n");
#endif /* PQOS_NO_PID_API */

        if (m_core_map != NULL && m_cpu != NULL) {
                /**
                 * Assign monitored cores back to RMID0
                 */
                unsigned i;

                for (i = 0; i < m_cpu->num_cores; i++) {
                        if (m_core_map[i].rmid == RMID0 ||
                            m_core_map[i].unavailable != 0)
                                continue;

                        if (mon_assoc_set_nocheck(m_cpu->cores[i].lcore,
                                                  RMID0) != PQOS_RETVAL_OK)
                                LOG_ERROR("Failed to associate core %u "
                                          "with RMID0!\n",
                                          m_cpu->cores[i].lcore);
                }
        }

        /**
         * Free up allocated cluster structures for tracking
         * RMID allocations.
         */
        if (m_rmid_cluster_map != NULL) {
                unsigned i;

                for (i = 0; i < m_num_clusters; i++) {
                        if (m_rmid_cluster_map[i] != NULL) {
                                free(m_rmid_cluster_map[i]);
                                m_rmid_cluster_map[i] = NULL;
                        }
                }
                free(m_rmid_cluster_map);
                m_rmid_cluster_map = NULL;
        }
        m_rmid_max = 0;
        m_num_clusters = 0;

        /**
         * Free up allocated core map used to track
         * core <=> RMID assignment
         */
        if (m_core_map != NULL) {
                free(m_core_map);
                m_core_map = NULL;
        }
        m_dim_cores = 0;
        m_cpu = NULL;
        m_cap = NULL;
        return retval;
}

/**
 * =======================================
 * =======================================
 *
 * RMID allocation
 *
 * =======================================
 * =======================================
 */

/**
 * @brief Validates cluster id parameter for RMID allocation operation
 *
 * @param cluster cluster id on which rmid is to allocated from
 * @param p_table place to store pointer to cluster map of RMIDs
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK success
 */
static int
mon_rmid_alloc_param_check(const unsigned cluster,
                           enum rmid_state **p_table)
{
        int ret;

        if (cluster >= m_num_clusters)
                return PQOS_RETVAL_PARAM;

        ret = _pqos_check_init(1);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        if (m_rmid_cluster_map[cluster] == NULL) {
                LOG_WARN("Monitoring capability not detected for "
                         "cluster id %u\n", cluster);
                return PQOS_RETVAL_PARAM;
        }

        (*p_table) = m_rmid_cluster_map[cluster];

        return PQOS_RETVAL_OK;
}

/**
 * @brief Allocates RMID for given \a event
 *
 * @param [in] cluster CPU cluster id
 * @param [in] event Monitoring event type
 * @param [out] rmid resource monitoring id
 *
 * @return Operations status
 */
static int
rmid_alloc(const unsigned cluster,
           const enum pqos_mon_event event,
           pqos_rmid_t *rmid)
{
        enum rmid_state *rmid_table = NULL;
        const struct pqos_capability *item = NULL;
        const struct pqos_cap_mon *mon = NULL;
        int ret = PQOS_RETVAL_OK;
        unsigned max_rmid = 0;
        unsigned i;
        int j;
        unsigned mask_found = 0;

        if (rmid == NULL)
                return PQOS_RETVAL_PARAM;

        ret = mon_rmid_alloc_param_check(cluster, &rmid_table);
        if (ret != PQOS_RETVAL_OK)
                return ret;
        ASSERT(rmid_table != NULL);

        /**
         * This is not so straight forward as it appears to be.
         * We first have to figure out max RMID
         * for given event type. In order to do so we need:
         * - go through capabilities structure
         * - find monitoring capability
         * - look for the \a event in the event list
         * - find max RMID matching the \a event
         */
        ASSERT(m_cap != NULL);
        ret = pqos_cap_get_type(m_cap, PQOS_CAP_TYPE_MON, &item);
        if (ret != PQOS_RETVAL_OK)
                return ret;
        ASSERT(item != NULL);
        mon = item->u.mon;

        /* Find which events are supported */
        max_rmid = m_rmid_max;
        for (i = 0; i < mon->num_events; i++) {
                if (event & mon->events[i].type) {
                        mask_found |= mon->events[i].type;
                        max_rmid = (max_rmid > mon->events[i].max_rmid) ?
                                    mon->events[i].max_rmid : max_rmid;
                }
        }

        /**
         * Check if all of the events are supported
         */
        if (event != mask_found || max_rmid == 0)
                return PQOS_RETVAL_ERROR;
        ASSERT(m_rmid_max >= max_rmid);

        /**
         * Check for free RMID in the table
         * Do it backwards (from max to 0) in order to preserve low RMID values
         * for overlapping RMID ranges for future events.
         */
        ret = PQOS_RETVAL_ERROR;
        for (j = (int)max_rmid-1; j >= 0; j--) {
                if (rmid_table[j] != RMID_STATE_FREE)
                        continue;
                rmid_table[j] = RMID_STATE_ALLOCATED;
                *rmid = (pqos_rmid_t) j;
                ret = PQOS_RETVAL_OK;
                break;
        }

        return ret;
}

/**
 * @brief Frees previously allocated \a rmid
 *
 * @param [in] cluster CPU cluster id
 * @param [in] rmid resource monitoring id
 *
 * @return Operations status
 */
static int
rmid_free(const unsigned cluster,
          const pqos_rmid_t rmid)
{
        enum rmid_state *rmid_table = NULL;
        int ret = PQOS_RETVAL_OK;

        ret = mon_rmid_alloc_param_check(cluster, &rmid_table);
        if (ret != PQOS_RETVAL_OK)
                return ret;
        ASSERT(rmid_table != NULL);

        if (rmid >= m_rmid_max)
                return PQOS_RETVAL_PARAM;

        if (rmid_table[rmid] != RMID_STATE_ALLOCATED)
                return PQOS_RETVAL_ERROR;

        rmid_table[rmid] = RMID_STATE_FREE;

        return ret;
}

/**
 * =======================================
 * =======================================
 *
 * Monitoring
 *
 * =======================================
 * =======================================
 */

/**
 * @brief Checks logical core parameter for core association get operation
 *
 * @param lcore logical core id
 * @param p_cluster place to store pointer to cluster map of RMIDs
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK success
 */
static int
mon_assoc_param_check(const unsigned lcore,
                      unsigned *p_cluster)
{
        int ret = PQOS_RETVAL_OK;

        ret = _pqos_check_init(1);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        ASSERT(m_cpu != NULL);
        ret = pqos_cpu_check_core(m_cpu, lcore);
        if (ret != PQOS_RETVAL_OK)
                return PQOS_RETVAL_PARAM;

        ASSERT(p_cluster != NULL);
        ret = pqos_cpu_get_clusterid(m_cpu, lcore, p_cluster);
        if (ret != PQOS_RETVAL_OK)
                return PQOS_RETVAL_PARAM;

        if ((*p_cluster) >= m_num_clusters)
                return PQOS_RETVAL_PARAM;

        if (m_rmid_cluster_map[(*p_cluster)] == NULL) {
                LOG_WARN("Monitoring capability not detected\n");
                return PQOS_RETVAL_PARAM;
        }

        return PQOS_RETVAL_OK;
}

/**
 * @brief Associates core with RMID at register level
 *
 * This function doesn't acquire API lock
 * and can be used internally when lock is already taken.
 *
 * @param lcore logical core id
 * @param rmid resource monitoring ID
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 */
static int
mon_assoc_set_nocheck(const unsigned lcore,
                      const pqos_rmid_t rmid)
{
        int ret = 0;
        uint32_t reg = 0;
        uint64_t val = 0;

        reg = PQOS_MSR_ASSOC;
        ret = msr_read(lcore, reg, &val);
        if (ret != MACHINE_RETVAL_OK)
                return PQOS_RETVAL_ERROR;

        val &= PQOS_MSR_ASSOC_QECOS_MASK;
        val |= (uint64_t)(rmid & PQOS_MSR_ASSOC_RMID_MASK);

        ret = msr_write(lcore, reg, val);
        if (ret != MACHINE_RETVAL_OK)
                return PQOS_RETVAL_ERROR;

        return PQOS_RETVAL_OK;
}

/**
 * @brief Associates core with RMID
 *
 * This function doesn't acquire API lock
 * and can be used internally when lock is already taken.
 *
 * @param lcore logical core id
 * @param cluster cluster that core belongs to
 * @param rmid resource monitoring ID
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 */
static int
mon_assoc_set(const unsigned lcore,
              const unsigned cluster,
              const pqos_rmid_t rmid)
{
        enum rmid_state *rmid_table = NULL;
        int ret = 0;

        if (cluster >= m_num_clusters)
                return PQOS_RETVAL_PARAM;

        rmid_table = m_rmid_cluster_map[cluster];
        if (rmid_table[rmid] != RMID_STATE_ALLOCATED)
                return PQOS_RETVAL_PARAM;

        ret = mon_assoc_set_nocheck(lcore, rmid);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        m_core_map[lcore].rmid = rmid;

        return PQOS_RETVAL_OK;
}

/**
 * @brief Reads \a lcore to RMID association
 *
 * @param lcore logical core id
 * @param rmid place to store RMID \a lcore is assigned to
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK success
 */
static int
mon_assoc_get(const unsigned lcore,
              pqos_rmid_t *rmid)
{
        int ret = 0;
        uint32_t reg = PQOS_MSR_ASSOC;
        uint64_t val = 0;

        ASSERT(rmid != NULL);

        ret = msr_read(lcore, reg, &val);
        if (ret != MACHINE_RETVAL_OK)
                return PQOS_RETVAL_ERROR;

        val &= PQOS_MSR_ASSOC_RMID_MASK;
        *rmid = (pqos_rmid_t) val;

        return PQOS_RETVAL_OK;
}

int
pqos_mon_assoc_get(const unsigned lcore,
                   pqos_rmid_t *rmid)
{
        int ret = PQOS_RETVAL_OK;
        unsigned cluster = 0;

        _pqos_api_lock();

        ret = mon_assoc_param_check(lcore, &cluster);
        if (ret != PQOS_RETVAL_OK) {
                _pqos_api_unlock();
                return ret;
        }

        if (rmid == NULL) {
                _pqos_api_unlock();
                return PQOS_RETVAL_PARAM;
        }

        ret = mon_assoc_get(lcore, rmid);

        _pqos_api_unlock();
        return ret;
}

/**
 * @brief Reads monitoring event data from given core
 *
 * This function doesn't acquire API lock.
 *
 * @param lcore logical core id
 * @param rmid RMID to be read
 * @param event monitoring event
 * @param value place to store read value
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 */
static int
mon_read(const unsigned lcore,
         const pqos_rmid_t rmid,
         const enum pqos_mon_event event,
         uint64_t *value)
{
        int retries = 3, retval = PQOS_RETVAL_OK;
        uint32_t reg = 0;
        uint64_t val = 0;

        /**
         * Set event selection register (RMID + event id)
         */
        reg = PQOS_MSR_MON_EVTSEL;
        val = ((uint64_t)rmid) & PQOS_MSR_MON_EVTSEL_RMID_MASK;
        val <<= PQOS_MSR_MON_EVTSEL_RMID_SHIFT;
        val |= ((uint64_t)event) & PQOS_MSR_MON_EVTSEL_EVTID_MASK;
        if (msr_write(lcore, reg, val) != MACHINE_RETVAL_OK)
                return PQOS_RETVAL_ERROR;

        /**
         * read selected data associated with previously selected RMID+event
         */
        reg = PQOS_MSR_MON_QMC;
        do {
                if (msr_read(lcore, reg, &val) != MACHINE_RETVAL_OK) {
                        retval = PQOS_RETVAL_ERROR;
                        break;
                }
                if ((val&(PQOS_MSR_MON_QMC_ERROR)) != 0ULL) {
                        /**
                         * Unsupported event id or RMID selected
                         */
                        retval = PQOS_RETVAL_ERROR;
                        break;
                }
                retries--;
        } while ((val&PQOS_MSR_MON_QMC_UNAVAILABLE) != 0ULL && retries > 0);

        /**
         * Store event value
         */
        if (retval == PQOS_RETVAL_OK)
                *value = (val & PQOS_MSR_MON_QMC_DATA_MASK);
        else
                LOG_WARN("Error reading event %u on core %u (RMID%u)!\n",
                         (unsigned) event, lcore, (unsigned) rmid);

        return retval;
}

/**
 * @brief Reads monitoring event data from given core
 *
 * @param p pointer to monitoring structure
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 */
static int
pqos_core_poll(struct pqos_mon_data *p)
{
        struct pqos_event_values *pv = &p->values;
        int retval = PQOS_RETVAL_OK;
        unsigned i;

        if (p->event & PQOS_MON_EVENT_L3_OCCUP) {
                uint64_t total = 0;

                for (i = 0; i < p->num_poll_ctx; i++) {
                        uint64_t tmp = 0;
                        int ret;

                        ret = mon_read(p->poll_ctx[i].lcore,
                                       p->poll_ctx[i].rmid,
                                       get_event_id(PQOS_MON_EVENT_L3_OCCUP),
                                       &tmp);
                        if (ret != PQOS_RETVAL_OK) {
                                retval = PQOS_RETVAL_ERROR;
                                goto pqos_core_poll__exit;
                        }
                        total += tmp;
                }
                pv->llc = total;
        }
        if (p->event & (PQOS_MON_EVENT_LMEM_BW | PQOS_MON_EVENT_RMEM_BW)) {
                uint64_t total = 0, old_value = pv->mbm_local;

                for (i = 0; i < p->num_poll_ctx; i++) {
                        uint64_t tmp = 0;
                        int ret;

                        ret = mon_read(p->poll_ctx[i].lcore,
                                       p->poll_ctx[i].rmid,
                                       get_event_id(PQOS_MON_EVENT_LMEM_BW),
                                       &tmp);
                        if (ret != PQOS_RETVAL_OK) {
                                retval = PQOS_RETVAL_ERROR;
                                goto pqos_core_poll__exit;
                        }
                        total += tmp;
                }
                pv->mbm_local = total;
                pv->mbm_local_delta = get_delta(old_value, pv->mbm_local);
        }
        if (p->event & (PQOS_MON_EVENT_TMEM_BW | PQOS_MON_EVENT_RMEM_BW)) {
                uint64_t total = 0, old_value = pv->mbm_total;

                for (i = 0; i < p->num_poll_ctx; i++) {
                        uint64_t tmp = 0;
                        int ret;

                        ret = mon_read(p->poll_ctx[i].lcore,
                                       p->poll_ctx[i].rmid,
                                       get_event_id(PQOS_MON_EVENT_TMEM_BW),
                                       &tmp);
                        if (ret != PQOS_RETVAL_OK) {
                                retval = PQOS_RETVAL_ERROR;
                                goto pqos_core_poll__exit;
                        }
                        total += tmp;
                }
                pv->mbm_total = total;
                pv->mbm_total_delta = get_delta(old_value, pv->mbm_total);
        }
        if (p->event & PQOS_MON_EVENT_RMEM_BW) {
                pv->mbm_remote = 0;
                if (pv->mbm_total > pv->mbm_local)
                        pv->mbm_remote = pv->mbm_total - pv->mbm_local;
                pv->mbm_remote_delta = 0;
                if (pv->mbm_total_delta > pv->mbm_local_delta)
                        pv->mbm_remote_delta =
                                pv->mbm_total_delta - pv->mbm_local_delta;
        }
        if (p->event & PQOS_PERF_EVENT_IPC) {
                /**
                 * If multiple cores monitored in one group
                 * then we have to accumulate the values in the group.
                 */
                uint64_t unhalted = 0, retired = 0;
                unsigned n;

                for (n = 0; n < p->num_cores; n++) {
                        uint64_t tmp = 0;
                        int ret = msr_read(p->cores[n],
                                           IA32_MSR_INST_RETIRED_ANY, &tmp);
                        if (ret != MACHINE_RETVAL_OK) {
                                retval = PQOS_RETVAL_ERROR;
                                goto pqos_core_poll__exit;
                        }
                        retired += tmp;

                        ret = msr_read(p->cores[n],
                                       IA32_MSR_CPU_UNHALTED_THREAD, &tmp);
                        if (ret != MACHINE_RETVAL_OK) {
                                retval = PQOS_RETVAL_ERROR;
                                goto pqos_core_poll__exit;
                        }
                        unhalted += tmp;
                }

                pv->ipc_unhalted_delta = unhalted - pv->ipc_unhalted;
                pv->ipc_retired_delta = retired - pv->ipc_retired;
                pv->ipc_unhalted = unhalted;
                pv->ipc_retired = retired;
                if (pv->ipc_unhalted_delta == 0)
                        pv->ipc = 0.0;
                else
                        pv->ipc = (double) pv->ipc_retired_delta /
                                (double) pv->ipc_unhalted_delta;
        }
        if (p->event & PQOS_PERF_EVENT_LLC_MISS) {
                /**
                 * If multiple cores monitored in one group
                 * then we have to accumulate the values in the group.
                 */
                uint64_t missed = 0;
                unsigned n;

                for (n = 0; n < p->num_cores; n++) {
                        uint64_t tmp = 0;
                        int ret = msr_read(p->cores[n],
                                           IA32_MSR_PMC0, &tmp);
                        if (ret != MACHINE_RETVAL_OK) {
                                retval = PQOS_RETVAL_ERROR;
                                goto pqos_core_poll__exit;
                        }
                        missed += tmp;
                }

                pv->llc_misses_delta = missed - pv->llc_misses;
                pv->llc_misses = missed;
        }

 pqos_core_poll__exit:
        return retval;
}

/**
 * @brief Sets up IA32 performance counters for IPC and LLC miss ratio events
 *
 * @param num_cores number of cores in \a cores table
 * @param cores table with core id's
 * @param event mask of selected monitoring events
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 */
static int
ia32_perf_counter_start(const unsigned num_cores,
                        const unsigned *cores,
                        const enum pqos_mon_event event)
{
        uint64_t global_ctrl_mask = 0;
        unsigned i;

        ASSERT(cores != NULL && num_cores > 0);

        if (!(event & (PQOS_PERF_EVENT_LLC_MISS | PQOS_PERF_EVENT_IPC)))
                return PQOS_RETVAL_OK;

        if (event & PQOS_PERF_EVENT_IPC)
                global_ctrl_mask |= (0x3ULL << 32); /**< fixed counters 0&1 */

        if (event & PQOS_PERF_EVENT_LLC_MISS)
                global_ctrl_mask |= 0x1ULL;     /**< programmable counter 0 */

        if (!m_force_mon) {
                /**
                 * Fixed counters are used for IPC calculations.
                 * Programmable counters are used for LLC miss calculations.
                 * Let's check if they are in use.
                 */
                for (i = 0; i < num_cores; i++) {
                        uint64_t global_inuse = 0;
                        int ret;

                        ret = msr_read(cores[i], IA32_MSR_PERF_GLOBAL_CTRL,
                                           &global_inuse);
                        if (ret != MACHINE_RETVAL_OK)
                                return PQOS_RETVAL_ERROR;
                        if (global_inuse & global_ctrl_mask) {
                                LOG_ERROR("IPC and/or LLC miss performance "
                                          "counters already in use!\n");
                                return PQOS_RETVAL_PERF_CTR;
                        }
                }
        }

        /**
         * - Disable counters in global control and
         *   reset counter values to 0.
         * - Program counters for desired events
         * - Enable counters in global control
         */
        for (i = 0; i < num_cores; i++) {
                const uint64_t fixed_ctrl = 0x33ULL; /**< track usr + os */
                int ret;

                ret = msr_write(cores[i], IA32_MSR_PERF_GLOBAL_CTRL, 0);
                if (ret != MACHINE_RETVAL_OK)
                        break;

                if (event & PQOS_PERF_EVENT_IPC) {
                        ret = msr_write(cores[i], IA32_MSR_INST_RETIRED_ANY, 0);
                        if (ret != MACHINE_RETVAL_OK)
                                break;
                        ret = msr_write(cores[i],
                                        IA32_MSR_CPU_UNHALTED_THREAD, 0);
                        if (ret != MACHINE_RETVAL_OK)
                                break;
                        ret = msr_write(cores[i],
                                        IA32_MSR_FIXED_CTR_CTRL, fixed_ctrl);
                        if (ret != MACHINE_RETVAL_OK)
                                break;
                }

                if (event & PQOS_PERF_EVENT_LLC_MISS) {
                        const uint64_t evtsel0_miss = IA32_EVENT_LLC_MISS_MASK |
                                (IA32_EVENT_LLC_MISS_UMASK << 8) |
                                (1ULL << 16) | (1ULL << 17) | (1ULL << 22);

                        ret = msr_write(cores[i], IA32_MSR_PMC0, 0);
                        if (ret != MACHINE_RETVAL_OK)
                                break;
                        ret = msr_write(cores[i], IA32_MSR_PERFEVTSEL0,
                                        evtsel0_miss);
                        if (ret != MACHINE_RETVAL_OK)
                                break;
                }

                ret = msr_write(cores[i],
                                IA32_MSR_PERF_GLOBAL_CTRL, global_ctrl_mask);
                if (ret != MACHINE_RETVAL_OK)
                                break;
        }

        if (i < num_cores)
                return PQOS_RETVAL_ERROR;

        return PQOS_RETVAL_OK;
}

/**
 * @brief Disables IA32 performance counters
 *
 * @param num_cores number of cores in \a cores table
 * @param cores table with core id's
 * @param event mask of selected monitoring events
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 */
static int
ia32_perf_counter_stop(const unsigned num_cores,
                       const unsigned *cores,
                       const enum pqos_mon_event event)
{
        int retval = PQOS_RETVAL_OK;
        unsigned i;

        ASSERT(cores != NULL && num_cores > 0);

        if (!(event & (PQOS_PERF_EVENT_LLC_MISS | PQOS_PERF_EVENT_IPC)))
                return retval;

        for (i = 0; i < num_cores; i++) {
                int ret = msr_write(cores[i], IA32_MSR_PERF_GLOBAL_CTRL, 0);

                if (ret != MACHINE_RETVAL_OK)
                        retval = PQOS_RETVAL_ERROR;
        }
        return retval;
}

int
pqos_mon_start(const unsigned num_cores,
               const unsigned *cores,
               const enum pqos_mon_event event,
               void *context,
               struct pqos_mon_data *group)
{
        unsigned core2cluster[num_cores];
        struct pqos_mon_poll_ctx ctxs[num_cores];
        unsigned num_ctxs = 0;
        unsigned i = 0;
        int ret = PQOS_RETVAL_OK;
        int retval = PQOS_RETVAL_OK;

        if (group == NULL || cores == NULL || num_cores == 0 || event == 0)
                return PQOS_RETVAL_PARAM;

        memset(group, 0, sizeof(*group));

        _pqos_api_lock();

        ret = _pqos_check_init(1);
        if (ret != PQOS_RETVAL_OK) {
                _pqos_api_unlock();
                return ret;
        }

        ASSERT(m_cpu != NULL);

        /**
         * Validate event parameter
         * - only combinations of events allowed
         * - do not allow non-PQoS events to be monitored on its own
         */
        if (event & (~(PQOS_MON_EVENT_L3_OCCUP | PQOS_MON_EVENT_LMEM_BW |
                       PQOS_MON_EVENT_TMEM_BW | PQOS_MON_EVENT_RMEM_BW |
                       PQOS_PERF_EVENT_IPC | PQOS_PERF_EVENT_LLC_MISS))) {
                _pqos_api_unlock();
                return PQOS_RETVAL_PARAM;
        }
        if ((event & (PQOS_MON_EVENT_L3_OCCUP | PQOS_MON_EVENT_LMEM_BW |
                      PQOS_MON_EVENT_TMEM_BW | PQOS_MON_EVENT_RMEM_BW)) == 0 &&
            (event & (PQOS_PERF_EVENT_IPC | PQOS_PERF_EVENT_LLC_MISS)) != 0) {
                _pqos_api_unlock();
                return PQOS_RETVAL_PARAM;
        }

        /**
         * Validate if event is listed in capabilities
         */
        for (i = 0; i < (sizeof(event) * 8); i++) {
                const enum pqos_mon_event evt_mask = (1 << i);
                const struct pqos_monitor *ptr = NULL;

                if (!(evt_mask & event))
                        continue;

                ret = pqos_cap_get_event(m_cap, evt_mask, &ptr);
                if (ret != PQOS_RETVAL_OK || ptr == NULL) {
                        _pqos_api_unlock();
                        return PQOS_RETVAL_PARAM;
                }
        }

        /**
         * Check if all requested cores are valid
         * and not used by other monitoring processes.
         *
         * Check if any of requested cores is already subject to monitoring
         * within this process.
         *
         * Initialize poll context table:
         * - get core cluster
         * - allocate RMID
         */
        for (i = 0; i < num_cores; i++) {
                const unsigned lcore = cores[i];
                unsigned j, cluster = 0;

                ret = pqos_cpu_check_core(m_cpu, lcore);
                if (ret != PQOS_RETVAL_OK) {
                        retval = PQOS_RETVAL_PARAM;
                        goto pqos_mon_start_error1;
                }

                if (m_core_map[lcore].unavailable) {
                        retval = PQOS_RETVAL_RESOURCE;
                        goto pqos_mon_start_error1;
                }

                if (m_core_map[lcore].grp != NULL) {
                        retval = PQOS_RETVAL_RESOURCE;
                        goto pqos_mon_start_error1;
                }

                ret = pqos_cpu_get_clusterid(m_cpu, lcore, &cluster);
                if (ret != PQOS_RETVAL_OK) {
                        retval = PQOS_RETVAL_PARAM;
                        goto pqos_mon_start_error1;
                }
                core2cluster[i] = cluster;

                for (j = 0; j < num_ctxs; j++)
                        if (ctxs[j].lcore == lcore ||
                            ctxs[j].cluster == cluster)
                                break;

                if (j >= num_ctxs) {
                        /**
                         * New cluster is found
                         * - save cluster id in the table
                         * - allocate RMID for the cluster
                         */
                        ctxs[num_ctxs].lcore = lcore;
                        ctxs[num_ctxs].cluster = cluster;

                        ret = rmid_alloc(cluster,
                                         event & (~(PQOS_PERF_EVENT_IPC |
                                                    PQOS_PERF_EVENT_LLC_MISS)),
                                         &ctxs[num_ctxs].rmid);
                        if (ret != PQOS_RETVAL_OK) {
                                retval = ret;
                                goto pqos_mon_start_error1;
                        }

                        num_ctxs++;
                }
        }

        /**
         * Fill in the monitoring group structure
         */
        group->cores = (unsigned *) malloc(sizeof(group->cores[0]) * num_cores);
        if (group->cores == NULL) {
                retval = PQOS_RETVAL_RESOURCE;
                goto pqos_mon_start_error1;
        }

        group->poll_ctx = (struct pqos_mon_poll_ctx *)
                malloc(sizeof(group->poll_ctx[0]) * num_ctxs);
        if (group->poll_ctx == NULL) {
                retval = PQOS_RETVAL_RESOURCE;
                goto pqos_mon_start_error2;
        }

        ret = ia32_perf_counter_start(num_cores, cores, event);
        if (ret != PQOS_RETVAL_OK) {
                retval = ret;
                goto pqos_mon_start_error2;
        }

        /**
         * Associate requested cores with
         * the allocated RMID
         */
        group->num_cores = num_cores;
        for (i = 0; i < num_cores; i++) {
                unsigned cluster, j;
                pqos_rmid_t rmid;

                cluster = core2cluster[i];
                for (j = 0; j < num_ctxs; j++)
                        if (ctxs[j].cluster == cluster)
                                break;
                if (j >= num_ctxs) {
                        retval = PQOS_RETVAL_ERROR;
                        goto pqos_mon_start_error2;
                }
                rmid = ctxs[j].rmid;

                group->cores[i] = cores[i];
                ret = mon_assoc_set(cores[i], cluster, rmid);
                if (ret != PQOS_RETVAL_OK) {
                        retval = ret;
                        goto pqos_mon_start_error2;
                }
        }

        group->num_poll_ctx = num_ctxs;
        for (i = 0; i < num_ctxs; i++)
                group->poll_ctx[i] = ctxs[i];

        group->event = event;
        group->context = context;

        for (i = 0; i < num_cores; i++) {
                /**
                 * Mark monitoring activity in the core map
                 */
                const unsigned lcore = cores[i];

                m_core_map[lcore].grp = group;
        }

 pqos_mon_start_error2:
        if (retval != PQOS_RETVAL_OK) {
                for (i = 0; i < num_cores; i++)
                        (void) mon_assoc_set(cores[i], core2cluster[i], RMID0);

                if (group->poll_ctx != NULL)
                        free(group->poll_ctx);

                if (group->cores != NULL)
                        free(group->cores);
        }
 pqos_mon_start_error1:
        if (retval != PQOS_RETVAL_OK) {
                for (i = 0; i < num_ctxs; i++)
                        (void) rmid_free(ctxs[i].cluster, ctxs[i].rmid);
        }

        if (retval == PQOS_RETVAL_OK)
                group->valid = GROUP_VALID_MARKER;

        _pqos_api_unlock();
        return retval;
}

int
pqos_mon_start_pid(const pid_t pid,
                   const enum pqos_mon_event event,
                   void *context,
                   struct pqos_mon_data *group)
{
        if (group == NULL || event == 0 || pid < 0)
                return PQOS_RETVAL_PARAM;

#ifdef PQOS_NO_PID_API
        UNUSED_PARAM(context);
        LOG_ERROR("PID monitoring API not built\n");
        return PQOS_RETVAL_ERROR;
#else
        int ret = PQOS_RETVAL_OK;

        memset(group, 0, sizeof(*group));

        _pqos_api_lock();

        ret = _pqos_check_init(1);
        if (ret != PQOS_RETVAL_OK) {
                _pqos_api_unlock();
                return ret;
        }

        /**
         * Validate event parameter
         * - only combinations of events allowed
         * - do not allow non-PQoS events to be monitored on its own
         */
        if (event & (~(PQOS_MON_EVENT_L3_OCCUP | PQOS_MON_EVENT_LMEM_BW |
                       PQOS_MON_EVENT_TMEM_BW | PQOS_MON_EVENT_RMEM_BW |
                       PQOS_PERF_EVENT_IPC | PQOS_PERF_EVENT_LLC_MISS))) {
                _pqos_api_unlock();
                return PQOS_RETVAL_PARAM;
        }
        if ((event & (PQOS_MON_EVENT_L3_OCCUP | PQOS_MON_EVENT_LMEM_BW |
                      PQOS_MON_EVENT_TMEM_BW | PQOS_MON_EVENT_RMEM_BW)) == 0 &&
            (event & (PQOS_PERF_EVENT_IPC | PQOS_PERF_EVENT_LLC_MISS)) != 0) {
                _pqos_api_unlock();
                return PQOS_RETVAL_PARAM;
        }
	group->event = event;
        group->pid = pid;
        group->context = context;

        ret = pqos_pid_start(group);

        if (ret == PQOS_RETVAL_OK)
                group->valid = GROUP_VALID_MARKER;

        _pqos_api_unlock();
        return ret;
#endif /* PQOS_NO_PID_API */
}

int
pqos_mon_stop(struct pqos_mon_data *group)
{
        int ret = PQOS_RETVAL_OK;
        int retval = PQOS_RETVAL_OK;
        unsigned i = 0;

        if (group == NULL)
                return PQOS_RETVAL_PARAM;

        if (group->valid != GROUP_VALID_MARKER)
                return PQOS_RETVAL_PARAM;

        _pqos_api_lock();

        ret = _pqos_check_init(1);
        if (ret != PQOS_RETVAL_OK) {
                _pqos_api_unlock();
                return ret;
        }

        /**
         * If monitoring PID's then stop
         * counters and return
         */
        if (group->pid > 0) {
#ifdef PQOS_NO_PID_API
                LOG_ERROR("PID monitoring API not built\n");
                _pqos_api_unlock();
                return PQOS_RETVAL_ERROR;
#else
                /**
                 * Stop perf counters
                 */
                ret = pqos_pid_stop(group);
                group->valid = 0;
                _pqos_api_unlock();
                return ret;
#endif /* PQOS_NO_PID_API */
        }

        if (group->num_cores == 0 || group->cores == NULL ||
            group->num_poll_ctx == 0 || group->poll_ctx == NULL) {
                _pqos_api_unlock();
                return PQOS_RETVAL_PARAM;
        }

        ASSERT(m_cpu != NULL);
        for (i = 0; i < group->num_cores; i++) {
                /**
                 * Validate core list in the group structure is correct
                 */
                const unsigned lcore = group->cores[i];

                ret = pqos_cpu_check_core(m_cpu, lcore);
                if (ret != PQOS_RETVAL_OK) {
                        _pqos_api_unlock();
                        return PQOS_RETVAL_PARAM;
                }
                if (m_core_map[lcore].grp == NULL) {
                        _pqos_api_unlock();
                        return PQOS_RETVAL_RESOURCE;
                }
        }

        for (i = 0; i < group->num_cores; i++) {
                /**
                 * Associate cores from the group back with RMID0
                 */
                const unsigned lcore = group->cores[i];

                m_core_map[lcore].grp = NULL;
                m_core_map[lcore].rmid = 0;
                ret = mon_assoc_set_nocheck(lcore, RMID0);
                if (ret != PQOS_RETVAL_OK)
                        retval = PQOS_RETVAL_RESOURCE;
        }

        /**
         * Free previously allocated RMID
         */
        for (i = 0; i < group->num_poll_ctx; i++) {
                ret = rmid_free(group->poll_ctx[i].cluster,
                                group->poll_ctx[i].rmid);
                if (ret != PQOS_RETVAL_OK)
                        retval = PQOS_RETVAL_RESOURCE;
        }

        /**
         * Stop IA32 performance counters
         */
        ret = ia32_perf_counter_stop(group->num_cores, group->cores,
                                     group->event);
        if (ret != PQOS_RETVAL_OK)
                retval = PQOS_RETVAL_RESOURCE;

        /**
         * Free poll contexts, core list and clear the group structure
         */
        free(group->cores);
        free(group->poll_ctx);
        memset(group, 0, sizeof(*group));

        _pqos_api_unlock();
        return retval;
}

int
pqos_mon_poll(struct pqos_mon_data **groups,
              const unsigned num_groups)
{
        int ret = PQOS_RETVAL_OK;
        unsigned i = 0;

        ASSERT(groups != NULL);
        ASSERT(num_groups > 0);
        if (groups == NULL || num_groups == 0 || *groups == NULL)
                return PQOS_RETVAL_PARAM;

        for (i = 0; i < num_groups; i++) {
                if (groups[i] == NULL)
                        return PQOS_RETVAL_PARAM;
                if (groups[i]->valid != GROUP_VALID_MARKER)
                        return PQOS_RETVAL_PARAM;
        }
        _pqos_api_lock();

        ret = _pqos_check_init(1);
        if (ret != PQOS_RETVAL_OK) {
                _pqos_api_unlock();
                return ret;
        }

        for (i = 0; i < num_groups; i++) {
	        /**
                 * If monitoring PID then read
                 * counter values
                 */
                if (groups[i]->pid > 0) {
#ifdef PQOS_NO_PID_API
                        LOG_ERROR("PID monitoring API not built\n");
                        _pqos_api_unlock();
                        return PQOS_RETVAL_ERROR;
#else
                        ret = pqos_pid_poll(groups[i]);
			if (ret != PQOS_RETVAL_OK)
			        LOG_WARN("Failed to read event values "
					 "for PID %u!\n", groups[i]->pid);
#endif /* PQOS_NO_PID_API */
                } else {
                        ret = pqos_core_poll(groups[i]);
                        if (ret != PQOS_RETVAL_OK)
                                LOG_WARN("Failed to read event on "
                                         "core %u\n", groups[i]->cores[0]);
                }
	}

        _pqos_api_unlock();
        return PQOS_RETVAL_OK;
}
/**
 * =======================================
 * =======================================
 *
 * Small utils
 *
 * =======================================
 * =======================================
 */

/**
 * @brief Finds maximum number of clusters in the topology
 *
 * @param cpu cpu topology structure
 *
 * @return Max cluster id (plus one) in the topology.
 *         This indicates size of look up table for cases in which
 *         cluster_id is an index.
 */
static unsigned
cpu_get_num_clusters(const struct pqos_cpuinfo *cpu)
{
        unsigned i = 0, n = 0;

        ASSERT(cpu != NULL);
        for (i = 0; i < cpu->num_cores; i++)
                if (cpu->cores[i].cluster > n)
                        n = cpu->cores[i].cluster;

        return n+1;
}

/**
 * @brief Finds maximum number of logical cores in the topology
 *
 * @param cpu cpu topology structure
 *
 * @return Max core id (plus one) in the topology.
 *         This indicates size of look up table where core id is an index.
 */
static unsigned
cpu_get_num_cores(const struct pqos_cpuinfo *cpu)
{
        unsigned i = 0, n = 0;

        ASSERT(cpu != NULL);
        for (i = 0; i < cpu->num_cores; i++)
                if (cpu->cores[i].lcore > n)
                        n = cpu->cores[i].lcore;

        return n+1;
}
/**
 * @brief Maps PQoS API event onto an MSR event id
 *
 * @param [in] event PQoS API event id
 *
 * @return MSR event id
 * @retval 0 if not successful
 */
static unsigned
get_event_id(const enum pqos_mon_event event)
{
        switch (event) {
        case PQOS_MON_EVENT_L3_OCCUP:
                return 1;
                break;
        case PQOS_MON_EVENT_LMEM_BW:
                return 3;
                break;
        case PQOS_MON_EVENT_TMEM_BW:
                return 2;
                break;
        case PQOS_MON_EVENT_RMEM_BW:
        default:
                ASSERT(0); /**< this means bug */
                break;
        }
        return 0;
}

/**
 * @brief Gives the difference between two values with regard to the possible overrun
 *
 * @param old_value previous value
 * @param new_value current value
 * @return difference between the two values
 */
static uint64_t
get_delta(const uint64_t old_value, const uint64_t new_value)
{
        if (old_value > new_value)
                return (MBM_MAX_VALUE - old_value) + new_value;
        else
                return new_value - old_value;
}
