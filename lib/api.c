/*
 * BSD LICENSE
 *
 * Copyright(c) 2017-2019 Intel Corporation. All rights reserved.
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

#include <string.h>

#include "pqos.h"
#include "api.h"
#include "allocation.h"
#include "os_allocation.h"
#include "os_monitoring.h"
#include "monitoring.h"
#include "os_monitoring.h"
#include "cap.h"
#include "log.h"
#include "types.h"
#include "cpu_registers.h"

/**
 * Value marking monitoring group structure as "valid".
 * Group becomes "valid" after successful pqos_mon_start() or
 * pqos_mon_start_pid() call.
 */
#define GROUP_VALID_MARKER (0x00DEAD00)

/**
 * Flag used to determine what interface to use:
 *      - MSR is 0
 *      - OS is 1
 */
static int m_interface = PQOS_INTER_MSR;

/*
 * =======================================
 * Init module
 * =======================================
 */
int
api_init(int interface)
{
        if (interface != PQOS_INTER_MSR && interface != PQOS_INTER_OS &&
                interface != PQOS_INTER_OS_RESCTRL_MON)
                return PQOS_RETVAL_PARAM;

        m_interface = interface;

        return PQOS_RETVAL_OK;
}

/*
 * =======================================
 * Allocation Technology
 * =======================================
 */
int
pqos_alloc_assoc_set(const unsigned lcore,
                     const unsigned class_id)
{
        int ret;

        _pqos_api_lock();

        ret = _pqos_check_init(1);
        if (ret != PQOS_RETVAL_OK) {
                _pqos_api_unlock();
                return ret;
        }

        if (m_interface == PQOS_INTER_MSR)
                ret = hw_alloc_assoc_set(lcore, class_id);
        else {
#ifdef __linux__
                ret = os_alloc_assoc_set(lcore, class_id);
#else
                LOG_INFO("OS interface not supported!\n");
                ret = PQOS_RETVAL_RESOURCE;
#endif
        }
        _pqos_api_unlock();

        return ret;
}

int
pqos_alloc_assoc_get(const unsigned lcore,
                     unsigned *class_id)
{
        int ret;

        if (class_id == NULL)
                return PQOS_RETVAL_PARAM;

        _pqos_api_lock();

        ret = _pqos_check_init(1);
        if (ret != PQOS_RETVAL_OK) {
                _pqos_api_unlock();
                return ret;
        }

        if (m_interface == PQOS_INTER_MSR)
                ret = hw_alloc_assoc_get(lcore, class_id);
        else {
#ifdef __linux__
                ret = os_alloc_assoc_get(lcore, class_id);
#else
                LOG_INFO("OS interface not supported!\n");
                ret = PQOS_RETVAL_RESOURCE;
#endif
        }
        _pqos_api_unlock();

        return ret;
}

int
pqos_alloc_assoc_set_pid(const pid_t task,
                         const unsigned class_id)
{
        int ret;

        _pqos_api_lock();

        ret = _pqos_check_init(1);
        if (ret != PQOS_RETVAL_OK) {
                _pqos_api_unlock();
                return ret;
        }

        if (m_interface != PQOS_INTER_OS &&
                m_interface != PQOS_INTER_OS_RESCTRL_MON) {
                LOG_ERROR("Incompatible interface "
                          "selected for task association!\n");
                _pqos_api_unlock();
                return PQOS_RETVAL_ERROR;
        }

#ifdef __linux__
        ret = os_alloc_assoc_set_pid(task, class_id);
#else
        UNUSED_PARAM(task);
        UNUSED_PARAM(class_id);
        LOG_INFO("OS interface not supported!\n");
        ret = PQOS_RETVAL_RESOURCE;
#endif
        _pqos_api_unlock();

        return ret;

}

int
pqos_alloc_assoc_get_pid(const pid_t task,
                         unsigned *class_id)
{
        int ret;

        if (class_id == NULL)
                return PQOS_RETVAL_PARAM;

        _pqos_api_lock();

        ret = _pqos_check_init(1);
        if (ret != PQOS_RETVAL_OK) {
                _pqos_api_unlock();
                return ret;
        }

        if (m_interface != PQOS_INTER_OS &&
                m_interface != PQOS_INTER_OS_RESCTRL_MON) {
                LOG_ERROR("Incompatible interface "
                          "selected for task association!\n");
                _pqos_api_unlock();
                return PQOS_RETVAL_ERROR;
        }

#ifdef __linux__
        ret = os_alloc_assoc_get_pid(task, class_id);
#else
        UNUSED_PARAM(task);
        UNUSED_PARAM(class_id);
        LOG_INFO("OS interface not supported!\n");
        ret = PQOS_RETVAL_RESOURCE;
#endif
        _pqos_api_unlock();

        return ret;
}

