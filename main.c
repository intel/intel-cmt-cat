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
 *  version: CMT_CAT_Refcode.L.0.1.3-10
 */

/**
 * @brief Platform QoS utility module
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#include <ctype.h>                                      /**< isspace() */

#include <sys/types.h>                                  /**< open() */
#include <sys/stat.h>
#include <sys/ioctl.h>                                  /**< terminal ioctl */

#include <sys/time.h>                                   /** gettimeofday() */
#include <time.h>                                       /** localtime() */
#include <fcntl.h>

#include "pqos.h"
#include "profiles.h"

#ifdef DEBUG
#include <assert.h>
#endif

/**
 * Macros
 */

#ifndef DIM
#define DIM(x) (sizeof(x)/sizeof(x[0]))
#endif

#ifdef DEBUG
#define ASSERT assert
#else
#define ASSERT(x)
#endif

#define PQOS_MAX_SOCKETS      2
#define PQOS_MAX_SOCKET_CORES 64
#define PQOS_MAX_CORES        (PQOS_MAX_SOCKET_CORES*PQOS_MAX_SOCKETS)

/**
 * Local data structures
 *
 */

const char *xml_root_open = "<records>";
const char *xml_root_close = "</records>";
const char *xml_child_open = "<record>";
const char *xml_child_close = "</record>";
const long xml_root_close_size = DIM("</records>") - 1;

/**
 * Free RMID's being in use
 */
int sel_free_in_use_rmid = 0;

/**
 * Reset CAT configuration
 */
static int sel_reset_CAT = 0;

/**
 * Number of cores that are selected in config string
 * for monitoring LLC occupancy
 */
static int sel_monitor_num = 0;

/**
 * The mask to tell which events to display
 */
static enum pqos_mon_event sel_events_max = 0;

/**
 * Maintains a table of core, event, number of events that are selected in
 * config string for monitoring LLC occupancy
 */
static struct {
        unsigned core;
        struct pqos_mon_data *pgrp;
        enum pqos_mon_event events;
} sel_monitor_tab[PQOS_MAX_CORES];

static struct pqos_mon_data m_mon_grps[PQOS_MAX_CORES];

/**
 * Maintains number of Class of Services supported by socket for
 * L3 cache allocation
 */
static int sel_l3ca_cos_num = 0;

/**
 * Maintains table for L3 cache allocation class of service data structure
 */
static struct pqos_l3ca sel_l3ca_cos_tab[PQOS_MAX_L3CA_COS];

/**
 * Number of cores selected for cache allocation association
 */
static int sel_l3ca_assoc_num = 0;

/**
 * Maintains a table of core and class_id that are selected in config string for
 * setting up allocation policy per core
 */
static struct {
        unsigned core;
        unsigned class_id;
} sel_l3ca_assoc_tab[PQOS_MAX_CORES];

/**
 * Maintains monitoring interval that is selected in config string for
 * monitoring L3 occupancy
 */
static int sel_mon_interval = 10; /**< 10 = 10x100ms = 1s */

/**
 * Maintains TOP like output that is selected in config string for
 * monitoring L3 occupancy
 */
static int sel_mon_top_like = 0;

/**
 * Maintains monitoring time that is selected in config string for
 * monitoring L3 occupancy
 */
static int sel_timeout = -1;

/**
 * Enable showing cache allocation settings
 */
static int sel_show_allocation_config = 0;

/**
 * Maintains selected monitoring output file name
 */
static char *sel_output_file = NULL;

/**
 * Maintains selected type of monitoring output file
 */
static char *sel_output_type = NULL;

/**
 * Maintains pointer to selected log file name
 */
static char *sel_log_file = NULL;

/**
 * Maintains pointer to selected config file
 */
static char *sel_config_file = NULL;

/**
 * Maintains pointer to allocation profile from internal DB
 */
static char *sel_allocation_profile = NULL;

/**
 * Maintains verbose mode choice selected in config string
 */
static int sel_verbose_mode = 0;

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

        ASSERT(s != NULL);

        if (strncasecmp(s, "0x", 2) == 0) {
                base = 16;
                s += 2;
        }

        n = strtoull(s, &endptr, base);

        if ((endptr != NULL) && (*endptr != '\0') && !isspace(*endptr)) {
                printf("Error converting '%s' to unsigned number!\n", str);
                exit(EXIT_FAILURE);
        }

        return n;
}

/**
 * @brief Converts string of characters representing list of
 *        numbers into table of numbers.
 *
 * Allowed formats are:
 *     0,1,2,3
 *     0-10,20-18
 *     1,3,5-8,10,0x10-12
 *
 * Numbers can be in decimal or hexadecimal format.
 *
 * On error, this functions causes process to exit with FAILURE code.
 *
 * @param s string representing list of unsigned numbers.
 * @param tab table to put converted numeric values into
 * @param max maximum number of elements that \a tab can accommodate
 *
 * @return Number of elements placed into \a tab
 */
static unsigned
strlisttotab(char *s, uint64_t *tab, const unsigned max)
{
        unsigned index = 0;
        char *saveptr = NULL;

        if (s == NULL || tab == NULL || max == 0)
                return index;

        for (;;) {
                char *p = NULL;
                char *token = NULL;

                token = strtok_r(s, ",", &saveptr);
                if (token == NULL)
                        break;

                s = NULL;

                p = strchr(token, '-');
                if (p != NULL) {
                        /**
                         * range of numbers provided
                         * example: 1-5 or 12-9
                         */
                        uint64_t n, start, end;
                        *p = '\0';
                        start = strtouint64(token);
                        end = strtouint64(p+1);
                        if (start > end) {
                                /**
                                 * no big deal just swap start with end
                                 */
                                n = start;
                                start = end;
                                end = n;
                        }
                        for (n = start; n <= end; n++) {
                                tab[index] = n;
                                index++;
                                if (index >= max)
                                        return index;
                        }
                } else {
                        /**
                         * single number provided here
                         */
                        tab[index] = strtouint64(token);
                        index++;
                        if (index >= max)
                                return index;
                }
        }

        return index;
}

/**
 * @brief Common function to handle string parsing errors
 *
 * On error, this function causes process to exit with FAILURE code.
 *
 * @param arg string that caused error when parsing
 * @param note context and information about encountered error
 */
static void
parse_error(const char *arg, const char *note)
{
        printf("Error parsing \"%s\" command line argument. %s\n",
               arg ? arg : "<null>",
               note ? note : "");
        exit(EXIT_FAILURE);
}

