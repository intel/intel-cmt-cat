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
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @brief Platform QoS utility - main module
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>                                      /**< isspace() */
#include <sys/types.h>                                  /**< open() */
#include <fcntl.h>

#include "pqos.h"

#include "main.h"
#include "profiles.h"
#include "monitor.h"
#include "alloc.h"

/**
 * Default CDP configuration option - don't enforce on or off
 */
static enum pqos_cdp_config selfn_cdp_config = PQOS_REQUIRE_CDP_ANY;

/**
 * Free RMID's being in use
 */
static int sel_free_in_use_rmid = 0;

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
 * Reset CAT configuration
 */
static int sel_reset_CAT = 0;

/**
 * Enable showing cache allocation settings
 */
static int sel_show_allocation_config = 0;

/**
 * @brief Function to check if a value is already contained in a table
 *
 * @param tab table of values to check
 * @param size size of the table
 * @param val value to search for
 *
 * @return If the value is already in the table
 * @retval 1 if value if found
 * @retval 0 if value is not found
 */
static int
isdup(const uint64_t *tab, const unsigned size, const uint64_t val)
{
        unsigned i;

        for (i = 0; i < size; i++)
                if (tab[i] == val)
                        return 1;
        return 0;
}

uint64_t strtouint64(const char *s)
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

        if (!(*s != '\0' && *endptr == '\0')) {
                printf("Error converting '%s' to unsigned number!\n", str);
                exit(EXIT_FAILURE);
        }

        return n;
}

unsigned strlisttotab(char *s, uint64_t *tab, const unsigned max)
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

                /* get rid of leading spaces & skip empty tokens */
                while (isspace(*token))
                        token++;
                if (*token == '\0')
                        continue;

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
                                if (!(isdup(tab, index, n))) {
                                        tab[index] = n;
                                        index++;
                                }
                                if (index >= max)
                                        return index;
                        }
                } else {
                        /**
                         * single number provided here
                         * remove duplicates if necessary
                         */
                        uint64_t val = strtouint64(token);

                        if (!(isdup(tab, index, val))) {
                                tab[index] = val;
                                index++;
                        }
                        if (index >= max)
                                return index;
                }
        }

        return index;
}

__attribute__ ((noreturn)) void
parse_error(const char *arg, const char *note)
{
        printf("Error parsing \"%s\" command line argument. %s\n",
               arg ? arg : "<null>",
               note ? note : "");
        exit(EXIT_FAILURE);
}

void selfn_strdup(char **sel, const char *arg)
{
        ASSERT(sel != NULL && arg != NULL);
        if (*sel != NULL) {
                free(*sel);
                *sel = NULL;
        }
        *sel = strdup(arg);
        ASSERT(*sel != NULL);
        if (*sel == NULL) {
                printf("String duplicate error!\n");
                exit(EXIT_FAILURE);
        }
}

/**
 * @brief Function to print warning to users as utility begins
 */
static void
print_warning(void)
{
        printf("NOTE:  Mixed use of MSR and kernel interfaces "
               "to manage\n       CAT or CMT & MBM may lead to "
               "unexpected behavior.\n");
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
        UNUSED_ARG(arg);
        sel_verbose_mode = 1;
}

/**
 * @brief Selects super verbose mode on
 *
 * @param arg not used
 */
static void
selfn_super_verbose_mode(const char *arg)
{
        UNUSED_ARG(arg);
        sel_verbose_mode = 2;
}

/**
 * @brief Processes configuration setting
 *
 * For now CDP config settings are only accepted.
 *
 * @param arg string detailing configuration setting
 */
static void
selfn_set_config(const char *arg)
{
        if (strcasecmp(arg, "cdp-on") == 0)
                selfn_cdp_config = PQOS_REQUIRE_CDP_ON;
        else if (strcasecmp(arg, "cdp-off") == 0)
                selfn_cdp_config = PQOS_REQUIRE_CDP_OFF;
        else if (strcasecmp(arg, "cdp-any") == 0)
                selfn_cdp_config = PQOS_REQUIRE_CDP_ANY;
        else {
                printf("Unrecognized '%s' setting!\n", arg);
                exit(EXIT_FAILURE);
        }
}