int
pqos_alloc_assign(const unsigned technology,
                  const unsigned *core_array,
                  const unsigned core_num,
                  unsigned *class_id)
{
        int ret;
        const int l2_req = ((technology & (1 << PQOS_CAP_TYPE_L2CA)) != 0);
        const int l3_req = ((technology & (1 << PQOS_CAP_TYPE_L3CA)) != 0);
        const int mba_req = ((technology & (1 << PQOS_CAP_TYPE_MBA)) != 0);

        if (core_num == 0 || core_array == NULL || class_id == NULL ||
            !(l2_req || l3_req || mba_req))
                return PQOS_RETVAL_PARAM;

        _pqos_api_lock();

        ret = _pqos_check_init(1);
        if (ret != PQOS_RETVAL_OK) {
                _pqos_api_unlock();
                return ret;
        }
        if (m_interface == PQOS_INTER_MSR)
                ret = hw_alloc_assign(technology, core_array,
                        core_num, class_id);
        else {
#ifdef __linux__
                ret = os_alloc_assign(technology, core_array, core_num,
                                      class_id);
#else
                LOG_INFO("OS interface not supported!\n");
                ret = PQOS_RETVAL_RESOURCE;
#endif
        }
        _pqos_api_unlock();

        return ret;
}

int
pqos_alloc_release(const unsigned *core_array,
                   const unsigned core_num)
{
        int ret;

        if (core_num == 0 || core_array == NULL)
                return PQOS_RETVAL_PARAM;

        _pqos_api_lock();

        ret = _pqos_check_init(1);
        if (ret != PQOS_RETVAL_OK) {
                _pqos_api_unlock();
                return ret;
        }

        if (m_interface == PQOS_INTER_MSR)
                ret = hw_alloc_release(core_array, core_num);
        else {
#ifdef __linux__
                ret = os_alloc_release(core_array, core_num);
#else
                LOG_INFO("OS interface not supported!\n");
                ret = PQOS_RETVAL_RESOURCE;
#endif
        }
        _pqos_api_unlock();

        return ret;
}

int
pqos_alloc_assign_pid(const unsigned technology,
                      const pid_t *task_array,
                      const unsigned task_num,
                      unsigned *class_id)
{
        int ret;

        if (task_array == NULL || task_num == 0 || class_id == NULL)
                return PQOS_RETVAL_PARAM;

        _pqos_api_lock();

        ret = _pqos_check_init(1);
        if (ret != PQOS_RETVAL_OK) {
                _pqos_api_unlock();
                return ret;
        }

        if (m_interface != PQOS_INTER_OS &&
                m_interface != PQOS_INTER_OS_RESCTRL_MON) {
                LOG_ERROR("Incompatible interface "
                          "selected for task association!\n");
                _pqos_api_unlock();
                return PQOS_RETVAL_ERROR;
        }

#ifdef __linux__
        ret = os_alloc_assign_pid(technology, task_array, task_num, class_id);
#else
        UNUSED_PARAM(technology);
        LOG_INFO("OS interface not supported!\n");
        ret = PQOS_RETVAL_RESOURCE;
#endif
        _pqos_api_unlock();

        return ret;
}

int
pqos_alloc_release_pid(const pid_t *task_array,
                       const unsigned task_num)
{
        int ret;

        if (task_array == NULL || task_num == 0)
                return PQOS_RETVAL_PARAM;

        _pqos_api_lock();

        ret = _pqos_check_init(1);
        if (ret != PQOS_RETVAL_OK) {
                _pqos_api_unlock();
                return ret;
        }

        if (m_interface != PQOS_INTER_OS &&
                m_interface != PQOS_INTER_OS_RESCTRL_MON) {
                LOG_ERROR("Incompatible interface "
                          "selected for task association!\n");
                _pqos_api_unlock();
                return PQOS_RETVAL_ERROR;
        }

#ifdef __linux__
        ret = os_alloc_release_pid(task_array, task_num);
#else
        LOG_INFO("OS interface not supported!\n");
        ret = PQOS_RETVAL_RESOURCE;
#endif
        _pqos_api_unlock();

        return ret;
}

