/*
 * BSD LICENSE
 *
 * Copyright(c) 2014-2020 Intel Corporation. All rights reserved.
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
 * @brief Implementation of CAT related PQoS API
 *
 * CPUID and MSR operations are done on the 'local'/host system.
 * Module operate directly on CAT registers.
 */

#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "pqos.h"

#include "cap.h"
#include "allocation.h"
#include "os_allocation.h"

#include "machine.h"
#include "types.h"
#include "log.h"
#include "cpu_registers.h"
#include "cpuinfo.h"

/**
 * ---------------------------------------
 * Local macros
 * ---------------------------------------
 */

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
static const struct pqos_cpuinfo *m_cpu = NULL;
static int m_interface = PQOS_INTER_MSR;
/**
 * ---------------------------------------
 * External data
 * ---------------------------------------
 */

/**
 * ---------------------------------------
 * Internal functions
 * ---------------------------------------
 */

/**
 * @brief Gets COS associated to \a lcore
 *
 * @param [in] lcore lcore to read COS association from
 * @param [out] class_id associated COS
 *
 * @return Operation status
 */
static int
cos_assoc_get(const unsigned lcore, unsigned *class_id)
{
        const uint32_t reg = PQOS_MSR_ASSOC;
        uint64_t val = 0;

        if (class_id == NULL)
                return PQOS_RETVAL_PARAM;

        if (msr_read(lcore, reg, &val) != MACHINE_RETVAL_OK)
                return PQOS_RETVAL_ERROR;

        val >>= PQOS_MSR_ASSOC_QECOS_SHIFT;
        *class_id = (unsigned)val;

        return PQOS_RETVAL_OK;
}

/**
 * @brief Gets unused COS on a socket or L2 cluster
 *
 * The lowest acceptable COS is 1, as 0 is a default one
 *
 * @param [in] technology selection of allocation technologies
 * @param [in] l3cat_id L3 CAT resource id
 * @param [in] l2cat_id L2 CAT resource id
 * @param [in] mba_id MBA resource id
 * @param [out] class_id unused COS
 *
 * NOTE: It is our assumption that mba id and cat ids are same for
 * a core. In future, if a core can have different mba id and cat ids
 * then, we need to change this function to handle it.
 *
 * @return Operation status
 */
static int
get_unused_cos(const unsigned technology,
               unsigned l3cat_id,
               unsigned l2cat_id,
               unsigned mba_id,
               unsigned *class_id)
{
        unsigned num_l2_cos = 0, num_l3_cos = 0, num_mba_cos = 0;
        unsigned num_cos = 0;
        unsigned i, cos;
        int ret;
        const int l2_req = ((technology & (1 << PQOS_CAP_TYPE_L2CA)) != 0);
        const int l3_req = ((technology & (1 << PQOS_CAP_TYPE_L3CA)) != 0);
        const int mba_req = ((technology & (1 << PQOS_CAP_TYPE_MBA)) != 0);
        int l2cat_id_set = l2_req;
        int l3cat_id_set = l3_req;
        int mba_id_set = mba_req;
        const struct pqos_cap *cap;
        const struct pqos_cpuinfo *cpu;
        unsigned used_classes[PQOS_MAX_COS];

        if (class_id == NULL)
                return PQOS_RETVAL_PARAM;

        _pqos_cap_get(&cap, &cpu);

        memset(used_classes, 0, sizeof(used_classes));

        ret = pqos_l3ca_get_cos_num(cap, &num_l3_cos);
        if (ret != PQOS_RETVAL_OK && ret != PQOS_RETVAL_RESOURCE)
                return ret;

        ret = pqos_l2ca_get_cos_num(cap, &num_l2_cos);
        if (ret != PQOS_RETVAL_OK && ret != PQOS_RETVAL_RESOURCE)
                return ret;

        ret = pqos_mba_get_cos_num(cap, &num_mba_cos);
        if (ret != PQOS_RETVAL_OK && ret != PQOS_RETVAL_RESOURCE)
                return ret;

        /* Obtain highest COS number for requested technologies */
        {
                if (l3_req)
                        num_cos = num_l3_cos;

                if (l2_req && (num_cos == 0 || num_cos > num_l2_cos))
                        num_cos = num_l2_cos;

                if (mba_req && (num_cos == 0 || num_cos > num_mba_cos))
                        num_cos = num_mba_cos;

                if (num_cos == 0)
                        return PQOS_RETVAL_ERROR;
        }

        /* Obtain L3 and MBA ids for L2 cluster*/
        if (l2_req && !l3cat_id_set && !mba_id_set) {
                for (i = 0; i < cpu->num_cores; i++)
                        if (cpu->cores[i].l2_id == l2cat_id) {

                                if (num_l3_cos > 0 && !l3cat_id_set) {
                                        l3cat_id = cpu->cores[i].l3cat_id;
                                        l3cat_id_set = 1;
                                        break;
                                }
                                if (num_mba_cos > 0 && !mba_id_set) {
                                        mba_id = cpu->cores[i].mba_id;
                                        mba_id_set = 1;
                                        break;
                                }
                        }
        }

        /* Create a list of used COS */
        for (i = 0; i < cpu->num_cores; i++) {
                if (l3cat_id_set && cpu->cores[i].l3cat_id != l3cat_id)
                        continue;
                if (mba_id_set && cpu->cores[i].mba_id != mba_id)
                        continue;

                ret = cos_assoc_get(cpu->cores[i].lcore, &cos);
                if (ret != PQOS_RETVAL_OK)
                        return ret;

                if (cos >= num_cos)
                        continue;

                /* COS does not support L3CAT and MBA need to check
                L2 cluster only */
                if (cos >= num_l3_cos && cos >= num_mba_cos && l2cat_id_set &&
                    cpu->cores[i].l2_id != l2cat_id)
                        continue;

                /* Mark as used */
                used_classes[cos] = 1;
        }

        /* Find unused COS */
        for (cos = num_cos - 1; cos != 0; cos--) {
                if (used_classes[cos] == 0) {
                        *class_id = cos;
                        return PQOS_RETVAL_OK;
                }
        }

        return PQOS_RETVAL_RESOURCE;
}

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

#ifndef __linux__
        UNUSED_PARAM(cap);
#endif

        m_cpu = cpu;
        if (cfg == NULL)
                m_interface = PQOS_INTER_MSR;
        else if (cfg->interface == PQOS_INTER_OS_RESCTRL_MON)
                m_interface = PQOS_INTER_OS;
        else
                m_interface = cfg->interface;
#ifdef __linux__
        if (m_interface == PQOS_INTER_OS)
                ret = os_alloc_init(cpu, cap);
#endif
        return ret;
}

int
pqos_alloc_fini(void)
{
        int ret = PQOS_RETVAL_OK;

        m_cpu = NULL;
#ifdef __linux__
        if (m_interface == PQOS_INTER_OS)
                ret = os_alloc_fini();
#endif
        return ret;
}

/**
 * =======================================
 * L3 cache allocation
 * =======================================
 */

