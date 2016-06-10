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
 * @brief Platform QoS utility - allocation module
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pqos.h"

#include "main.h"
#include "alloc.h"

/**
 * Defines used to identify CAT mask definitions
 */
#define CAT_UPDATE_SCOPE_BOTH 0    /**< update COS code & data masks */
#define CAT_UPDATE_SCOPE_DATA 1    /**< update COS data mask */
#define CAT_UPDATE_SCOPE_CODE 2    /**< update COS code mask */

/**
 * Number of classes of service (COS) selected for modification
 * (L3 cache allocation)
 */
static int sel_l3ca_cos_num = 0;

/**
 * New settings for L3 cache
 * allocation classes of service
 */
struct l3ca_cos_sock {
        unsigned cos_num;
        struct pqos_l3ca cos_tab[PQOS_MAX_L3CA_COS];
};

/**
 * Table of COS settings for selected sockets
 */
static struct l3ca_cos_sock sel_l3ca_cos_tab[PQOS_MAX_SOCKETS];

/**
 * COS settings for all sockets
 */
static struct l3ca_cos_sock sel_l3ca_cos_all;

/**
 * Number of core to COS associations to be done
 */
static int sel_l3ca_assoc_num = 0;

/**
 * Core to L3 COS associations details
 */
static struct {
        unsigned core;
        unsigned class_id;
} sel_l3ca_assoc_tab[PQOS_MAX_CORES];

/**
 * @brief Sets up allocation classes of service on selected CPU sockets
 *
 * @param sock_count number of CPU sockets
 * @param sockets arrays with CPU socket id's
 *
 * @return Number of classes of service set
 * @retval 0 no class of service set, no class selected for change
 * @retval negative error
 * @retval positive success
 */
static int
set_allocation_class(const unsigned sock_count, const unsigned *sockets)
{
        unsigned i, set = 0;

        /**
         * Check if selected sockets exist on the system
         */
        for (i = 0; i < DIM(sel_l3ca_cos_tab); i++) {
                unsigned j;

                if (sel_l3ca_cos_tab[i].cos_num == 0)
                        continue;

                for (j = 0; j < sock_count; j++)
                        if (sockets[j] == i)
                                break;
                if (j == sock_count) {
                        printf("Socket %u not detected!\n", i);
                        return -1;
                }
        }
        /**
         * Set selected classes of service
         */
        for (i = 0; i < sock_count; i++) {
                unsigned s = sockets[i];
                int ret;

                if (s >= DIM(sel_l3ca_cos_tab))
                        continue;
                /**
                 * Set COS that apply to all sockets
                 */
                if (sel_l3ca_cos_all.cos_num) {
                        ret = pqos_l3ca_set(s, sel_l3ca_cos_all.cos_num,
                                            sel_l3ca_cos_all.cos_tab);
                        ASSERT(ret == PQOS_RETVAL_OK);
                        if (ret != PQOS_RETVAL_OK)
                                goto set_cos_failure;

                        set += sel_l3ca_cos_all.cos_num;
                }
                /**
                 * Set COS that apply to this socket
                 */
                if (sel_l3ca_cos_tab[s].cos_num) {
                        ret = pqos_l3ca_set(s, sel_l3ca_cos_tab[s].cos_num,
                                            sel_l3ca_cos_tab[s].cos_tab);
                        ASSERT(ret == PQOS_RETVAL_OK);
                        if (ret != PQOS_RETVAL_OK)
                                goto set_cos_failure;

                        set += sel_l3ca_cos_tab[s].cos_num;
                }
        }

        return set;

set_cos_failure:
        printf("Setting up cache allocation "
               "class of service failed!\n");

        return -1;
}

/**
 * @brief Converts string describing CAT COS into ID and mask scope
 *
 * Current string format is: <ID>[CcDd]
 *
 * Some examples:
 *  1d  - class 1, data mask
 *  5C  - class 5, code mask
 *  0   - class 0, common mask for code & data
 *
 * @param [in] str string describing CAT COS. Function may modify
 *             the last character of the string in some conditions.
 * @param [out] scope indicates if string \a str refers to both COS masks
 *              or just one of them
 * @param [out] id class ID referred to in the string \a str
 */