/**
 * @brief Verifies and translates monitoring config string into
 *        internal monitoring configuration.
 *
 * @param str string passed to -m command line option
 */
static void
parse_monitor_event(char *str)
{
        uint64_t cores[PQOS_MAX_CORES];
        unsigned i = 0, n = 0;
        enum pqos_mon_event evt = 0;

        if (strncasecmp(str, "llc:", 4) == 0)
		evt = PQOS_MON_EVENT_L3_OCCUP;
        else if (strncasecmp(str, "mbr:", 4) == 0)
                evt = PQOS_MON_EVENT_RMEM_BW;
        else if (strncasecmp(str, "mbl:", 4) == 0)
                evt = PQOS_MON_EVENT_LMEM_BW;
        else
                parse_error(str, "Unrecognized monitoring event type");

        /**
         * This is to tell which events to display (out of all possible)
         */
        sel_events_max |= evt;

        n = strlisttotab(strchr(str, ':') + 1, cores, DIM(cores));

        if (n == 0)
                parse_error(str, "No cores selected for an event");

        if (n >= DIM(sel_monitor_tab))
                parse_error(str,
                            "too many cores selected "
                            "for monitoring");

        /**
         *  For each core we are processing:
         *  - if it's already there in the sel_monitor_tab - update the entry
         *  - else - add it to the sel_monitor_tab
         */
        for (i = 0; i < n; i++) {
                unsigned found;
                int j;

                for (found = 0, j = 0; j < sel_monitor_num && found == 0; j++) {
                        if (sel_monitor_tab[j].core == (unsigned) cores[i]) {
                                sel_monitor_tab[j].events |= evt;
                                found = 1;
                        }
                }
                if (!found) {
                        sel_monitor_tab[sel_monitor_num].core =
                                (unsigned) cores[i];
                        sel_monitor_tab[sel_monitor_num].events = evt;
                        sel_monitor_tab[sel_monitor_num].pgrp =
                                &m_mon_grps[sel_monitor_num];
                        ++sel_monitor_num;
                }
        }
        return;
}

/**
 * @brief Verifies and translates multiple monitoring config strings into
 *        internal monitoring configuration.
 *
 * @param str string passed to -m command line option
 */
static void
selfn_monitor_events(const char *arg)
{
        char *cp = NULL, *str = NULL;
        char *saveptr = NULL;

        if (arg == NULL)
                parse_error(arg, "NULL pointer!");

        if (strlen(arg) <= 0)
                parse_error(arg, "Empty string!");

        /**
         * The parser will add to the display only necessary columns
         */
        sel_events_max = 0;

        cp = strdup(arg);
        ASSERT(cp != NULL);

        for (str = cp; ; str = NULL) {
                char *token = NULL;

                token = strtok_r(str, ";", &saveptr);
                if (token == NULL)
                        break;
                parse_monitor_event(token);
        }

        free(cp);
}

/**
 * @brief Starts monitoring on selected cores
 *
 * @param cpu_info cpu information structure
 *
 * @return Operation status
 * @retval 0 OK
 * @retval -1 error
 */
static int
setup_monitoring(const struct pqos_cpuinfo *cpu_info,
                 const struct pqos_capability const *cap_mon)
{
        unsigned i;
        int ret;

        if (sel_monitor_num == 0) {
                /**
                 * no cores and events selected through command line
                 * by default let's monitor all cores
                 */

                for (i = 0; (unsigned)i < cap_mon->u.mon->num_events; i++)
                        sel_events_max |= (cap_mon->u.mon->events[i].type);

                for (i = 0; i < cpu_info->num_cores; i++) {
                        unsigned lcore = cpu_info->cores[i].lcore;

                        sel_monitor_tab[sel_monitor_num].core = lcore;
                        sel_monitor_tab[sel_monitor_num].events =
                                sel_events_max;
                        sel_monitor_tab[sel_monitor_num].pgrp =
                                &m_mon_grps[sel_monitor_num];
                        sel_monitor_num++;
                }
        }

        for (i = 0; i < (unsigned)sel_monitor_num; i++) {
                unsigned lcore = sel_monitor_tab[i].core;

                ret = pqos_mon_start(1, &lcore,
                                     sel_monitor_tab[i].events,
                                     NULL,
                                     sel_monitor_tab[i].pgrp);
                ASSERT(ret == PQOS_RETVAL_OK);

                /**
                 * The error raised also if two instances of PQoS
                 *attempt to use the same core id.
                 */
                if (ret != PQOS_RETVAL_OK) {
                        printf("Monitoring start error on core %u, status %d\n",
                               lcore, ret);
                        return -1;
                }
	}
        return 0;
}

/**
 * @brief Stops monitoring on selected cores
 *
 */
static void
stop_monitoring(void)
{
        unsigned i;
        int ret;

        for (i = 0; i < (unsigned)sel_monitor_num; i++) {
                ret = pqos_mon_stop(&m_mon_grps[i]);
                ASSERT(ret == PQOS_RETVAL_OK);
                if (ret != PQOS_RETVAL_OK)
                        printf("Monitoring stop error!\n");
        }
}

/**
 * @brief Sets up allocation classes of service on selected CPU sockets
 *
 * @param sock_count number of CPU sockets
 * @param sockets arrays with CPU socket id's
 *
 * @return Number of classes of service set
 * @retval 0 no class of service set (nor selected)
 * @retval negative error
 * @retval positive success
 */
static int
set_allocation_class(unsigned sock_count,
                     const unsigned *sockets)
{
        int ret;

        while (sock_count > 0 && sel_l3ca_cos_num > 0) {
                ret = pqos_l3ca_set(*sockets,
                                    sel_l3ca_cos_num,
                                    sel_l3ca_cos_tab);
                ASSERT(ret == PQOS_RETVAL_OK);
                if (ret != PQOS_RETVAL_OK) {
                        printf("Setting up cache allocation class of service "
                               "failed!\n");
                        return -1;
                }
                sock_count--;
                sockets++;
        }

        return sel_l3ca_cos_num;
}

/**
 * @brief Verifies and translates definition of single
 *        allocation class of service
 *        from text string into internal configuration.
 *
 * @param str fragment of string passed to -e command line option
 */
