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
 * @brief Implementation of CAT releated PQoS API
 *
 * CPUID and MSR operations are done on the 'local'/host system.
 * Module operate directly on CAT registers.
 */

#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "pqos.h"

#include "host_cap.h"
#include "host_allocation.h"

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

/**
 * Allocation class of service (COS) MSR registers
 */
#define PQOS_MSR_L3CA_MASK_START 0xC90
#define PQOS_MSR_L3CA_MASK_END   0xD0F
#define PQOS_MSR_L3CA_MASK_NUMOF \
        (PQOS_MSR_L3CA_MASK_END - PQOS_MSR_L3CA_MASK_START + 1)

#define PQOS_MSR_L2CA_MASK_START 0xD10
#define PQOS_MSR_L2CA_MASK_END   0xD4F
#define PQOS_MSR_L2CA_MASK_NUMOF \
        (PQOS_MSR_L2CA_MASK_END - PQOS_MSR_L2CA_MASK_START + 1)

/**
 * ---------------------------------------
 * Local data types
 * ---------------------------------------
 */

/**
 * ---------------------------------------
 * Local data structures
 * ---------------------------------------
 */
const struct pqos_cap *m_cap = NULL;
const struct pqos_cpuinfo *m_cpu = NULL;

/**
 * ---------------------------------------
 * External data
 * ---------------------------------------
 */

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
pqos_alloc_init(const struct pqos_cpuinfo *cpu,
                const struct pqos_cap *cap,
                const struct pqos_config *cfg)
{
        int ret = PQOS_RETVAL_OK;

        UNUSED_PARAM(cfg);
        m_cap = cap;
        m_cpu = cpu;
        return ret;
}

int
pqos_alloc_fini(void)
{
        int ret = PQOS_RETVAL_OK;

        m_cap = NULL;
        m_cpu = NULL;
        return ret;
}

/**
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
static int is_contiguous(uint64_t bitmask)
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
pqos_l3ca_set(const unsigned socket,
              const unsigned num_ca,
              const struct pqos_l3ca *ca)
{
        int ret = PQOS_RETVAL_OK;
        unsigned i = 0, count = 0, core = 0;
        int cdp_enabled = 0;

        _pqos_api_lock();

        ret = _pqos_check_init(1);
        if (ret != PQOS_RETVAL_OK) {
                _pqos_api_unlock();
                return ret;
        }

        if (ca == NULL || num_ca == 0) {
                _pqos_api_unlock();
                return PQOS_RETVAL_PARAM;
        }

        /**
         * Check if class bitmasks are contiguous.
         */
        for (i = 0; i < num_ca; i++) {
                int is_contig = 0;

                if (ca[i].cdp) {
                        is_contig = is_contiguous(ca[i].data_mask) &&
                                is_contiguous(ca[i].code_mask);
                } else {
                        is_contig = is_contiguous(ca[i].ways_mask);
                }
                if (!is_contig) {
                        LOG_ERROR("COS%u bit mask is not contiguous!\n",
                                  ca[i].class_id);
                        _pqos_api_unlock();
                        return PQOS_RETVAL_PARAM;
                }
        }

        ASSERT(m_cap != NULL);
        ret = pqos_l3ca_get_cos_num(m_cap, &count);
        if (ret != PQOS_RETVAL_OK) {
                _pqos_api_unlock();
                return ret;             /**< perhaps no L3CA capability */
        }

        if (num_ca > count) {
                _pqos_api_unlock();
                return PQOS_RETVAL_ERROR;
        }

        ret = pqos_l3ca_cdp_enabled(m_cap, NULL, &cdp_enabled);
        if (ret != PQOS_RETVAL_OK) {
                _pqos_api_unlock();
                return ret;
        }

        ASSERT(m_cpu != NULL);
        ret = pqos_cpu_get_cores(m_cpu, socket, 1, &count, &core);
        if (ret != PQOS_RETVAL_OK) {
                _pqos_api_unlock();
                return ret;
        }

        if (cdp_enabled) {
                for (i = 0; i < num_ca; i++) {
                        uint32_t reg =
                                (ca[i].class_id*2) + PQOS_MSR_L3CA_MASK_START;
                        int retval = MACHINE_RETVAL_OK;
                        uint64_t cmask = 0, dmask = 0;

                        if (ca[i].cdp) {
                                dmask = ca[i].data_mask;
                                cmask = ca[i].code_mask;
                        } else {
                                dmask = ca[i].ways_mask;
                                cmask = ca[i].ways_mask;
                        }

                        retval = msr_write(core, reg, dmask);
                        if (retval != MACHINE_RETVAL_OK) {
                                _pqos_api_unlock();
                                return PQOS_RETVAL_ERROR;
                        }

                        retval = msr_write(core, reg+1, cmask);
                        if (retval != MACHINE_RETVAL_OK) {
                                _pqos_api_unlock();
                                return PQOS_RETVAL_ERROR;
                        }
                }
        } else {
                for (i = 0; i < num_ca; i++) {
                        uint32_t reg =
                                ca[i].class_id + PQOS_MSR_L3CA_MASK_START;
                        uint64_t val = ca[i].ways_mask;
                        int retval = MACHINE_RETVAL_OK;

                        if (ca[i].cdp) {
                                LOG_ERROR("Attempting to set CDP COS "
                                          "while CDP is disabled!\n");
                                _pqos_api_unlock();
                                return PQOS_RETVAL_ERROR;
                        }

                        retval = msr_write(core, reg, val);
                        if (retval != MACHINE_RETVAL_OK) {
                                _pqos_api_unlock();
                                return PQOS_RETVAL_ERROR;
                        }
                }
        }

        _pqos_api_unlock();
        return ret;
}