/**
 * @brief Sets CAT reset flag
 *
 * @param arg not used
 */
static void selfn_reset_cat(const char *arg)
{
        UNUSED_ARG(arg);
        sel_reset_CAT = 1;
}

/**
 * @brief Selects showing allocation settings
 *
 * @param arg not used
 */
static void selfn_show_allocation(const char *arg)
{
        UNUSED_ARG(arg);
        sel_show_allocation_config = 1;
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
                { "monitor-pids:",          selfn_monitor_pids },     /**< -p */
                { "monitor-cores:",         selfn_monitor_cores },    /**< -m */
                { "monitor-time:",          selfn_monitor_time },     /**< -t */
                { "monitor-interval:",      selfn_monitor_interval }, /**< -i */
                { "monitor-file:",          selfn_monitor_file },     /**< -o */
                { "monitor-file-type:",     selfn_monitor_file_type },/**< -u */
                { "monitor-top-like:",      selfn_monitor_top_like }, /**< -T */
                { "set-config:",            selfn_set_config },       /**< -S */
                { "reset-cat:",             selfn_reset_cat },        /**< -R */
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
                remain = (int)strlen(cp);

                /**
                 * remove trailing white spaces
                 */
                for (i = (int)strlen(cp)-1; i > 0; i--)
                        if (!isspace(cp[i])) {
                                cp[i+1] = '\0';
                                break;
                        }

                for (i = 0; i < (int)DIM(optab); i++) {
                        int len = (int)strlen(optab[i].option);

                        if (len > remain)
                                continue;

                        if (strncasecmp(cp, optab[i].option, (size_t)len) != 0)
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
               "       %s [-m <event_type>:<core_list> | -p <event_type>:"
               "<pid_list>]\n"
               "          [-t <time in sec>]\n"
               "          [-i <interval in 100ms>] [-T]\n"
               "          [-o <output_file>] [-u <output_type>] [-r]\n"
               "       %s [-e <allocation_type>:<class_num>=<class_definition>;"
               "...]\n"
               "          [-c <allocation_type>:<profile_name>;...]\n"
               "          [-a <allocation_type>:<class_num>=<core_list>;"
               "...]\n"
               "       %s [-R]\n"
               "       %s [-S cdp-on|cdp-off|cdp-any]\n"
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
               "\t-S\tset a configuration setting:\n"
               "\t\tcdp-on\tsets CDP on\n"
               "\t\tcdp-off\tsets CDP off\n"
               "\t\tcdp-any\tkeep current CDP setting (default)\n"
               "\t\tNOTE: change of CDP on/off setting results in CAT reset.\n"
               "\t-m\tselect cores and events for monitoring, example: "
               "\"all:0,2,4-10;llc:1,3;mbr:11-12\"\n"
               "\t\tNOTE: group core statistics together by enclosing the core "
               "list in\n\t\tsquare brackets, example: "
               "\"llc:[0-3];all:[4,5,6];mbr:[0-3],7,8\"\n"
               "\t-o\tselect output file to store monitored data in. "
               "stdout by default.\n"
               "\t-u\tselect output format type for monitored data. "
               "\"text\" (default) and \"xml\" are the options.\n"
               "\t-i\tdefine monitoring sampling interval, 1=100ms, "
               "default 10=10x100ms=1s\n"
               "\t-T\ttop like monitoring output\n"
               "\t-t\tdefine monitoring time (use 'inf' or 'infinite' for "
               "infinite loop monitoring loop)\n"
               "\t-p\tselect process ids and events to monitor, "
	       "example: \"llc:22,25673\" or \"all:892,4588-4592\"\n\t\tNote: "
	       "it is not possible to track both processes and cores\n",
               m_cmd_name, m_cmd_name, m_cmd_name, m_cmd_name, m_cmd_name,
               m_cmd_name, m_cmd_name, m_cmd_name);
}

