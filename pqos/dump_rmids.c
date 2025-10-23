/*
 * BSD LICENSE
 *
 * Copyright(c) 2025 Intel Corporation. All rights reserved.
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
 * @brief Dump RMID Registers module
 *
 */

#include "dump_rmids.h"

#include "common.h"
#include "dump.h"
#include "log.h"
#include "main.h"
#include "pqos.h"

#include <inttypes.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __linux__
#include <sys/utsname.h>
#endif

/**
 * RMIDs dump data structure
 */
static struct sel_dump_rmids_info {
        uint32_t num_domain_ids;              /**< Number of domain ids */
        uint64_t domain_ids[MAX_DOMAIN_IDS];  /**< List of domain ids */
        int num_mem_regions;                  /**< Number of memory regions */
        int region_num[PQOS_MAX_MEM_REGIONS]; /**< List of memory regions */
        unsigned int num_rmids;               /**< Number of RMIDs */
        uint64_t rmids[MAX_RMIDS];            /**< List of RMIDs */
        enum pqos_mmio_dump_rmid_type rmid_type; /**< RMIDs type. Default: 0,
                                                    means MBM */
        unsigned int bin;                        /**< Binary flag. Default: 0,
                                                    means hexadecimal */
        unsigned int upscale;                    /**< Upscale raw value.
                                                    Default: 0 */
} sel_dump_rmids;

void
selfn_dump_rmids(const char *arg)
{
        char *str = NULL;
        unsigned int i = 0;
        unsigned int j = 0;
        unsigned int n = 0;

        if (arg == NULL)
                parse_error(arg, "NULL pointer!");

        if (*arg == '\0')
                parse_error(arg, "Empty string!");

        selfn_strdup(&str, arg);

        n = strlisttotab(str, sel_dump_rmids.rmids, MAX_RMIDS);
        if (n == 0) {
                printf("No RMID specified: %s\n", str);
                exit(EXIT_FAILURE);
        }

        sel_dump_rmids.num_rmids = n;

        /* Check for invalid RMID */
        for (i = 0; i < n; i++) {
                if (sel_dump_rmids.rmids[i] >= MAX_RMIDS) {
                        printf("RMID out of range: %s\n", str);
                        exit(EXIT_FAILURE);
                }
        }

        /* Check duplicate RMID entry */
        for (i = 0; i < n; i++) {
                for (j = i + 1; j < n; j++) {
                        if (sel_dump_rmids.rmids[i] ==
                            sel_dump_rmids.rmids[j]) {
                                parse_error(str, "Duplicate RMID selection");
                                printf("The RMID %ld is entered twice\n",
                                       sel_dump_rmids.rmids[i]);
                                exit(EXIT_FAILURE);
                        }
                }
        }

        free(str);
}

void
selfn_dump_rmid_domain_ids(const char *arg)
{
        char *str = NULL;
        unsigned int i = 0;
        unsigned int j = 0;
        unsigned int n = 0;

        if (arg == NULL)
                parse_error(arg, "NULL pointer!");

        if (*arg == '\0')
                parse_error(arg, "Empty string!");

        selfn_strdup(&str, arg);

        n = strlisttotab(str, sel_dump_rmids.domain_ids, MAX_DOMAIN_IDS);
        if (n == 0) {
                printf("No Domain ID specified: %s\n", str);
                exit(EXIT_FAILURE);
        }

        sel_dump_rmids.num_domain_ids = n;

        /* Check for invalid Domain ID */
        for (i = 0; i < n; i++) {
                if (sel_dump_rmids.domain_ids[i] >= MAX_DOMAINS) {
                        printf("Domain ID out of range: %s\n", str);
                        exit(EXIT_FAILURE);
                }
        }

        /* Check duplicate Domain ID entry */
        for (i = 0; i < n; i++) {
                for (j = i + 1; j < n; j++) {
                        if (sel_dump_rmids.domain_ids[i] ==
                            sel_dump_rmids.domain_ids[j]) {
                                parse_error(str,
                                            "Duplicate Domain ID selection");
                                printf("The Domain ID %ld is entered twice\n",
                                       sel_dump_rmids.domain_ids[i]);
                                exit(EXIT_FAILURE);
                        }
                }
        }

        free(str);
}