int
hw_l3ca_set(const unsigned l3cat_id,
            const unsigned num_ca,
            const struct pqos_l3ca *ca)
{
        int ret = PQOS_RETVAL_OK;
        unsigned i = 0, count = 0, core = 0;
        int cdp_enabled = 0;
        const struct pqos_cap *cap;
        const struct pqos_cpuinfo *cpu;

        ASSERT(ca != NULL);
        ASSERT(num_ca != 0);

        _pqos_cap_get(&cap, &cpu);

        ret = pqos_l3ca_get_cos_num(cap, &count);
        if (ret != PQOS_RETVAL_OK)
                return ret; /**< perhaps no L3CA capability */

        if (num_ca > count)
                return PQOS_RETVAL_ERROR;

        ret = pqos_l3ca_cdp_enabled(cap, NULL, &cdp_enabled);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        ASSERT(m_cpu != NULL);
        ret = pqos_cpu_get_one_by_l3cat_id(cpu, l3cat_id, &core);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        if (cdp_enabled) {
                for (i = 0; i < num_ca; i++) {
                        uint32_t reg =
                            (ca[i].class_id * 2) + PQOS_MSR_L3CA_MASK_START;
                        int retval = MACHINE_RETVAL_OK;
                        uint64_t cmask = 0, dmask = 0;

                        if (ca[i].cdp) {
                                dmask = ca[i].u.s.data_mask;
                                cmask = ca[i].u.s.code_mask;
                        } else {
                                dmask = ca[i].u.ways_mask;
                                cmask = ca[i].u.ways_mask;
                        }

                        retval = msr_write(core, reg, dmask);
                        if (retval != MACHINE_RETVAL_OK)
                                return PQOS_RETVAL_ERROR;

                        retval = msr_write(core, reg + 1, cmask);
                        if (retval != MACHINE_RETVAL_OK)
                                return PQOS_RETVAL_ERROR;
                }
        } else {
                for (i = 0; i < num_ca; i++) {
                        uint32_t reg =
                            ca[i].class_id + PQOS_MSR_L3CA_MASK_START;
                        uint64_t val = ca[i].u.ways_mask;
                        int retval = MACHINE_RETVAL_OK;

                        if (ca[i].cdp) {
                                LOG_ERROR("Attempting to set CDP COS "
                                          "while L3 CDP is disabled!\n");
                                return PQOS_RETVAL_ERROR;
                        }

                        retval = msr_write(core, reg, val);
                        if (retval != MACHINE_RETVAL_OK)
                                return PQOS_RETVAL_ERROR;
                }
        }
        return ret;
}

int
hw_l3ca_get(const unsigned l3cat_id,
            const unsigned max_num_ca,
            unsigned *num_ca,
            struct pqos_l3ca *ca)
{
        int ret = PQOS_RETVAL_OK;
        unsigned i = 0, count = 0, core = 0;
        uint32_t reg = 0;
        uint64_t val = 0;
        int retval = MACHINE_RETVAL_OK;
        int cdp_enabled = 0;
        const struct pqos_cap *cap;

        ASSERT(num_ca != NULL);
        ASSERT(ca != NULL);
        ASSERT(max_num_ca != 0);

        _pqos_cap_get(&cap, NULL);

        ret = pqos_l3ca_get_cos_num(cap, &count);
        if (ret != PQOS_RETVAL_OK)
                return ret; /**< perhaps no L3CA capability */

        ret = pqos_l3ca_cdp_enabled(cap, NULL, &cdp_enabled);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        if (count > max_num_ca)
                return PQOS_RETVAL_ERROR;

        ASSERT(m_cpu != NULL);
        ret = pqos_cpu_get_one_by_l3cat_id(m_cpu, l3cat_id, &core);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        if (cdp_enabled) {
                for (i = 0, reg = PQOS_MSR_L3CA_MASK_START; i < count;
                     i++, reg += 2) {
                        ca[i].cdp = 1;
                        ca[i].class_id = i;

                        retval = msr_read(core, reg, &val);
                        if (retval != MACHINE_RETVAL_OK)
                                return PQOS_RETVAL_ERROR;

                        ca[i].u.s.data_mask = val;

                        retval = msr_read(core, reg + 1, &val);
                        if (retval != MACHINE_RETVAL_OK)
                                return PQOS_RETVAL_ERROR;

                        ca[i].u.s.code_mask = val;
                }
        } else {
                for (i = 0, reg = PQOS_MSR_L3CA_MASK_START; i < count;
                     i++, reg++) {
                        retval = msr_read(core, reg, &val);
                        if (retval != MACHINE_RETVAL_OK)
                                return PQOS_RETVAL_ERROR;

                        ca[i].cdp = 0;
                        ca[i].class_id = i;
                        ca[i].u.ways_mask = val;
                }
        }
        *num_ca = count;

        return ret;
}

int
hw_l3ca_get_min_cbm_bits(unsigned *min_cbm_bits)
{
        int ret;
        unsigned *l3cat_ids, l3cat_id, l3cat_id_num;
        unsigned class_id, l3ca_num, ways, i;
        int technology = 1 << PQOS_CAP_TYPE_L3CA;
        const struct pqos_capability *l3_cap = NULL;
        struct pqos_l3ca l3ca_config[PQOS_MAX_L3CA_COS];

        ASSERT(m_cpu != NULL);
        ASSERT(min_cbm_bits != NULL);

        /**
         * Get L3 CAT capabilities
         */
        ret = _pqos_cap_get_type(PQOS_CAP_TYPE_L3CA, &l3_cap);
        if (ret != PQOS_RETVAL_OK)
                return PQOS_RETVAL_RESOURCE; /* L3 CAT not supported */

        /**
         * Get number & list of l3cat_ids in the system
         */
        l3cat_ids = pqos_cpu_get_l3cat_ids(m_cpu, &l3cat_id_num);
        if (l3cat_ids == NULL || l3cat_id_num == 0) {
                ret = PQOS_RETVAL_ERROR;
                goto pqos_l3ca_get_min_cbm_bits_exit;
        }

        /**
         * Find free COS
         */
        for (l3cat_id = 0; l3cat_id < l3cat_id_num; l3cat_id++) {
                ret = get_unused_cos(technology, l3cat_id, 0, 0, &class_id);
                if (ret == PQOS_RETVAL_OK)
                        break;

                if (ret != PQOS_RETVAL_RESOURCE)
                        goto pqos_l3ca_get_min_cbm_bits_exit;
        }

        if (ret == PQOS_RETVAL_RESOURCE) {
                LOG_INFO("No free L3 COS available. "
                         "Unable to determine minimum L3 CBM bits\n");
                goto pqos_l3ca_get_min_cbm_bits_exit;
        }

        /**
         * Get current configuration
         */
        ret = hw_l3ca_get(l3cat_id, PQOS_MAX_L3CA_COS, &l3ca_num, l3ca_config);
        if (ret != PQOS_RETVAL_OK)
                goto pqos_l3ca_get_min_cbm_bits_exit;

        /**
         * Probe for min cbm bits
         */
        for (ways = 1; ways <= l3_cap->u.l3ca->num_ways; ways++) {
                struct pqos_l3ca l3ca_tab[PQOS_MAX_L3CA_COS];
                unsigned num_ca;
                uint64_t mask = (1 << ways) - 1;

                memset(l3ca_tab, 0, sizeof(struct pqos_l3ca));
                l3ca_tab[0].class_id = class_id;
                l3ca_tab[0].u.ways_mask = mask;

                /**
                 * Try to set mask
                 */
                ret = hw_l3ca_set(l3cat_id, 1, l3ca_tab);
                if (ret != PQOS_RETVAL_OK)
                        continue;

                /**
                 * Validate if mask was correctly set
                 */
                ret =
                    hw_l3ca_get(l3cat_id, PQOS_MAX_L3CA_COS, &num_ca, l3ca_tab);
                if (ret != PQOS_RETVAL_OK)
                        goto pqos_l3ca_get_min_cbm_bits_restore;

                for (i = 0; i < num_ca; i++) {
                        struct pqos_l3ca *l3ca = &(l3ca_tab[i]);

                        if (l3ca->class_id != class_id)
                                continue;

                        if ((l3ca->cdp && l3ca->u.s.data_mask == mask &&
                             l3ca->u.s.code_mask == mask) ||
                            (!l3ca->cdp && l3ca->u.ways_mask == mask)) {
                                *min_cbm_bits = ways;
                                ret = PQOS_RETVAL_OK;
                                goto pqos_l3ca_get_min_cbm_bits_restore;
                        }
                }
        }

        /**
         * Restore old settings
         */
pqos_l3ca_get_min_cbm_bits_restore:
        for (i = 0; i < l3ca_num; i++) {
                int ret_val;

                if (l3ca_config[i].class_id != class_id)
                        continue;

                ret_val = hw_l3ca_set(l3cat_id, 1, &(l3ca_config[i]));
                if (ret_val != PQOS_RETVAL_OK) {
                        LOG_ERROR("Failed to restore CAT configuration. CAT"
                                  " configuration has been altered!\n");
                        ret = ret_val;
                        break;
                }
        }

pqos_l3ca_get_min_cbm_bits_exit:
        if (l3cat_ids != NULL)
                free(l3cat_ids);

        return ret;
}