int
pqos_alloc_reset(const enum pqos_cdp_config l3_cdp_cfg,
                 const enum pqos_cdp_config l2_cdp_cfg,
                 const enum pqos_mba_config mba_cfg)
{
        int ret;

        if (l3_cdp_cfg != PQOS_REQUIRE_CDP_ON &&
            l3_cdp_cfg != PQOS_REQUIRE_CDP_OFF &&
            l3_cdp_cfg != PQOS_REQUIRE_CDP_ANY) {
                LOG_ERROR("Unrecognized L3 CDP configuration setting %d!\n",
                          l3_cdp_cfg);
                return PQOS_RETVAL_PARAM;
        }

        if (l2_cdp_cfg != PQOS_REQUIRE_CDP_ON &&
            l2_cdp_cfg != PQOS_REQUIRE_CDP_OFF &&
            l2_cdp_cfg != PQOS_REQUIRE_CDP_ANY) {
                LOG_ERROR("Unrecognized L2 CDP configuration setting %d!\n",
                          l2_cdp_cfg);
                return PQOS_RETVAL_PARAM;
        }

        if (mba_cfg != PQOS_MBA_ANY &&
            mba_cfg != PQOS_MBA_DEFAULT &&
            mba_cfg != PQOS_MBA_CTRL) {
                LOG_ERROR("Unrecognized MBA configuration setting %d!\n",
                          mba_cfg);
                return PQOS_RETVAL_PARAM;
        }

        _pqos_api_lock();

        ret = _pqos_check_init(1);
        if (ret != PQOS_RETVAL_OK) {
                _pqos_api_unlock();
                return ret;
        }

        if (m_interface == PQOS_INTER_MSR)
                ret = hw_alloc_reset(l3_cdp_cfg, l2_cdp_cfg, mba_cfg);
        else {
#ifdef __linux__
                ret = os_alloc_reset(l3_cdp_cfg, l2_cdp_cfg, mba_cfg);
#else
                LOG_INFO("OS interface not supported!\n");
                ret = PQOS_RETVAL_RESOURCE;
#endif
        }
        _pqos_api_unlock();

        return ret;
}

unsigned *
pqos_pid_get_pid_assoc(const unsigned class_id, unsigned *count)
{
        unsigned *tasks = NULL;
        int ret;

        if (count == NULL)
                return NULL;

        if (m_interface != PQOS_INTER_OS &&
                m_interface != PQOS_INTER_OS_RESCTRL_MON) {
                LOG_ERROR("Incompatible interface "
                          "selected for task association!\n");
                return NULL;
        }
        _pqos_api_lock();

        ret = _pqos_check_init(1);
        if (ret != PQOS_RETVAL_OK) {
                _pqos_api_unlock();
                return NULL;
        }

#ifdef __linux__
        tasks = os_pid_get_pid_assoc(class_id, count);
        if (tasks == NULL)
                LOG_ERROR("Error retrieving task information!\n");
#else
        UNUSED_PARAM(class_id);
        LOG_INFO("OS interface not supported!\n");
#endif

        _pqos_api_unlock();

        return tasks;
}

/*
 * =======================================
 * L3 cache allocation
 * =======================================
 */

/**
 * @brief Tests if \a bitmask is contiguous
 *
 * Zero bit mask is regarded as not contiguous.
 *
 * The function shifts out first group of contiguous 1's in the bit mask.
 * Next it checks remaining bitmask content to make a decision.
 *
 * @param bitmask bit mask to be validated for contiguity
 *
 * @return Bit mask contiguity check result
 * @retval 0 not contiguous
 * @retval 1 contiguous
 */
static int
is_contiguous(uint64_t bitmask)
{
        if (bitmask == 0)
                return 0;

        while ((bitmask & 1) == 0) /**< Shift until 1 found at position 0 */
                bitmask >>= 1;

        while ((bitmask & 1) != 0) /**< Shift until 0 found at position 0 */
                bitmask >>= 1;

        return (bitmask) ? 0 : 1;  /**< non-zero bitmask is not contiguous */
}

