/*
 * BSD LICENSE
 *
 * Copyright(c) 2014-2017 Intel Corporation. All rights reserved.
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
 * L3/L2 CAT selected
 */
enum sel_cat_type {
	L3CA,
	L2CA
};

/**
 * Array to store allocation options
 */
static char *alloc_opts[32];

/**
 * Number of allocation options selected
 */
static unsigned sel_alloc_opt_num = 0;

/**
 * Number of allocation settings successfully modified
 */
static unsigned sel_cat_alloc_mod = 0;

/**
 * Number of core to COS associations to be done
 */
static int sel_cat_assoc_num = 0;

/**
 * Core to COS associations details
 */
static struct {
        unsigned core;
        unsigned class_id;
} sel_cat_assoc_tab[PQOS_MAX_CORES];

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
 * @brief Set L3 class definitions on selected sockets
 *
 * @param class_id L3 class ID to set
 * @param mask class bitmask to set
 * @param sock_ids Array of socket ID's to set class definition
 * @param sock_num Number of socket ID's in the array
 * @scope scope L3 CAT update scope i.e. CDP Code/Data
 *
 * @return Number of classes set
 * @retval -1 on error
 */
static int
set_l3_cos(const unsigned class_id, const uint64_t mask,
           const unsigned *sock_ids, const unsigned sock_num,
           const unsigned scope)
{
        unsigned i, set = 0;

        if (sock_ids == NULL || mask == 0) {
                printf("Failed to set L3 CAT configuration!\n");
                return -1;
        }

        /**
         * Loop through each socket and set selected classes
         */
        for (i = 0; i < sock_num; i++) {
                int ret;
                unsigned j, num_ca;
                struct pqos_l3ca ca, sock_l3ca[PQOS_MAX_L3CA_COS];

                /* get current L3 definitions for this socket */
                ret = pqos_l3ca_get(sock_ids[i], DIM(sock_l3ca),
                                    &num_ca, sock_l3ca);
                if (ret != PQOS_RETVAL_OK) {
                        printf("Failed to retrieve socket %u "
                               "L3 classes!\n", sock_ids[i]);
                        break;
                }
                /* find selected class in array */
                for (j = 0; j < num_ca; j++)
                        if (sock_l3ca[j].class_id == class_id) {
                                ca = sock_l3ca[j];
                                break;
                        }
                if (j == num_ca) {
                        printf("Invalid class ID: %u!\n", class_id);
                        break;
                }
                /* check if CDP is selected but disabled */
                if (!ca.cdp && scope != CAT_UPDATE_SCOPE_BOTH) {
                        printf("Failed to set L3 class on socket %u, "
                               "CDP not enabled!\n", sock_ids[i]);
                        break;
                }
                /* if CDP is enabled */
                if (ca.cdp) {
                        if (scope == CAT_UPDATE_SCOPE_BOTH) {
                                ca.u.s.code_mask = mask;
                                ca.u.s.data_mask = mask;
                        } else if (scope == CAT_UPDATE_SCOPE_CODE)
                                ca.u.s.code_mask = mask;
                        else if (scope == CAT_UPDATE_SCOPE_DATA)
                                ca.u.s.data_mask = mask;
                } else
                        ca.u.ways_mask = mask;

                /* set new L3 class definition */
                ret = pqos_l3ca_set(sock_ids[i], 1, &ca);
                if (ret != PQOS_RETVAL_OK) {
                        printf("SOCKET %u L3CA COS%u - FAILED!\n",
                               sock_ids[i], ca.class_id);
                        break;
                }
                if (ca.cdp)
                        printf("SOCKET %u L3CA COS%u => DATA 0x%lx,CODE "
                               "0x%lx\n", sock_ids[i], ca.class_id,
                               ca.u.s.data_mask, ca.u.s.code_mask);
                else
                        printf("SOCKET %u L3CA COS%u => MASK 0x%lx\n",
                               sock_ids[i], ca.class_id, ca.u.ways_mask);
                set++;
        }
        sel_cat_alloc_mod += set;
        if (set < sock_num)
                return -1;

        return (int)set;
}
/**
 * @brief Set L2 class definitions on selected resources/clusters
 *
 * @param class_id L2 class ID to set
 * @param mask class bitmask to set
 * @param l2_ids Array of L2 ID's to set class definition
 * @param id_num Number of L2 ID's in the array
 *
 * @return Number of classes set
 * @retval -1 on error
 */