int
hw_l2ca_set(const unsigned l2id,
            const unsigned num_ca,
            const struct pqos_l2ca *ca)
{
        int ret = PQOS_RETVAL_OK;
        unsigned i = 0, count = 0, core = 0;
        int cdp_enabled = 0;
        const struct pqos_cap *cap;
        const struct pqos_cpuinfo *cpu;

        ASSERT(ca != NULL);
        ASSERT(num_ca != 0);

        _pqos_cap_get(&cap, &cpu);

        /*
         * Check if L2 CAT is supported
         */
        ret = pqos_l2ca_get_cos_num(cap, &count);
        if (ret != PQOS_RETVAL_OK)
                return PQOS_RETVAL_RESOURCE; /* L2 CAT not supported */

        /**
         * Check if class id's are within allowed range.
         */
        for (i = 0; i < num_ca; i++) {
                if (ca[i].class_id >= count) {
                        LOG_ERROR("L2 COS%u is out of range (COS%u is max)!\n",
                                  ca[i].class_id, count - 1);
                        return PQOS_RETVAL_PARAM;
                }
        }

        ret = pqos_l2ca_cdp_enabled(cap, NULL, &cdp_enabled);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        /**
         * Pick one core from the L2 cluster and
         * perform MSR writes to COS registers on the cluster.
         */
        ret = pqos_cpu_get_one_by_l2id(cpu, l2id, &core);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        for (i = 0; i < num_ca; i++) {
                if (cdp_enabled) {
                        uint32_t reg =
                            (ca[i].class_id * 2) + PQOS_MSR_L2CA_MASK_START;
                        int retval = MACHINE_RETVAL_OK;
                        uint64_t cmask = 0, dmask = 0;

                        if (ca[i].cdp) {
                                dmask = ca[i].u.s.data_mask;
                                cmask = ca[i].u.s.code_mask;
                        } else {
                                dmask = ca[i].u.ways_mask;
                                cmask = ca[i].u.ways_mask;
                        }

                        retval = msr_write(core, reg, dmask);
                        if (retval != MACHINE_RETVAL_OK)
                                return PQOS_RETVAL_ERROR;

                        retval = msr_write(core, reg + 1, cmask);
                        if (retval != MACHINE_RETVAL_OK)
                                return PQOS_RETVAL_ERROR;
                } else {
                        uint32_t reg =
                            ca[i].class_id + PQOS_MSR_L2CA_MASK_START;
                        uint64_t val = ca[i].u.ways_mask;
                        int retval;

                        if (ca[i].cdp) {
                                LOG_ERROR("Attempting to set CDP COS "
                                          "while L2 CDP is disabled!\n");
                                return PQOS_RETVAL_ERROR;
                        }

                        retval = msr_write(core, reg, val);
                        if (retval != MACHINE_RETVAL_OK)
                                return PQOS_RETVAL_ERROR;
                }
        }

        return ret;
}

int
hw_l2ca_get(const unsigned l2id,
            const unsigned max_num_ca,
            unsigned *num_ca,
            struct pqos_l2ca *ca)
{
        int ret = PQOS_RETVAL_OK;
        unsigned i = 0, count = 0;
        unsigned core = 0;
        int cdp_enabled = 0;
        const struct pqos_cap *cap;

        ASSERT(num_ca != NULL);
        ASSERT(ca != NULL);
        ASSERT(max_num_ca != 0);

        _pqos_cap_get(&cap, NULL);

        ret = pqos_l2ca_get_cos_num(cap, &count);
        if (ret != PQOS_RETVAL_OK)
                return PQOS_RETVAL_RESOURCE; /* L2 CAT not supported */

        ret = pqos_l2ca_cdp_enabled(cap, NULL, &cdp_enabled);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        if (max_num_ca < count)
                /* Not enough space to store the classes */
                return PQOS_RETVAL_PARAM;

        ASSERT(m_cpu != NULL);
        ret = pqos_cpu_get_one_by_l2id(m_cpu, l2id, &core);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        for (i = 0; i < count; i++) {
                int retval;
                uint64_t val;

                ca[i].class_id = i;
                ca[i].cdp = cdp_enabled;

                if (cdp_enabled) {
                        const uint32_t reg = PQOS_MSR_L2CA_MASK_START + i * 2;

                        retval = msr_read(core, reg, &val);
                        if (retval != MACHINE_RETVAL_OK)
                                return PQOS_RETVAL_ERROR;

                        ca[i].u.s.data_mask = val;

                        retval = msr_read(core, reg + 1, &val);
                        if (retval != MACHINE_RETVAL_OK)
                                return PQOS_RETVAL_ERROR;

                        ca[i].u.s.code_mask = val;

                } else {
                        const uint32_t reg = PQOS_MSR_L2CA_MASK_START + i;

                        retval = msr_read(core, reg, &val);
                        if (retval != MACHINE_RETVAL_OK)
                                return PQOS_RETVAL_ERROR;

                        ca[i].u.ways_mask = val;
                }
        }
        *num_ca = count;