static void
parse_allocation_cos(char *str)
{
        char *p = NULL;
        unsigned class_id = 0;
        uint64_t mask = 0;
        int j = 0;

        p = strchr(str, '=');
        if (p == NULL)
                parse_error(str, "invalid class of service definition");
        *p = '\0';

        class_id = (unsigned) strtouint64(str);
        mask = strtouint64(p+1);

        if (sel_l3ca_cos_num <= 0) {
                sel_l3ca_cos_tab[0].class_id = class_id;
                sel_l3ca_cos_tab[0].ways_mask = mask;
                sel_l3ca_cos_num = 1;
                return;
        }

        for (j = 0; j < sel_l3ca_cos_num; j++)
                if (sel_l3ca_cos_tab[j].class_id == class_id)
                        break;

        if (j < sel_l3ca_cos_num) {
                /**
                 * this class is already on the list
                 * - update mask but warn about it
                 */
                printf("warn: updating COS %u definition from mask "
                       "0x%llx to 0x%llx\n",
                       class_id,
                       (long long) sel_l3ca_cos_tab[j].ways_mask,
                       (long long) mask);
                sel_l3ca_cos_tab[j].ways_mask = mask;
        } else {
                /**
                 * New class selected - extend the list
                 */
                unsigned k = (unsigned) sel_l3ca_cos_num;

                if (k >= DIM(sel_l3ca_cos_tab))
                        parse_error(str,
                                    "too many allocation classes selected");

                sel_l3ca_cos_tab[k].class_id = class_id;
                sel_l3ca_cos_tab[k].ways_mask = mask;
                sel_l3ca_cos_num++;
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

        if (strncasecmp(str, "llc:", 4) != 0)
                parse_error(str, "Unrecognized allocation type");

        for (p = str+strlen("llc:"); ; p = NULL) {
                char *token = NULL;

                token = strtok_r(p, ",", &saveptr);
                if (token == NULL)
                        break;
                parse_allocation_cos(token);
        }
}

/**
 * @brief Defines allocation class of service
 *
 * @param arg string passed to -e command line option
 */
static void
selfn_allocation_class(const char *arg)
{
        char *cp = NULL, *str = NULL;
        char *saveptr = NULL;

        if (arg == NULL)
                parse_error(arg, "NULL pointer!");

        if (strlen(arg) <= 0)
                parse_error(arg, "Empty string!");

        cp = strdup(arg);
        ASSERT(cp != NULL);

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
                ret = pqos_l3ca_assoc_set(sel_l3ca_assoc_tab[i].core,
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

/**
 * @brief Associates cores with selected class of service
 *
 * @param arg string passed to -a command line option
 */
static void
selfn_allocation_assoc(const char *arg)
{
        char *cp = NULL, *str = NULL;
        char *saveptr = NULL;

        if (arg == NULL)
                parse_error(arg, "NULL pointer!");

        if (strlen(arg) <= 0)
                parse_error(arg, "Empty string!");

        cp = strdup(arg);
        ASSERT(cp != NULL);

        for (str = cp; ; str = NULL) {
                char *token = NULL;

                token = strtok_r(str, ";", &saveptr);
                if (token == NULL)
                        break;
                parse_allocation_assoc(token);
        }

        free(cp);
}

/**
 * @brief Prints information about cache allocation settings in the system
 *
 * @param sock_count number of detected CPU sockets
 * @param sockets arrays with detected CPU socket id's
 * @param cpu_info cpu information structure
 */
static void
print_allocation_config(const struct pqos_capability *cap_mon,
			const struct pqos_capability *cap_l3ca,
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
                        printf("    L3CA COS%u => MASK 0x%llx\n",
                               tab[n].class_id,
                               (unsigned long long)tab[n].ways_mask);
                }
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
                        int ret1 = PQOS_RETVAL_OK;
                        int ret2 = PQOS_RETVAL_OK;

			if (cap_l3ca != NULL)
				ret1 = pqos_l3ca_assoc_get(lcores[n],
                                                           &class_id);
			if (cap_mon != NULL)
				ret2 = pqos_mon_assoc_get(lcores[n], &rmid);

                        if (ret1 == PQOS_RETVAL_OK && ret2 == PQOS_RETVAL_OK) {
				if (cap_l3ca != NULL && cap_mon != NULL)
					printf("    Core %u => COS%u, RMID%u\n",
					       lcores[n], class_id,
                                               (unsigned)rmid);
				if (cap_l3ca == NULL && cap_mon != NULL)
					printf("    Core %u => RMID%u\n",
					       lcores[n], (unsigned)rmid);
				if (cap_l3ca != NULL && cap_mon == NULL)
					printf("    Core %u => COS%u\n",
					       lcores[n], class_id);
                        } else {
                                printf("    Core %u => ERROR\n", lcores[n]);
                        }
                }
        }
}

/**
 * @brief Selects monitoring time
 *
 * @param arg string passed to -t command line option
 */
static void
selfn_monitor_time(const char *arg)
{
        if (!strcasecmp(arg, "inf") || !strcasecmp(arg, "infinite"))
                sel_timeout = -1; /**< infinite timeout */
        else
                sel_timeout = (int) strtouint64(arg);
}

/**
 * @brief Duplicates \a arg and stores at \a sel
 *
 * @param sel place to store duplicate of \a arg
 * @param arg string passed through command line option
 */
static void
selfn_strdup(char **sel, const char *arg)
{
        ASSERT(sel != NULL && arg != NULL);
        if (*sel != NULL) {
                free(*sel);
                *sel = NULL;
        }
        *sel = strdup(arg);
}

/**
 * @brief Selects monitoring output file
 *
 * @param arg string passed to -o command line option
 */
static void
selfn_monitor_file(const char *arg)
{
        selfn_strdup(&sel_output_file, arg);
}

/**
 * @brief Selects type of monitoring output file
 *
 * @param arg string passed to -u command line option
 */
static void
selfn_monitor_file_type(const char *arg)
{
        selfn_strdup(&sel_output_type, arg);
}

/**
 * @brief Selects monitoring interval
 *
 * @param arg string passed to -i command line option
 */
static void
selfn_monitor_interval(const char *arg)
{
        sel_mon_interval = (int) strtouint64(arg);
}

/**
 * @brief Selects top-like monitoring format
 *
 * @param arg not used
 */
static void
selfn_monitor_top_like(const char *arg)
{
        arg = arg;
        sel_mon_top_like = 1;
}

/**
 * @brief Selects allocation profile from internal DB
 *
 * @param arg string passed to -c command line option
 */
static void
selfn_allocation_select(const char *arg)
{
        selfn_strdup(&sel_allocation_profile, arg);
}

/**
 * @brief Selects log file
 *
 * @param arg string passed to -l command line option
 */