static int
set_l2_cos(const unsigned class_id, const uint64_t mask,
           const unsigned *l2_ids, const unsigned id_num)
{
        unsigned i, set = 0;
        struct pqos_l2ca ca;

        if (l2_ids == NULL || mask == 0) {
                printf("Failed to set L2 CAT configuration!\n");
                return -1;
        }
        ca.class_id = class_id;
        ca.ways_mask = mask;

        /* Set all selected classes */
        for (i = 0; i < id_num; i++) {
                int ret = pqos_l2ca_set(l2_ids[i], 1, &ca);

                if (ret != PQOS_RETVAL_OK) {
                        printf("L2ID %u L2CA COS%u - FAILED!\n",
                               l2_ids[i], ca.class_id);
                        break;
                }
                printf("L2ID %u L2CA COS%u => MASK 0x%x\n",
                       l2_ids[i], ca.class_id, ca.ways_mask);
                set++;
        }
        sel_cat_alloc_mod += set;
        if (set < id_num)
                return -1;

        return (int)set;
}

/**
 * @brief Verifies and translates definition of single allocation
 *        class of service from text string into internal configuration
 *        for specified sockets and set selected classes.
 *
 * @param str fragment of string passed to -e command line option
 * @param res_ids array of resource ID's to set COS definition
 * @param res_num number of resource ID's in array
 * @param type CAT type (L2/L3)
 *
 * @return Number of classes set
 * @retval Positive on success
 * @retval Negative on error
 */
static int
set_allocation_cos(char *str, unsigned *res_ids,
                   const unsigned res_num,
                   const enum sel_cat_type type,
                   const struct pqos_cpuinfo *cpu)
{
        char *sp, *p = NULL;
        uint64_t mask = 0;
        unsigned class_id = 0, n = res_num;
        unsigned *ids = res_ids;
        int ret = -1, update_scope = CAT_UPDATE_SCOPE_BOTH;

        sp = strdup(str);
        if (sp == NULL)
                sp = str;

        p = strchr(str, '=');
        if (p == NULL) {
                printf("Invalid class of service definition: %s\n", sp);
                return ret;
        }
        *p = '\0';

        parse_cos_mask_type(str, &update_scope, &class_id);
        mask = strtouint64(p+1);

        if (type == L2CA && update_scope != CAT_UPDATE_SCOPE_BOTH) {
                parse_error(sp, "CDP not supported for L2 CAT!\n");
                return ret;
        }
        free(sp);

        /* if L2 CAT selected, set L2 classes */
        if (type == L2CA) {
                if (ids == NULL)
                        ids = pqos_cpu_get_l2ids(cpu, &n);
                if (ids == NULL) {
                        printf("Failed to retrieve L2 cluster info!\n");
                        return -1;
                }
                ret = set_l2_cos(class_id, mask, ids, n);
                if (res_ids == NULL && ids != NULL)
                        free(ids);
                return ret;
        }
        /* set L3 CAT classes */
        if (ids == NULL)
                ids = pqos_cpu_get_sockets(cpu, &n);
        if (ids == NULL) {
                printf("Failed to retrieve socket info!\n");
                return -1;
        }
        ret = set_l3_cos(class_id, mask, ids, n, update_scope);
        if (res_ids == NULL && ids != NULL)
                free(ids);

        return ret;
}

/**
 * @brief Verifies and translates definition of allocation class of service
 *        from text string into internal configuration and sets selected classes
 *
 * @param str string passed to -e command line option
 * @param cpu pointer to cpu topology structure
 *
 * @return Number of classes set
 * @retval Positive on success
 * @retval Negative on error
 */