        return ret;
}

int
hw_l2ca_get_min_cbm_bits(unsigned *min_cbm_bits)
{
        int ret;
        unsigned *l2ids = NULL, l2id_num = 0, l2id;
        unsigned class_id, l2ca_num, ways, i;
        int technology = 1 << PQOS_CAP_TYPE_L2CA;
        const struct pqos_capability *l2_cap = NULL;
        struct pqos_l2ca l2ca_config[PQOS_MAX_L2CA_COS];

        ASSERT(m_cpu != NULL);
        ASSERT(min_cbm_bits != NULL);

        /**
         * Get L2 CAT capabilities
         */
        ret = _pqos_cap_get_type(PQOS_CAP_TYPE_L2CA, &l2_cap);
        if (ret != PQOS_RETVAL_OK)
                return PQOS_RETVAL_RESOURCE; /* L2 CAT not supported */

        /**
         * Get number & list of L2ids in the system
         */
        l2ids = pqos_cpu_get_l2ids(m_cpu, &l2id_num);
        if (l2ids == NULL || l2id_num == 0) {
                ret = PQOS_RETVAL_ERROR;
                goto hw_l2ca_get_min_cbm_bits_exit;
        }

        /**
         * Find free COS
         */
        for (l2id = 0; l2id < l2id_num; l2id++) {
                ret = get_unused_cos(technology, 0, l2id, 0, &class_id);
                if (ret == PQOS_RETVAL_OK)
                        break;
                if (ret != PQOS_RETVAL_RESOURCE)
                        goto hw_l2ca_get_min_cbm_bits_exit;
        }

        if (ret == PQOS_RETVAL_RESOURCE) {
                LOG_INFO("No free L2 COS available. "
                         "Unable to determine minimum L2 CBM bits\n");
                goto hw_l2ca_get_min_cbm_bits_exit;
        }

        /**
         * Get current configuration
         */
        ret = hw_l2ca_get(l2id, PQOS_MAX_L2CA_COS, &l2ca_num, l2ca_config);
        if (ret != PQOS_RETVAL_OK)
                goto hw_l2ca_get_min_cbm_bits_exit;

        /**
         * Probe for min cbm bits
         */
        for (ways = 1; ways <= l2_cap->u.l2ca->num_ways; ways++) {
                struct pqos_l2ca l2ca_tab[PQOS_MAX_L2CA_COS];
                unsigned num_ca;
                uint64_t mask = (1 << ways) - 1;

                memset(l2ca_tab, 0, sizeof(struct pqos_l2ca));
                l2ca_tab[0].class_id = class_id;
                l2ca_tab[0].u.ways_mask = mask;

                /**
                 * Try to set mask
                 */
                ret = hw_l2ca_set(l2id, 1, l2ca_tab);
                if (ret != PQOS_RETVAL_OK)
                        continue;

                /**
                 * Validate if mask was correctly set
                 */
                ret = hw_l2ca_get(l2id, PQOS_MAX_L2CA_COS, &num_ca, l2ca_tab);
                if (ret != PQOS_RETVAL_OK)
                        goto hw_l2ca_get_min_cbm_bits_restore;

                for (i = 0; i < num_ca; i++) {
                        struct pqos_l2ca *l2ca = &(l2ca_tab[i]);

                        if (l2ca->class_id != class_id)
                                continue;

                        if ((l2ca->cdp && l2ca->u.s.data_mask == mask &&
                             l2ca->u.s.code_mask == mask) ||
                            (!l2ca->cdp && l2ca->u.ways_mask == mask)) {
                                *min_cbm_bits = ways;
                                ret = PQOS_RETVAL_OK;
                                goto hw_l2ca_get_min_cbm_bits_restore;
                        }
                }
        }

        /**
         * Restore old settings
         */
hw_l2ca_get_min_cbm_bits_restore:
        for (i = 0; i < l2ca_num; i++) {
                int ret_val;

                if (l2ca_config[i].class_id != class_id)
                        continue;

                ret_val = hw_l2ca_set(l2id, 1, &(l2ca_config[i]));
                if (ret_val != PQOS_RETVAL_OK) {
                        LOG_ERROR("Failed to restore CAT configuration. CAT"
                                  " configuration has been altered!\n");
                        ret = ret_val;
                        break;
                }
        }

hw_l2ca_get_min_cbm_bits_exit:
        if (l2ids != NULL)
                free(l2ids);

        return ret;
}

int
hw_mba_set(const unsigned mba_id,
           const unsigned num_cos,
           const struct pqos_mba *requested,
           struct pqos_mba *actual)
{
        int ret = PQOS_RETVAL_OK;
        unsigned i = 0, count = 0, core = 0, step = 0;
        const struct pqos_capability *mba_cap = NULL;

        ASSERT(requested != NULL);
        ASSERT(num_cos != 0);

        /**
         * Check if MBA is supported
         */
        ret = _pqos_cap_get_type(PQOS_CAP_TYPE_MBA, &mba_cap);
        if (ret != PQOS_RETVAL_OK)
                return PQOS_RETVAL_RESOURCE; /* MBA not supported */

        count = mba_cap->u.mba->num_classes;
        step = mba_cap->u.mba->throttle_step;

        /**
         * Non-linear mode not currently supported
         */
        if (!mba_cap->u.mba->is_linear) {
                LOG_ERROR("MBA non-linear mode not currently supported!\n");
                return PQOS_RETVAL_RESOURCE;
        }
        /**
         * Check if class id's are within allowed range
         * and if a controller is not requested.
         */
        for (i = 0; i < num_cos; i++) {
                if (requested[i].class_id >= count) {
                        LOG_ERROR("MBA COS%u is out of range (COS%u is max)!\n",
                                  requested[i].class_id, count - 1);
                        return PQOS_RETVAL_PARAM;
                }

                if (requested[i].ctrl != 0) {
                        LOG_ERROR("MBA controller not supported!\n");
                        return PQOS_RETVAL_PARAM;
                }
        }

        ASSERT(m_cpu != NULL);
        ret = pqos_cpu_get_one_by_mba_id(m_cpu, mba_id, &core);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        for (i = 0; i < num_cos; i++) {
                const uint32_t reg =
                    requested[i].class_id + PQOS_MSR_MBA_MASK_START;
                uint64_t val =
                    PQOS_MBA_LINEAR_MAX -
                    (((requested[i].mb_max + (step / 2)) / step) * step);
                int retval = MACHINE_RETVAL_OK;

                if (val > mba_cap->u.mba->throttle_max)
                        val = mba_cap->u.mba->throttle_max;

                retval = msr_write(core, reg, val);
                if (retval != MACHINE_RETVAL_OK)
                        return PQOS_RETVAL_ERROR;

                /**
                 * If table to store actual values set is passed,
                 * read MSR values and store in table
                 */
                if (actual == NULL)
                        continue;

                retval = msr_read(core, reg, &val);
                if (retval != MACHINE_RETVAL_OK)
                        return PQOS_RETVAL_ERROR;

                actual[i] = requested[i];
                actual[i].mb_max = (PQOS_MBA_LINEAR_MAX - val);
        }

