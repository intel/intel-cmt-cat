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
 * L3/L2 CAT selected
 */
enum sel_cat_type {
	L3CA,
	L2CA
};

/**
 * Number of classes of service (COS) selected for modification
 * (L3/L2 cache allocation)
 */
static int sel_cat_cos_num = 0;

/**
 * New settings for L3 & L2 cache
 * allocation classes of service
 */
struct cat_cos_sock {
        unsigned cos_num_l3ca;
	unsigned cos_num_l2ca;
        struct pqos_l3ca cos_tab_l3ca[PQOS_MAX_L3CA_COS];
	struct pqos_l2ca cos_tab_l2ca[PQOS_MAX_L2CA_COS];
};

/**
 * Table of COS settings for selected sockets
 */
static struct cat_cos_sock sel_cat_cos_tab[PQOS_MAX_SOCKETS];

/**
 * COS settings for all sockets
 */
static struct cat_cos_sock sel_cat_cos_all;

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
 * @brief Sets L3/L2 allocation classes on a socket
 *
 * @param ca structure containing L3/L2 CAT info
 * @param s socket id
 * @return number of classes that have been set
 * @retval positive - number of classes set
 * @retval negative - error
 */
static int
set_alloc(struct cat_cos_sock *ca, const unsigned s)
{
	int ret, set = 0;

	if (s >= PQOS_MAX_SOCKETS || ca == NULL)
		return -1;

	if (ca->cos_num_l3ca) {
		ret = pqos_l3ca_set(s, ca->cos_num_l3ca, ca->cos_tab_l3ca);
		ASSERT(ret == PQOS_RETVAL_OK);
		if (ret != PQOS_RETVAL_OK)
			goto set_l3_cos_failure;
		set += ca->cos_num_l3ca;
	}
	if (ca->cos_num_l2ca) {
		ret = pqos_l2ca_set(s, ca->cos_num_l2ca, ca->cos_tab_l2ca);
		ASSERT(ret == PQOS_RETVAL_OK);
		if (ret != PQOS_RETVAL_OK)
			goto set_l2_cos_failure;
		set += ca->cos_num_l2ca;
	}
	return set;

set_l3_cos_failure:
	printf("Setting up L3 CAT class of service failed!\n");
	return -1;

set_l2_cos_failure:
	printf("Setting up L2 CAT class of service failed!\n");
	return -1;
}

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
        for (i = 0; i < DIM(sel_cat_cos_tab); i++) {
                unsigned j;

                if (sel_cat_cos_tab[i].cos_num_l3ca == 0 &&
		    sel_cat_cos_tab[i].cos_num_l2ca == 0)
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
		int ret;
                unsigned s = sockets[i];

                if (s >= DIM(sel_cat_cos_tab))
                        continue;
                /**
                 * Set L3/L2 COS that apply to all sockets
                 */
		ret = set_alloc(&sel_cat_cos_all, s);
		if (ret < 0)
			return ret;
		set += ret;
                /**
                 * Set L3/L2 COS that apply to this socket
                 */
		ret = set_alloc(&sel_cat_cos_tab[s], s);
		if (ret < 0)
			return ret;
		set += ret;
        }
        return set;
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
 * @brief Function to update L3 COS table
 *
 * @param sock structure that holds COS info for a socket
 * @param id class id
 * @param mask CBM to set
 * @param scope CDP scope to be updated
 */