static int
set_allocation_class(char *str, const struct pqos_cpuinfo *cpu)
{
        int ret = -1;
        char *s, *q, *p = NULL;
        char *saveptr = NULL;
        enum sel_cat_type type;
        const unsigned max_res_sz = MAX(PQOS_MAX_SOCKETS, PQOS_MAX_L2IDS);
        unsigned res_ids[max_res_sz], *sp = NULL, i, n = 1;

        s = strdup(str);
        if (s == NULL)
                s = str;

        p = strchr(str, ':');
        if (p == NULL) {
                printf("Unrecognized allocation format: %s\n", str);
                return ret;
        }
        /**
         * Set up selected res_ids table
         */
	q = strchr(str, '@');
	if (q != NULL) {
                uint64_t ids[max_res_sz];
                /**
                 * Socket ID's selected - set up res_ids table
                 */
                *p = '\0';
		*q = '\0';
                n = strlisttotab(++q, ids, DIM(ids));
                if (n == 0) {
                        printf("No socket ID specified: %s\n", s);
                        return ret;
                }
                /* check for invalid resource ID */
                for (i = 0; i < n; i++) {
                        if (ids[i] >= max_res_sz) {
                                printf("Resource ID out of range: %s\n", s);
                                return ret;
                        }
                        res_ids[i] = (unsigned)ids[i];
                }
                sp = res_ids;
	} else
                *p = '\0';
        /**
	 * Determine selected CAT type (L3/L2)
	 */
	if (strcasecmp(str, "llc") == 0)
                type = L3CA;
        else if (strcasecmp(str, "l2") == 0)
                type = L2CA;
	else {
		printf("Unrecognized allocation type: %s\n", s);
                return ret;
        }
        /**
         * Parse COS masks and apply to selected res_ids
         */
        for (++p; ; p = NULL) {
                char *token = NULL;

                token = strtok_r(p, ",", &saveptr);
                if (token == NULL)
                        break;
                ret = set_allocation_cos(token, sp, n, type, cpu);
                if (ret <= 0)
                        break;
        }
        free(s);
        return ret;
}

/**
 * @brief Parse and apply selected allocation options
 *
 * @param cpu cpu topology structure
 *
 * @return Number of modified classes
 * @retval Positive on success
 * @retval Negative on error
 */