/**
 * @brief Verifies and translates memory region number string and stores into
 *        internal sel_dump_rmids_info data structure.
 *
 * @param str single region number string passed to --dump-rmid-mem-regions
 * command line option
 */
static void
parse_dump_mem_regions(char *str)
{
        static int idx = 0;
        int mem_region;
        int i = 0;
        char *endptr = NULL;

        mem_region = (int)strtol(str, &endptr, 0);
        if (*endptr != '\0') {
                parse_error(str, "Invalid memory region number");
                exit(EXIT_FAILURE);
        }

        if (mem_region >= 0 && mem_region < PQOS_MAX_MEM_REGIONS) {
                /* Check duplicate memory region entry */
                for (i = 0; i < sel_dump_rmids.num_mem_regions; i++) {
                        if (mem_region == sel_dump_rmids.region_num[i]) {
                                parse_error(
                                    str, "Duplicate memory region selection");
                                printf("The memory region %d "
                                       "is entered twice\n",
                                       mem_region);
                                exit(EXIT_FAILURE);
                        }
                }
                sel_dump_rmids.region_num[idx] = mem_region;
                idx++;
                sel_dump_rmids.num_mem_regions++;
        } else {
                parse_error(str, "Wrong memory region selection");
                exit(EXIT_FAILURE);
        }
}

void
selfn_dump_rmid_mem_regions(const char *arg)
{
        char *cp = NULL, *str = NULL;
        char *saveptr = NULL;
        unsigned int idx = 0;
        unsigned int i = 0;

        if (arg == NULL)
                parse_error(arg, "NULL pointer!");

        if (*arg == '\0')
                parse_error(arg, "Empty string!");

        selfn_strdup(&cp, arg);

        sel_dump_rmids.num_mem_regions = 0;
        for (idx = 0; idx < PQOS_MAX_MEM_REGIONS; idx++)
                sel_dump_rmids.region_num[idx] = -1;

        for (idx = 0, str = cp;; str = NULL, idx++) {
                char *token = NULL;

                token = strtok_r(str, ",", &saveptr);
                if (token == NULL)
                        break;
                if (idx >= PQOS_MAX_MEM_REGIONS) {
                        parse_error(token, "Wrong memory region selection");
                        printf("Available Memory Regions: ");
                        for (i = 0; i < PQOS_MAX_MEM_REGIONS; i++)
                                printf("%d ", i);
                        exit(EXIT_FAILURE);
                }
                parse_dump_mem_regions(token);
        }

        free(cp);
}

void
selfn_dump_rmid_type(const char *arg)
{
        if (arg == NULL)
                parse_error(arg, "NULL pointer!");

        /**
         * Determine selected type (MBM/L3/IO-L3/IO-TOTAL/IO-MISS)
         */
        if (strcasecmp(arg, "mbm") == 0)
                sel_dump_rmids.rmid_type = MMIO_DUMP_RMID_TYPE_MBM;
        else if (strcasecmp(arg, "l3") == 0)
                sel_dump_rmids.rmid_type = MMIO_DUMP_RMID_TYPE_CMT;
        else if (strcasecmp(arg, "io-l3") == 0)
                sel_dump_rmids.rmid_type = MMIO_DUMP_RMID_IO_L3;
        else if (strcasecmp(arg, "io-total") == 0)
                sel_dump_rmids.rmid_type = MMIO_DUMP_RMID_IO_TOTAL;
        else if (strcasecmp(arg, "io-miss") == 0)
                sel_dump_rmids.rmid_type = MMIO_DUMP_RMID_IO_MISS;
        else {
                if (*arg == '\0')
                        printf("Missing input in --dump-rmid-type\n");
                else
                        printf("Wrong input in --dump-rmid-type=%s.\n", arg);
                printf("Available inputs in --dump-rmid-type: mbm l3 io-l3 "
                       "io-total io-miss\n");
                exit(EXIT_FAILURE);
        }
}