int
pqos_l3ca_set(const unsigned l3cat_id,
              const unsigned num_cos,
              const struct pqos_l3ca *ca)
{
        int ret;
        unsigned i;

        if (ca == NULL || num_cos == 0)
                return PQOS_RETVAL_PARAM;

        _pqos_api_lock();

        ret = _pqos_check_init(1);
        if (ret != PQOS_RETVAL_OK) {
                _pqos_api_unlock();
                return ret;
        }

        /**
         * Check if class bitmasks are contiguous.
         */
        for (i = 0; i < num_cos; i++) {
                int is_contig = 0;

                if (ca[i].cdp) {
                        is_contig = is_contiguous(ca[i].u.s.data_mask) &&
                                is_contiguous(ca[i].u.s.code_mask);
                } else
                        is_contig = is_contiguous(ca[i].u.ways_mask);

                if (!is_contig) {
                        LOG_ERROR("L3 COS%u bit mask is not contiguous!\n",
                                  ca[i].class_id);
                        _pqos_api_unlock();
                        return PQOS_RETVAL_PARAM;
                }
        }

        if (m_interface == PQOS_INTER_MSR)
                ret = hw_l3ca_set(l3cat_id, num_cos, ca);
        else {
#ifdef __linux__
                ret = os_l3ca_set(l3cat_id, num_cos, ca);
#else
                LOG_INFO("OS interface not supported!\n");
                ret = PQOS_RETVAL_RESOURCE;
#endif
        }
        _pqos_api_unlock();

        return ret;
}

int
pqos_l3ca_get(const unsigned l3cat_id,
              const unsigned max_num_ca,
              unsigned *num_ca,
              struct pqos_l3ca *ca)
{
        int ret;

        if (num_ca == NULL || ca == NULL || max_num_ca == 0)
                return PQOS_RETVAL_PARAM;

        _pqos_api_lock();

        ret = _pqos_check_init(1);
        if (ret != PQOS_RETVAL_OK) {
                _pqos_api_unlock();
                return ret;
        }
        if (m_interface == PQOS_INTER_MSR)
                ret = hw_l3ca_get(l3cat_id, max_num_ca, num_ca, ca);
        else {
#ifdef __linux__
                ret = os_l3ca_get(l3cat_id, max_num_ca, num_ca, ca);
#else
                LOG_INFO("OS interface not supported!\n");
                ret = PQOS_RETVAL_RESOURCE;
#endif
        }
        _pqos_api_unlock();

        return ret;
}

int
pqos_l3ca_get_min_cbm_bits(unsigned *min_cbm_bits)
{
        int ret;

        if (min_cbm_bits == NULL)
                return PQOS_RETVAL_PARAM;

        _pqos_api_lock();

        ret = _pqos_check_init(1);
        if (ret != PQOS_RETVAL_OK) {
                _pqos_api_unlock();
                return ret;
        }

        if (m_interface == PQOS_INTER_MSR)
                ret = hw_l3ca_get_min_cbm_bits(min_cbm_bits);
        else {
#ifdef __linux__
                ret = os_l3ca_get_min_cbm_bits(min_cbm_bits);
#else
                LOG_INFO("OS interface not supported!\n");
                ret = PQOS_RETVAL_RESOURCE;
#endif
        }

        _pqos_api_unlock();

        return ret;
}

/*
 * =======================================
 * L2 cache allocation
 * =======================================
 */

int
pqos_l2ca_set(const unsigned l2id,
              const unsigned num_cos,
              const struct pqos_l2ca *ca)
{
        int ret;
        unsigned i;

        if (ca == NULL || num_cos == 0)
                return PQOS_RETVAL_PARAM;

        _pqos_api_lock();

        ret = _pqos_check_init(1);
        if (ret != PQOS_RETVAL_OK) {
                _pqos_api_unlock();
                return ret;
        }

        /**
         * Check if class bitmasks are contiguous
         */
        for (i = 0; i < num_cos; i++) {
                int is_contig = 0;

                if (ca[i].cdp) {
                        is_contig = is_contiguous(ca[i].u.s.data_mask) &&
                                is_contiguous(ca[i].u.s.code_mask);
                } else
                        is_contig = is_contiguous(ca[i].u.ways_mask);

                if (!is_contig) {
                        LOG_ERROR("L2 COS%u bit mask is not contiguous!\n",
                                  ca[i].class_id);
                        _pqos_api_unlock();
                        return PQOS_RETVAL_PARAM;
                }
        }
        if (m_interface == PQOS_INTER_MSR)
                ret = hw_l2ca_set(l2id, num_cos, ca);
        else {
#ifdef __linux__
                ret = os_l2ca_set(l2id, num_cos, ca);
#else
                LOG_INFO("OS interface not supported!\n");
                ret = PQOS_RETVAL_RESOURCE;
#endif
        }
        _pqos_api_unlock();

        return ret;
}