        return ret;
}

int
hw_mba_set_amd(const unsigned mba_id,
               const unsigned num_cos,
               const struct pqos_mba *requested,
               struct pqos_mba *actual)
{
        int ret = PQOS_RETVAL_OK;
        unsigned i = 0, count = 0, core = 0;
        const struct pqos_capability *mba_cap = NULL;

        ASSERT(requested != NULL);
        ASSERT(num_cos != 0);

        /**
         * Check if MBA is supported
         */
        ret = _pqos_cap_get_type(PQOS_CAP_TYPE_MBA, &mba_cap);
        if (ret != PQOS_RETVAL_OK)
                return PQOS_RETVAL_RESOURCE; /* MBA not supported */

        count = mba_cap->u.mba->num_classes;

        /**
         * Check if class id's are within allowed range
         * and if a controller is not requested.
         */
        for (i = 0; i < num_cos; i++) {
                if (requested[i].class_id >= count) {
                        LOG_ERROR("MBA COS%u is out of range (COS%u is max)!\n",
                                  requested[i].class_id, count - 1);
                        return PQOS_RETVAL_PARAM;
                }

                if (requested[i].ctrl != 0) {
                        LOG_ERROR("MBA controller not supported!\n");
                        return PQOS_RETVAL_PARAM;
                }
        }

        ASSERT(m_cpu != NULL);
        ret = pqos_cpu_get_one_by_mba_id(m_cpu, mba_id, &core);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        for (i = 0; i < num_cos; i++) {
                const uint32_t reg =
                    requested[i].class_id + PQOS_MSR_MBA_MASK_START_AMD;
                uint64_t val = requested[i].mb_max;
                int retval = MACHINE_RETVAL_OK;

                retval = msr_write(core, reg, val);
                if (retval != MACHINE_RETVAL_OK)
                        return PQOS_RETVAL_ERROR;

                /**
                 * If table to store actual values set is passed,
                 * read MSR values and store in table
                 */
                if (actual == NULL)
                        continue;

                retval = msr_read(core, reg, &val);
                if (retval != MACHINE_RETVAL_OK)
                        return PQOS_RETVAL_ERROR;

                actual[i] = requested[i];
                actual[i].mb_max = val;
        }

        return ret;
}

int
hw_mba_get(const unsigned mba_id,
           const unsigned max_num_cos,
           unsigned *num_cos,
           struct pqos_mba *mba_tab)
{
        int ret = PQOS_RETVAL_OK;
        unsigned i = 0, count = 0, core = 0;
        const struct pqos_cap *cap;

        ASSERT(num_cos != NULL);
        ASSERT(mba_tab != NULL);
        ASSERT(max_num_cos != 0);

        _pqos_cap_get(&cap, NULL);

        ret = pqos_mba_get_cos_num(cap, &count);
        if (ret != PQOS_RETVAL_OK)
                return ret; /**< no MBA capability */

        if (count > max_num_cos)
                return PQOS_RETVAL_ERROR;

        ASSERT(m_cpu != NULL);
        ret = pqos_cpu_get_one_by_mba_id(m_cpu, mba_id, &core);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        for (i = 0; i < count; i++) {
                const uint32_t reg = PQOS_MSR_MBA_MASK_START + i;
                uint64_t val = 0;
                int retval = msr_read(core, reg, &val);

                if (retval != MACHINE_RETVAL_OK)
                        return PQOS_RETVAL_ERROR;

                mba_tab[i].ctrl = 0;
                mba_tab[i].class_id = i;
                mba_tab[i].mb_max = (unsigned)PQOS_MBA_LINEAR_MAX - val;
        }
        *num_cos = count;

        return ret;
}

int
hw_mba_get_amd(const unsigned mba_id,
               const unsigned max_num_cos,
               unsigned *num_cos,
               struct pqos_mba *mba_tab)
{
        int ret = PQOS_RETVAL_OK;
        unsigned i = 0, count = 0, core = 0;
        const struct pqos_cap *cap;

        ASSERT(num_cos != NULL);
        ASSERT(mba_tab != NULL);
        ASSERT(max_num_cos != 0);

        _pqos_cap_get(&cap, NULL);

        ret = pqos_mba_get_cos_num(cap, &count);
        if (ret != PQOS_RETVAL_OK)
                return ret; /**< no MBA capability */

        if (count > max_num_cos)
                return PQOS_RETVAL_ERROR;

        ASSERT(m_cpu != NULL);
        ret = pqos_cpu_get_one_by_mba_id(m_cpu, mba_id, &core);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        for (i = 0; i < count; i++) {
                const uint32_t reg = PQOS_MSR_MBA_MASK_START_AMD + i;
                uint64_t val = 0;
                int retval = msr_read(core, reg, &val);

                if (retval != MACHINE_RETVAL_OK)
                        return PQOS_RETVAL_ERROR;

                mba_tab[i].ctrl = 0;
                mba_tab[i].class_id = i;
                mba_tab[i].mb_max = val;
        }
        *num_cos = count;

        return ret;
}
/**
 * @brief Sets COS associated to \a lcore
 *
 * @param [in] lcore lcore to set COS association
 * @param [in] class_id COS to associate lcore to
 *
 * @return Operation status
 */
static int
cos_assoc_set(const unsigned lcore, const unsigned class_id)
{
        const uint32_t reg = PQOS_MSR_ASSOC;
        uint64_t val = 0;
        int ret;

        ret = msr_read(lcore, reg, &val);
        if (ret != MACHINE_RETVAL_OK)
                return PQOS_RETVAL_ERROR;

        val &= (~PQOS_MSR_ASSOC_QECOS_MASK);
        val |= (((uint64_t)class_id) << PQOS_MSR_ASSOC_QECOS_SHIFT);

        ret = msr_write(lcore, reg, val);
        if (ret != MACHINE_RETVAL_OK)
                return PQOS_RETVAL_ERROR;

        return PQOS_RETVAL_OK;
}

int
hw_alloc_assoc_set(const unsigned lcore, const unsigned class_id)
{
        int ret = PQOS_RETVAL_OK;
        unsigned num_l2_cos = 0, num_l3_cos = 0;
        const struct pqos_cap *cap;

        ASSERT(m_cpu != NULL);
        ret = pqos_cpu_check_core(m_cpu, lcore);
        if (ret != PQOS_RETVAL_OK)
                return PQOS_RETVAL_PARAM;

        _pqos_cap_get(&cap, NULL);

        ret = pqos_l3ca_get_cos_num(cap, &num_l3_cos);
        if (ret != PQOS_RETVAL_OK && ret != PQOS_RETVAL_RESOURCE)
                return ret;

        ret = pqos_l2ca_get_cos_num(cap, &num_l2_cos);
        if (ret != PQOS_RETVAL_OK && ret != PQOS_RETVAL_RESOURCE)
                return ret;

        if (class_id >= num_l3_cos && class_id >= num_l2_cos)
                /* class_id is out of bounds */
                return PQOS_RETVAL_PARAM;

        ret = cos_assoc_set(lcore, class_id);

        return ret;
}