static void
selfn_log_file(const char *arg)
{
        selfn_strdup(&sel_log_file, arg);
}

/**
 * @brief Selects verbose mode on
 *
 * @param arg not used
 */
static void
selfn_verbose_mode(const char *arg)
{
        arg = arg;
        sel_verbose_mode = 1;
}

/**
 * @brief Selects showing allocation settings
 *
 * @param arg not used
 */
static void
selfn_show_allocation(const char *arg)
{
        arg = arg;
        sel_show_allocation_config = 1;
}

/**
 * @brief Opens configuration file and parses its contents
 *
 * @param fname Name of the file with configuration parameters
 */
static void
parse_config_file(const char *fname)
{
        static const struct {
                const char *option;
                void (*fn)(const char *);
        } optab[] = {
                { "show-alloc:",            selfn_show_allocation },  /**< -s */
                { "log-file:",              selfn_log_file },         /**< -l */
                { "verbose-mode:",          selfn_verbose_mode },     /**< -v */
                { "alloc-class-set:",       selfn_allocation_class }, /**< -e */
                { "alloc-assoc-set:",       selfn_allocation_assoc }, /**< -a */
                { "alloc-class-select:",    selfn_allocation_select },/**< -c */
                { "monitor-select-events:", selfn_monitor_events },   /**< -m */
                { "monitor-time:",          selfn_monitor_time },     /**< -t */
                { "monitor-interval:",      selfn_monitor_interval }, /**< -i */
                { "monitor-file:",          selfn_monitor_file },     /**< -o */
                { "monitor-file-type:",     selfn_monitor_file_type },/**< -u */
                { "monitor-top-like:",      selfn_monitor_top_like }, /**< -T */
        };
        FILE *fp = NULL;
        char cb[256];

        fp = fopen(fname, "r");
        if (fp == NULL)
                parse_error(fname, "cannot open configuration file!");

        memset(cb, 0, sizeof(cb));

        while (fgets(cb, sizeof(cb)-1, fp) != NULL) {
                int i, j, remain;
                char *cp = NULL;

                for (j = 0; j < (int)sizeof(cb)-1; j++)
                        if (!isspace(cb[j]))
                                break;

                if (j >= (int)(sizeof(cb)-1))
                        continue; /**< blank line */

                if (strlen(cb+j) == 0)
                        continue; /**< blank line */

                if (cb[j] == '#')
                        continue; /**< comment */

                cp = cb+j;
                remain = strlen(cp);

                /**
                 * remove trailing white spaces
                 */
                for (i = strlen(cp)-1; i > 0; i--)
                        if (!isspace(cp[i])) {
                                cp[i+1] = '\0';
                                break;
                        }

                for (i = 0; i < (int)DIM(optab); i++) {
                        int len = strlen(optab[i].option);

                        if (len > remain)
                                continue;

                        if (strncasecmp(cp, optab[i].option, len) != 0)
                                continue;

                        while (isspace(cp[len]))
                                len++; /**< skip space characters */

                        optab[i].fn(cp+len);
                        break;
                }

                if (i >= (int)DIM(optab))
                        parse_error(cp,
                                    "Unrecognized configuration file command");
        }

        fclose(fp);
}

/**
 * @brief Compare LLC occupancy in two monitoring data sets
 *
 * @param a monitoring data A
 * @param b monitoring data B
 *
 * @return LLC monitoring data compare status for descending order
 * @retval 0 if \a  = \b
 * @retval >0 if \b > \a
 * @retval <0 if \b < \a
 */
static int
mon_qsort_llc_cmp_desc(const void *a, const void *b)
{
        const struct pqos_mon_data *ap = (const struct pqos_mon_data *)a;
        const struct pqos_mon_data *bp = (const struct pqos_mon_data *)b;
        /**
         * This (b-a) is to get descending order
         * otherwise it would be (a-b)
         */
        return (int) (((int64_t)bp->values.llc) - ((int64_t)ap->values.llc));
}

/**
 * @brief Compare core id in two monitoring data sets
 *
 * @param a monitoring data A
 * @param b monitoring data B
 *
 * @return Core id compare status for ascending order
 * @retval 0 if \a  = \b
 * @retval >0 if \b > \a
 * @retval <0 if \b < \a
 */
static int
mon_qsort_coreid_cmp_asc(const void *a, const void *b)
{
        const struct pqos_mon_data *ap = (const struct pqos_mon_data *)a;
        const struct pqos_mon_data *bp = (const struct pqos_mon_data *)b;
        /**
         * This (a-b) is to get ascending order
         * otherwise it would be (b-a)
         */
        return (int)ap->cores[0] - (int)bp->cores[0];
}



/**
 * Stop monitoring indicator for infinite monitoring loop
 */
static int stop_monitoring_loop = 0;

/**
 * @brief CTRL-C handler for infinite monitoring loop
 *
 * @param signo signal number
 */
static void monitoring_ctrlc(int signo)
{
        signo = signo;
        stop_monitoring_loop = 1;
}

/**
 * @brief Gets scale factors to display events data
 *
 * LLC factor is scaled to kilobytes (1024 bytes = 1KB)
 * MBM factors are scaled to megabytes / s (1024x1024 bytes = 1MB)
 *
 * @param cap capability structure
 * @param llc_factor cache occupancy monitoring data
 * @param mbr_factor remote memory bandwidth monitoring data
 * @param mbl_factor local memory bandwidth monitoring data
 * @return operation status
 * @retval PQOS_RETVAL_OK on success
 */