static void
parse_cos_mask_type(char *str, int *scope, unsigned *id)
{
        size_t len = 0;

        ASSERT(str != NULL);
        ASSERT(scope != NULL);
        ASSERT(id != NULL);
        len = strlen(str);
        if (len > 1 && (str[len - 1] == 'c' || str[len - 1] == 'C')) {
                *scope = CAT_UPDATE_SCOPE_CODE;
                str[len - 1] = '\0';
        } else if (len > 1 && (str[len - 1] == 'd' || str[len - 1] == 'D')) {
                *scope = CAT_UPDATE_SCOPE_DATA;
                str[len - 1] = '\0';
        } else {
                *scope = CAT_UPDATE_SCOPE_BOTH;
        }
        *id = (unsigned) strtouint64(str);
}

/**
 * @brief Updates the COS table with specified COS for a socket
 *
 * @param sock pointer to socket COS structure to be updated
 * @param id COS id to be added to the table
 * @param mask CBM to be added to the table
 * @param scope CDP config update scope
 */
static void
update_l3ca_cos_tab(struct l3ca_cos_sock *sock, const unsigned id,
                    const uint64_t mask, const int scope)
{
        unsigned i;

        for (i = 0; i < sock->cos_num; i++)
                if (sock->cos_tab[i].class_id == id)
                        break;

        if (i < sock->cos_num) {
                /**
                 * This class is already on the list.
                 * Don't allow for mixing CDP and non-CDP formats.
                 */
                if ((sock->cos_tab[i].cdp && scope == CAT_UPDATE_SCOPE_BOTH) ||
                    (!sock->cos_tab[i].cdp && scope != CAT_UPDATE_SCOPE_BOTH)) {
                        printf("error: COS%u defined twice using CDP and "
                               "non-CDP format\n", id);
                        exit(EXIT_FAILURE);
                }

                switch (scope) {
                case CAT_UPDATE_SCOPE_BOTH:
                        sock->cos_tab[i].ways_mask = mask;
                        break;
                case CAT_UPDATE_SCOPE_CODE:
                        sock->cos_tab[i].code_mask = mask;
                        break;
                case CAT_UPDATE_SCOPE_DATA:
                        sock->cos_tab[i].data_mask = mask;
                        break;
                }
        } else {
                /**
                 * New class selected - extend the list
                 */
                unsigned k = sock->cos_num;

                if (k >= PQOS_MAX_L3CA_COS) {
                        printf("too many allocation classes selected!\n");
                        exit(EXIT_FAILURE);
                }

                sock->cos_tab[k].class_id = id;

                switch (scope) {
                case CAT_UPDATE_SCOPE_BOTH:
                        sock->cos_tab[k].cdp = 0;
                        sock->cos_tab[k].ways_mask = mask;
                        break;
                case CAT_UPDATE_SCOPE_CODE:
                        sock->cos_tab[k].cdp = 1;
                        sock->cos_tab[k].code_mask = mask;
                        /**
                         * This will result in error during set operation
                         * if data mask is not defined by the user.
                         */
                        sock->cos_tab[k].data_mask = (uint64_t) (-1LL);
                        break;
                case CAT_UPDATE_SCOPE_DATA:
                        sock->cos_tab[k].cdp = 1;
                        sock->cos_tab[k].data_mask = mask;
                        /**
                         * This will result in error during set operation
                         * if code mask is not defined by the user.
                         */
                        sock->cos_tab[k].code_mask = (uint64_t) (-1LL);
                        break;
                }

                sock->cos_num++;
                sel_l3ca_cos_num++;
        }
}

/**
 * @brief Verifies and translates definition of single allocation
 *        class of service from text string into internal configuration
 *        for specified sockets.
 *
 * @param str fragment of string passed to -e command line option
 * @param sockets array of sockets to set COS tables
 * @param sock_num number of sockets in array
 */