int
hw_alloc_assoc_get(const unsigned lcore, unsigned *class_id)
{
        const struct pqos_capability *l3_cap = NULL;
        const struct pqos_capability *l2_cap = NULL;
        const struct pqos_capability *mba_cap = NULL;
        int ret = PQOS_RETVAL_OK;

        ASSERT(class_id != NULL);
        ASSERT(m_cpu != NULL);
        ret = pqos_cpu_check_core(m_cpu, lcore);
        if (ret != PQOS_RETVAL_OK)
                return PQOS_RETVAL_PARAM;

        ret = _pqos_cap_get_type(PQOS_CAP_TYPE_L3CA, &l3_cap);
        if (ret != PQOS_RETVAL_OK && ret != PQOS_RETVAL_RESOURCE)
                return ret;

        ret = _pqos_cap_get_type(PQOS_CAP_TYPE_L2CA, &l2_cap);
        if (ret != PQOS_RETVAL_OK && ret != PQOS_RETVAL_RESOURCE)
                return ret;

        ret = _pqos_cap_get_type(PQOS_CAP_TYPE_MBA, &mba_cap);
        if (ret != PQOS_RETVAL_OK && ret != PQOS_RETVAL_RESOURCE)
                return ret;

        if (l2_cap == NULL && l3_cap == NULL && mba_cap == NULL)
                /* no L2/L3 CAT or MBA detected */
                return PQOS_RETVAL_RESOURCE;

        ret = cos_assoc_get(lcore, class_id);

        return ret;
}

int
hw_alloc_assign(const unsigned technology,
                const unsigned *core_array,
                const unsigned core_num,
                unsigned *class_id)
{
        const int l3_req = ((technology & (1 << PQOS_CAP_TYPE_L3CA)) != 0);
        const int l2_req = ((technology & (1 << PQOS_CAP_TYPE_L2CA)) != 0);
        const int mba_req = ((technology & (1 << PQOS_CAP_TYPE_MBA)) != 0);
        unsigned l3cat_id = 0, l2cat_id = 0, mba_id = 0;
        unsigned i;
        int ret;

        ASSERT(core_num > 0);
        ASSERT(core_array != NULL);
        ASSERT(class_id != NULL);
        ASSERT(technology != 0);

        /* Check if core belongs to one resource entity */
        for (i = 0; i < core_num; i++) {
                const struct pqos_coreinfo *pi = NULL;

                pi = pqos_cpu_get_core_info(m_cpu, core_array[i]);
                if (pi == NULL) {
                        ret = PQOS_RETVAL_PARAM;
                        goto pqos_alloc_assign_exit;
                }

                if (l3_req) {
                        if (i != 0 && l3cat_id != pi->l3cat_id) {
                                ret = PQOS_RETVAL_PARAM;
                                goto pqos_alloc_assign_exit;
                        }
                        l3cat_id = pi->l3cat_id;
                }
                if (mba_req) {
                        if (i != 0 && mba_id != pi->mba_id) {
                                ret = PQOS_RETVAL_PARAM;
                                goto pqos_alloc_assign_exit;
                        }
                        mba_id = pi->mba_id;
                }
                if (l2_req && !l3_req && !mba_req) {
                        /* only L2 is requested
                         * The smallest manageable entity is L2 cluster
                         */
                        if (i != 0 && l2cat_id != pi->l2_id) {
                                ret = PQOS_RETVAL_PARAM;
                                goto pqos_alloc_assign_exit;
                        }
                        l2cat_id = pi->l2_id;
                }
        }

        /* find an unused class from highest down */
        ret = get_unused_cos(technology, l3cat_id, l2cat_id, mba_id, class_id);
        if (ret != PQOS_RETVAL_OK)
                goto pqos_alloc_assign_exit;

        /* assign cores to the unused class */
        for (i = 0; i < core_num; i++) {
                ret = cos_assoc_set(core_array[i], *class_id);
                if (ret != PQOS_RETVAL_OK)
                        goto pqos_alloc_assign_exit;
        }

pqos_alloc_assign_exit:
        return ret;
}

int
hw_alloc_release(const unsigned *core_array, const unsigned core_num)
{
        unsigned i;
        int ret = PQOS_RETVAL_OK;

        ASSERT(core_num > 0 && core_array != NULL);

        for (i = 0; i < core_num; i++)
                if (cos_assoc_set(core_array[i], 0) != PQOS_RETVAL_OK)
                        ret = PQOS_RETVAL_ERROR;

        return ret;
}

/**
 * @brief Enables or disables CDP across selected CPU sockets
 *
 * @param [in] sockets_num dimension of \a sockets array
 * @param [in] sockets array with socket ids to change CDP config on
 * @param [in] enable CDP enable/disable flag, 1 - enable, 0 - disable
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 * @retval PQOS_RETVAL_ERROR on failure, MSR read/write error
 */
static int
l3cdp_enable(const unsigned l3cat_id_num,
             const unsigned *l3cat_ids,
             const int enable)
{
        unsigned j = 0;

        ASSERT(l3cat_id_num > 0 && l3cat_ids != NULL);

        LOG_INFO("%s L3 CDP across sockets...\n",
                 (enable) ? "Enabling" : "Disabling");

        for (j = 0; j < l3cat_id_num; j++) {
                uint64_t reg = 0;
                unsigned core = 0;
                int ret = PQOS_RETVAL_OK;

                ret = pqos_cpu_get_one_by_l3cat_id(m_cpu, l3cat_ids[j], &core);
                if (ret != PQOS_RETVAL_OK)
                        return ret;

                ret = msr_read(core, PQOS_MSR_L3_QOS_CFG, &reg);
                if (ret != MACHINE_RETVAL_OK)
                        return PQOS_RETVAL_ERROR;

                if (enable)
                        reg |= PQOS_MSR_L3_QOS_CFG_CDP_EN;
                else
                        reg &= ~PQOS_MSR_L3_QOS_CFG_CDP_EN;

                ret = msr_write(core, PQOS_MSR_L3_QOS_CFG, reg);
                if (ret != MACHINE_RETVAL_OK)
                        return PQOS_RETVAL_ERROR;
        }

        return PQOS_RETVAL_OK;
}

/**
 * @brief Enables or disables CDP across selected CPU clusters
 *
 * @param [in] l2id_num dimension of \a l2ids array
 * @param [in] l2ids array with clusters ids to change CDP config on
 * @param [in] enable CDP enable/disable flag, 1 - enable, 0 - disable
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 * @retval PQOS_RETVAL_ERROR on failure, MSR read/write error
 */