int
pqos_l3ca_get(const unsigned socket,
              const unsigned max_num_ca,
              unsigned *num_ca,
              struct pqos_l3ca *ca)
{
        int ret = PQOS_RETVAL_OK;
        unsigned i = 0, count = 0,
                core = 0, core_count = 0;
        uint32_t reg = 0;
        uint64_t val = 0;
        int retval = MACHINE_RETVAL_OK;
        int cdp_enabled = 0;

        _pqos_api_lock();

        ret = _pqos_check_init(1);
        if (ret != PQOS_RETVAL_OK) {
                _pqos_api_unlock();
                return ret;
        }

        if (num_ca == NULL || ca == NULL || max_num_ca == 0) {
                _pqos_api_unlock();
                return PQOS_RETVAL_PARAM;
        }

        ASSERT(m_cap != NULL);
        ret = pqos_l3ca_get_cos_num(m_cap, &count);
        if (ret != PQOS_RETVAL_OK) {
                _pqos_api_unlock();
                return ret;             /**< perhaps no L3CA capability */
        }

        ret = pqos_l3ca_cdp_enabled(m_cap, NULL, &cdp_enabled);
        if (ret != PQOS_RETVAL_OK) {
                _pqos_api_unlock();
                return ret;
        }

        if (count > max_num_ca) {
                _pqos_api_unlock();
                return PQOS_RETVAL_ERROR;
        }

        ASSERT(m_cpu != NULL);
        ret = pqos_cpu_get_cores(m_cpu, socket, 1, &core_count, &core);
        if (ret != PQOS_RETVAL_OK) {
                _pqos_api_unlock();
                return ret;
        }
        ASSERT(core_count > 0);

        if (cdp_enabled) {
                for (i = 0, reg = PQOS_MSR_L3CA_MASK_START;
                     i < count; i++, reg += 2) {
                        ca[i].cdp = 1;
                        ca[i].class_id = i;

                        retval = msr_read(core, reg, &val);
                        if (retval != MACHINE_RETVAL_OK) {
                                _pqos_api_unlock();
                                return PQOS_RETVAL_ERROR;
                        }
                        ca[i].data_mask = val;

                        retval = msr_read(core, reg+1, &val);
                        if (retval != MACHINE_RETVAL_OK) {
                                _pqos_api_unlock();
                                return PQOS_RETVAL_ERROR;
                        }
                        ca[i].code_mask = val;
                }
        } else {
                for (i = 0, reg = PQOS_MSR_L3CA_MASK_START;
                     i < count; i++, reg++) {
                        retval = msr_read(core, reg, &val);
                        if (retval != MACHINE_RETVAL_OK) {
                                _pqos_api_unlock();
                                return PQOS_RETVAL_ERROR;
                        }
                        ca[i].cdp = 0;
                        ca[i].class_id = i;
                        ca[i].ways_mask = val;
                }
        }