static void
parse_allocation_cos(char *str, const uint64_t *sockets,
                     const unsigned sock_num)
{
        char *p = NULL;
        uint64_t mask = 0;
        unsigned i, class_id = 0;
        int update_scope = CAT_UPDATE_SCOPE_BOTH;

        p = strchr(str, '=');
        if (p == NULL)
                parse_error(str, "Invalid class of service definition");
        *p = '\0';

        parse_cos_mask_type(str, &update_scope, &class_id);
        mask = strtouint64(p+1);

        /**
         * Update all sockets COS table
         */
        if (sockets == NULL) {
                update_l3ca_cos_tab(&sel_l3ca_cos_all, class_id,
                                    mask, update_scope);
                return;
        }
        /**
         * Update COS tables
         */
        for (i = 0; i < sock_num; i++) {
                unsigned s = (unsigned) sockets[i];
                /**
                 * Update specified socket COS table
                 */
                update_l3ca_cos_tab(&sel_l3ca_cos_tab[s], class_id,
                                    mask, update_scope);
        }
}

/**
 * @brief Verifies and translates definition of allocation class of service
 *        from text string into internal configuration.
 *
 * @param str string passed to -e command line option
 */
static void
parse_allocation_class(char *str)
{
        char *p = NULL;
        char *saveptr = NULL;
        uint64_t *sp = NULL;
        uint64_t sockets[PQOS_MAX_SOCKETS];
        unsigned i, n = 1;

        /**
         * Set up selected sockets table
         */
        p = strchr(str, ':');
        if (p == NULL)
                parse_error(str, "Unrecognized allocation format");
        if (strncasecmp(str, "llc@", 4) == 0) {
                char *s = strdup(str);
                /**
                 * Socket ID's selected
                 * - add selected sockets to the table
                 */
                if (s == NULL)
                        s = str;
                *p = '\0';
                str += strlen("llc@");
                n = strlisttotab(str, sockets, DIM(sockets));
                if (n == 0)
                        parse_error(s, "No socket ID specified");
                /**
                 * Check selected sockets are within range
                 */
                for (i = 0; i < n; i++)
                        if (sockets[i] >= PQOS_MAX_SOCKETS)
                                parse_error(s, "Socket ID out of range");
                sp = sockets;
                free(s);
        } else {
                /**
                 * No sockets selected
                 * - apply COS to all sockets
                 */
                if (strncasecmp(str, "llc:", 4) != 0)
                        parse_error(str, "Unrecognized allocation type");
        }
        /**
         * Parse COS masks and apply to selected sockets
         */
        for (++p; ; p = NULL) {
                char *token = NULL;

                token = strtok_r(p, ",", &saveptr);
                if (token == NULL)
                        break;
                parse_allocation_cos(token, sp, n);
        }
}

void selfn_allocation_class(const char *arg)
{
        char *cp = NULL, *str = NULL;
        char *saveptr = NULL;

        if (arg == NULL)
                parse_error(arg, "NULL pointer!");

        if (strlen(arg) <= 0)
                parse_error(arg, "Empty string!");

        selfn_strdup(&cp, arg);

        for (str = cp; ; str = NULL) {
                char *token = NULL;

                token = strtok_r(str, ";", &saveptr);
                if (token == NULL)
                        break;
                parse_allocation_class(token);
        }

        free(cp);
}

/**
 * @brief Sets up association between cores and allocation classes of service
 *
 * @return Number of associations made
 * @retval 0 no association made (nor requested)
 * @retval negative error
 * @retval positive success
 */
static int
set_allocation_assoc(void)
{
        int i;
        int ret;

        for (i = 0; i < sel_l3ca_assoc_num; i++) {
                ret = pqos_alloc_assoc_set(sel_l3ca_assoc_tab[i].core,
                                           sel_l3ca_assoc_tab[i].class_id);
                ASSERT(ret == PQOS_RETVAL_OK);
                if (ret != PQOS_RETVAL_OK) {
                        printf("Setting allocation class of service "
                               "association failed!\n");
                        return -1;
                }
        }

        return sel_l3ca_assoc_num;
}