static int
l2cdp_enable(const unsigned l2id_num, const unsigned *l2ids, const int enable)
{
        unsigned i = 0;
        int ret;

        ASSERT(l2id_num > 0 && l2ids != NULL);

        LOG_INFO("%s L2 CDP across clusters...\n",
                 (enable) ? "Enabling" : "Disabling");

        for (i = 0; i < l2id_num; i++) {
                uint64_t reg = 0;
                unsigned core = 0;

                ret = pqos_cpu_get_one_by_l2id(m_cpu, l2ids[i], &core);
                if (ret != PQOS_RETVAL_OK)
                        return ret;

                ret = msr_read(core, PQOS_MSR_L2_QOS_CFG, &reg);
                if (ret != PQOS_RETVAL_OK)
                        return PQOS_RETVAL_ERROR;

                if (enable)
                        reg |= PQOS_MSR_L2_QOS_CFG_CDP_EN;
                else
                        reg &= ~PQOS_MSR_L2_QOS_CFG_CDP_EN;

                ret = msr_write(core, PQOS_MSR_L2_QOS_CFG, reg);
                if (ret != MACHINE_RETVAL_OK)
                        return PQOS_RETVAL_ERROR;
        }

        return PQOS_RETVAL_OK;
}

/**
 * @brief Writes range of MBA/CAT COS MSR's with \a msr_val value
 *
 * Used as part of CAT/MBA reset process.
 *
 * @param [in] msr_start First MSR to be written
 * @param [in] msr_num Number of MSR's to be written
 * @param [in] coreid Core ID to be used for MSR write operations
 * @param [in] msr_val Value to be written to MSR's
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 * @retval PQOS_RETVAL_ERROR on MSR write error
 */
static int
alloc_cos_reset(const unsigned msr_start,
                const unsigned msr_num,
                const unsigned coreid,
                const uint64_t msr_val)
{
        int ret = PQOS_RETVAL_OK;
        unsigned i;

        for (i = 0; i < msr_num; i++) {
                int retval = msr_write(coreid, msr_start + i, msr_val);

                if (retval != MACHINE_RETVAL_OK)
                        ret = PQOS_RETVAL_ERROR;
        }

        return ret;
}

/**
 * @brief Associates each of the cores with COS0
 *
 * Operates on m_cpu structure.
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 * @retval PQOS_RETVAL_ERROR on MSR write error
 */
static int
alloc_assoc_reset(void)
{
        int ret = PQOS_RETVAL_OK;
        unsigned i;

        for (i = 0; i < m_cpu->num_cores; i++)
                if (cos_assoc_set(m_cpu->cores[i].lcore, 0) != PQOS_RETVAL_OK)
                        ret = PQOS_RETVAL_ERROR;

        return ret;
}