static int
set_alloc(const struct pqos_cpuinfo *cpu)
{
        int ret = -1;
        unsigned i;

        if (!sel_alloc_opt_num)
                return 0;

        /* for each alloc option set selected class definitions */
        for (i = 0; i < sel_alloc_opt_num; i++) {
                ret = set_allocation_class(alloc_opts[i], cpu);
                if (ret <= 0)
                        break;
        }
        /* free all selected alloc options */
        for (i = 0; i < sel_alloc_opt_num; i++) {
                if (alloc_opts[i] == NULL)
                        continue;
                free(alloc_opts[i]);
        }
        if (ret <= 0)
                return -1;

        return (int)sel_cat_alloc_mod;
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
                selfn_strdup(&alloc_opts[sel_alloc_opt_num++], token);
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

        for (i = 0; i < sel_cat_assoc_num; i++) {
                ret = pqos_alloc_assoc_set(sel_cat_assoc_tab[i].core,
                                           sel_cat_assoc_tab[i].class_id);
                ASSERT(ret == PQOS_RETVAL_OK);
                if (ret == PQOS_RETVAL_PARAM) {
                        printf("Core number or class id is out of bounds!\n");
                        return -1;
                } else if (ret != PQOS_RETVAL_OK) {
                        printf("Setting allocation class of service "
                               "association failed!\n");
                        return -1;
                }
        }

        return sel_cat_assoc_num;
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

        if (sel_cat_assoc_num <= 0) {
                for (i = 0; i < n; i++) {
                        if (i >= DIM(sel_cat_assoc_tab))
                                parse_error(str,
                                            "too many cores selected for "
                                            "allocation association");
                        sel_cat_assoc_tab[i].core = (unsigned) cores[i];
                        sel_cat_assoc_tab[i].class_id = cos;
                }
                sel_cat_assoc_num = (int) n;
                return;
        }

        for (i = 0; i < n; i++) {
                int j;

                for (j = 0; j < sel_cat_assoc_num; j++)
                        if (sel_cat_assoc_tab[j].core == (unsigned) cores[i])
                                break;

                if (j < sel_cat_assoc_num) {
                        /**
                         * this core is already on the list
                         * - update COS but warn about it
                         */
                        printf("warn: updating COS for core %u from %u to %u\n",
                               (unsigned) cores[i],
                               sel_cat_assoc_tab[j].class_id, cos);
                        sel_cat_assoc_tab[j].class_id = cos;
                } else {
                        /**
                         * New core is selected - extend the list
                         */
                        unsigned k = (unsigned) sel_cat_assoc_num;

                        if (k >= DIM(sel_cat_assoc_tab))
                                parse_error(str,
                                            "too many cores selected for "
                                            "allocation association");

                        sel_cat_assoc_tab[k].core = (unsigned) cores[i];
                        sel_cat_assoc_tab[k].class_id = cos;
                        sel_cat_assoc_num++;
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
                                       (unsigned long long)tab[n].u.s.data_mask,
                                       (unsigned long long)
                                       tab[n].u.s.code_mask);
                        } else {
                                printf("    L3CA COS%u => MASK 0x%llx\n",
                                       tab[n].class_id,
                                       (unsigned long long)tab[n].u.ways_mask);
                        }
                }
        }

        if (cap_l2ca != NULL) {
                unsigned *l2id, count = 0;

                l2id = pqos_cpu_get_l2ids(cpu_info, &count);
                if (l2id == NULL) {
                        printf("Error retrieving information for L2\n");
                        return;
                }

                for (i = 0; i < count; i++) {
                        struct pqos_l2ca tab[PQOS_MAX_L2CA_COS];
                        unsigned num = 0, n = 0;

                        ret = pqos_l2ca_get(l2id[i], PQOS_MAX_L2CA_COS,
                                            &num, tab);
                        if (ret != PQOS_RETVAL_OK)
                                continue;

                        printf("L2CA COS definitions for L2ID %u:\n",
                               l2id[i]);
                        for (n = 0; n < num; n++)
                                printf("    L2CA COS%u => MASK 0x%llx\n",
                                       tab[n].class_id,
                                       (unsigned long long)tab[n].ways_mask);
                }
                free(l2id);
        }

        for (i = 0; i < sock_count; i++) {
                unsigned *lcores = NULL;
                unsigned lcount = 0, n = 0;

                lcores = pqos_cpu_get_cores(cpu_info, sockets[i], &lcount);
                if (lcores == NULL) {
                        printf("Error retrieving core information!\n");
                        return;
                }
                printf("Core information for socket %u:\n",
                       sockets[i]);
                for (n = 0; n < lcount; n++) {
                        unsigned class_id = 0;
                        unsigned l2id, l3id;
                        pqos_rmid_t rmid = 0;
                        int ret = PQOS_RETVAL_OK;
                        const int is_mon = (cap_mon != NULL);
                        const int is_alloc = (cap_l3ca != NULL) ||
                                (cap_l2ca != NULL);
                        const struct pqos_coreinfo *core_info = NULL;

                        core_info = pqos_cpu_get_core_info(cpu_info, lcores[n]);
                        if (core_info == NULL) {
                                printf("Error retrieving information "
                                       "for core %u!\n", lcores[n]);
                                free(lcores);
                                return;
                        }
                        l2id = core_info->l2_id;
                        l3id = core_info->l3_id;

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
                                printf("    Core %u, L2ID %u, L3ID %u "
                                       "=> COS%u, RMID%u\n", lcores[n],
                                       l2id, l3id, class_id, (unsigned)rmid);
                        if (is_alloc && !is_mon) {
                                if (cap_l3ca == NULL) {
                                        printf("    Core %u, L2ID %u "
                                               "=> COS%u\n", lcores[n], l2id,
                                               class_id);
                                } else {
                                        printf("    Core %u, L2ID %u, L3ID %u "
                                               "=> COS%u\n", lcores[n], l2id,
                                               l3id, class_id);
                                }
                        }
                        if (!is_alloc && is_mon)
                                printf("    Core %u, L2ID %u, L3ID %u "
                                       "=> RMID%u\n", lcores[n], l2id,
                                       l3id, (unsigned)rmid);
                }
                free(lcores);
        }
}

int alloc_apply(const struct pqos_capability *cap_l3ca,
                const struct pqos_capability *cap_l2ca,
                const struct pqos_cpuinfo *cpu)
{
        if (cap_l3ca != NULL || cap_l2ca != NULL) {
                /**
                 * If allocation config changed then exit.
                 * For monitoring, start the program again unless
                 * config file was provided
                 */
                int ret_assoc = 0, ret_cos = 0;

                ret_cos = set_alloc(cpu);
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
                if (sel_cat_assoc_num > 0 || sel_alloc_opt_num > 0) {
                        printf("Allocation capability not detected!\n");
                        return -1;
                }
        }

        return 0;
}