        *num_ca = count;

        _pqos_api_unlock();
        return ret;
}

int
pqos_alloc_assoc_set(const unsigned lcore,
                     const unsigned class_id)
{
        int ret = PQOS_RETVAL_OK;
        unsigned num_classes = 0;
        uint32_t reg = 0;
        uint64_t val = 0;
        int retval = MACHINE_RETVAL_OK;

        _pqos_api_lock();

        ret = _pqos_check_init(1);
        if (ret != PQOS_RETVAL_OK) {
                _pqos_api_unlock();
                return ret;
        }

        ASSERT(m_cpu != NULL);
        ret = pqos_cpu_check_core(m_cpu, lcore);
        if (ret != PQOS_RETVAL_OK) {
                _pqos_api_unlock();
                return PQOS_RETVAL_PARAM;
        }

        ASSERT(m_cap != NULL);
        ret = pqos_l3ca_get_cos_num(m_cap, &num_classes);
        if (ret != PQOS_RETVAL_OK) {
                _pqos_api_unlock();
                return ret;             /**< perhaps no L3CA capability */
        }

        if (class_id > num_classes) {
                _pqos_api_unlock();
                return PQOS_RETVAL_PARAM;
        }

        reg = PQOS_MSR_ASSOC;
        retval = msr_read(lcore, reg, &val);
        if (retval != MACHINE_RETVAL_OK) {
                _pqos_api_unlock();
                return PQOS_RETVAL_ERROR;
        }

        val &= (~PQOS_MSR_ASSOC_QECOS_MASK);
        val |= (((uint64_t) class_id) << PQOS_MSR_ASSOC_QECOS_SHIFT);

        retval = msr_write(lcore, reg, val);
        if (retval != MACHINE_RETVAL_OK) {
                _pqos_api_unlock();
                return PQOS_RETVAL_ERROR;
        }

        _pqos_api_unlock();
        return ret;
}

int
pqos_alloc_assoc_get(const unsigned lcore,
                     unsigned *class_id)
{
        const struct pqos_capability *cap = NULL;
        int ret = PQOS_RETVAL_OK;
        uint32_t reg = 0;
        uint64_t val = 0;
        int retval = MACHINE_RETVAL_OK;

        _pqos_api_lock();

        ret = _pqos_check_init(1);
        if (ret != PQOS_RETVAL_OK) {
                _pqos_api_unlock();
                return ret;
        }

        if (class_id == NULL) {
                _pqos_api_unlock();
                return PQOS_RETVAL_PARAM;
        }

        ASSERT(m_cpu != NULL);
        ret = pqos_cpu_check_core(m_cpu, lcore);
        if (ret != PQOS_RETVAL_OK) {
                _pqos_api_unlock();
                return PQOS_RETVAL_PARAM;
        }

        ASSERT(m_cap != NULL);
        ret = pqos_cap_get_type(m_cap, PQOS_CAP_TYPE_L3CA, &cap);
        if (ret != PQOS_RETVAL_OK) {
                _pqos_api_unlock();
                return ret;             /**< no L3CA capability */
        }

        reg = PQOS_MSR_ASSOC;
        retval = msr_read(lcore, reg, &val);
        if (retval != MACHINE_RETVAL_OK) {
                _pqos_api_unlock();
                return PQOS_RETVAL_ERROR;
        }

        val >>= PQOS_MSR_ASSOC_QECOS_SHIFT;
        *class_id = (unsigned) val;

        _pqos_api_unlock();
        return ret;
}