/**
 * @brief Verifies and translates allocation association config string into
 *        internal configuration.
 *
 * @param str string passed to -a command line option
 */
static void
parse_allocation_assoc(char *str)
{
        uint64_t cores[PQOS_MAX_CORES];
        unsigned i = 0, n = 0, cos = 0;
        char *p = NULL;

        if (strncasecmp(str, "llc:", 4) != 0)
                parse_error(str, "Unrecognized allocation type");

        str += strlen("llc:");
        p = strchr(str, '=');
        if (p == NULL)
                parse_error(str,
                            "Invalid allocation class of service "
                            "association format");
        *p = '\0';

        cos = (unsigned) strtouint64(str);

        n = strlisttotab(p+1, cores, DIM(cores));
        if (n == 0)
                return;

        if (sel_l3ca_assoc_num <= 0) {
                for (i = 0; i < n; i++) {
                        if (i >= DIM(sel_l3ca_assoc_tab))
                                parse_error(str,
                                            "too many cores selected for "
                                            "allocation association");
                        sel_l3ca_assoc_tab[i].core = (unsigned) cores[i];
                        sel_l3ca_assoc_tab[i].class_id = cos;
                }
                sel_l3ca_assoc_num = (int) n;
                return;
        }

        for (i = 0; i < n; i++) {
                int j;

                for (j = 0; j < sel_l3ca_assoc_num; j++)
                        if (sel_l3ca_assoc_tab[j].core == (unsigned) cores[i])
                                break;

                if (j < sel_l3ca_assoc_num) {
                        /**
                         * this core is already on the list
                         * - update COS but warn about it
                         */
                        printf("warn: updating COS for core %u from %u to %u\n",
                               (unsigned) cores[i],
                               sel_l3ca_assoc_tab[j].class_id, cos);
                        sel_l3ca_assoc_tab[j].class_id = cos;
                } else {
                        /**
                         * New core is selected - extend the list
                         */
                        unsigned k = (unsigned) sel_l3ca_assoc_num;

                        if (k >= DIM(sel_l3ca_assoc_tab))
                                parse_error(str,
                                            "too many cores selected for "
                                            "allocation association");

                        sel_l3ca_assoc_tab[k].core = (unsigned) cores[i];
                        sel_l3ca_assoc_tab[k].class_id = cos;
                        sel_l3ca_assoc_num++;
                }
        }
}

void selfn_allocation_assoc(const char *arg)
{
        char *cp = NULL, *str = NULL;
        char *saveptr = NULL;

        if (arg == NULL)
                parse_error(arg, "NULL pointer!");

        if (strlen(arg) <= 0)
                parse_error(arg, "Empty string!");

        selfn_strdup(&cp, arg);

        for (str = cp; ; str = NULL) {
                char *token = NULL;

                token = strtok_r(str, ";", &saveptr);
                if (token == NULL)
                        break;
                parse_allocation_assoc(token);
        }

        free(cp);
}