int
pqos_l2ca_get(const unsigned l2id,
              const unsigned max_num_ca,
              unsigned *num_ca,
              struct pqos_l2ca *ca)
{
        int ret;

        if (num_ca == NULL || ca == NULL || max_num_ca == 0)
                return PQOS_RETVAL_PARAM;

        _pqos_api_lock();

        ret = _pqos_check_init(1);
        if (ret != PQOS_RETVAL_OK) {
                _pqos_api_unlock();
                return ret;
        }

        if (m_interface == PQOS_INTER_MSR)
                ret = hw_l2ca_get(l2id, max_num_ca, num_ca, ca);
        else {
#ifdef __linux__
                ret = os_l2ca_get(l2id, max_num_ca, num_ca, ca);
#else
                LOG_INFO("OS interface not supported!\n");
                ret = PQOS_RETVAL_RESOURCE;
#endif
        }
        _pqos_api_unlock();

        return ret;
}

int
pqos_l2ca_get_min_cbm_bits(unsigned *min_cbm_bits)
{
        int ret;

        if (min_cbm_bits == NULL)
                return PQOS_RETVAL_PARAM;

        _pqos_api_lock();

        ret = _pqos_check_init(1);
        if (ret != PQOS_RETVAL_OK) {
                _pqos_api_unlock();
                return ret;
        }

        if (m_interface == PQOS_INTER_MSR)
                ret = hw_l2ca_get_min_cbm_bits(min_cbm_bits);
        else {
#ifdef __linux__
                ret = os_l2ca_get_min_cbm_bits(min_cbm_bits);
#else
                LOG_INFO("OS interface not supported!\n");
                ret = PQOS_RETVAL_RESOURCE;
#endif
        }

        _pqos_api_unlock();

        return ret;
}

/*
 * =======================================
 * Memory Bandwidth Allocation
 * =======================================
 */

int
pqos_mba_set(const unsigned mba_id,
             const unsigned num_cos,
             const struct pqos_mba *requested,
             struct pqos_mba *actual)
{
        const struct pqos_vendor_config *vconfig;
        int ret;
        unsigned i;

        _pqos_get_vendor_config(&vconfig);

        if (requested == NULL || num_cos == 0)
                return PQOS_RETVAL_PARAM;

        /**
         * Check if MBA rate is within allowed range
         */
        for (i = 0; i < num_cos; i++)
                if (requested[i].ctrl == 0 &&
                    (requested[i].mb_max == 0 ||
                     requested[i].mb_max > vconfig->mba_max)) {
                        LOG_ERROR("MBA COS%u rate out of range (from 1-%d)!\n",
                                  requested[i].class_id, vconfig->mba_max);
                        return PQOS_RETVAL_PARAM;
                }

        _pqos_api_lock();

        ret = _pqos_check_init(1);
        if (ret != PQOS_RETVAL_OK) {
                _pqos_api_unlock();
                return ret;
        }

        ret = vconfig->mba_set(mba_id, num_cos, requested, actual);

        _pqos_api_unlock();

        return ret;

}

int
pqos_mba_get(const unsigned mba_id,
             const unsigned max_num_cos,
             unsigned *num_cos,
             struct pqos_mba *mba_tab)
{
        const struct pqos_vendor_config *vconfig;
        int ret;

        _pqos_get_vendor_config(&vconfig);

        if (num_cos == NULL || mba_tab == NULL || max_num_cos == 0)
                return PQOS_RETVAL_PARAM;

        _pqos_api_lock();

        ret = _pqos_check_init(1);
        if (ret != PQOS_RETVAL_OK) {
                _pqos_api_unlock();
                return ret;
        }

        ret = vconfig->mba_get(mba_id, max_num_cos, num_cos, mba_tab);

        _pqos_api_unlock();

        return ret;
}

/*
 * =======================================
 * Monitoring
 * =======================================
 */