int
hw_alloc_reset(const enum pqos_cdp_config l3_cdp_cfg,
               const enum pqos_cdp_config l2_cdp_cfg,
               const enum pqos_mba_config mba_cfg)
{
        unsigned *l3cat_ids = NULL;
        unsigned l3cat_id_num = 0;
        unsigned *mba_ids = NULL;
        unsigned mba_id_num = 0;
        unsigned *l2ids = NULL;
        unsigned l2id_num = 0;
        const struct pqos_cap *cap;
        const struct pqos_capability *alloc_cap = NULL;
        const struct pqos_cap_l3ca *l3_cap = NULL;
        const struct pqos_cap_l2ca *l2_cap = NULL;
        const struct pqos_cap_mba *mba_cap = NULL;
        const struct cpuinfo_config *vconfig;
        int ret = PQOS_RETVAL_OK;
        unsigned max_l3_cos = 0;
        unsigned max_l2_cos = 0;
        unsigned j;
        int cdp_supported;

        ASSERT(l3_cdp_cfg == PQOS_REQUIRE_CDP_ON ||
               l3_cdp_cfg == PQOS_REQUIRE_CDP_OFF ||
               l3_cdp_cfg == PQOS_REQUIRE_CDP_ANY);

        ASSERT(l2_cdp_cfg == PQOS_REQUIRE_CDP_ON ||
               l2_cdp_cfg == PQOS_REQUIRE_CDP_OFF ||
               l2_cdp_cfg == PQOS_REQUIRE_CDP_ANY);

        ASSERT(mba_cfg == PQOS_MBA_DEFAULT || mba_cfg == PQOS_MBA_CTRL ||
               mba_cfg == PQOS_MBA_ANY);

        _pqos_cap_get(&cap, NULL);
        cpuinfo_get_config(&vconfig);

        /* Get L3 CAT capabilities */
        (void)_pqos_cap_get_type(PQOS_CAP_TYPE_L3CA, &alloc_cap);
        if (alloc_cap != NULL)
                l3_cap = alloc_cap->u.l3ca;

        /* Get L2 CAT capabilities */
        alloc_cap = NULL;
        (void)_pqos_cap_get_type(PQOS_CAP_TYPE_L2CA, &alloc_cap);
        if (alloc_cap != NULL)
                l2_cap = alloc_cap->u.l2ca;

        /* Get MBA capabilities */
        alloc_cap = NULL;
        (void)_pqos_cap_get_type(PQOS_CAP_TYPE_MBA, &alloc_cap);
        if (alloc_cap != NULL)
                mba_cap = alloc_cap->u.mba;

        /* Check if either L2 CAT, L3 CAT or MBA is supported */
        if (l2_cap == NULL && l3_cap == NULL && mba_cap == NULL) {
                LOG_ERROR("L2 CAT/L3 CAT/MBA not present!\n");
                ret = PQOS_RETVAL_RESOURCE; /* no L2/L3 CAT present */
                goto pqos_alloc_reset_exit;
        }
        /* Check L3 CDP requested while not present */
        if (l3_cap == NULL && l3_cdp_cfg != PQOS_REQUIRE_CDP_ANY) {
                LOG_ERROR("L3 CDP setting requested but no L3 CAT present!\n");
                ret = PQOS_RETVAL_RESOURCE;
                goto pqos_alloc_reset_exit;
        }
        /* Check L2 CDP requested while not present */
        if (l2_cap == NULL && l2_cdp_cfg != PQOS_REQUIRE_CDP_ANY) {
                LOG_ERROR("L2 CDP setting requested but no L2 CAT present!\n");
                ret = PQOS_RETVAL_RESOURCE;
                goto pqos_alloc_reset_exit;
        }
        /* Check MBA CTRL requested while not present */
        if (mba_cap == NULL && mba_cfg != PQOS_MBA_ANY) {
                LOG_ERROR("MBA CTRL setting requested"
                          " but no MBA CTRL present!\n");
                ret = PQOS_RETVAL_RESOURCE;
                goto pqos_alloc_reset_exit;
        }
        if (l3_cap != NULL) {
                ret = pqos_l3ca_cdp_enabled(cap, &cdp_supported, NULL);
                if (ret != PQOS_RETVAL_OK)
                        goto pqos_alloc_reset_exit;

                /* Check against erroneous L3 CDP request */
                if (l3_cdp_cfg == PQOS_REQUIRE_CDP_ON && !cdp_supported) {
                        LOG_ERROR("L3 CAT/CDP requested but not supported by "
                                  "the platform!\n");
                        ret = PQOS_RETVAL_PARAM;
                        goto pqos_alloc_reset_exit;
                }

                /* Get maximum number of L3 CAT classes */
                max_l3_cos = l3_cap->num_classes;
                if (l3_cap->cdp && l3_cap->cdp_on)
                        max_l3_cos = max_l3_cos * 2;
        }
        if (l2_cap != NULL) {
                ret = pqos_l2ca_cdp_enabled(cap, &cdp_supported, NULL);
                if (ret != PQOS_RETVAL_OK)
                        goto pqos_alloc_reset_exit;

                /* Check against erroneous L2 CDP request */
                if (l2_cdp_cfg == PQOS_REQUIRE_CDP_ON && !cdp_supported) {
                        LOG_ERROR("L2 CAT/CDP requested but not supported by "
                                  "the platform!\n");
                        ret = PQOS_RETVAL_PARAM;
                        goto pqos_alloc_reset_exit;
                }

                /* Get maximum number of L2 CAT classes */
                max_l2_cos = l2_cap->num_classes;
                if (l2_cap->cdp && l2_cap->cdp_on)
                        max_l2_cos = max_l2_cos * 2;
        }
        if (mba_cap != NULL && mba_cfg == PQOS_MBA_CTRL) {
                LOG_ERROR("MBA CTRL requested but not supported by the "
                          "platform!\n");
                ret = PQOS_RETVAL_PARAM;
                goto pqos_alloc_reset_exit;
        }

        if (l3_cap != NULL) {
                /**
                 * Get number & list of l3cat_ids in the system
                 */
                l3cat_ids = pqos_cpu_get_l3cat_ids(m_cpu, &l3cat_id_num);
                if (l3cat_ids == NULL || l3cat_id_num == 0)
                        goto pqos_alloc_reset_exit;
                /**
                 * Change L3 COS definition on all l3cat ids
                 * so that each COS allows for access to all cache ways
                 */
                for (j = 0; j < l3cat_id_num; j++) {
                        unsigned core = 0;

                        ret = pqos_cpu_get_one_by_l3cat_id(m_cpu, l3cat_ids[j],
                                                           &core);
                        if (ret != PQOS_RETVAL_OK)
                                goto pqos_alloc_reset_exit;

                        const uint64_t ways_mask =
                            (1ULL << l3_cap->num_ways) - 1ULL;

                        ret = alloc_cos_reset(PQOS_MSR_L3CA_MASK_START,
                                              max_l3_cos, core, ways_mask);
                        if (ret != PQOS_RETVAL_OK)
                                goto pqos_alloc_reset_exit;
                }
        }

        if (l2_cap != NULL) {
                /**
                 * Get number & list of L2ids in the system
                 * Then go through all L2 ids and reset L2 classes on them
                 */
                l2ids = pqos_cpu_get_l2ids(m_cpu, &l2id_num);
                if (l2ids == NULL || l2id_num == 0)
                        goto pqos_alloc_reset_exit;

                for (j = 0; j < l2id_num; j++) {
                        const uint64_t ways_mask =
                            (1ULL << l2_cap->num_ways) - 1ULL;
                        unsigned core = 0;

                        ret = pqos_cpu_get_one_by_l2id(m_cpu, l2ids[j], &core);
                        if (ret != PQOS_RETVAL_OK)
                                goto pqos_alloc_reset_exit;

                        ret = alloc_cos_reset(PQOS_MSR_L2CA_MASK_START,
                                              max_l2_cos, core, ways_mask);
                        if (ret != PQOS_RETVAL_OK)
                                goto pqos_alloc_reset_exit;
                }
        }

        if (mba_cap != NULL) {
                /**
                 * Get number & list of mba_ids in the system
                 */
                mba_ids = pqos_cpu_get_mba_ids(m_cpu, &mba_id_num);
                if (mba_ids == NULL || mba_id_num == 0)
                        goto pqos_alloc_reset_exit;

                /**
                 * Go through all L3 CAT ids and reset MBA class definitions
                 * 0 is the default MBA COS value in linear mode.
                 */
                for (j = 0; j < mba_id_num; j++) {
                        unsigned core = 0;

                        ret = pqos_cpu_get_one_by_mba_id(m_cpu, mba_ids[j],
                                                         &core);
                        if (ret != PQOS_RETVAL_OK)
                                goto pqos_alloc_reset_exit;

                        ret = alloc_cos_reset(vconfig->mba_msr_reg,
                                              mba_cap->num_classes, core,
                                              vconfig->mba_default_val);
                        if (ret != PQOS_RETVAL_OK)
                                goto pqos_alloc_reset_exit;
                }
        }

        /**
         * Associate all cores with COS0
         */
        ret = alloc_assoc_reset();
        if (ret != PQOS_RETVAL_OK)
                goto pqos_alloc_reset_exit;

        /**
         * Turn L3 CDP ON or OFF upon the request
         */
        if (l3_cap != NULL) {
                if (l3_cdp_cfg == PQOS_REQUIRE_CDP_ON && !l3_cap->cdp_on) {
                        /**
                         * Turn on L3 CDP
                         */
                        LOG_INFO("Turning L3 CDP ON ...\n");
                        ret = l3cdp_enable(l3cat_id_num, l3cat_ids, 1);
                        if (ret != PQOS_RETVAL_OK) {
                                LOG_ERROR("L3 CDP enable error!\n");
                                goto pqos_alloc_reset_exit;
                        }
                }

                if (l3_cdp_cfg == PQOS_REQUIRE_CDP_OFF && l3_cap->cdp_on) {
                        /**
                         * Turn off L3 CDP
                         */
                        LOG_INFO("Turning L3 CDP OFF ...\n");
                        ret = l3cdp_enable(l3cat_id_num, l3cat_ids, 0);
                        if (ret != PQOS_RETVAL_OK) {
                                LOG_ERROR("L3 CDP disable error!\n");
                                goto pqos_alloc_reset_exit;
                        }
                }
                _pqos_cap_l3cdp_change(l3_cdp_cfg);
        }

        /**
         * Turn L2 CDP ON or OFF upon the request
         */
        if (l2_cap != NULL) {
                if (l2_cdp_cfg == PQOS_REQUIRE_CDP_ON && !l2_cap->cdp_on) {
                        /**
                         * Turn on L2 CDP
                         */
                        LOG_INFO("Turning L2 CDP ON ...\n");
                        ret = l2cdp_enable(l2id_num, l2ids, 1);
                        if (ret != PQOS_RETVAL_OK) {
                                LOG_ERROR("L2 CDP enable error!\n");
                                goto pqos_alloc_reset_exit;
                        }
                }

                if (l2_cdp_cfg == PQOS_REQUIRE_CDP_OFF && l2_cap->cdp_on) {
                        /**
                         * Turn off L2 CDP
                         */
                        LOG_INFO("Turning L2 CDP OFF ...\n");
                        ret = l2cdp_enable(l2id_num, l2ids, 0);
                        if (ret != PQOS_RETVAL_OK) {
                                LOG_ERROR("L2 CDP disable error!\n");
                                goto pqos_alloc_reset_exit;
                        }
                }
                _pqos_cap_l2cdp_change(l2_cdp_cfg);
        }

pqos_alloc_reset_exit:
        if (l3cat_ids != NULL)
                free(l3cat_ids);
        if (mba_ids != NULL)
                free(mba_ids);
        if (l2ids != NULL)
                free(l2ids);
        return ret;
}
