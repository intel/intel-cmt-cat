/*
 * BSD LICENSE
 *
 * Copyright(c) 2018-2020 Intel Corporation. All rights reserved.
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
 * @brief Platform QoS/RDT sample application
 * to demonstrate setting MBA COS definitions
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "pqos.h"

/**
 * MBA struct type
 */
enum mba_type { REQUESTED = 0, ACTUAL, MAX_MBA_TYPES };
/**
 * Maintains number of MBA COS to be set
 */
static int sel_mba_cos_num = 0;
/**
 * Table containing  MBA requested and actual COS definitions
 * Requested is set by the user
 * Actual is set by the library
 */
static struct pqos_mba mba[MAX_MBA_TYPES];

/**
 * @brief Converts string into 64-bit unsigned number.
 *
 * Numbers can be in decimal or hexadecimal format.
 *
 * On error, this functions causes process to exit with FAILURE code.
 *
 * @param s string to be converted into 64-bit unsigned number
 *
 * @return Numeric value of the string representing the number
 */
static uint64_t
strtouint64(const char *s)
{
        const char *str = s;
        int base = 10;
        uint64_t n = 0;
        char *endptr = NULL;

        if (strncasecmp(s, "0x", 2) == 0) {
                base = 16;
                s += 2;
        }
        n = strtoull(s, &endptr, base);
        if (endptr != NULL && *endptr != '\0' && !isspace(*endptr)) {
                printf("Error converting '%s' to unsigned number!\n", str);
                exit(EXIT_FAILURE);
        }
        return n;
}
/**
 * @brief Verifies and translates definition of single
 *        allocation class of service
 *        from args into internal configuration.
 *
 * @param argc Number of arguments in input command
 * @param argv Input arguments for COS allocation
 */
static void
allocation_get_input(int argc, char *argv[])
{
        if (argc < 2)
                sel_mba_cos_num = 0;
        else if (!strcmp(argv[1], "-h") || !strcmp(argv[1], "-H")) {
                printf("Usage: %s [<COS#> <Available BW>]\n", argv[0]);
                printf("Example: %s 1 80\n\n", argv[0]);
                sel_mba_cos_num = 0;
        } else {
                mba[REQUESTED].class_id = (unsigned)atoi(argv[1]);
                mba[REQUESTED].mb_max = strtouint64(argv[2]);
                mba[REQUESTED].ctrl = 0;
                sel_mba_cos_num = 1;
        }
}
/**
 * @brief Sets up allocation classes of service on selected MBA ids
 *
 * @param mba_count number of CPU mba ids
 * @param mba_ids arrays
 *
 * @return Number of classes of service set
 * @retval 0 no class of service set (nor selected)
 * @retval negative error
 * @retval positive success
 */
static int
set_allocation_class(unsigned mba_count, const unsigned *mba_ids)
{
        int ret;

        while (mba_count > 0 && sel_mba_cos_num > 0) {
                ret = pqos_mba_set(*mba_ids, sel_mba_cos_num, &mba[REQUESTED],
                                   &mba[ACTUAL]);
                if (ret != PQOS_RETVAL_OK) {
                        printf("Failed to set MBA!\n");
                        return -1;
                }
                printf("SKT%u: MBA COS%u => %u%% requested, %u%% applied\n",
                       *mba_ids, mba[REQUESTED].class_id, mba[REQUESTED].mb_max,
                       mba[ACTUAL].mb_max);

                mba_count--;
                mba_ids++;
        }

        return sel_mba_cos_num;
}
/**
 * @brief Prints allocation configuration
 * @param sock_count number of CPU sockets
 * @param sockets arrays with CPU socket id's
 *
 * @return PQOS_RETVAL_OK on success
 * @return error value on failure
 */
static int
print_allocation_config(const struct pqos_cap *p_cap,
                        const unsigned mba_count,
                        const unsigned *mba_ids)
{
        const struct pqos_capability *cap = NULL;
        const struct pqos_cap_mba *mba_cap = NULL;

        int ret = PQOS_RETVAL_OK;
        unsigned i;

        ret = pqos_cap_get_type(p_cap, PQOS_CAP_TYPE_MBA, &cap);
        if (ret != PQOS_RETVAL_OK)
                return ret;
        mba_cap = cap->u.mba;

        for (i = 0; i < mba_count; i++) {
                struct pqos_mba tab[mba_cap->num_classes];
                unsigned num = 0;

                ret = pqos_mba_get(mba_ids[i], mba_cap->num_classes, &num, tab);
                if (ret == PQOS_RETVAL_OK) {
                        unsigned n = 0;

                        printf("MBA COS definitions for Socket %u:\n",
                               mba_ids[i]);
                        for (n = 0; n < num; n++) {
                                printf("    MBA COS%u => %u%% available\n",
                                       tab[n].class_id, tab[n].mb_max);
                        }
                } else {
                        printf("Error:%d", ret);
                        return ret;
                }
        }
        return ret;
}

int
main(int argc, char *argv[])
{
        struct pqos_config cfg;
        const struct pqos_cpuinfo *p_cpu = NULL;
        const struct pqos_cap *p_cap = NULL;
        unsigned mba_id_count, *p_mba_ids = NULL;
        int ret, exit_val = EXIT_SUCCESS;

        memset(&cfg, 0, sizeof(cfg));
        cfg.fd_log = STDOUT_FILENO;
        cfg.verbose = 0;
        /* PQoS Initialization - Check and initialize MBA capability */
        ret = pqos_init(&cfg);
        if (ret != PQOS_RETVAL_OK) {
                printf("Error initializing PQoS library!\n");
                exit_val = EXIT_FAILURE;
                goto error_exit;
        }
        /* Get capability and CPU info pointers */
        ret = pqos_cap_get(&p_cap, &p_cpu);
        if (ret != PQOS_RETVAL_OK) {
                printf("Error retrieving PQoS capabilities!\n");
                exit_val = EXIT_FAILURE;
                goto error_exit;
        }
        /* Get CPU mba_id information to set COS */
        p_mba_ids = pqos_cpu_get_mba_ids(p_cpu, &mba_id_count);
        if (p_mba_ids == NULL) {
                printf("Error retrieving MBA ID information!\n");
                exit_val = EXIT_FAILURE;
                goto error_exit;
        }
        /* Get input from user  */
        allocation_get_input(argc, argv);
        if (sel_mba_cos_num != 0) {
                /* Set delay value for MBA COS allocation */
                ret = set_allocation_class(mba_id_count, p_mba_ids);
                if (ret < 0) {
                        printf("Allocation configuration error!\n");
                        goto error_exit;
                }
                printf("Allocation configuration altered.\n");
        }
        /* Print COS definition */
        ret = print_allocation_config(p_cap, mba_id_count, p_mba_ids);
        if (ret != PQOS_RETVAL_OK) {
                printf("Allocation capability not detected!\n");
                exit_val = EXIT_FAILURE;
                goto error_exit;
        }
error_exit:
        /* reset and deallocate all the resources */
        ret = pqos_fini();
        if (ret != PQOS_RETVAL_OK)
                printf("Error shutting down PQoS library!\n");
        if (p_mba_ids != NULL)
                free(p_mba_ids);
        return exit_val;
}