int
pqos_mon_reset(void)
{
        int ret;

        _pqos_api_lock();

        ret = _pqos_check_init(1);
        if (ret != PQOS_RETVAL_OK) {
                _pqos_api_unlock();
                return ret;
        }

        if (m_interface == PQOS_INTER_MSR)
                ret = hw_mon_reset();
        else {
#ifdef __linux__
                ret = os_mon_reset();
#else
                LOG_INFO("OS interface not supported!\n");
                ret = PQOS_RETVAL_RESOURCE;
#endif
        }

        _pqos_api_unlock();

        return ret;
}

int
pqos_mon_assoc_get(const unsigned lcore,
                   pqos_rmid_t *rmid)
{
        int ret;

        _pqos_api_lock();

        ret = _pqos_check_init(1);
        if (ret != PQOS_RETVAL_OK) {
                _pqos_api_unlock();
                return ret;
        }

        if (m_interface == PQOS_INTER_MSR)
                ret = hw_mon_assoc_get(lcore, rmid);
        else {
                LOG_INFO("OS interface not supported!\n");
                ret = PQOS_RETVAL_RESOURCE;
        }

        _pqos_api_unlock();

        return ret;
}

int
pqos_mon_start(const unsigned num_cores,
               const unsigned *cores,
               const enum pqos_mon_event event,
               void *context,
               struct pqos_mon_data *group)
{
        int ret;

        if (group == NULL || cores == NULL || num_cores == 0 || event == 0)
                return PQOS_RETVAL_PARAM;

        if (group->valid == GROUP_VALID_MARKER)
                return PQOS_RETVAL_PARAM;

        /**
         * Validate event parameter
         * - only combinations of events allowed
         * - do not allow non-PQoS events to be monitored on its own
         */
        if (event & (~(PQOS_MON_EVENT_L3_OCCUP | PQOS_MON_EVENT_LMEM_BW |
                       PQOS_MON_EVENT_TMEM_BW | PQOS_MON_EVENT_RMEM_BW |
                       PQOS_PERF_EVENT_IPC | PQOS_PERF_EVENT_LLC_MISS)))
                return PQOS_RETVAL_PARAM;

        if ((event & (PQOS_MON_EVENT_L3_OCCUP | PQOS_MON_EVENT_LMEM_BW |
                      PQOS_MON_EVENT_TMEM_BW | PQOS_MON_EVENT_RMEM_BW)) == 0 &&
            (event & (PQOS_PERF_EVENT_IPC | PQOS_PERF_EVENT_LLC_MISS)) != 0)
                return PQOS_RETVAL_PARAM;

        _pqos_api_lock();

        ret = _pqos_check_init(1);
        if (ret != PQOS_RETVAL_OK) {
                _pqos_api_unlock();
                return ret;
        }

        if (m_interface == PQOS_INTER_MSR)
                ret = hw_mon_start(num_cores, cores, event, context, group);
        else {
#ifdef __linux__
                ret = os_mon_start(num_cores, cores, event, context, group);
#else
                LOG_INFO("OS interface not supported!\n");
                ret = PQOS_RETVAL_RESOURCE;
#endif
        }
        if (ret == PQOS_RETVAL_OK)
                group->valid = GROUP_VALID_MARKER;

        _pqos_api_unlock();

        return ret;
}

int
pqos_mon_stop(struct pqos_mon_data *group)
{
        int ret;

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

        if (m_interface == PQOS_INTER_MSR)
                ret = hw_mon_stop(group);
        else {
#ifdef __linux__
                ret = os_mon_stop(group);
#else
                LOG_INFO("OS interface not supported!\n");
                ret = PQOS_RETVAL_RESOURCE;
#endif
        }
        _pqos_api_unlock();

        return ret;
}

int
pqos_mon_poll(struct pqos_mon_data **groups,
              const unsigned num_groups)
{
        int ret;
        unsigned i;

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

        if (m_interface == PQOS_INTER_MSR)
                ret = hw_mon_poll(groups, num_groups);
        else {
#ifdef __linux__
                ret = os_mon_poll(groups, num_groups);
#else
                LOG_INFO("OS interface not supported!\n");
                ret = PQOS_RETVAL_RESOURCE;
#endif
        }
        _pqos_api_unlock();

        return ret;
}

int
pqos_mon_start_pid(const pid_t pid,
                   const enum pqos_mon_event event,
                   void *context,
                   struct pqos_mon_data *group)
{
        return pqos_mon_start_pids(1, &pid, event, context, group);
}

int
pqos_mon_start_pids(const unsigned num_pids,
                    const pid_t *pids,
                    const enum pqos_mon_event event,
                    void *context,
                    struct pqos_mon_data *group)
{
        int ret;

