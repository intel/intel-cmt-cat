/*
 * BSD LICENSE
 *
 * Copyright(c) 2014-2023 Intel Corporation. All rights reserved.
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

#include "allocation.h"

#include "cap.h"
#include "cpu_registers.h"
#include "cpuinfo.h"
#include "iordt.h"
#include "log.h"
#include "machine.h"
#include "os_allocation.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>

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

int
hw_alloc_assoc_read(const unsigned lcore, unsigned *class_id)
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
int
hw_alloc_assoc_unused(const unsigned technology,
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
        const struct pqos_cap *cap = _pqos_get_cap();
        const struct pqos_cpuinfo *cpu = _pqos_get_cpu();
        unsigned used_classes[PQOS_MAX_COS];

        if (class_id == NULL)
                return PQOS_RETVAL_PARAM;

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

                ret = hw_alloc_assoc_read(cpu->cores[i].lcore, &cos);
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
#ifdef __linux__
        enum pqos_interface interface = _pqos_get_inter();
#endif

#ifndef __linux__
        UNUSED_PARAM(cpu);
        UNUSED_PARAM(cap);
#endif

        UNUSED_PARAM(cfg);

#ifdef __linux__
        if (interface == PQOS_INTER_OS ||
            interface == PQOS_INTER_OS_RESCTRL_MON)
                ret = os_alloc_init(cpu, cap);
#endif
        return ret;
}

int
pqos_alloc_fini(void)
{
        int ret = PQOS_RETVAL_OK;
#ifdef __linux__
        enum pqos_interface interface = _pqos_get_inter();

        if (interface == PQOS_INTER_OS ||
            interface == PQOS_INTER_OS_RESCTRL_MON)
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
        const struct pqos_cap *cap = _pqos_get_cap();
        const struct pqos_cpuinfo *cpu = _pqos_get_cpu();

        ASSERT(ca != NULL);
        ASSERT(num_ca != 0);

        /* Check L3 CBM is non-contiguous */
        if (!cap_get_l3ca_non_contignous()) {
                unsigned idx;
                /* Check all COS CBM are contiguous */
                for (idx = 0; idx < num_ca; idx++) {
                        if (!IS_CONTIGNOUS(ca[idx])) {
                                LOG_ERROR("L3 CAT COS%u bit mask is not "
                                          "contiguous!\n",
                                          ca[idx].class_id);
                                return PQOS_RETVAL_PARAM;
                        }
                }
        }

        ret = pqos_l3ca_get_cos_num(cap, &count);
        if (ret != PQOS_RETVAL_OK)
                return ret; /**< perhaps no L3CA capability */

        if (num_ca > count)
                return PQOS_RETVAL_ERROR;

        ret = pqos_l3ca_cdp_enabled(cap, NULL, &cdp_enabled);
        if (ret != PQOS_RETVAL_OK)
                return ret;

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
        const struct pqos_cap *cap = _pqos_get_cap();
        const struct pqos_cpuinfo *cpu = _pqos_get_cpu();

        ASSERT(num_ca != NULL);
        ASSERT(ca != NULL);
        ASSERT(max_num_ca != 0);

        ret = pqos_l3ca_get_cos_num(cap, &count);
        if (ret != PQOS_RETVAL_OK)
                return ret; /**< perhaps no L3CA capability */

        ret = pqos_l3ca_cdp_enabled(cap, NULL, &cdp_enabled);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        if (count > max_num_ca)
                return PQOS_RETVAL_ERROR;

        ret = pqos_cpu_get_one_by_l3cat_id(cpu, l3cat_id, &core);
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
        const struct pqos_cap *cap = _pqos_get_cap();
        const struct pqos_cpuinfo *cpu = _pqos_get_cpu();

        ASSERT(min_cbm_bits != NULL);

        /**
         * Get L3 CAT capabilities
         */
        ret = pqos_cap_get_type(cap, PQOS_CAP_TYPE_L3CA, &l3_cap);
        if (ret != PQOS_RETVAL_OK)
                return PQOS_RETVAL_RESOURCE; /* L3 CAT not supported */

        /**
         * Get number & list of l3cat_ids in the system
         */
        l3cat_ids = pqos_cpu_get_l3cat_ids(cpu, &l3cat_id_num);
        if (l3cat_ids == NULL || l3cat_id_num == 0) {
                ret = PQOS_RETVAL_ERROR;
                goto pqos_l3ca_get_min_cbm_bits_exit;
        }

        /**
         * Find free COS
         */
        for (l3cat_id = 0; l3cat_id < l3cat_id_num; l3cat_id++) {
                ret = hw_alloc_assoc_unused(technology, l3cat_id, 0, 0,
                                            &class_id);
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
        const struct pqos_cap *cap = _pqos_get_cap();
        const struct pqos_cpuinfo *cpu = _pqos_get_cpu();

        ASSERT(ca != NULL);
        ASSERT(num_ca != 0);

        /* Check L2 CBM is non-contiguous */
        if (!cap_get_l2ca_non_contignous()) {
                unsigned idx;
                /* Check all COS CBM are contiguous */
                for (idx = 0; idx < num_ca; idx++) {
                        if (!IS_CONTIGNOUS(ca[idx])) {
                                LOG_ERROR("L2 CAT COS%u bit mask is not "
                                          "contiguous!\n",
                                          ca[idx].class_id);
                                return PQOS_RETVAL_PARAM;
                        }
                }
        }

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
        const struct pqos_cap *cap = _pqos_get_cap();
        const struct pqos_cpuinfo *cpu = _pqos_get_cpu();

        ASSERT(num_ca != NULL);
        ASSERT(ca != NULL);
        ASSERT(max_num_ca != 0);

        ret = pqos_l2ca_get_cos_num(cap, &count);
        if (ret != PQOS_RETVAL_OK)
                return PQOS_RETVAL_RESOURCE; /* L2 CAT not supported */

        ret = pqos_l2ca_cdp_enabled(cap, NULL, &cdp_enabled);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        if (max_num_ca < count)
                /* Not enough space to store the classes */
                return PQOS_RETVAL_PARAM;

        ret = pqos_cpu_get_one_by_l2id(cpu, l2id, &core);
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
        const struct pqos_cap *cap = _pqos_get_cap();
        const struct pqos_cpuinfo *cpu = _pqos_get_cpu();
        const struct pqos_capability *l2_cap = NULL;
        struct pqos_l2ca l2ca_config[PQOS_MAX_L2CA_COS];

        ASSERT(min_cbm_bits != NULL);

        /**
         * Get L2 CAT capabilities
         */
        ret = pqos_cap_get_type(cap, PQOS_CAP_TYPE_L2CA, &l2_cap);
        if (ret != PQOS_RETVAL_OK)
                return PQOS_RETVAL_RESOURCE; /* L2 CAT not supported */

        /**
         * Get number & list of L2ids in the system
         */
        l2ids = pqos_cpu_get_l2ids(cpu, &l2id_num);
        if (l2ids == NULL || l2id_num == 0) {
                ret = PQOS_RETVAL_ERROR;
                goto hw_l2ca_get_min_cbm_bits_exit;
        }

        /**
         * Find free COS
         */
        for (l2id = 0; l2id < l2id_num; l2id++) {
                ret = hw_alloc_assoc_unused(technology, 0, l2id, 0, &class_id);
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
        const struct pqos_cap *cap = _pqos_get_cap();
        const struct pqos_cpuinfo *cpu = _pqos_get_cpu();

        ASSERT(requested != NULL);
        ASSERT(num_cos != 0);

        /**
         * Check if MBA is supported
         */
        ret = pqos_cap_get_type(cap, PQOS_CAP_TYPE_MBA, &mba_cap);
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

        ret = pqos_cpu_get_one_by_mba_id(cpu, mba_id, &core);
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
        const struct pqos_cap *cap = _pqos_get_cap();
        const struct pqos_cpuinfo *cpu = _pqos_get_cpu();
        const struct pqos_capability *mba_cap = NULL;

        ASSERT(requested != NULL);
        ASSERT(num_cos != 0);

        if (requested->smba) {
                ret = hw_smba_set_amd(mba_id, num_cos, requested, actual);
                return ret;
        }

        /**
         * Check if MBA is supported
         */
        ret = pqos_cap_get_type(cap, PQOS_CAP_TYPE_MBA, &mba_cap);
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

        ret = pqos_cpu_get_one_by_mba_id(cpu, mba_id, &core);
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
        const struct pqos_cap *cap = _pqos_get_cap();
        const struct pqos_cpuinfo *cpu = _pqos_get_cpu();

        ASSERT(num_cos != NULL);
        ASSERT(mba_tab != NULL);
        ASSERT(max_num_cos != 0);

        ret = pqos_mba_get_cos_num(cap, &count);
        if (ret != PQOS_RETVAL_OK)
                return ret; /**< no MBA capability */

        if (count > max_num_cos)
                return PQOS_RETVAL_ERROR;

        ret = pqos_cpu_get_one_by_mba_id(cpu, mba_id, &core);
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
        const struct pqos_cap *cap = _pqos_get_cap();
        const struct pqos_cpuinfo *cpu = _pqos_get_cpu();

        ASSERT(num_cos != NULL);
        ASSERT(mba_tab != NULL);
        ASSERT(max_num_cos != 0);

        if (mba_tab->smba) {
                ret = hw_smba_get_amd(mba_id, max_num_cos, num_cos, mba_tab);
                return ret;
        }

        ret = pqos_mba_get_cos_num(cap, &count);
        if (ret != PQOS_RETVAL_OK)
                return ret; /**< no MBA capability */

        if (count > max_num_cos)
                return PQOS_RETVAL_ERROR;

        ret = pqos_cpu_get_one_by_mba_id(cpu, mba_id, &core);
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

int
hw_smba_get_amd(const unsigned smba_id,
                const unsigned max_num_cos,
                unsigned *num_cos,
                struct pqos_mba *smba_tab)
{
        int ret = PQOS_RETVAL_OK;
        unsigned i = 0, count = 0, core = 0;
        const struct pqos_cap *cap = _pqos_get_cap();
        const struct pqos_cpuinfo *cpu = _pqos_get_cpu();

        ASSERT(num_cos != NULL);
        ASSERT(smba_tab != NULL);
        ASSERT(max_num_cos != 0);

        ret = pqos_smba_get_cos_num(cap, &count);
        if (ret != PQOS_RETVAL_OK)
                return ret; /**< no SMBA capability */

        if (count > max_num_cos)
                return PQOS_RETVAL_ERROR;

        ret = pqos_cpu_get_one_by_smba_id(cpu, smba_id, &core);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        for (i = 0; i < count; i++) {
                const uint32_t reg = PQOS_MSR_SMBA_MASK_START_AMD + i;
                uint64_t val = 0;
                int retval = msr_read(core, reg, &val);

                if (retval != MACHINE_RETVAL_OK)
                        return PQOS_RETVAL_ERROR;

                smba_tab[i].ctrl = 0;
                smba_tab[i].class_id = i;
                smba_tab[i].mb_max = val;
        }
        *num_cos = count;

        return ret;
}

int
hw_smba_set_amd(const unsigned smba_id,
                const unsigned num_cos,
                const struct pqos_mba *requested,
                struct pqos_mba *actual)
{
        int ret = PQOS_RETVAL_OK;
        unsigned i = 0, count = 0, core = 0;
        const struct pqos_cap *cap = _pqos_get_cap();
        const struct pqos_cpuinfo *cpu = _pqos_get_cpu();
        const struct pqos_capability *smba_cap = NULL;

        ASSERT(requested != NULL);
        ASSERT(num_cos != 0);

        /**
         * Check if SMBA is supported
         */
        ret = pqos_cap_get_type(cap, PQOS_CAP_TYPE_SMBA, &smba_cap);
        if (ret != PQOS_RETVAL_OK)
                return PQOS_RETVAL_RESOURCE; /* SMBA not supported */

        count = smba_cap->u.smba->num_classes;

        /**
         * Check if class id's are within allowed range
         * and if a controller is not requested.
         */
        for (i = 0; i < num_cos; i++) {
                if (requested[i].class_id >= count) {
                        LOG_ERROR(
                            "SMBA COS%u is out of range (COS%u is max)!\n",
                            requested[i].class_id, count - 1);
                        return PQOS_RETVAL_PARAM;
                }

                if (requested[i].ctrl != 0) {
                        LOG_ERROR("SMBA controller not supported!\n");
                        return PQOS_RETVAL_PARAM;
                }
        }

        ret = pqos_cpu_get_one_by_smba_id(cpu, smba_id, &core);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        for (i = 0; i < num_cos; i++) {
                const uint32_t reg =
                    requested[i].class_id + PQOS_MSR_SMBA_MASK_START_AMD;
                uint64_t val = requested[i].mb_max;
                int retval = MACHINE_RETVAL_OK;

                retval = msr_write(core, reg, val);
                if (retval != MACHINE_RETVAL_OK)
                        return retval;

                /**
                 * If table to store actual values set is passed,
                 * read MSR values and store in table
                 */
                if (actual == NULL)
                        continue;

                retval = msr_read(core, reg, &val);
                if (retval != MACHINE_RETVAL_OK)
                        return retval;

                actual[i] = requested[i];
                actual[i].mb_max = val;
        }

        return ret;
}

int
hw_alloc_assoc_write(const unsigned lcore, const unsigned class_id)
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
        unsigned num_l2_cos = 0, num_l3_cos = 0, num_mba_cos = 0;
        const struct pqos_cap *cap = _pqos_get_cap();
        const struct pqos_cpuinfo *cpu = _pqos_get_cpu();

        ret = pqos_cpu_check_core(cpu, lcore);
        if (ret != PQOS_RETVAL_OK)
                return PQOS_RETVAL_PARAM;

        ret = pqos_l3ca_get_cos_num(cap, &num_l3_cos);
        if (ret != PQOS_RETVAL_OK && ret != PQOS_RETVAL_RESOURCE)
                return ret;

        ret = pqos_l2ca_get_cos_num(cap, &num_l2_cos);
        if (ret != PQOS_RETVAL_OK && ret != PQOS_RETVAL_RESOURCE)
                return ret;

        ret = pqos_mba_get_cos_num(cap, &num_mba_cos);
        if (ret != PQOS_RETVAL_OK && ret != PQOS_RETVAL_RESOURCE)
                return ret;

        if (class_id >= num_l3_cos && class_id >= num_l2_cos &&
            class_id >= num_mba_cos)
                /* class_id is out of bounds */
                return PQOS_RETVAL_PARAM;

        ret = hw_alloc_assoc_write(lcore, class_id);

        return ret;
}

int
hw_alloc_assoc_get(const unsigned lcore, unsigned *class_id)
{
        const struct pqos_capability *l3_cap = NULL;
        const struct pqos_capability *l2_cap = NULL;
        const struct pqos_capability *mba_cap = NULL;
        int ret = PQOS_RETVAL_OK;
        const struct pqos_cap *cap = _pqos_get_cap();
        const struct pqos_cpuinfo *cpu = _pqos_get_cpu();

        ASSERT(class_id != NULL);

        ret = pqos_cpu_check_core(cpu, lcore);
        if (ret != PQOS_RETVAL_OK)
                return PQOS_RETVAL_PARAM;

        ret = pqos_cap_get_type(cap, PQOS_CAP_TYPE_L3CA, &l3_cap);
        if (ret != PQOS_RETVAL_OK && ret != PQOS_RETVAL_RESOURCE)
                return ret;

        ret = pqos_cap_get_type(cap, PQOS_CAP_TYPE_L2CA, &l2_cap);
        if (ret != PQOS_RETVAL_OK && ret != PQOS_RETVAL_RESOURCE)
                return ret;

        ret = pqos_cap_get_type(cap, PQOS_CAP_TYPE_MBA, &mba_cap);
        if (ret != PQOS_RETVAL_OK && ret != PQOS_RETVAL_RESOURCE)
                return ret;

        if (l2_cap == NULL && l3_cap == NULL && mba_cap == NULL)
                /* no L2/L3 CAT or MBA detected */
                return PQOS_RETVAL_RESOURCE;

        ret = hw_alloc_assoc_read(lcore, class_id);

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
        const struct pqos_cpuinfo *cpu = _pqos_get_cpu();

        ASSERT(core_num > 0);
        ASSERT(core_array != NULL);
        ASSERT(class_id != NULL);
        ASSERT(technology != 0);

        /* Check if core belongs to one resource entity */
        for (i = 0; i < core_num; i++) {
                const struct pqos_coreinfo *pi = NULL;

                pi = pqos_cpu_get_core_info(cpu, core_array[i]);
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
        ret = hw_alloc_assoc_unused(technology, l3cat_id, l2cat_id, mba_id,
                                    class_id);
        if (ret != PQOS_RETVAL_OK)
                goto pqos_alloc_assign_exit;

        /* assign cores to the unused class */
        for (i = 0; i < core_num; i++) {
                ret = hw_alloc_assoc_write(core_array[i], *class_id);
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
                if (hw_alloc_assoc_write(core_array[i], 0) != PQOS_RETVAL_OK)
                        ret = PQOS_RETVAL_ERROR;

        return ret;
}

int
hw_alloc_reset_l3cdp(const unsigned l3cat_id_num,
                     const unsigned *l3cat_ids,
                     const int enable)
{
        unsigned j = 0;
        const struct pqos_cpuinfo *cpu = _pqos_get_cpu();

        ASSERT(l3cat_id_num > 0 && l3cat_ids != NULL);

        LOG_INFO("%s L3 CDP across sockets...\n",
                 (enable) ? "Enabling" : "Disabling");

        for (j = 0; j < l3cat_id_num; j++) {
                uint64_t reg = 0;
                unsigned core = 0;
                int ret = PQOS_RETVAL_OK;

                ret = pqos_cpu_get_one_by_l3cat_id(cpu, l3cat_ids[j], &core);
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

int
hw_alloc_reset_l3iordt(const unsigned l3cat_id_num,
                       const unsigned *l3cat_ids,
                       const int enable)
{
        unsigned j = 0;
        const struct pqos_cpuinfo *cpu = _pqos_get_cpu();

        ASSERT(l3cat_id_num > 0 && l3cat_ids != NULL);

        LOG_INFO("%s L3 I/O RDT across sockets...\n",
                 (enable) ? "Enabling" : "Disabling");

        for (j = 0; j < l3cat_id_num; j++) {
                uint64_t reg = 0;
                unsigned core = 0;
                int ret = PQOS_RETVAL_OK;

                ret = pqos_cpu_get_one_by_l3cat_id(cpu, l3cat_ids[j], &core);
                if (ret != PQOS_RETVAL_OK)
                        return ret;

                ret = msr_read(core, PQOS_MSR_L3_IO_QOS_CFG, &reg);
                if (ret != MACHINE_RETVAL_OK)
                        return PQOS_RETVAL_ERROR;

                if (enable)
                        reg |= PQOS_MSR_L3_IO_QOS_CA_EN;
                else
                        reg &= ~PQOS_MSR_L3_IO_QOS_CA_EN;

                ret = msr_write(core, PQOS_MSR_L3_IO_QOS_CFG, reg);
                if (ret != MACHINE_RETVAL_OK)
                        return PQOS_RETVAL_ERROR;
        }

        return PQOS_RETVAL_OK;
}

int
hw_alloc_reset_l2cdp(const unsigned l2id_num,
                     const unsigned *l2ids,
                     const int enable)
{
        unsigned i = 0;
        int ret;
        const struct pqos_cpuinfo *cpu = _pqos_get_cpu();

        ASSERT(l2id_num > 0 && l2ids != NULL);

        LOG_INFO("%s L2 CDP across clusters...\n",
                 (enable) ? "Enabling" : "Disabling");

        for (i = 0; i < l2id_num; i++) {
                uint64_t reg = 0;
                unsigned core = 0;

                ret = pqos_cpu_get_one_by_l2id(cpu, l2ids[i], &core);
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

int
hw_alloc_reset_cos(const unsigned msr_start,
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

int
hw_alloc_reset_assoc(void)
{
        int ret = PQOS_RETVAL_OK;
        int retval;

        retval = hw_alloc_reset_assoc_cores();
        if (retval != PQOS_RETVAL_OK)
                ret = retval;

        retval = hw_alloc_reset_assoc_channels();
        if (retval != PQOS_RETVAL_OK)
                ret = retval;

        return ret;
}

int
hw_alloc_reset_assoc_cores(void)
{
        int ret = PQOS_RETVAL_OK;
        const struct pqos_cpuinfo *cpu = _pqos_get_cpu();
        unsigned i;

        for (i = 0; i < cpu->num_cores; i++)
                if (hw_alloc_assoc_write(cpu->cores[i].lcore, 0) !=
                    PQOS_RETVAL_OK)
                        ret = PQOS_RETVAL_ERROR;

        return ret;
}

int
hw_alloc_reset_assoc_channels(void)
{
        int ret = PQOS_RETVAL_OK;
        const struct pqos_cap *cap = _pqos_get_cap();
        const struct pqos_devinfo *dev = _pqos_get_dev();
        int iordt_enabled;
        int iordt_supported;

        ret = pqos_l3ca_iordt_enabled(cap, &iordt_supported, &iordt_enabled);
        /* If L3CA not supported */
        if (ret == PQOS_RETVAL_RESOURCE)
                return PQOS_RETVAL_OK;
        else if (ret != PQOS_RETVAL_OK)
                return ret;
        else if (!iordt_supported || !iordt_enabled)
                return PQOS_RETVAL_OK;

        if (dev == NULL)
                return PQOS_RETVAL_OK;

        ret = iordt_assoc_reset(dev);

        return ret;
}

int
hw_alloc_reset_mba40(const unsigned mba_ids_num,
                     const unsigned *mba_ids,
                     const int enable)
{
        unsigned i = 0;
        int ret;
        const struct pqos_cpuinfo *cpu = _pqos_get_cpu();

        ASSERT(mba_ids_num > 0 && mba_ids != NULL);

        LOG_INFO("%s MBA 4.0 across clusters...\n",
                 (enable) ? "Enabling" : "Disabling");

        for (i = 0; i < mba_ids_num; i++) {
                uint64_t reg = 0;
                unsigned core = 0;

                ret = pqos_cpu_get_one_by_mba_id(cpu, mba_ids[i], &core);
                if (ret != PQOS_RETVAL_OK)
                        return PQOS_RETVAL_ERROR;

                ret = msr_read(core, PQOS_MSR_MBA_CFG, &reg);
                if (ret != PQOS_RETVAL_OK)
                        return PQOS_RETVAL_ERROR;

                if (enable)
                        reg |= PQOS_MSR_MBA_CFG_MBA40_EN;
                else
                        reg &= ~PQOS_MSR_MBA_CFG_MBA40_EN;

                ret = msr_write(core, PQOS_MSR_MBA_CFG, reg);
                if (ret != MACHINE_RETVAL_OK)
                        return PQOS_RETVAL_ERROR;
        }

        return PQOS_RETVAL_OK;
}

int
hw_alloc_reset(const struct pqos_alloc_config *cfg)
{
        unsigned *l3cat_ids = NULL;
        unsigned l3cat_id_num = 0;
        unsigned *mba_ids = NULL;
        unsigned mba_id_num = 0;
        unsigned *smba_ids = NULL;
        unsigned smba_id_num = 0;
        unsigned *l2ids = NULL;
        unsigned l2id_num = 0;
        const struct pqos_cap *cap = _pqos_get_cap();
        const struct pqos_cpuinfo *cpu = _pqos_get_cpu();
        const struct pqos_capability *alloc_cap = NULL;
        const struct pqos_cap_l3ca *l3_cap = NULL;
        const struct pqos_cap_l2ca *l2_cap = NULL;
        const struct pqos_cap_mba *mba_cap = NULL;
        const struct pqos_cap_mba *smba_cap = NULL;
        const struct cpuinfo_config *vconfig;
        int ret = PQOS_RETVAL_OK;
        unsigned max_l3_cos = 0;
        unsigned max_l2_cos = 0;
        unsigned j;
        enum pqos_cdp_config l3_cdp_cfg = PQOS_REQUIRE_CDP_ANY;
        enum pqos_iordt_config l3_iordt_cfg = PQOS_REQUIRE_IORDT_ANY;
        enum pqos_cdp_config l2_cdp_cfg = PQOS_REQUIRE_CDP_ANY;
        enum pqos_mba_config mba_cfg = PQOS_MBA_ANY;
        enum pqos_mba_config smba_cfg = PQOS_MBA_ANY;
        enum pqos_feature_cfg mba40_cfg = PQOS_FEATURE_ANY;

        ASSERT(cfg == NULL || cfg->l3_cdp == PQOS_REQUIRE_CDP_ON ||
               cfg->l3_cdp == PQOS_REQUIRE_CDP_OFF ||
               cfg->l3_cdp == PQOS_REQUIRE_CDP_ANY);

        ASSERT(cfg == NULL || cfg->l2_cdp == PQOS_REQUIRE_CDP_ON ||
               cfg->l2_cdp == PQOS_REQUIRE_CDP_OFF ||
               cfg->l2_cdp == PQOS_REQUIRE_CDP_ANY);

        ASSERT(cfg == NULL || cfg->mba == PQOS_MBA_DEFAULT ||
               cfg->mba == PQOS_MBA_CTRL || cfg->mba == PQOS_MBA_ANY);

        ASSERT(cfg == NULL || cfg->smba == PQOS_MBA_DEFAULT ||
               cfg->smba == PQOS_MBA_CTRL || cfg->smba == PQOS_MBA_ANY);

        ASSERT(cfg == NULL || cfg->mba40 == PQOS_FEATURE_ON ||
               cfg->mba40 == PQOS_FEATURE_OFF ||
               cfg->mba40 == PQOS_FEATURE_ANY);

        cpuinfo_get_config(&vconfig);

        if (cfg != NULL) {
                l3_cdp_cfg = cfg->l3_cdp;
                l3_iordt_cfg = cfg->l3_iordt;
                l2_cdp_cfg = cfg->l2_cdp;
                mba_cfg = cfg->mba;
                smba_cfg = cfg->smba;
                mba40_cfg = cfg->mba40;
        }

        /* Get L3 CAT capabilities */
        (void)pqos_cap_get_type(cap, PQOS_CAP_TYPE_L3CA, &alloc_cap);
        if (alloc_cap != NULL)
                l3_cap = alloc_cap->u.l3ca;

        /* Get L2 CAT capabilities */
        alloc_cap = NULL;
        (void)pqos_cap_get_type(cap, PQOS_CAP_TYPE_L2CA, &alloc_cap);
        if (alloc_cap != NULL)
                l2_cap = alloc_cap->u.l2ca;

        /* Get MBA capabilities */
        alloc_cap = NULL;
        (void)pqos_cap_get_type(cap, PQOS_CAP_TYPE_MBA, &alloc_cap);
        if (alloc_cap != NULL)
                mba_cap = alloc_cap->u.mba;

        /* Get SMBA capabilities */
        alloc_cap = NULL;
        (void)pqos_cap_get_type(cap, PQOS_CAP_TYPE_SMBA, &alloc_cap);
        if (alloc_cap != NULL)
                smba_cap = alloc_cap->u.smba;

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
        /* Check L3 I/O RDT requested while not present */
        if (l3_cap == NULL && l3_iordt_cfg != PQOS_REQUIRE_IORDT_ANY) {
                LOG_ERROR(
                    "L3 I/O RDT setting requested but no L3 CAT present!\n");
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
                LOG_ERROR("MBA CTRL setting requested but no MBA present!\n");
                ret = PQOS_RETVAL_RESOURCE;
                goto pqos_alloc_reset_exit;
        }
        /* Check MBA 4.0 requested while not present */
        if (mba_cap == NULL && mba40_cfg != PQOS_FEATURE_ANY) {
                LOG_ERROR("MBA 4.0 setting requested but no MBA present!\n");
                ret = PQOS_RETVAL_RESOURCE;
                goto pqos_alloc_reset_exit;
        }

        /* Check SMBA CTRL requested while not present */
        if (smba_cap == NULL && smba_cfg != PQOS_MBA_ANY) {
                LOG_ERROR("SMBA CTRL setting requested but no SMBA present!\n");
                ret = PQOS_RETVAL_RESOURCE;
                goto pqos_alloc_reset_exit;
        }

        if (l3_cap != NULL) {
                /* Check against erroneous L3 CDP request */
                if (l3_cdp_cfg == PQOS_REQUIRE_CDP_ON && !l3_cap->cdp) {
                        LOG_ERROR("L3 CAT/CDP requested but not supported by "
                                  "the platform!\n");
                        ret = PQOS_RETVAL_PARAM;
                        goto pqos_alloc_reset_exit;
                }

                /* Check against erroneous L3 I/O RDT request */
                if (l3_iordt_cfg == PQOS_REQUIRE_IORDT_ON && !l3_cap->iordt) {
                        LOG_ERROR("L3 I/O RDT requested but not supported by "
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
                /* Check against erroneous L2 CDP request */
                if (l2_cdp_cfg == PQOS_REQUIRE_CDP_ON && !l2_cap->cdp) {
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

        if (mba_cap != NULL) {
                if (mba_cfg == PQOS_MBA_CTRL) {
                        LOG_ERROR("MBA CTRL requested but not supported by the "
                                  "platform!\n");
                        ret = PQOS_RETVAL_PARAM;
                        goto pqos_alloc_reset_exit;
                }

                if (!mba_cap->mba40 && mba40_cfg == PQOS_FEATURE_ON) {
                        LOG_ERROR("MBA 4.0 extensions requested but "
                                  "not supported by the platform!\n");
                        ret = PQOS_RETVAL_PARAM;
                        goto pqos_alloc_reset_exit;
                }
        }

        if (l3_cap != NULL) {
                /**
                 * Get number & list of l3cat_ids in the system
                 */
                l3cat_ids = pqos_cpu_get_l3cat_ids(cpu, &l3cat_id_num);
                if (l3cat_ids == NULL || l3cat_id_num == 0)
                        goto pqos_alloc_reset_exit;
                /**
                 * Change L3 COS definition on all l3cat ids
                 * so that each COS allows for access to all cache ways
                 */
                for (j = 0; j < l3cat_id_num; j++) {
                        unsigned core = 0;

                        ret = pqos_cpu_get_one_by_l3cat_id(cpu, l3cat_ids[j],
                                                           &core);
                        if (ret != PQOS_RETVAL_OK)
                                goto pqos_alloc_reset_exit;

                        const uint64_t ways_mask =
                            (1ULL << l3_cap->num_ways) - 1ULL;

                        ret = hw_alloc_reset_cos(PQOS_MSR_L3CA_MASK_START,
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
                l2ids = pqos_cpu_get_l2ids(cpu, &l2id_num);
                if (l2ids == NULL || l2id_num == 0)
                        goto pqos_alloc_reset_exit;

                for (j = 0; j < l2id_num; j++) {
                        const uint64_t ways_mask =
                            (1ULL << l2_cap->num_ways) - 1ULL;
                        unsigned core = 0;

                        ret = pqos_cpu_get_one_by_l2id(cpu, l2ids[j], &core);
                        if (ret != PQOS_RETVAL_OK)
                                goto pqos_alloc_reset_exit;

                        ret = hw_alloc_reset_cos(PQOS_MSR_L2CA_MASK_START,
                                                 max_l2_cos, core, ways_mask);
                        if (ret != PQOS_RETVAL_OK)
                                goto pqos_alloc_reset_exit;
                }
        }

        if (mba_cap != NULL) {
                /**
                 * Get number & list of mba_ids in the system
                 */
                mba_ids = pqos_cpu_get_mba_ids(cpu, &mba_id_num);
                if (mba_ids == NULL || mba_id_num == 0)
                        goto pqos_alloc_reset_exit;

                /**
                 * Go through all L3 CAT ids and reset MBA class definitions
                 * 0 is the default MBA COS value in linear mode.
                 */
                for (j = 0; j < mba_id_num; j++) {
                        unsigned core = 0;

                        ret =
                            pqos_cpu_get_one_by_mba_id(cpu, mba_ids[j], &core);
                        if (ret != PQOS_RETVAL_OK)
                                goto pqos_alloc_reset_exit;

                        ret = hw_alloc_reset_cos(vconfig->mba_msr_reg,
                                                 mba_cap->num_classes, core,
                                                 vconfig->mba_default_val);
                        if (ret != PQOS_RETVAL_OK)
                                goto pqos_alloc_reset_exit;
                }
        }

        if (smba_cap != NULL) {
                /**
                 * Get number & list of smba_ids in the system
                 */
                smba_ids = pqos_cpu_get_smba_ids(cpu, &smba_id_num);
                if (smba_ids == NULL || smba_id_num == 0)
                        goto pqos_alloc_reset_exit;

                /**
                 * Go through all L3 CAT ids and reset SMBA class definitions
                 * 0 is the default SMBA COS value in linear mode.
                 */
                for (j = 0; j < smba_id_num; j++) {
                        unsigned core = 0;

                        ret =
                            pqos_cpu_get_one_by_mba_id(cpu, smba_ids[j], &core);
                        if (ret != PQOS_RETVAL_OK)
                                goto pqos_alloc_reset_exit;

                        ret = hw_alloc_reset_cos(vconfig->smba_msr_reg,
                                                 smba_cap->num_classes, core,
                                                 vconfig->mba_default_val);
                        if (ret != PQOS_RETVAL_OK)
                                goto pqos_alloc_reset_exit;
                }
        }

        /**
         * Associate all cores with COS0
         */
        ret = hw_alloc_reset_assoc();
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
                        ret = hw_alloc_reset_l3cdp(l3cat_id_num, l3cat_ids, 1);
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
                        ret = hw_alloc_reset_l3cdp(l3cat_id_num, l3cat_ids, 0);
                        if (ret != PQOS_RETVAL_OK) {
                                LOG_ERROR("L3 CDP disable error!\n");
                                goto pqos_alloc_reset_exit;
                        }
                }
                _pqos_cap_l3cdp_change(l3_cdp_cfg);
        }

        /**
         * Turn L3 I/O RDT ON or OFF upon the request
         */
        if (l3_cap != NULL) {
                if (l3_iordt_cfg == PQOS_REQUIRE_IORDT_ON &&
                    !l3_cap->iordt_on) {
                        /**
                         * Turn on L3 CDP
                         */
                        LOG_INFO("Turning L3 I/O RDT ON ...\n");
                        ret =
                            hw_alloc_reset_l3iordt(l3cat_id_num, l3cat_ids, 1);
                        if (ret != PQOS_RETVAL_OK) {
                                LOG_ERROR("L3 I/O RDT enable error!\n");
                                goto pqos_alloc_reset_exit;
                        }

                        /* reset channel assoc - initialize mmio tables */
                        ret = hw_alloc_reset_assoc_channels();
                }

                if (l3_iordt_cfg == PQOS_REQUIRE_IORDT_OFF &&
                    l3_cap->iordt_on) {
                        /**
                         * Turn off L3 CDP
                         */
                        LOG_INFO("Turning L3 I/O RDT OFF ...\n");
                        ret =
                            hw_alloc_reset_l3iordt(l3cat_id_num, l3cat_ids, 0);
                        if (ret != PQOS_RETVAL_OK) {
                                LOG_ERROR("L3 I/O RDT disable error!\n");
                                goto pqos_alloc_reset_exit;
                        }
                }
                _pqos_cap_l3iordt_change(l3_iordt_cfg);
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
                        ret = hw_alloc_reset_l2cdp(l2id_num, l2ids, 1);
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
                        ret = hw_alloc_reset_l2cdp(l2id_num, l2ids, 0);
                        if (ret != PQOS_RETVAL_OK) {
                                LOG_ERROR("L2 CDP disable error!\n");
                                goto pqos_alloc_reset_exit;
                        }
                }
                _pqos_cap_l2cdp_change(l2_cdp_cfg);
        }

        /**
         * Enable/disable MBA 4.0 extensions as requested
         */
        if (mba_cap != NULL) {
                if (mba40_cfg == PQOS_FEATURE_ON && !mba_cap->mba40_on) {
                        LOG_INFO("Enabling MBA 4.0 extensions...\n");
                        ret = hw_alloc_reset_mba40(mba_id_num, mba_ids, 1);
                        if (ret != PQOS_RETVAL_OK) {
                                LOG_ERROR("MBA 4.0 enable error!\n");
                                goto pqos_alloc_reset_exit;
                        }
                }

                if (mba40_cfg == PQOS_FEATURE_OFF && mba_cap->mba40_on) {
                        LOG_INFO("Disabling MBA 4.0 extensions...\n");
                        ret = hw_alloc_reset_mba40(mba_id_num, mba_ids, 0);
                        if (ret != PQOS_RETVAL_OK) {
                                LOG_ERROR("MBA 4.0 disable error!\n");
                                goto pqos_alloc_reset_exit;
                        }
                }
        }

pqos_alloc_reset_exit:
        if (l3cat_ids != NULL)
                free(l3cat_ids);
        if (mba_ids != NULL)
                free(mba_ids);
        if (smba_ids != NULL)
                free(smba_ids);
        if (l2ids != NULL)
                free(l2ids);
        return ret;
}

int
alloc_is_bitmask_contiguous(uint64_t bitmask)
{
        if (bitmask == 0)
                return 0;

        while ((bitmask & 1) == 0) /**< Shift until 1 found at position 0 */
                bitmask >>= 1;

        while ((bitmask & 1) != 0) /**< Shift until 0 found at position 0 */
                bitmask >>= 1;

        return (bitmask) ? 0 : 1; /**< non-zero bitmask is not contiguous */
}

int
hw_alloc_assoc_get_channel(const pqos_channel_t channel, unsigned *class_id)
{
        ASSERT(channel != 0);
        ASSERT(class_id != NULL);

        const struct pqos_sysconfig *sys = _pqos_get_sysconfig();

        if (!sys || !sys->cap || !sys->dev)
                return PQOS_RETVAL_ERROR;

        int iordt_enabled;
        int ret = pqos_l3ca_iordt_enabled(sys->cap, NULL, &iordt_enabled);

        if (ret != PQOS_RETVAL_OK)
                return ret;

        if (!iordt_enabled) {
                LOG_ERROR("I/O RDT is unsupported or disabled!\n");
                return PQOS_RETVAL_ERROR;
        }

        size_t i;

        for (i = 0; i < sys->dev->num_channels; ++i) {
                if (sys->dev->channels[i].channel_id != channel)
                        continue;

                if (!sys->dev->channels[i].clos_tagging)
                        break;

                return iordt_assoc_read(channel, class_id);
        }

        return PQOS_RETVAL_PARAM;
}

int
hw_alloc_assoc_set_channel(const pqos_channel_t channel,
                           const unsigned class_id)
{
        size_t i;
        const struct pqos_sysconfig *sys = _pqos_get_sysconfig();

        ASSERT(channel != 0);

        if (!sys || !sys->cap || !sys->dev)
                return PQOS_RETVAL_ERROR;

        int iordt_enabled;
        int ret = pqos_l3ca_iordt_enabled(sys->cap, NULL, &iordt_enabled);

        if (ret != PQOS_RETVAL_OK)
                return ret;

        if (!iordt_enabled) {
                LOG_ERROR("I/O RDT is unsupported or disabled!\n");
                return PQOS_RETVAL_ERROR;
        }

        unsigned num_l3_cos = 0;
        const struct pqos_cap *cap = _pqos_get_cap();

        ret = pqos_l3ca_get_cos_num(cap, &num_l3_cos);
        if (ret != PQOS_RETVAL_OK && ret != PQOS_RETVAL_RESOURCE)
                return ret;

        if (class_id != 0 && class_id >= num_l3_cos)
                /* class_id is out of bounds */
                return PQOS_RETVAL_PARAM;

        for (i = 0; i < sys->dev->num_channels; ++i) {
                if (sys->dev->channels[i].channel_id != channel)
                        continue;

                struct pqos_channel chan = sys->dev->channels[i];

                if (!chan.clos_tagging)
                        break;

                return iordt_assoc_write(channel, class_id);
        }

        return PQOS_RETVAL_PARAM;
}
