/*
 * BSD LICENSE
 *
 * Copyright(c) 2014-2026 Intel Corporation. All rights reserved.
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

#include "allocation_common.h"

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
alloc_reset(const struct pqos_alloc_config *cfg)
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
         * Turn L3 I/O RDT allocation ON or OFF upon the request
         */
        if (l3_cap != NULL) {
                if (l3_iordt_cfg == PQOS_REQUIRE_IORDT_ON &&
                    !l3_cap->iordt_on) {
                        /**
                         * Turn on I/O RDT allocation in L3 clusters
                         */
                        LOG_INFO("Turning L3 I/O RDT Allocation ON ...\n");
                        ret = hw_alloc_reset_l3iordt(cpu, 1);
                        if (ret != PQOS_RETVAL_OK) {
                                LOG_ERROR("L3 I/O RDT Allocation enable "
                                          "error!\n");
                                goto pqos_alloc_reset_exit;
                        }

                        /* reset channel assoc - initialize mmio tables */
                        ret = hw_alloc_reset_assoc_channels();
                }

                if (l3_iordt_cfg == PQOS_REQUIRE_IORDT_OFF &&
                    l3_cap->iordt_on) {
                        /**
                         * Turn off I/O RDT allocation in L3 clusters
                         */
                        LOG_INFO("Turning L3 I/O RDT Allocation OFF ...\n");
                        ret = hw_alloc_reset_l3iordt(cpu, 0);
                        if (ret != PQOS_RETVAL_OK) {
                                LOG_ERROR("L3 I/O RDT Allocation disable "
                                          "error!\n");
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
        free(l3cat_ids);
        free(mba_ids);
        free(smba_ids);
        free(l2ids);
        return ret;
}