static void
update_l3_tab(struct cat_cos_sock *sock, const unsigned id,
	      const uint64_t mask, const int scope)
{
	unsigned i;

	for (i = 0; i < sock->cos_num_l3ca; i++)
                if (sock->cos_tab_l3ca[i].class_id == id)
                        break;

        if (i < sock->cos_num_l3ca) {
		/**
		 * This class is already on the list.
                 * Don't allow for mixing CDP and non-CDP formats.
                 */
                if ((sock->cos_tab_l3ca[i].cdp &&
                     scope == CAT_UPDATE_SCOPE_BOTH) ||
                    (!sock->cos_tab_l3ca[i].cdp &&
                     scope != CAT_UPDATE_SCOPE_BOTH)) {
                        printf("error: L3 COS%u defined twice using "
			       "CDP and non-CDP format\n", id);
                        exit(EXIT_FAILURE);
                }

                switch (scope) {
                case CAT_UPDATE_SCOPE_BOTH:
                        sock->cos_tab_l3ca[i].u.ways_mask = mask;
                        break;
                case CAT_UPDATE_SCOPE_CODE:
                        sock->cos_tab_l3ca[i].u.s.code_mask = mask;
                        break;
                case CAT_UPDATE_SCOPE_DATA:
                        sock->cos_tab_l3ca[i].u.s.data_mask = mask;
                        break;
                }
        } else {
		/**
		 * New class selected - extend the list
		 */
                unsigned k = sock->cos_num_l3ca;

                if (k >= PQOS_MAX_L3CA_COS) {
                        printf("too many L3 allocation classes selected!\n");
                        exit(EXIT_FAILURE);
                }

                sock->cos_tab_l3ca[k].class_id = id;

                switch (scope) {
                case CAT_UPDATE_SCOPE_BOTH:
                        sock->cos_tab_l3ca[k].cdp = 0;
                        sock->cos_tab_l3ca[k].u.ways_mask = mask;
                        break;
                case CAT_UPDATE_SCOPE_CODE:
                        sock->cos_tab_l3ca[k].cdp = 1;
                        sock->cos_tab_l3ca[k].u.s.code_mask = mask;
                        /**
                         * This will result in error during set operation
                         * if data mask is not defined by the user.
                         */
                        sock->cos_tab_l3ca[k].u.s.data_mask = (uint64_t) (-1LL);
                        break;
                case CAT_UPDATE_SCOPE_DATA:
                        sock->cos_tab_l3ca[k].cdp = 1;
                        sock->cos_tab_l3ca[k].u.s.data_mask = mask;
                        /**
                         * This will result in error during set operation
                         * if code mask is not defined by the user.
                         */
                        sock->cos_tab_l3ca[k].u.s.code_mask = (uint64_t) (-1LL);
                        break;
                }

                sock->cos_num_l3ca++;
                sel_cat_cos_num++;
        }
}

/**
 * @brief Function to update L2 COS tables
 *
 * @param sock structure that holds COS info for a socket
 * @param id class id
 * @param mask CBM to set
 */
static void
update_l2_tab(struct cat_cos_sock *sock, const unsigned id, const uint64_t mask)
{
	unsigned i;

	for (i = 0; i < sock->cos_num_l2ca; i++)
		if (sock->cos_tab_l2ca[i].class_id == id)
			break;

	if (i < sock->cos_num_l2ca)
		/**
		 * This class is already on the list.
		 */
		sock->cos_tab_l2ca[i].ways_mask = mask;
	else {
		/**
		 * New class selected - extend the list
		 */
		unsigned k = sock->cos_num_l2ca;

		if (k >= PQOS_MAX_L2CA_COS) {
			printf("too many L2 allocation "
			       "classes selected!\n");
			exit(EXIT_FAILURE);
		}

		sock->cos_tab_l2ca[k].class_id = id;
		sock->cos_tab_l2ca[k].ways_mask = mask;
		sock->cos_num_l2ca++;
		sel_cat_cos_num++;
	}
}

/**
 * @brief Updates the COS tables with specified COS for a socket
 *
 * @param sock pointer to socket COS structure to be updated
 * @param id COS id to be added to the table
 * @param mask CBM to be added to the table
 * @param scope CDP config update scope
 * @param type CAT type (L2/L3)
 */