        if (num_pids == 0 || pids == NULL || group == NULL || event == 0)
                return PQOS_RETVAL_PARAM;

        if (group->valid == GROUP_VALID_MARKER)
                return PQOS_RETVAL_PARAM;

        if (m_interface != PQOS_INTER_OS &&
                m_interface != PQOS_INTER_OS_RESCTRL_MON) {
                LOG_ERROR("Incompatible interface "
                          "selected for task monitoring!\n");
                return PQOS_RETVAL_ERROR;
        }

        /**
         * Validate event parameter
         * - only combinations of events allowed
         * - do not allow non-PQoS events to be monitored on its own
         */
        if (event & (~(PQOS_MON_EVENT_L3_OCCUP | PQOS_MON_EVENT_LMEM_BW |
                       PQOS_MON_EVENT_TMEM_BW | PQOS_MON_EVENT_RMEM_BW |
                       PQOS_PERF_EVENT_IPC | PQOS_PERF_EVENT_LLC_MISS)))
                return PQOS_RETVAL_PARAM;

        if ((event & (PQOS_MON_EVENT_L3_OCCUP | PQOS_MON_EVENT_LMEM_BW |
                      PQOS_MON_EVENT_TMEM_BW | PQOS_MON_EVENT_RMEM_BW)) == 0 &&
            (event & (PQOS_PERF_EVENT_IPC | PQOS_PERF_EVENT_LLC_MISS)) != 0)
                return PQOS_RETVAL_PARAM;

        _pqos_api_lock();

        ret = _pqos_check_init(1);
        if (ret != PQOS_RETVAL_OK) {
                _pqos_api_unlock();
                return ret;
        }

#ifdef __linux__
        ret = os_mon_start_pids(num_pids, pids, event, context, group);
#else
        UNUSED_PARAM(context);
        LOG_INFO("OS interface not supported!\n");
        ret = PQOS_RETVAL_RESOURCE;
#endif

        if (ret == PQOS_RETVAL_OK)
                group->valid = GROUP_VALID_MARKER;

        _pqos_api_unlock();

        return ret;
}

int pqos_mon_add_pids(const unsigned num_pids,
                      const pid_t *pids,
                      struct pqos_mon_data *group)
{
        int ret;

        if (num_pids == 0 || pids == NULL || group == NULL)
                return PQOS_RETVAL_PARAM;

        if (group->valid != GROUP_VALID_MARKER)
                return PQOS_RETVAL_PARAM;

        if (m_interface != PQOS_INTER_OS &&
                m_interface != PQOS_INTER_OS_RESCTRL_MON) {
                LOG_ERROR("Incompatible interface "
                          "selected for task monitoring!\n");
                return PQOS_RETVAL_ERROR;
        }

        _pqos_api_lock();

        ret = _pqos_check_init(1);
        if (ret != PQOS_RETVAL_OK) {
                _pqos_api_unlock();
                return ret;
        }

#ifdef __linux__
        ret = os_mon_add_pids(num_pids, pids, group);
#else
        LOG_INFO("OS interface not supported!\n");
        ret = PQOS_RETVAL_RESOURCE;
#endif

        _pqos_api_unlock();

        return ret;
}

int pqos_mon_remove_pids(const unsigned num_pids,
                         const pid_t *pids,
                         struct pqos_mon_data *group)
{
        int ret;

        if (num_pids == 0 || pids == NULL || group == NULL)
                return PQOS_RETVAL_PARAM;

        if (group->valid != GROUP_VALID_MARKER)
                return PQOS_RETVAL_PARAM;

        if (m_interface != PQOS_INTER_OS &&
                m_interface != PQOS_INTER_OS_RESCTRL_MON) {
                LOG_ERROR("Incompatible interface "
                          "selected for task monitoring!\n");
                return PQOS_RETVAL_ERROR;
        }

        _pqos_api_lock();

        ret = _pqos_check_init(1);
        if (ret != PQOS_RETVAL_OK) {
                _pqos_api_unlock();
                return ret;
        }

#ifdef __linux__
        ret = os_mon_remove_pids(num_pids, pids, group);
#else
        LOG_INFO("OS interface not supported!\n");
        ret = PQOS_RETVAL_RESOURCE;
#endif

        _pqos_api_unlock();

        return ret;
}