static int
get_event_factors(const struct pqos_cap * const cap,
                  double * const llc_factor,
                  double * const mbr_factor,
                  double * const mbl_factor)
{
        const struct pqos_monitor *l3mon = NULL, *mbr_mon = NULL,
                *mbl_mon = NULL;
        int ret = PQOS_RETVAL_OK;

        if ((cap == NULL) || (llc_factor == NULL) ||
            (mbr_factor == NULL) || (mbl_factor == NULL))
                return PQOS_RETVAL_PARAM;

        if (sel_events_max & PQOS_MON_EVENT_L3_OCCUP) {
                ret = pqos_cap_get_event(cap, PQOS_MON_EVENT_L3_OCCUP, &l3mon);
                if (ret != PQOS_RETVAL_OK) {
                        printf("Failed to obtain LLC occupancy event data!\n");
                        return PQOS_RETVAL_ERROR;
                }
                *llc_factor = ((double)l3mon->scale_factor) / 1024.0;
        } else {
                *llc_factor = 1.0;
        }

        if (sel_events_max & PQOS_MON_EVENT_RMEM_BW) {
                ret = pqos_cap_get_event(cap, PQOS_MON_EVENT_RMEM_BW, &mbr_mon);
                if (ret != PQOS_RETVAL_OK) {
                        printf("Failed to obtain MBR event data!\n");
                        return PQOS_RETVAL_ERROR;
                }
                *mbr_factor = ((double) mbr_mon->scale_factor) /
                        (1024.0*1024.0);
        } else {
                *mbr_factor = 1.0;
        }

        if (sel_events_max & PQOS_MON_EVENT_LMEM_BW) {
                ret = pqos_cap_get_event(cap, PQOS_MON_EVENT_LMEM_BW, &mbl_mon);
                if (ret != PQOS_RETVAL_OK) {
                        printf("Failed to obtain MBL occupancy event data!\n");
                        return PQOS_RETVAL_ERROR;
                }
                *mbl_factor = ((double)mbl_mon->scale_factor) /
                        (1024.0*1024.0);
        } else {
                *mbl_factor = 1.0;
        }

        return PQOS_RETVAL_OK;
}

/**
 * @brief Fills in single text column in the monitoring table
 *
 * @param val numerical value to be put into the column
 * @param data place to put formatted column into
 * @param sz_data available size for the column
 * @param is_monitored if true then \a val holds valid data
 * @param is_column_present if true then corresponding event is
 *        selected for display
 * @return Number of characters added to \a data excluding NULL
 */
static size_t
fillin_text_column(const double val, char data[], const size_t sz_data,
                   const int is_monitored, const int is_column_present)
{
        const char blank_column[] = "          ";
        size_t offset = 0;

        if (is_monitored) {
                /**
                 * This is monitored and we have the data
                 */
                snprintf(data, sz_data - 1, "%10.1f", val);
                offset = strlen(data);
        } else if (is_column_present) {
                /**
                 * the column exists though there's no data
                 */
                strncpy(data, blank_column, sz_data - 1);
                offset = strlen(data);
        }

        return offset;
}

/**
 * @brief Fills in text row in the monitoring table
 *
 * @param data the table to store the data row
 * @param sz_data the size of the table
 * @param mon_event events selected in monitoring group
 * @param llc LLC occupancy data
 * @param mbr remote memory bandwidth data
 * @param mbl local memory bandwidth data
 */
static void
fillin_text_row(char data[], const size_t sz_data,
                const enum pqos_mon_event mon_event,
                const double llc, const double mbr,
                const double mbl)
{
        size_t offset = 0;

        ASSERT(sz_data >= 64);

        memset(data, 0, sz_data);

        offset += fillin_text_column(llc, data + offset, sz_data - offset,
                                     mon_event & PQOS_MON_EVENT_L3_OCCUP,
                                     sel_events_max & PQOS_MON_EVENT_L3_OCCUP);

        offset += fillin_text_column(mbl, data + offset, sz_data - offset,
                                     mon_event & PQOS_MON_EVENT_LMEM_BW,
                                     sel_events_max & PQOS_MON_EVENT_LMEM_BW);

        offset += fillin_text_column(mbr, data + offset, sz_data - offset,
                                     mon_event & PQOS_MON_EVENT_RMEM_BW,
                                     sel_events_max & PQOS_MON_EVENT_RMEM_BW);
}

/**
 * @brief Fills in single XML column in the monitoring table
 *
 * @param val numerical value to be put into the column
 * @param data place to put formatted column into
 * @param sz_data available size for the column
 * @param is_monitored if true then \a val holds valid data
 * @param is_column_present if true then corresponding event is
 *        selected for display
 * @param node_name defines XML node name for the column
 * @return Number of characters added to \a data excluding NULL
 */
static size_t
fillin_xml_column(const double val, char data[], const size_t sz_data,
                  const int is_monitored, const int is_column_present,
                  const char node_name[])
{
        size_t offset = 0;

        if (is_monitored) {
                /**
                 * This is monitored and we have the data
                 */
                snprintf(data, sz_data - 1, "\t<%s>%.1f</%s>\n",
                         node_name, val, node_name);
                offset = strlen(data);
        } else if (is_column_present) {
                /**
                 * the column exists though there's no data
                 */
                snprintf(data, sz_data - 1, "\t<%s></%s>\n",
                         node_name, node_name);
                offset = strlen(data);
        }

        return offset;
}

/**
 * @brief Fills in the row in the XML file with the monitoring data
 *
 * @param data the table to store the data row
 * @param sz_data the size of the table
 * @param mon_event events selected in monitoring group
 * @param llc LLC occupancy data
 * @param mbr remote memory bandwidth data
 * @param mbl local memory bandwidth data
 */
static void
fillin_xml_row(char data[], const size_t sz_data,
               const enum pqos_mon_event mon_event,
               const double llc, const double mbr,
               const double mbl)
{
        size_t offset = 0;

        ASSERT(sz_data >= 128);

        memset(data, 0, sz_data);

        offset += fillin_xml_column(llc, data + offset, sz_data - offset,
                                    mon_event & PQOS_MON_EVENT_L3_OCCUP,
                                    sel_events_max & PQOS_MON_EVENT_L3_OCCUP,
                                    "l3_occupancy_kB");

        offset += fillin_xml_column(mbl, data + offset, sz_data - offset,
                                     mon_event & PQOS_MON_EVENT_LMEM_BW,
                                     sel_events_max & PQOS_MON_EVENT_LMEM_BW,
                                    "mbm_local_MB");

        offset += fillin_xml_column(mbr, data + offset, sz_data - offset,
                                    mon_event & PQOS_MON_EVENT_RMEM_BW,
                                    sel_events_max & PQOS_MON_EVENT_RMEM_BW,
                                    "mbm_remote_MB");
}

/**
 * @brief Reads monitoring event data at given \a interval for \a sel_time time span
 *
 * @param fp FILE to be used for storing monitored data
 * @param sel_time time span to monitor data for, negative value indicates infinite time
 * @param interval time interval to gather data and print to the file, this is in 100ms units
 * @param top_mode output style similar to top utility
 * @param cap detected PQoS capabilities
 * @param output_type text or XML output file
 */