void
selfn_dump_rmid_binary(const char *arg)
{
        UNUSED_ARG(arg);
        sel_dump_rmids.bin = 1;
}

void
selfn_dump_rmid_upscaling(const char *arg)
{
        UNUSED_ARG(arg);
        sel_dump_rmids.upscale = 1;
}

void
dump_rmid_regs(const struct pqos_sysconfig *sys)
{
        struct pqos_mmio_dump_rmids dump_rmids;
        int ret = PQOS_RETVAL_OK;
        unsigned int idx = 0;
        int region_idx = 0;

        if (!sys || !sys->erdt) {
                printf("ERDT info not available!\n");
                exit(EXIT_FAILURE);
        }

        if (sel_dump_rmids.num_domain_ids == 0) {
                printf("Missing --dump-rmid-domain-ids option\n");
                exit(EXIT_FAILURE);
        }

        if (sel_dump_rmids.num_rmids == 0) {
                printf("Missing --dump-rmids option\n");
                exit(EXIT_FAILURE);
        }

        memset(&dump_rmids, 0, sizeof(struct pqos_mmio_dump_rmids));

        /* Copy Domain IDs */
        dump_rmids.num_domain_ids = sel_dump_rmids.num_domain_ids;
        dump_rmids.domain_ids =
            (uint16_t *)malloc(sizeof(uint16_t) * dump_rmids.num_domain_ids);
        if (dump_rmids.domain_ids == NULL) {
                printf("Error with memory allocation for domain_ids!\n");
                exit(EXIT_FAILURE);
        }

        for (idx = 0; idx < sel_dump_rmids.num_domain_ids; idx++)
                dump_rmids.domain_ids[idx] = sel_dump_rmids.domain_ids[idx];

        /* All memory regions are selected, if none is specified
         * in command line
         */
        if (sel_dump_rmids.num_mem_regions == 0) {
                dump_rmids.num_mem_regions = PQOS_MAX_MEM_REGIONS;
                for (idx = 0; idx < PQOS_MAX_MEM_REGIONS; idx++)
                        dump_rmids.region_num[idx] = idx;
        } else {
                dump_rmids.num_mem_regions = sel_dump_rmids.num_mem_regions;
                for (region_idx = 0; region_idx < dump_rmids.num_mem_regions;
                     region_idx++)
                        dump_rmids.region_num[region_idx] =
                            sel_dump_rmids.region_num[region_idx];
        }

        /* Copy RMIDs */
        dump_rmids.num_rmids = sel_dump_rmids.num_rmids;
        dump_rmids.rmids =
            (pqos_rmid_t *)malloc(sizeof(pqos_rmid_t) * dump_rmids.num_rmids);
        if (dump_rmids.rmids == NULL) {
                printf("Error with memory allocation for rmids!\n");
                free(dump_rmids.domain_ids);
                exit(EXIT_FAILURE);
        }

        for (idx = 0; idx < sel_dump_rmids.num_rmids; idx++)
                dump_rmids.rmids[idx] = sel_dump_rmids.rmids[idx];

        /* Copy RMID type, bin and upscale */
        dump_rmids.rmid_type = sel_dump_rmids.rmid_type;
        dump_rmids.bin = sel_dump_rmids.bin;
        dump_rmids.upscale = sel_dump_rmids.upscale;

        ret = pqos_dump_rmids(&dump_rmids);
        if (ret != PQOS_RETVAL_OK)
                printf("RMID Registers Dump failed!\n");

        free(dump_rmids.domain_ids);
        free(dump_rmids.rmids);
}