static void
update_cat_cos_tab(struct cat_cos_sock *sock, const unsigned id,
		   const uint64_t mask, const int scope,
		   const enum sel_cat_type type)
{
	/**
	 * If L2 CAT selected - Update L2 COS table
	 * - Otherwise update L3 COS table
	 */
	if (type == L2CA)
		update_l2_tab(sock, id, mask);
	else if (type == L3CA)
		update_l3_tab(sock, id, mask, scope);
	else {
		printf("error: invalid CAT type selected!\n");
		exit(EXIT_FAILURE);
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
 * @param type CAT type (L2/L3)
 */
static void
parse_allocation_cos(char *str, const uint64_t *sockets,
                     const unsigned sock_num, const enum sel_cat_type type)
{
        char *sp, *p = NULL;
        uint64_t mask = 0;
        unsigned i, class_id = 0;
        int update_scope = CAT_UPDATE_SCOPE_BOTH;

        sp = strdup(str);
        if (sp == NULL)
                sp = str;

        p = strchr(str, '=');
        if (p == NULL)
                parse_error(sp, "Invalid class of service definition");
        *p = '\0';

        parse_cos_mask_type(str, &update_scope, &class_id);
        mask = strtouint64(p+1);

        if (type == L2CA && update_scope != CAT_UPDATE_SCOPE_BOTH)
                parse_error(sp, "CDP not supported for L2 CAT!\n");
        free(sp);
        /**
         * Update all sockets COS table
         */
        if (sockets == NULL) {
                update_cat_cos_tab(&sel_cat_cos_all, class_id,
                                   mask, update_scope, type);
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
                update_cat_cos_tab(&sel_cat_cos_tab[s], class_id,
                                   mask, update_scope, type);
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
        char *s, *q, *p = NULL;
        char *saveptr = NULL;
        uint64_t *sp = NULL;
        uint64_t sockets[PQOS_MAX_SOCKETS];
        unsigned i, n = 1;
	enum sel_cat_type type;

        s = strdup(str);
        if (s == NULL)
                s = str;

        p = strchr(str, ':');
        if (p == NULL)
                parse_error(str, "Unrecognized allocation format");
	/**
         * Set up selected sockets table
         */
	q = strchr(str, '@');
	if (q != NULL) {
                /**
                 * Socket ID's selected - set up sockets table
                 */
                *p = '\0';
		*q = '\0';
                n = strlisttotab(++q, sockets, DIM(sockets));
                if (n == 0)
                        parse_error(s, "No socket ID specified");
                /**
                 * Check selected sockets are within range
                 */
                for (i = 0; i < n; i++)
                        if (sockets[i] >= PQOS_MAX_SOCKETS)
                                parse_error(s, "Socket ID out of range");
                sp = sockets;
	} else
                *p = '\0';
        /**
	 * Determine selected CAT type (L3/L2)
	 */
	if (strcasecmp(str, "llc") == 0)
		type = L3CA;
	else if (strcasecmp(str, "l2") == 0)
		type = L2CA;
	else
		parse_error(s, "Unrecognized allocation type");

	/**
         * Parse COS masks and apply to selected sockets
         */
        for (++p; ; p = NULL) {
                char *token = NULL;

                token = strtok_r(p, ",", &saveptr);
                if (token == NULL)
                        break;
                parse_allocation_cos(token, sp, n, type);
        }
        free(s);
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

        for (i = 0; i < sel_cat_assoc_num; i++) {
                ret = pqos_alloc_assoc_set(sel_cat_assoc_tab[i].core,
                                           sel_cat_assoc_tab[i].class_id);
                ASSERT(ret == PQOS_RETVAL_OK);
                if (ret != PQOS_RETVAL_OK) {
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
                const struct pqos_capability *cap_l2ca,
                unsigned sock_count, unsigned *sockets)
{
        if (cap_l3ca != NULL || cap_l2ca != NULL) {
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
                if (sel_cat_assoc_num > 0 || sel_cat_cos_num > 0) {
                        printf("Allocation capability not detected!\n");
                        return -1;
                }
        }

        return 0;
}