static void
monitoring_loop(FILE *fp,
                const int sel_time,
                long interval,
                const int top_mode,
                const struct pqos_cap *cap,
                const char *output_type)
{
#define TERM_MIN_NUM_LINES 3

        double llc_factor = 1, mbr_factor = 1, mbl_factor = 1;
        struct timeval tv_start;
        int ret = PQOS_RETVAL_OK;
        int istty = 0;
        unsigned max_lines = 0;
        const int istext = !strcasecmp(output_type, "text");

        /* for the dynamic display */
        const size_t sz_header = 128, sz_data = 128;
        char *header = NULL;
        char data[sz_data];

        if ((!istext)  && (strcasecmp(output_type, "xml") != 0)) {
                printf("Invalid selection of output file type '%s'!\n",
                       output_type);
                return;
        }

        ret = get_event_factors(cap, &llc_factor, &mbr_factor, &mbl_factor);
        if (ret != PQOS_RETVAL_OK) {
                printf("Error in retrieving monitoring scale factors!\n");
                return;
        }

        /**
         * capture ctrl-c to gracefully stop the loop
         */
        if (signal(SIGINT, monitoring_ctrlc) == SIG_ERR)
                printf("Failed to catch SIGINT!\n");
        if (signal(SIGHUP, monitoring_ctrlc) == SIG_ERR)
                printf("Failed to catch SIGHUP!\n");

        istty = isatty(fileno(fp));

        if (istty) {
                struct winsize w;

                if (ioctl(fileno(fp), TIOCGWINSZ, &w) != -1) {
                        max_lines = w.ws_row;
                        if (max_lines < TERM_MIN_NUM_LINES)
                                max_lines = TERM_MIN_NUM_LINES;
                }
        }

        /**
         * A coefficient to display the data as MB / s
         */
        double coeff = 10.0 / (double)interval;

        /**
         * Interval is passed in  100[ms] units
         * This converts interval to microseconds
         */
        interval = interval * 100000LL;

        gettimeofday(&tv_start, NULL);

        /**
         * Build the header
         */
        if (istext) {
                header = (char *) alloca(sz_header);
                if (header == NULL) {
                        printf("Failed to allocate stack frame memory!\n");
                        return;
                }
                memset(header, 0, sz_header);
                strncpy(header, "SOCKET     CORE     RMID", sz_header - 1);
                if (sel_events_max & PQOS_MON_EVENT_L3_OCCUP)
                        strncat(header, "    LLC[KB]",
                                sz_header - strlen(header) - 1);
                if (sel_events_max & PQOS_MON_EVENT_LMEM_BW)
                        strncat(header, " MBL[MB/s]",
                                sz_header - strlen(header) - 1);
                if (sel_events_max & PQOS_MON_EVENT_RMEM_BW)
                        strncat(header, " MBR[MB/s]",
                                sz_header - strlen(header) - 1);
        }

        while (!stop_monitoring_loop) {
                struct pqos_mon_data mon_data[PQOS_MAX_CORES];
                unsigned mon_number = (unsigned) sel_monitor_num;
                struct timeval tv_s, tv_e;
                struct tm *ptm = NULL;
                unsigned i = 0;
                struct timespec req, rem;
                long usec_start = 0, usec_end = 0, usec_diff = 0;
                char cb_time[64];

                gettimeofday(&tv_s, NULL);

                ret = pqos_mon_poll(m_mon_grps, (unsigned) sel_monitor_num);
                if (ret != PQOS_RETVAL_OK) {
                        printf("Failed to poll monitoring data!\n");
                        return;
                }

                memcpy(mon_data, m_mon_grps,
                       sel_monitor_num * sizeof(m_mon_grps[0]));

                if (istty)
                        fprintf(fp, "\033[2J");   /**< clear screen */

                ptm = localtime(&tv_s.tv_sec);
                if (ptm != NULL) {
                        /**
                         * Print time
                         */
                        strftime(cb_time, DIM(cb_time)-1,
                                 "%Y-%m-%d %H:%M:%S", ptm);

                        if (istty)
                                fprintf(fp, "\033[0;0H"); /**< Move to
                                                             position 0:0 */
                        if (istext)
                                fprintf(fp, "TIME %s\n", cb_time);
                } else {
                        strncpy(cb_time, "error", DIM(cb_time)-1);
                }

                if (top_mode) {
                        qsort(mon_data, mon_number, sizeof(mon_data[0]),
                              mon_qsort_llc_cmp_desc);
                } else {
                        qsort(mon_data, mon_number, sizeof(mon_data[0]),
                              mon_qsort_coreid_cmp_asc);
                }

                if (max_lines > 0) {
                        if ((mon_number+TERM_MIN_NUM_LINES-1) > max_lines)
                                mon_number = max_lines - TERM_MIN_NUM_LINES + 1;
                }

                if (istext)
                        fputs(header, fp);

                for (i = 0; i < mon_number; i++) {
                        double llc = ((double)mon_data[i].values.llc) *
                                llc_factor;
                        double mbr =
                                ((double)mon_data[i].values.mbm_remote_delta) *
                                mbr_factor * coeff;
                        double mbl =
                                ((double)mon_data[i].values.mbm_local_delta) *
                                mbl_factor * coeff;

                        if (istext) {
                                /* Text */
                                fillin_text_row(data, sz_data,
                                                mon_data[i].event,
                                                llc, mbr, mbl);
                                fprintf(fp, "\n%6u %8u %8u %s",
                                        mon_data[i].socket,
                                        mon_data[i].cores[0],
                                        mon_data[i].rmid,
                                        data);
                        } else {
                                /* XML */
                                fillin_xml_row(data, sz_data, mon_data[i].event,
                                               llc, mbr, mbl);
                                fprintf(fp,
                                        "%s\n"
                                        "\t<time>%s</time>\n"
                                        "\t<socket>%u</socket>\n"
                                        "\t<core>%u</core>\n"
                                        "\t<rmid>%u</rmid>\n"
                                        "%s"
                                        "%s\n"
                                        "%s",
                                        xml_child_open,
                                        cb_time,
                                        mon_data[i].socket,
                                        mon_data[i].cores[0],
                                        mon_data[i].rmid,
                                        data,
                                        xml_child_close,
                                        xml_root_close);
                                fseek(fp, -xml_root_close_size, SEEK_CUR);
                        }
                }
                fflush(fp);

                /**
                 * Move to position 0:0
                 */
                if (istty)
                        fputs("\033[0;0", fp);

                gettimeofday(&tv_e, NULL);

                if (stop_monitoring_loop) {
                        if (istty)
                                fputs("\n", fp);
                        break;
                }

                /**
                 * Calculate microseconds to the nearest measurement interval
                 */
                usec_start = ((long)tv_s.tv_usec) +
                        ((long)tv_s.tv_sec*1000000L);
                usec_end = ((long)tv_e.tv_usec) + ((long)tv_e.tv_sec*1000000L);
                usec_diff = usec_end - usec_start;

                if (usec_diff < interval) {
                        memset(&rem, 0, sizeof(rem));
                        memset(&req, 0, sizeof(req));

                        req.tv_sec = (interval - usec_diff) / 1000000L;
                        req.tv_nsec = ((interval - usec_diff)%1000000L) *
                                1000L;
                        if (nanosleep(&req, &rem) == -1) {
                                /**
                                 * nanosleep interrupted by a signal
                                 */
                                req = rem;
                                memset(&rem, 0, sizeof(rem));
                                nanosleep(&req, &rem);
                        }
                }

                if (sel_time >= 0) {
                        gettimeofday(&tv_e, NULL);
                        if ((tv_e.tv_sec - tv_start.tv_sec) > sel_time)
                                break;
                }

        }

}