void alloc_print_config(const struct pqos_capability *cap_mon,
                        const struct pqos_capability *cap_l3ca,
                        const struct pqos_capability *cap_l2ca,
                        const unsigned sock_count,
                        const unsigned *sockets,
                        const struct pqos_cpuinfo *cpu_info)
{
        int ret;
        unsigned i;

        for (i = 0; (i < sock_count) && (cap_l3ca != NULL); i++) {
                struct pqos_l3ca tab[PQOS_MAX_L3CA_COS];
                unsigned num = 0;
                unsigned n = 0;

                ret = pqos_l3ca_get(sockets[i], PQOS_MAX_L3CA_COS,
                                    &num, tab);
                if (ret != PQOS_RETVAL_OK)
                        continue;

                printf("L3CA COS definitions for Socket %u:\n",
                       sockets[i]);
                for (n = 0; n < num; n++) {
                        if (tab[n].cdp) {
                                printf("    L3CA COS%u => DATA 0x%llx,"
                                       "CODE 0x%llx\n",
                                       tab[n].class_id,
                                       (unsigned long long)tab[n].data_mask,
                                       (unsigned long long)tab[n].code_mask);
                        } else {
                                printf("    L3CA COS%u => MASK 0x%llx\n",
                                       tab[n].class_id,
                                       (unsigned long long)tab[n].ways_mask);
                        }
                }
        }

        for (i = 0; (i < sock_count) && (cap_l2ca != NULL); i++) {
                struct pqos_l2ca tab[PQOS_MAX_L2CA_COS];
                unsigned num = 0;
                unsigned n = 0;

                ret = pqos_l2ca_get(sockets[i], PQOS_MAX_L2CA_COS,
                                    &num, tab);
                if (ret != PQOS_RETVAL_OK)
                        continue;

                printf("L2CA COS definitions for Socket %u:\n",
                       sockets[i]);
                for (n = 0; n < num; n++)
                        printf("    L2CA COS%u => MASK 0x%llx\n",
                               tab[n].class_id,
                               (unsigned long long)tab[n].ways_mask);
        }

        for (i = 0; i < sock_count; i++) {
                unsigned lcores[PQOS_MAX_SOCKET_CORES];
                unsigned lcount = 0, n = 0;

                ret = pqos_cpu_get_cores(cpu_info, sockets[i],
                                         PQOS_MAX_SOCKET_CORES,
                                         &lcount, &lcores[0]);
                if (ret != PQOS_RETVAL_OK) {
                        printf("Error retrieving core information!\n");
                        return;
                }
                ASSERT(ret == PQOS_RETVAL_OK);
                printf("Core information for socket %u:\n",
                       sockets[i]);
                for (n = 0; n < lcount; n++) {
                        unsigned class_id = 0;
                        pqos_rmid_t rmid = 0;
                        int ret = PQOS_RETVAL_OK;
                        const int is_mon = (cap_mon != NULL);
                        const int is_alloc = (cap_l3ca != NULL) ||
                                (cap_l2ca != NULL);

			if (is_alloc)
				ret = pqos_alloc_assoc_get(lcores[n],
                                                           &class_id);
			if (is_mon && ret == PQOS_RETVAL_OK)
				ret = pqos_mon_assoc_get(lcores[n], &rmid);

                        if (ret != PQOS_RETVAL_OK) {
                                printf("    Core %u => ERROR\n", lcores[n]);
                                continue;
                        }

                        if (is_alloc && is_mon)
                                printf("    Core %u => COS%u, RMID%u\n",
                                       lcores[n], class_id, (unsigned)rmid);
                        if (is_alloc && !is_mon)
                                printf("    Core %u => COS%u\n", lcores[n],
                                       class_id);
                        if (!is_alloc && is_mon)
                                printf("    Core %u => RMID%u\n", lcores[n],
                                       (unsigned)rmid);
                }
        }
}

int alloc_apply(const struct pqos_capability *cap_l3ca,
                unsigned sock_count, unsigned *sockets)
{
        if (cap_l3ca != NULL) {
                /**
                 * If allocation config changed then exit.
                 * For monitoring, start the program again unless
                 * config file was provided
                 */
                int ret_assoc = 0, ret_cos = 0;

                ret_cos = set_allocation_class(sock_count, sockets);
                if (ret_cos < 0) {
                        printf("Allocation configuration error!\n");
                        return -1;
                }

                ret_assoc = set_allocation_assoc();
                if (ret_assoc < 0) {
                        printf("CAT association error!\n");
                        return -1;
                }
                /**
                 * Check if any allocation configuration has changed
                 */
                if (ret_assoc > 0 || ret_cos > 0) {
                        printf("Allocation configuration altered.\n");
                        return 1;
                }
        } else {
                if (sel_l3ca_assoc_num > 0 || sel_l3ca_cos_num > 0) {
                        printf("Allocation capability not detected!\n");
                        return -1;
                }
        }

        return 0;
}