int main(int argc, char **argv)
{
        struct pqos_config cfg;
        const struct pqos_cpuinfo *p_cpu = NULL;
        const struct pqos_cap *p_cap = NULL;
        const struct pqos_capability *cap_mon = NULL, *cap_l3ca = NULL;
        unsigned sock_count, sockets[PQOS_MAX_SOCKETS];
        int cmd, ret, exit_val = EXIT_SUCCESS;

        m_cmd_name = argv[0];
        print_warning();

        while ((cmd = getopt(argc, argv, "Hhf:i:m:Tt:l:o:u:e:c:a:p:S:srvVR"))
               != -1) {
                switch (cmd) {
                case 'h':
                        print_help();
                        return EXIT_SUCCESS;
                case 'H':
                        profile_l3ca_list(stdout);
                        return EXIT_SUCCESS;
                case 'S':
                        selfn_set_config(optarg);
                        break;
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
                case 'p':
		        selfn_monitor_pids(optarg);
                        break;
                case 'm':
                        selfn_monitor_cores(optarg);
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
                        selfn_reset_cat(NULL);
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
                case 'V':
                        selfn_super_verbose_mode(NULL);
                        break;
                default:
                        printf("Unsupported option: -%c. "
                               "See option -h for help.\n", optopt);
                        return EXIT_FAILURE;
                        break;
                case '?':
                        print_help();
                        return EXIT_SUCCESS;
                        break;
                }
        }

        memset(&cfg, 0, sizeof(cfg));
        cfg.verbose = sel_verbose_mode;
        cfg.free_in_use_rmid = sel_free_in_use_rmid;
        cfg.cdp_cfg = selfn_cdp_config;

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
        if (ret == PQOS_RETVAL_PARAM) {
                printf("Error retrieving monitoring capabilities!\n");
                exit_val = EXIT_FAILURE;
                goto error_exit_2;
        }

        ret = pqos_cap_get_type(p_cap, PQOS_CAP_TYPE_L3CA, &cap_l3ca);
        if (ret == PQOS_RETVAL_PARAM) {
                printf("Error retrieving allocation capabilities!\n");
                exit_val = EXIT_FAILURE;
                goto error_exit_2;
        }

        if (sel_reset_CAT) {
                /**
                 * Reset CAT configuration to after-reset state and exit
                 */
                if (pqos_l3ca_reset(p_cap, p_cpu) != PQOS_RETVAL_OK) {
                        exit_val = EXIT_FAILURE;
                        printf("CAT reset failed!\n");
                } else
                        printf("CAT reset successful\n");
                goto allocation_exit;
        }

        if (sel_show_allocation_config) {
                /**
                 * Show info about allocation config and exit
                 */
		alloc_print_config(cap_mon, cap_l3ca, sock_count,
                                   sockets, p_cpu);
                goto allocation_exit;
        }

        if (sel_allocation_profile != NULL) {
                if (profile_l3ca_apply(sel_allocation_profile, cap_l3ca) != 0) {
                        exit_val = EXIT_FAILURE;
                        goto error_exit_2;
                }
        }

        switch (alloc_apply(cap_l3ca, sock_count, sockets)) {
        case 0: /* nothing to apply */
                break;
        case 1: /* new allocation config applied and all is good */
                goto allocation_exit;
                break;
        case -1: /* something went wrong */
        default:
                exit_val = EXIT_FAILURE;
                goto error_exit_2;
                break;
        }

        /**
         * Just monitoring option left on the table now
         */
        if (cap_mon == NULL) {
                printf("Monitoring capability not detected!\n");
                exit_val = EXIT_FAILURE;
                goto error_exit_2;
        }

        if (monitor_setup(p_cpu, cap_mon) != 0) {
                exit_val = EXIT_FAILURE;
                goto error_exit_2;
        }
        monitor_loop(p_cap);
        monitor_stop();

 allocation_exit:
 error_exit_2:
        ret = pqos_fini();
        ASSERT(ret == PQOS_RETVAL_OK);
        if (ret != PQOS_RETVAL_OK)
                printf("Error shutting down PQoS library!\n");

 error_exit_1:
        monitor_cleanup();

        /**
         * Close file descriptor for message log
         */
        if (cfg.fd_log > 0 && cfg.fd_log != STDOUT_FILENO)
                close(cfg.fd_log);

        /**
         * Free allocated memory
         */
        if (sel_allocation_profile != NULL)
                free(sel_allocation_profile);
        if (sel_log_file != NULL)
                free(sel_log_file);
        if (sel_config_file != NULL)
                free(sel_config_file);

        return exit_val;
}