const char *m_cmd_name = "pqos";                          /**< command name */

/**
 * @brief Displays help information
 *
 */
static void print_help(void)
{
        printf("Usage: %s [-h] [-H]\n"
               "       %s [-f <config_file_name>]\n"
               "       %s [-l <log_file_name>]\n"
               "       %s [-m <event_type>:<list_of_cores>;...] "
               "[-t <time in sec>]\n"
               "          [-i <interval in 100ms>] [-T]\n"
               "          [-o <output_file>] [-u <output_type>] [-r]\n"
               "       %s [-e <allocation_type>:<class_num>=<class_definiton>;"
               "...]\n"
               "          [-c <allocation_type>:<profile_name>;...]\n"
               "          [-a <allocation_type>:<class_num>=<list_of_cores>;"
               "...]\n"
               "       %s [-R]\n"
               "       %s [-s]\n"
               "Notes:\n"
               "\t-h\thelp\n"
               "\t-v\tverbose mode\n"
               "\t-H\tlist of supported allocation profiles\n"
               "\t-f\tloads parameters from selected configuration file\n"
               "\t-l\tlogs messages into selected log file\n"
               "\t-e\tdefine allocation classes, example: \"llc:0=0xffff;"
               "llc:1=0x00ff;\"\n"
               "\t-c\tselect a profile of predefined allocation classes, "
               "see -H to list available profiles\n"
               "\t-a\tassociate cores with allocation classes, example: "
               "\"llc:0=0,2,4,6-10;llc:1=1\"\n"
               "\t-r\tuses all RMID's and cores in the system\n"
               "\t-R\tresets CAT configuration\n"
               "\t-s\tshow current cache allocation configuration\n"
               "\t-m\tselect cores and events for monitoring, example: "
               "\"llc:0,2,4-10;mbl:1,3;mbr:3,4\"\n"
               "\t-o\tselect output file to store monitored data in. "
               "stdout by default.\n"
               "\t-u\tselect output format type for monitored data. "
               "\"text\" (defualt) and \"xml\" are the options.\n"
               "\t-i\tdefine monitoring sampling interval, 1=100ms, "
               "default 10=10x100ms=1s\n"
               "\t-T\ttop like monitoring output\n"
               "\t-t\tdefine monitoring time (use 'inf' or 'infinite' for "
               "inifinite loop monitoring loop)\n",
               m_cmd_name, m_cmd_name, m_cmd_name, m_cmd_name, m_cmd_name,
               m_cmd_name, m_cmd_name);
}

int main(int argc, char **argv)
{
        struct pqos_config cfg;
        const struct pqos_cpuinfo *p_cpu = NULL;
        const struct pqos_cap *p_cap = NULL;
        const struct pqos_capability *cap_mon = NULL, *cap_l3ca = NULL;
        unsigned sock_count, sockets[PQOS_MAX_SOCKETS];
        int cmd, ret, exit_val = EXIT_SUCCESS;
        FILE *fp_monitor = NULL;

        m_cmd_name = argv[0];

        while ((cmd = getopt(argc, argv, "Hhf:i:m:Tt:l:o:u:e:c:a:srvR"))
               != -1) {
                switch (cmd) {
                case 'h':
                        print_help();
                        return EXIT_SUCCESS;
                case 'H':
                        profile_l3ca_list(stdout);
                        return EXIT_SUCCESS;
                case 'f':
                        if (sel_config_file != NULL) {
                                printf("Only one config file argument is "
                                       "accepted!\n");
                                return EXIT_FAILURE;
                        }
                        selfn_strdup(&sel_config_file, optarg);
                        parse_config_file(sel_config_file);
                        break;
                case 'i':
                        selfn_monitor_interval(optarg);
                        break;
                case 'm':
                        selfn_monitor_events(optarg);
                        break;
                case 't':
                        selfn_monitor_time(optarg);
                        break;
                case 'T':
                        selfn_monitor_top_like(NULL);
                        break;
                case 'l':
                        selfn_log_file(optarg);
                        break;
                case 'o':
                        selfn_monitor_file(optarg);
                        break;
                case 'u':
                        selfn_monitor_file_type(optarg);
                        break;
                case 'e':
                        selfn_allocation_class(optarg);
                        break;
                case 'r':
                        sel_free_in_use_rmid = 1;
                        break;
                case 'R':
                        sel_reset_CAT = 1;
                        break;
                case 'a':
                        selfn_allocation_assoc(optarg);
                        break;
                case 'c':
                        selfn_allocation_select(optarg);
                        break;
                case 's':
                        selfn_show_allocation(NULL);
                        break;
                case 'v':
                        selfn_verbose_mode(NULL);
                        break;
                default:
                        printf("Unsupported option: %c\n", optopt);
                case '?':
                        print_help();
                        return EXIT_SUCCESS;
                }
        }

        memset(&cfg, 0, sizeof(cfg));
        cfg.verbose = sel_verbose_mode;
        cfg.free_in_use_rmid = sel_free_in_use_rmid;

        /**
         * Check output file type
         */
        if (sel_output_type == NULL)
                sel_output_type = strdup("text");

        if (sel_output_type == NULL) {
                printf("Memory allocation error!\n");
                exit_val = EXIT_FAILURE;
                goto error_exit_1;
        }

        if (strcasecmp(sel_output_type, "text") != 0 &&
            strcasecmp(sel_output_type, "xml") != 0) {
                printf("Invalid selection of file output type'%s'!\n",
                       sel_output_type);
                exit_val = EXIT_FAILURE;
                goto error_exit_1;
        }

        /**
         * Set up file descriptor for monitored data
         */
        if (sel_output_file == NULL) {
                fp_monitor = stdout;
        } else {
                if (strcasecmp(sel_output_type, "xml") == 0)
                        fp_monitor = fopen(sel_output_file, "w+");
                else
                        fp_monitor = fopen(sel_output_file, "a");
                if (fp_monitor == NULL) {
                        perror("Monitoring output file open error:");
                        printf("Error opening '%s' output file!\n",
                               sel_output_file);
                        exit_val = EXIT_FAILURE;
                        goto error_exit_1;
                }
                if (strcasecmp(sel_output_type, "xml") == 0) {
                        fseek(fp_monitor, 0, SEEK_END);
                        if (ftell(fp_monitor) == 0)
                                fprintf(fp_monitor,
                                        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                                        "%s\n%s",
                                        xml_root_open, xml_root_close);
                        fseek(fp_monitor, -xml_root_close_size, SEEK_CUR);
                }
        }

        /**
         * Set up file descriptor for message log
         */
        if (sel_log_file == NULL) {
                cfg.fd_log = STDOUT_FILENO;
        } else {
                cfg.fd_log = open(sel_log_file, O_WRONLY|O_CREAT,
                                  S_IRUSR|S_IWUSR);
                if (cfg.fd_log == -1) {
                        printf("Error opening %s log file!\n", sel_log_file);
                        exit_val = EXIT_FAILURE;
                        goto error_exit_2;
                }
        }

        ret = pqos_init(&cfg);
        if (ret != PQOS_RETVAL_OK) {
                printf("Error initializing PQoS library!\n");
                exit_val = EXIT_FAILURE;
                goto error_exit_1;
        }

        ret = pqos_cap_get(&p_cap, &p_cpu);
        if (ret != PQOS_RETVAL_OK) {
                printf("Error retrieving PQoS capabilities!\n");
                exit_val = EXIT_FAILURE;
                goto error_exit_2;
        }

        ret = pqos_cpu_get_sockets(p_cpu, PQOS_MAX_SOCKETS,
                                   &sock_count,
                                   sockets);
        if (ret != PQOS_RETVAL_OK) {
                printf("Error retrieving CPU socket information!\n");
                exit_val = EXIT_FAILURE;
                goto error_exit_2;
        }

        ret = pqos_cap_get_type(p_cap, PQOS_CAP_TYPE_MON, &cap_mon);
        ret = pqos_cap_get_type(p_cap, PQOS_CAP_TYPE_L3CA, &cap_l3ca);

        if (sel_allocation_profile != NULL) {
                /**
                 * Allocation profile selected
                 */
                unsigned cnum = 0;
                const char * const *cptr = NULL;

                if (cap_l3ca != NULL &&
		    profile_l3ca_get(sel_allocation_profile,
                                     cap_l3ca->u.l3ca,
                                     &cnum, &cptr) == PQOS_RETVAL_OK) {
                        /**
                         * All profile classes are defined as strings
                         * in format that is friendly with command line
                         * options.
                         * This effectively simulates series of -e command
                         * line options. "llc:" is glued to each of the strings
                         * so that profile class definitions don't have to
                         * include it.
                         */
                        char cb[64];
                        unsigned i = 0, offset = 0;

                        memset(cb, 0, sizeof(cb));
                        strcpy(cb, "llc:");
                        offset = (unsigned)strlen("llc:");

                        for (i = 0; i < cnum; i++) {
                                strncpy(cb+offset, cptr[i],
                                        sizeof(cb)-1-offset);
                                selfn_allocation_class(cb);
                        }
                } else {
                        printf("Allocation profile '%s' not found or "
                               "cache allocation not supported!\n",
                               sel_allocation_profile);
                        exit_val = EXIT_FAILURE;
                        goto error_exit_2;
                }
        }

        if (sel_show_allocation_config) {
                /**
                 * Show info about allocation config and exit
                 */
		print_allocation_config(cap_mon, cap_l3ca, sock_count,
                                        sockets, p_cpu);
                goto allocation_exit;
        }

        if (sel_reset_CAT) {
                /**
                 * Reset CAT configuration to after-reset state and exit
                 */
                if (pqos_l3ca_reset(p_cap, p_cpu) != PQOS_RETVAL_OK)
                        printf("CAT reset failed!\n");
                else
                        printf("CAT reset successful\n");
                goto allocation_exit;
        }

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
                        goto error_exit_2;
                }

                ret_assoc = set_allocation_assoc();
                if (ret_assoc < 0) {
                        printf("CAT association error!\n");
                        goto error_exit_2;
                }

                if ((ret_assoc > 0 || ret_cos > 0) && sel_config_file == NULL) {
                        printf("Allocation configuration altered.\n");
                        goto allocation_exit;
                }
        } else {
                if (sel_l3ca_assoc_num > 0 || sel_l3ca_cos_num > 0 ||
                    sel_config_file != NULL || sel_allocation_profile != NULL) {
                        printf("Allocation capability not detected!\n");
                        exit_val = EXIT_FAILURE;
                        goto error_exit_2;
                }
        }

        /**
         * Just monitoring option left on the table now
         */
        if (cap_mon == NULL) {
                printf("Monitoring capability not detected!\n");
                exit_val = EXIT_FAILURE;
                goto error_exit_2;
        }

        if (setup_monitoring(p_cpu, cap_mon) != 0)
                goto error_exit_2;

        monitoring_loop(fp_monitor, sel_timeout, sel_mon_interval,
                        sel_mon_top_like, p_cap, sel_output_type);

        stop_monitoring();

 allocation_exit:
 error_exit_2:
        ret = pqos_fini();
        ASSERT(ret == PQOS_RETVAL_OK);
        if (ret != PQOS_RETVAL_OK)
                printf("Error shutting down PQoS library!\n");

 error_exit_1:
        /**
         * Close file descriptor for monitoring output
         */
        if (fp_monitor != NULL && fp_monitor != stdout)
                fclose(fp_monitor);

        /**
         * Close file descriptor for message log
         */
        if (sel_log_file != NULL)
                close(cfg.fd_log);

        /**
         * Free allocated memory
         */
        if (sel_output_file != NULL)
                free(sel_output_file);
        if (sel_output_type != NULL)
                free(sel_output_type);
        if (sel_allocation_profile != NULL)
                free(sel_allocation_profile);
        if (sel_log_file != NULL)
                free(sel_log_file);
        if (sel_config_file != NULL)
                free(sel_config_file);

        return exit_val;
}
