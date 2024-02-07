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
 */

/**
 * @brief Platform QoS utility - allocation module
 *
 */
#include "alloc.h"

#include "common.h"
#include "main.h"
#include "pqos.h"

#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

/**
 * Defines used to identify CAT mask definitions
 */
#define CAT_UPDATE_SCOPE_BOTH 0 /**< update COS code & data masks */
#define CAT_UPDATE_SCOPE_DATA 1 /**< update COS data mask */
#define CAT_UPDATE_SCOPE_CODE 2 /**< update COS code mask */

/**
 * Length of max unit64_t value in DEC string
 * + single char for code or data + '\0'
 */
#define MAX_COS_MASK_STR_LEN 22

/**
 * Allocation type selected
 */
enum sel_alloc_type { L3CA, L2CA, MBA, MBA_CTRL };

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
static unsigned sel_alloc_mod = 0;

/**
 * Number of core to COS associations to be done
 */
static int sel_assoc_core_num = 0;

/**
 * Max number of elements in sel_assoc_tab array
 */
static unsigned sel_assoc_tab_size = 128;

/**
 * Core to COS associations details
 */
static struct {
        unsigned core;
        unsigned class_id;
} *sel_assoc_tab = NULL;

/**
 * Number of Task ID to COS associations to be done
 */
static int sel_assoc_pid_num = 0;

/**
 * Task ID to COS associations details
 */
static struct {
        pid_t task_id;
        unsigned class_id;
} sel_assoc_pid_tab[128];

/**
 * Number of IO RDT Channel to COS associations to be done
 */
static int sel_assoc_channel_num = 0;

/**
 * IO RDT Channel to COS associations details
 */
static struct {
        pqos_channel_t channel;
        unsigned class_id;
} sel_assoc_channel_tab[128];

/**
 * Number of IO RDT Device to COS associations to be done
 */
static int sel_assoc_dev_num = 0;

/**
 * IO RDT Device to COS associations details
 */
static struct {
        uint16_t segment;
        uint16_t bdf;
        unsigned vc;
        unsigned class_id;
} sel_assoc_dev_tab[128];

/**
 * Maintains alloc option - allocate cores or task id's
 */
int alloc_pid_flag;

/**
 * @brief Converts string describing allocation COS into ID and mask scope
 *
 * Current string format is: <ID>[CcDd]
 *
 * Some examples:
 *  1d  - class 1, data mask
 *  5C  - class 5, code mask
 *  0   - class 0, common mask for code & data
 *
 * @param [in] str string describing allocation COS. Function may modify
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

        len = strnlen(str, (size_t)MAX_COS_MASK_STR_LEN);
        if (len == MAX_COS_MASK_STR_LEN) {
                printf("Error converting allocation COS string!\n");
                exit(EXIT_FAILURE);
        }

        if (len > 1 && (str[len - 1] == 'c' || str[len - 1] == 'C')) {
                *scope = CAT_UPDATE_SCOPE_CODE;
                str[len - 1] = '\0';
        } else if (len > 1 && (str[len - 1] == 'd' || str[len - 1] == 'D')) {
                *scope = CAT_UPDATE_SCOPE_DATA;
                str[len - 1] = '\0';
        } else {
                *scope = CAT_UPDATE_SCOPE_BOTH;
        }
        *id = (unsigned)strtouint64(str);
}

/**
 * @brief Set L3 class definitions on selected sockets
 *
 * @param class_id L3 class ID to set
 * @param mask class bitmask to set
 * @param sock_ids Array of socket ID's to set class definition
 * @param sock_num Number of socket ID's in the array
 * @param scope L3 CAT update scope i.e. CDP Code/Data
 *
 * @return Number of classes set
 * @retval -1 on error
 */
static int
set_l3_cos(const unsigned class_id,
           const uint64_t mask,
           const unsigned *sock_ids,
           const unsigned sock_num,
           const unsigned scope,
           const struct pqos_cpuinfo *cpu)
{
        unsigned i, set = 0;
        const char *package;

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

                memset(&ca, 0, sizeof(ca));

                /* get current L3 definitions for this socket */
                ret = pqos_l3ca_get(sock_ids[i], DIM(sock_l3ca), &num_ca,
                                    sock_l3ca);
                if (ret != PQOS_RETVAL_OK) {
                        printf("Failed to retrieve socket %u "
                               "L3 classes!\n",
                               sock_ids[i]);
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
                               "CDP not enabled!\n",
                               sock_ids[i]);
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
                if (cpu->vendor == PQOS_VENDOR_AMD)
                        package = "Core Complex";
                else
                        package = "SOCKET";

                if (ret != PQOS_RETVAL_OK) {
                        printf("%s %u L3CA COS%u - FAILED!\n", package,
                               sock_ids[i], ca.class_id);
                        break;
                }
                if (ca.cdp)
                        printf("%s %u L3CA COS%u => DATA 0x%lx,CODE "
                               "0x%lx\n",
                               package, sock_ids[i], ca.class_id,
                               (long)ca.u.s.data_mask, (long)ca.u.s.code_mask);
                else
                        printf("%s %u L3CA COS%u => MASK 0x%lx\n", package,
                               sock_ids[i], ca.class_id, (long)ca.u.ways_mask);
                set++;
        }
        sel_alloc_mod += set;
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
 * @param scope L2 CAT update scope i.e. CDP Code/Data
 *
 * @return Number of classes set
 * @retval -1 on error
 */
static int
set_l2_cos(const unsigned class_id,
           const uint64_t mask,
           const unsigned *l2_ids,
           const unsigned id_num,
           const unsigned scope)
{
        unsigned i, set = 0;

        if (l2_ids == NULL || mask == 0) {
                printf("Failed to set L2 CAT configuration!\n");
                return -1;
        }

        /**
         * Loop through each cluster and set selected classes
         */
        for (i = 0; i < id_num; i++) {
                int ret;
                unsigned j, num_ca;
                struct pqos_l2ca ca, cluster_l2ca[PQOS_MAX_L2CA_COS];

                memset(&ca, 0, sizeof(ca));

                /* get current L2 definitions for this cluster */
                ret = pqos_l2ca_get(l2_ids[i], DIM(cluster_l2ca), &num_ca,
                                    cluster_l2ca);
                if (ret != PQOS_RETVAL_OK) {
                        printf("Failed to retrieve cluster %u L2 classes!\n",
                               l2_ids[i]);
                        break;
                }
                /* find selected class in array */
                for (j = 0; j < num_ca; j++)
                        if (cluster_l2ca[j].class_id == class_id) {
                                ca = cluster_l2ca[j];
                                break;
                        }
                if (j == num_ca) {
                        printf("Invalid class ID: %u!\n", class_id);
                        break;
                }
                /* check if CDP is selected but disabled */
                if (!ca.cdp && scope != CAT_UPDATE_SCOPE_BOTH) {
                        printf("Failed to set L2 class on cluster %u, "
                               "CDP not enabled!\n",
                               l2_ids[i]);
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

                /* set new L2 class definition */
                ret = pqos_l2ca_set(l2_ids[i], 1, &ca);
                if (ret != PQOS_RETVAL_OK) {
                        printf("L2ID %u L2CA COS%u - FAILED!\n", l2_ids[i],
                               ca.class_id);
                        break;
                }
                if (ca.cdp)
                        printf("L2ID %u L2CA COS%u => DATA 0x%lx,CODE 0x%lx\n",
                               l2_ids[i], ca.class_id, (long)ca.u.s.data_mask,
                               (long)ca.u.s.code_mask);
                else
                        printf("L2ID %u L2CA COS%u => MASK 0x%lx\n", l2_ids[i],
                               ca.class_id, (long)ca.u.ways_mask);
                set++;
        }
        sel_alloc_mod += set;
        if (set < id_num)
                return -1;

        return (int)set;
}

/**
 * @brief Set MBA class definitions on selected sockets
 *
 * @param class_id MBA class ID to set
 * @param available_bw to set
 * @param sock_ids Array of socket ID's to set class definition
 * @param sock_num Number of socket ID's in the array
 * @param ctrl Flag indicating a use of MBA controller
 *
 * @return Number of classes set
 * @retval -1 on error
 */
static int
set_mba_cos(const unsigned class_id,
            const uint64_t available_bw,
            const unsigned *sock_ids,
            const unsigned sock_num,
            int ctrl,
            const struct pqos_cpuinfo *cpu)
{
        unsigned i, set = 0;
        struct pqos_mba mba, actual;

        if (sock_ids == NULL || available_bw == 0) {
                printf("Failed to set MBA configuration!\n");
                return -1;
        }
        mba.ctrl = ctrl;
        mba.class_id = class_id;
        mba.mb_max = available_bw;

        /**
         * Set all selected classes
         */
        for (i = 0; i < sock_num; i++) {
                const char *unit;
                const char *package;
                int ret = pqos_mba_set(sock_ids[i], 1, &mba, &actual);

                if (cpu->vendor == PQOS_VENDOR_AMD) {
                        package = "Core Complex";
                        unit = "";
                } else {
                        package = "SOCKET";
                        unit = "%";
                }

                if (ret != PQOS_RETVAL_OK) {
                        printf("%s %u MBA COS%u - FAILED!\n", package,
                               sock_ids[i], mba.class_id);
                        break;
                }

                printf("%s %u MBA COS%u => ", package, sock_ids[i],
                       actual.class_id);

                if (ctrl == 1)
                        printf("%u MBps\n", mba.mb_max);
                else
                        printf("%u%s requested, %u%s applied\n", mba.mb_max,
                               unit, actual.mb_max, unit);

                set++;
        }
        sel_alloc_mod += set;
        if (set < sock_num)
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
 * @param type allocation type (L2/L3/MBA/MBA CTRL)
 * @param cpu pointer to cpu topology structure
 *
 * @return Number of classes set
 * @retval Positive on success
 * @retval Negative on error
 */
static int
set_allocation_cos(char *str,
                   unsigned *res_ids,
                   const unsigned res_num,
                   const enum sel_alloc_type type,
                   const struct pqos_cpuinfo *cpu)
{
        char *p = NULL;
        uint64_t mask = 0;
        unsigned class_id = 0, n = res_num;
        unsigned *ids = res_ids;
        int ret = -1, update_scope = CAT_UPDATE_SCOPE_BOTH;

        p = strchr(str, '=');
        if (p == NULL) {
                printf("Invalid class of service definition: %s\n", str);
                return ret;
        }
        *p = '\0';

        parse_cos_mask_type(str, &update_scope, &class_id);
        mask = strtouint64(p + 1);

        /* if MBA selected, set MBA classes */
        if (type == MBA || type == MBA_CTRL) {
                int ctrl = (type == MBA_CTRL) ? 1 : 0;

                if (ids == NULL)
                        ids = pqos_cpu_get_mba_ids(cpu, &n);
                if (ids == NULL) {
                        printf("Failed to retrieve socket info!\n");
                        return -1;
                }
                ret = set_mba_cos(class_id, mask, ids, n, ctrl, cpu);
                if (res_ids == NULL && ids != NULL)
                        free(ids);
                return ret;
        }

        /* if L2 CAT selected, set L2 classes */
        if (type == L2CA) {
                if (ids == NULL)
                        ids = pqos_cpu_get_l2ids(cpu, &n);
                if (ids == NULL) {
                        printf("Failed to retrieve L2 cluster info!\n");
                        return -1;
                }
                ret = set_l2_cos(class_id, mask, ids, n, update_scope);
                if (res_ids == NULL && ids != NULL)
                        free(ids);
                return ret;
        }
        /* set L3 CAT classes */
        if (ids == NULL)
                ids = pqos_cpu_get_l3cat_ids(cpu, &n);
        if (ids == NULL) {
                printf("Failed to retrieve socket info!\n");
                return -1;
        }
        ret = set_l3_cos(class_id, mask, ids, n, update_scope, cpu);
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
        char *q = NULL, *p = NULL;
        char *s = NULL, *saveptr = NULL;
        enum sel_alloc_type type;

        const unsigned max_res_sz = 256;
        unsigned res_ids[max_res_sz], *sp = NULL, i, n = 1;

        selfn_strdup(&s, str);

        p = strchr(str, ':');
        if (p == NULL) {
                printf("Unrecognized allocation format: %s\n", str);
                free(s);
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
                        printf("No resource ID specified: %s\n", s);
                        free(s);
                        return ret;
                }
                /* check for invalid resource ID */
                for (i = 0; i < n; i++) {
                        if (ids[i] >= UINT_MAX) {
                                printf("Resource ID out of range: %s\n", s);
                                free(s);
                                return ret;
                        }
                        res_ids[i] = (unsigned)ids[i];
                }
                sp = res_ids;
        } else
                *p = '\0';
        /**
         * Determine selected type (L3/L2/MBA/MBA CTRL)
         */
        if (strcasecmp(str, "llc") == 0)
                type = L3CA;
        else if (strcasecmp(str, "l2") == 0)
                type = L2CA;
        else if (strcasecmp(str, "mba") == 0)
                type = MBA;
        else if (strcasecmp(str, "mba_max") == 0)
                type = MBA_CTRL;
        else {
                printf("Unrecognized allocation type: %s\n", s);
                free(s);
                return ret;
        }
        /**
         * Parse COS masks and apply to selected res_ids
         */
        for (++p;; p = NULL) {
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
 * @param cpu CPU topology structure
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

        return (int)sel_alloc_mod;
}

void
selfn_allocation_class(const char *arg)
{
        char *cp = NULL, *str = NULL;
        char *saveptr = NULL;

        if (arg == NULL)
                parse_error(arg, "NULL pointer!");

        if (*arg == '\0')
                parse_error(arg, "Empty string!");

        selfn_strdup(&cp, arg);

        for (str = cp;; str = NULL) {
                char *token = NULL;

                token = strtok_r(str, ";", &saveptr);
                if (token == NULL)
                        break;

                if (sel_alloc_opt_num >= DIM(alloc_opts))
                        parse_error(arg, "Too many allocation options!");

                selfn_strdup(&alloc_opts[sel_alloc_opt_num++], token);
        }

        free(cp);
}

/**
 * @brief Retrieve device information
 *
 * @param [in] segment Device segment/domain
 * @param [in] bdf Device ID
 *
 * @return Device information structure
 * @retval NULL on error
 */
static const struct pqos_dev *
_devinfo_get_dev(const struct pqos_devinfo *dev, uint16_t segment, uint16_t bdf)
{
        unsigned i;

        if (dev == NULL)
                return NULL;
        for (i = 0; i < dev->num_devs; ++i) {
                const struct pqos_dev *device = &dev->devs[i];

                if (device->segment == segment && device->bdf == bdf)
                        return device;
        }

        return NULL;
}

/**
 * @brief Sets up association between cores/tasks and allocation
 *        classes of service
 *
 * @param dev device information
 * @return Number of associations made
 * @retval 0 no association made (nor requested)
 * @retval negative error
 * @retval positive success
 */
static int
set_allocation_assoc(const struct pqos_devinfo *dev)
{
        int i;

        for (i = 0; i < sel_assoc_core_num; i++) {
                int ret;

                ret = pqos_alloc_assoc_set(sel_assoc_tab[i].core,
                                           sel_assoc_tab[i].class_id);
                if (ret == PQOS_RETVAL_PARAM) {
                        printf("Core number or class id is out of bounds!\n");
                        return -1;
                } else if (ret != PQOS_RETVAL_OK) {
                        printf("Setting allocation class of service "
                               "association failed!\n");
                        return -1;
                }
        }

        for (i = 0; i < sel_assoc_pid_num; i++) {
                int ret;

                ret = pqos_alloc_assoc_set_pid(sel_assoc_pid_tab[i].task_id,
                                               sel_assoc_pid_tab[i].class_id);
                if (ret == PQOS_RETVAL_PARAM) {
                        printf("Task ID number or class id is out of "
                               "bounds!\n");
                        return -1;
                } else if (ret != PQOS_RETVAL_OK) {
                        printf("Setting allocation class of service "
                               "association failed!\n");
                        return -1;
                }
        }

        for (i = 0; i < sel_assoc_channel_num; i++) {
                int ret;

                ret = pqos_alloc_assoc_set_channel(
                    sel_assoc_channel_tab[i].channel,
                    sel_assoc_channel_tab[i].class_id);
                if (ret == PQOS_RETVAL_PARAM) {
                        printf("Channel or class id is out of "
                               "bounds!\n");
                        return -1;
                } else if (ret != PQOS_RETVAL_OK) {
                        printf("Setting allocation class of service "
                               "association failed!\n");
                        return -1;
                }
        }

        for (i = 0; i < sel_assoc_dev_num; i++) {
                int ret = PQOS_RETVAL_OK;
                uint16_t segment = sel_assoc_dev_tab[i].segment;
                uint16_t bdf = sel_assoc_dev_tab[i].bdf;
                unsigned vc = sel_assoc_dev_tab[i].vc;
                unsigned cos = sel_assoc_dev_tab[i].class_id;

                const struct pqos_dev *device =
                    _devinfo_get_dev(dev, segment, bdf);

                if (device == NULL) {
                        printf("Invalid device!\n");
                        return -1;
                }

                /* All device channels */
                if (vc == DEV_ALL_VCS) {
                        unsigned c;

                        for (c = 0; c < PQOS_DEV_MAX_CHANNELS; ++c) {
                                if (device->channel[c] == 0)
                                        continue;

                                ret = pqos_alloc_assoc_set_dev(segment, bdf, c,
                                                               cos);
                                if (ret != PQOS_RETVAL_OK)
                                        break;
                        }
                } else
                        ret = pqos_alloc_assoc_set_dev(segment, bdf, vc, cos);

                if (ret == PQOS_RETVAL_PARAM) {
                        printf("Channel or class id is out of bounds!\n");
                        return -1;
                } else if (ret != PQOS_RETVAL_OK) {
                        printf("Setting allocation class of service "
                               "association failed!\n");
                        return -1;
                }
        }

        return sel_assoc_core_num | sel_assoc_pid_num | sel_assoc_channel_num |
               sel_assoc_dev_num;
}

/**
 * @brief Verifies and translates allocation association config string into
 *        internal core table.
 *
 * @param str string passed to -a command line option
 */
static void
fill_core_tab(char *str)
{
        unsigned max_cores_count = sysconf(_SC_NPROCESSORS_CONF);
        uint64_t *cores = calloc(max_cores_count, sizeof(uint64_t));
        unsigned i = 0, n = 0, cos = 0;
        char *p = NULL;

        if (cores == NULL) {
                printf("Error with memory allocation!\n");
                goto error_exit;
        }
        if (sel_assoc_tab == NULL) {
                sel_assoc_tab =
                    calloc(sel_assoc_tab_size, sizeof(*sel_assoc_tab));
                if (sel_assoc_tab == NULL) {
                        printf("Error with memory allocation!\n");
                        goto error_exit;
                }
        }

        if (strncasecmp(str, "cos:", 4) == 0)
                str += strlen("cos:");
        else if (strncasecmp(str, "llc:", 4) == 0)
                str += strlen("llc:");
        else
                str += strlen("core:");

        p = strchr(str, '=');
        if (p == NULL)
                parse_error(str, "Invalid allocation class of service "
                                 "association format");
        *p = '\0';

        cos = (unsigned)strtouint64(str);
        n = strlisttotabrealloc(p + 1, &cores, &max_cores_count);

        if (n == 0)
                goto normal_exit;

        if (sel_assoc_core_num <= 0) {
                for (i = 0; i < n; i++) {
                        if (i >= sel_assoc_tab_size) {
                                sel_assoc_tab = realloc_and_init(
                                    sel_assoc_tab, &sel_assoc_tab_size,
                                    sizeof(*sel_assoc_tab));
                                if (sel_assoc_tab == NULL) {
                                        printf("Reallocation error!\n");
                                        goto error_exit;
                                }
                        }
                        sel_assoc_tab[i].core = (unsigned)cores[i];
                        sel_assoc_tab[i].class_id = cos;
                }
                sel_assoc_core_num = (int)n;
                goto normal_exit;
        }
        for (i = 0; i < n; i++) {
                int j;

                for (j = 0; j < sel_assoc_core_num; j++)
                        if (sel_assoc_tab[j].core == (unsigned)cores[i])
                                break;

                if (j < sel_assoc_core_num) {
                        /**
                         * this core is already on the list
                         * - update COS but warn about it
                         */
                        printf("warn: updating COS for core %u from %u to %u\n",
                               (unsigned)cores[i], sel_assoc_tab[j].class_id,
                               cos);
                        sel_assoc_tab[j].class_id = cos;
                } else {
                        /**
                         * New core is selected - extend the list
                         */
                        unsigned k = (unsigned)sel_assoc_core_num;

                        if (k >= sel_assoc_tab_size) {
                                sel_assoc_tab = realloc_and_init(
                                    sel_assoc_tab, &sel_assoc_tab_size,
                                    sizeof(*sel_assoc_tab));
                                if (sel_assoc_tab == NULL) {
                                        printf("Reallocation error!\n");
                                        goto error_exit;
                                }
                        }

                        sel_assoc_tab[k].core = (unsigned)cores[i];
                        sel_assoc_tab[k].class_id = cos;
                        sel_assoc_core_num++;
                }
        }
normal_exit:
        if (cores != NULL)
                free(cores);
        return;
error_exit:
        if (cores != NULL)
                free(cores);
        exit(EXIT_FAILURE);
}

/**
 * @brief Verifies and translates allocation association config string into
 *        internal task ID table.
 *
 * @param str string passed to -a command line option
 */
static void
fill_pid_tab(char *str)
{
        uint64_t tasks[128];
        unsigned i = 0, n = 0, cos = 0;
        char *p = NULL;

        str += strlen("pid:");
        p = strchr(str, '=');
        if (p == NULL)
                parse_error(str, "Invalid allocation class of service "
                                 "association format");
        *p = '\0';

        cos = (unsigned)strtouint64(str);

        n = strlisttotab(p + 1, tasks, DIM(tasks));
        if (n == 0)
                return;

        if (sel_assoc_pid_num <= 0) {
                for (i = 0; i < n; i++) {
                        if (i >= DIM(sel_assoc_pid_tab))
                                parse_error(str, "too many tasks selected for "
                                                 "allocation association");
                        sel_assoc_pid_tab[i].task_id = (pid_t)tasks[i];
                        sel_assoc_pid_tab[i].class_id = cos;
                }
                sel_assoc_pid_num = (int)n;
                return;
        }

        for (i = 0; i < n; i++) {
                int j;

                for (j = 0; j < sel_assoc_pid_num; j++)
                        if (sel_assoc_pid_tab[j].task_id == (pid_t)tasks[i])
                                break;

                if (j < sel_assoc_pid_num) {
                        /**
                         * this task is already on the list
                         * - update COS but warn about it
                         */
                        printf("warn: updating COS for task %u from %u to %u\n",
                               (unsigned)tasks[i],
                               sel_assoc_pid_tab[j].class_id, cos);
                        sel_assoc_pid_tab[j].class_id = cos;
                } else {
                        /**
                         * New task is selected - extend the list
                         */
                        unsigned k = (unsigned)sel_assoc_pid_num;

                        if (k >= DIM(sel_assoc_pid_tab))
                                parse_error(str, "too many tasks selected for "
                                                 "allocation association");

                        sel_assoc_pid_tab[k].task_id = (pid_t)tasks[i];
                        sel_assoc_pid_tab[k].class_id = cos;
                        sel_assoc_pid_num++;
                }
        }
}

/**
 * @brief Adds channel and COS to internal channel table.
 *
 * @param channel channel to be added
 * @param cos COS for channel
 */
static void
add_channel_channel_tab(const pqos_channel_t channel, const unsigned cos)
{
        ASSERT(channel);

        if (sel_assoc_channel_num <= 0) {
                sel_assoc_channel_num = 0;
                sel_assoc_channel_tab[sel_assoc_channel_num].channel = channel;
                sel_assoc_channel_tab[sel_assoc_channel_num].class_id = cos;
                sel_assoc_channel_num++;
                return;
        }

        size_t i;

        for (i = 0; i < (size_t)sel_assoc_channel_num; i++)
                if (sel_assoc_channel_tab[i].channel == channel)
                        break;

        if (i < (size_t)sel_assoc_channel_num) {
                /**
                 * this channel is already on the list
                 * - update COS but warn about it
                 */
                printf("warn: updating COS for channel 0x%" PRIx64
                       " from %u to %u\n",
                       channel, sel_assoc_channel_tab[i].class_id, cos);

                sel_assoc_channel_tab[i].class_id = cos;
        } else {
                /**
                 * New channel is selected - extend the list
                 */
                size_t j = (size_t)sel_assoc_channel_num;

                if (j >= DIM(sel_assoc_channel_tab)) {
                        printf("warn: too many channels selected for "
                               "allocation association!\n");
                        return;
                }

                sel_assoc_channel_tab[j].channel = channel;
                sel_assoc_channel_tab[j].class_id = cos;
                sel_assoc_channel_num++;
        }
}

/**
 * @brief Verifies and translates allocation association config string into
 *        internal dev table.
 *
 * @param str string passed to -a command line option
 */
static void
fill_dev_tab(char *str)
{
        unsigned cos = 0;
        uint16_t segment = 0;
        uint16_t bus, device, function;
        uint16_t bdf = 0;
        unsigned vc = DEV_ALL_VCS; /* All channels by default */
        char *p = NULL;

        str += strlen("dev:");
        p = strchr(str, '=');
        if (p == NULL)
                parse_error(str, "Invalid allocation class of service "
                                 "association format.");
        *p = '\0';

        cos = (unsigned)strtouint64(str);

        str = ++p;

        /* Rough PCI ID validation */
        size_t colon_count = 0;
        size_t point_count = 0;

        while (*p) {
                if (*p == ':')
                        ++colon_count;
                if (*p == '.')
                        ++point_count;
                ++p;
        }

        if (!colon_count || (colon_count > 2) || (point_count != 1))
                parse_error(str, "Invalid PCI ID format.");

        /* PCI segment */
        if (colon_count > 1) {
                p = strchr(str, ':');
                if (p == NULL)
                        parse_error(str, "Invalid PCI ID format.");
                *p = '\0';
                segment = (uint16_t)strhextouint64(str);
                str = p + 1;
        }

        /* PCI bus */
        p = strchr(str, ':');
        if (p == NULL)
                parse_error(str, "Invalid PCI ID format.");
        *p = '\0';
        bus = (uint16_t)strhextouint64(str);
        str = p + 1;

        /* PCI device */
        p = strchr(str, '.');
        if (p == NULL)
                parse_error(str, "Invalid PCI ID format.");
        *p = '\0';
        device = (uint16_t)strhextouint64(str);
        str = p + 1;

        /* PCI virtual channel */
        p = strchr(str, '@');
        if (p) {
                *p = '\0';
                vc = (unsigned)strtouint64(p + 1);
        }

        /* PCI function */
        function = (uint16_t)strhextouint64(str);

        /* PCI BDF */
        bdf |= bus << 8;
        bdf |= (device & 0x1F) << 3;
        bdf |= function & 0x7;

        if (sel_assoc_dev_num <= 0) {
                sel_assoc_dev_num = 0;
                sel_assoc_dev_tab[sel_assoc_dev_num].segment = segment;
                sel_assoc_dev_tab[sel_assoc_dev_num].bdf = bdf;
                sel_assoc_dev_tab[sel_assoc_dev_num].vc = vc;
                sel_assoc_dev_tab[sel_assoc_dev_num].class_id = cos;
                sel_assoc_dev_num++;
                return;
        }

        size_t i;

        for (i = 0; i < (size_t)sel_assoc_dev_num; i++)
                if ((sel_assoc_dev_tab[i].segment == segment) &&
                    (sel_assoc_dev_tab[i].bdf == bdf) &&
                    (sel_assoc_dev_tab[i].vc == vc))
                        break;

        if (i < (size_t)sel_assoc_dev_num) {
                /**
                 * this dev is already on the list
                 * - update COS but warn about it
                 */
                printf("warn: updating COS for dev %.4x:%.4x:%.2x.%x", segment,
                       bus, device, function);
                if (vc != DEV_ALL_VCS)
                        printf("@%u", vc);
                printf(" from %u to %u.\n", sel_assoc_dev_tab[i].class_id, cos);

                sel_assoc_dev_tab[i].class_id = cos;
        } else {
                /**
                 * New dev is selected - extend the list
                 */
                i = (size_t)sel_assoc_dev_num;

                if (i >= DIM(sel_assoc_dev_tab)) {
                        printf("warn: too many devs selected for allocation "
                               "association.\n");
                        return;
                }

                sel_assoc_dev_tab[i].segment = segment;
                sel_assoc_dev_tab[i].bdf = bdf;
                sel_assoc_dev_tab[i].vc = vc;
                sel_assoc_dev_tab[i].class_id = cos;
                sel_assoc_dev_num++;
        }
}

/**
 * @brief Verifies and translates allocation association config string into
 *        internal channel table.
 *
 * @param str string passed to -a command line option
 */
static void
fill_channel_tab(char *str)
{
        pqos_channel_t channel;
        unsigned cos = 0;
        char *p = NULL;

        str += strlen("channel:");
        p = strchr(str, '=');
        if (p == NULL)
                parse_error(str, "Invalid allocation class of service "
                                 "association format");
        *p = '\0';

        cos = (unsigned)strtouint64(str);
        channel = (pqos_channel_t)strtouint64(p + 1);

        add_channel_channel_tab(channel, cos);
}

/**
 * @brief Verifies allocation association config string.
 *
 * @param str string passed to -a command line option
 */
static void
parse_allocation_assoc(char *str)
{
        if ((strncasecmp(str, "cos:", 4) == 0) ||
            (strncasecmp(str, "llc:", 4) == 0) ||
            (strncasecmp(str, "core:", 5) == 0)) {
                alloc_pid_flag = 0;
                fill_core_tab(str);
        } else if (strncasecmp(str, "pid:", 4) == 0) {
                alloc_pid_flag = 1;
                fill_pid_tab(str);
        } else if (strncasecmp(str, "dev:", 4) == 0) {
                alloc_pid_flag = 0;
                fill_dev_tab(str);
        } else if (strncasecmp(str, "channel:", 8) == 0) {
                alloc_pid_flag = 0;
                fill_channel_tab(str);
        } else
                parse_error(str, "Unrecognized allocation type");
}

void
selfn_allocation_assoc(const char *arg)
{
        char *cp = NULL, *str = NULL;
        char *saveptr = NULL;

        if (arg == NULL)
                parse_error(arg, "NULL pointer!");

        if (*arg == '\0')
                parse_error(arg, "Empty string!");

        selfn_strdup(&cp, arg);

        for (str = cp;; str = NULL) {
                char *token = NULL;

                token = strtok_r(str, ";", &saveptr);
                if (token == NULL)
                        break;
                parse_allocation_assoc(token);
        }

        free(cp);
}

/**
 * @brief Prints L3 CAT class definition
 *
 * @param [in] ca L3 CAT definition structure
 * @param [in] is_error indicates error condition when reading L3 CAT class
 */
static void
print_l3ca_config(const struct pqos_l3ca *ca, const int is_error)
{
        if (is_error) {
                printf("    L3CA COS%u => ERROR\n", ca->class_id);
                return;
        }

        if (ca->cdp) {
                printf("    L3CA COS%u => DATA 0x%llx, CODE 0x%llx\n",
                       ca->class_id, (unsigned long long)ca->u.s.data_mask,
                       (unsigned long long)ca->u.s.code_mask);
        } else {
                printf("    L3CA COS%u => MASK 0x%llx\n", ca->class_id,
                       (unsigned long long)ca->u.ways_mask);
        }
}

/**
 * @brief Prints L2 CAT class definition
 *
 * @param [in] ca L2 CAT definition structure
 * @param [in] is_error indicates error condition when reading L2 CAT class
 */
static void
print_l2ca_config(const struct pqos_l2ca *ca, const int is_error)
{
        if (is_error) {
                printf("    L2CA COS%u => ERROR\n", ca->class_id);
                return;
        }

        if (ca->cdp) {
                printf("    L2CA COS%u => DATA 0x%llx, CODE 0x%llx\n",
                       ca->class_id, (unsigned long long)ca->u.s.data_mask,
                       (unsigned long long)ca->u.s.code_mask);
        } else {
                printf("    L2CA COS%u => MASK 0x%llx\n", ca->class_id,
                       (unsigned long long)ca->u.ways_mask);
        }
}

/**
 * @brief Per socket L3 CAT and MBA class definition printing
 *
 * If new per socket technologies appear they should be added here, too.
 *
 * @param [in] cap_l3ca pointer to L3 CAT capability structure
 * @param [in] cap_mba pointer to MBA capability structure
 * @param [in] sock_count number of socket id's in \a sockets
 * @param [in] sockets arrays of socket id's
 */
static void
print_per_socket_config(const struct pqos_capability *cap_l3ca,
                        const struct pqos_capability *cap_mba,
                        const struct pqos_cpuinfo *cpu_info,
                        const unsigned sock_count,
                        const unsigned *sockets)
{
        int ret;
        unsigned i;

        if (cpu_info == NULL || sockets == NULL)
                return;

        if (cap_l3ca == NULL && cap_mba == NULL)
                return;

        for (i = 0; i < sock_count; i++) {

                printf("%s%s%s COS definitions for Socket %u:\n",
                       cap_l3ca != NULL ? "L3CA" : "",
                       (cap_l3ca != NULL && cap_mba != NULL) ? "/" : "",
                       cap_mba != NULL ? "MBA" : "", sockets[i]);

                if (cap_l3ca != NULL) {
                        const struct pqos_cap_l3ca *l3ca = cap_l3ca->u.l3ca;
                        struct pqos_l3ca tab[l3ca->num_classes];
                        unsigned num = 0;
                        unsigned n = 0;

                        ret = pqos_l3ca_get(sockets[i], l3ca->num_classes, &num,
                                            tab);
                        if (ret != PQOS_RETVAL_OK)
                                num = l3ca->num_classes;

                        for (n = 0; n < num; n++)
                                print_l3ca_config(&tab[n],
                                                  (ret != PQOS_RETVAL_OK));
                }

                if (cap_mba != NULL) {
                        const struct pqos_cap_mba *mba = cap_mba->u.mba;
                        struct pqos_mba tab[mba->num_classes];
                        unsigned num = 0;
                        unsigned n = 0;
                        const char *unit;
                        const char *available;

                        if (mba->ctrl_on == 1) {
                                unit = " MBps";
                                available = "";
                        } else {
                                available = " available";
                                if (cpu_info->vendor == PQOS_VENDOR_AMD)
                                        unit = "";
                                else
                                        unit = "%";
                        }

                        ret = pqos_mba_get(sockets[i], mba->num_classes, &num,
                                           tab);
                        if (ret != PQOS_RETVAL_OK)
                                num = mba->num_classes;

                        for (n = 0; n < num; n++) {
                                if (ret != PQOS_RETVAL_OK)
                                        printf("    MBA COS%u => ERROR\n",
                                               tab[n].class_id);
                                else
                                        printf("    MBA COS%u => %u%s%s\n",
                                               tab[n].class_id, tab[n].mb_max,
                                               unit, available);
                        }
                }
        }
}

/**
 * @brief Retrieves and prints core association
 *
 * @param [in] is_alloc indicates if any allocation technology is present
 * @param [in] is_l3 indicates if L3 cache is present
 * @param [in] is_mon indicates if monitoring technology is present
 * @param [in] ci core info structure with all topology details
 */
static void
print_core_assoc(const int is_alloc,
                 const int is_l3,
                 const int is_mon,
                 const struct pqos_coreinfo *ci)
{
        unsigned class_id = 0;
        pqos_rmid_t rmid = 0;
        int ret = PQOS_RETVAL_OK;
        enum pqos_interface interface;

        ret = pqos_inter_get(&interface);
        if (ret != PQOS_RETVAL_OK)
                return;

        if (is_alloc)
                ret = pqos_alloc_assoc_get(ci->lcore, &class_id);

        if (is_mon && ret == PQOS_RETVAL_OK && interface == PQOS_INTER_MSR)
                ret = pqos_mon_assoc_get(ci->lcore, &rmid);

        if (ret != PQOS_RETVAL_OK) {
                printf("    Core %u => ERROR\n", ci->lcore);
                return;
        }

        if (is_l3)
                printf("    Core %u, L2ID %u, L3ID %u => ", ci->lcore,
                       ci->l2_id, ci->l3_id);
        else
                printf("    Core %u, L2ID %u => ", ci->lcore, ci->l2_id);

        if (is_alloc)
                printf("COS%u", class_id);

        if (is_mon && interface == PQOS_INTER_MSR)
                printf("%sRMID%u\n", is_alloc ? ", " : "", (unsigned)rmid);
        else
                printf("\n");
}

/**
 * @brief Retrieves and prints device association
 *
 * @param [in] is_alloc indicates if allocation technology is present
 * @param [in] is_mon indicates if monitoring technology is present
 * @param [in] dev device info structure
 * @param [in] devinfo IO RDT info structure
 */
static void
print_dev_assoc(const int is_alloc,
                const int is_mon,
                const struct pqos_dev *dev,
                const struct pqos_devinfo *devinfo)
{
        unsigned vc;

        if (!dev || !devinfo || !(is_alloc || is_mon))
                return;

        for (vc = 0; vc < PQOS_DEV_MAX_CHANNELS; ++vc) {
                if (!dev->channel[vc])
                        continue;

                const struct pqos_channel *channel =
                    pqos_devinfo_get_channel(devinfo, dev->channel[vc]);

                if (!channel)
                        continue;

                const int print_clos = is_alloc && channel->clos_tagging;
                const int print_rmid = is_mon && channel->rmid_tagging;

                if (!(print_clos || print_rmid))
                        continue;

                printf("    Device %.4x:%.4x:%.2x.%x@%u, Channel 0x%" PRIx64
                       " => ",
                       dev->segment, BDF_BUS(dev->bdf), BDF_DEV(dev->bdf),
                       BDF_FUNC(dev->bdf), vc, dev->channel[vc]);

                if (print_clos) {
                        unsigned class_id = 0;
                        int ret = pqos_alloc_assoc_get_dev(
                            dev->segment, dev->bdf, vc, &class_id);

                        if (ret == PQOS_RETVAL_OK)
                                printf("COS%u", class_id);
                        else if (ret == PQOS_RETVAL_RESOURCE)
                                printf("NOCOS");
                        else
                                printf("ERROR");
                }

                if (print_rmid) {
                        pqos_rmid_t rmid = 0;
                        int ret = pqos_mon_assoc_get_dev(dev->segment, dev->bdf,
                                                         vc, &rmid);

                        if (print_clos)
                                printf(", ");
                        if (ret == PQOS_RETVAL_OK)
                                printf("RMID%u", (unsigned)rmid);
                        else if (ret == PQOS_RETVAL_RESOURCE)
                                printf("NORMID");
                        else
                                printf("ERROR");
                }

                printf("\n");
        }
}

/**
 * @brief Retrieves and prints channel association
 *
 * @param [in] is_alloc indicates if allocation technology is present
 * @param [in] is_mon indicates if monitoring technology is present
 * @param [in] channel channel info structure
 */
static void
print_channel_assoc(const int is_alloc,
                    const int is_mon,
                    const struct pqos_channel *channel)
{
        pqos_rmid_t rmid = 0;
        int ret = PQOS_RETVAL_OK;

        if (!channel)
                return;

        const int print_clos = is_alloc && channel->clos_tagging;
        const int print_rmid = is_mon && channel->rmid_tagging;

        if (!(print_clos || print_rmid))
                return;

        printf("    Channel 0x%" PRIx64 " => ", channel->channel_id);

        if (print_clos) {
                unsigned class_id = 0;
                int ret = pqos_alloc_assoc_get_channel(channel->channel_id,
                                                       &class_id);

                if (ret == PQOS_RETVAL_OK)
                        printf("COS%u", class_id);
                else if (ret == PQOS_RETVAL_RESOURCE)
                        printf("NOCOS");
                else
                        printf("ERROR");
        }

        if (print_rmid) {
                ret = pqos_mon_assoc_get_channel(channel->channel_id, &rmid);

                if (print_clos)
                        printf(", ");
                if (ret == PQOS_RETVAL_OK)
                        printf("RMID%u", (unsigned)rmid);
                else if (ret == PQOS_RETVAL_RESOURCE)
                        printf("NORMID");
                else
                        printf("ERROR");
        }

        printf("\n");
}

/**
 * @brief Retrieves and prints IO RDT association info
 *
 * @param [in] cap_mon monitoring capabilities
 * @param [in] cap_l3ca L3CA capabilities
 * @param [in] dev_info IO RDT device info
 */
static void
print_iordt_alloc(const struct pqos_capability *cap_mon,
                  const struct pqos_capability *cap_l3ca,
                  const struct pqos_devinfo *dev_info)
{
        size_t i;
        const int is_iordt_alloc = cap_l3ca && cap_l3ca->u.l3ca->iordt_on;
        const int is_iordt_mon = cap_mon && cap_mon->u.mon->iordt_on;

        if (!(is_iordt_alloc || is_iordt_mon) || !dev_info)
                return;

        printf("Device information:\n");
        for (i = 0; i < dev_info->num_devs; ++i)
                print_dev_assoc(is_iordt_alloc, is_iordt_mon,
                                &dev_info->devs[i], dev_info);

        printf("Control channel information:\n");
        for (i = 0; i < dev_info->num_channels; ++i)
                print_channel_assoc(is_iordt_alloc, is_iordt_mon,
                                    &dev_info->channels[i]);
}

void
alloc_print_config(const struct pqos_capability *cap_mon,
                   const struct pqos_capability *cap_l3ca,
                   const struct pqos_capability *cap_l2ca,
                   const struct pqos_capability *cap_mba,
                   const struct pqos_cpuinfo *cpu_info,
                   const struct pqos_devinfo *dev_info,
                   const int verbose)
{
        int ret;
        unsigned i;
        unsigned sock_count, *sockets = NULL;

        sockets = pqos_cpu_get_sockets(cpu_info, &sock_count);
        if (sockets == NULL) {
                printf("Error retrieving information for Sockets\n");
                return;
        }

        print_per_socket_config(cap_l3ca, cap_mba, cpu_info, sock_count,
                                sockets);

        if (cap_l2ca != NULL) {
                /* Print L2 CAT class definitions per L2 cluster */
                unsigned *l2id, count = 0;

                l2id = pqos_cpu_get_l2ids(cpu_info, &count);
                if (l2id == NULL) {
                        printf("Error retrieving information for L2\n");
                        goto free_and_return;
                }

                for (i = 0; i < count; i++) {
                        struct pqos_l2ca tab[PQOS_MAX_L2CA_COS];
                        unsigned num = 0, n = 0;

                        ret = pqos_l2ca_get(l2id[i], PQOS_MAX_L2CA_COS, &num,
                                            tab);
                        if (ret != PQOS_RETVAL_OK)
                                continue;

                        printf("L2CA COS definitions for L2ID %u:\n", l2id[i]);
                        for (n = 0; n < num; n++)
                                print_l2ca_config(&tab[n],
                                                  (ret != PQOS_RETVAL_OK));
                }
                free(l2id);
        }

        /* Print core to class associations */
        for (i = 0; i < sock_count; i++) {
                unsigned *lcores = NULL;
                unsigned lcount = 0, n = 0;

                lcores = pqos_cpu_get_cores(cpu_info, sockets[i], &lcount);
                if (lcores == NULL) {
                        printf("Error retrieving core information!\n");
                        goto free_and_return;
                }
                printf("Core information for socket %u:\n", sockets[i]);
                for (n = 0; n < lcount; n++) {
                        const struct pqos_coreinfo *core_info = NULL;

                        core_info = pqos_cpu_get_core_info(cpu_info, lcores[n]);
                        if (core_info == NULL) {
                                printf("Error retrieving information "
                                       "for core %u!\n",
                                       lcores[n]);
                                free(lcores);
                                goto free_and_return;
                        }

                        print_core_assoc(
                            (cap_l3ca != NULL) || (cap_l2ca != NULL) ||
                                (cap_mba != NULL) /* is_alloc */,
                            cpu_info->l3.detected /* is_l3 */,
                            (cap_mon != NULL) /* is_mon */, core_info);
                }
                free(lcores);
        }

        if (sel_interface == PQOS_INTER_OS) {
                unsigned max_cos = UINT_MAX;

                if (cap_l2ca != NULL)
                        max_cos = max_cos < cap_l2ca->u.l2ca->num_classes
                                      ? max_cos
                                      : cap_l2ca->u.l2ca->num_classes;
                if (cap_l3ca != NULL)
                        max_cos = max_cos < cap_l3ca->u.l3ca->num_classes
                                      ? max_cos
                                      : cap_l3ca->u.l3ca->num_classes;
                if (cap_mba != NULL)
                        max_cos = max_cos < cap_mba->u.mba->num_classes
                                      ? max_cos
                                      : cap_mba->u.mba->num_classes;

                ASSERT(max_cos < UINT_MAX);

                printf("PID association information:\n");

                for (i = !verbose; i < max_cos; i++) {
                        unsigned *tasks = NULL;
                        unsigned tcount = 0, j;

                        tasks = pqos_pid_get_pid_assoc(i, &tcount);
                        if (tasks == NULL) {
                                printf("Error retrieving PID information!\n");
                                goto free_and_return;
                        }
                        printf("    COS%u => ", i);
                        if (tcount == 0)
                                printf("(none)");
                        for (j = 0; j < tcount; j++)
                                if (j == 0)
                                        printf("%u", tasks[j]);
                                else
                                        printf(", %u", tasks[j]);

                        printf("\n");
                        free(tasks);
                }
        }

        /* Print IO RDT associations */
        print_iordt_alloc(cap_mon, cap_l3ca, dev_info);

free_and_return:
        if (sockets)
                free(sockets);
}

int
alloc_apply(const struct pqos_capability *cap_l3ca,
            const struct pqos_capability *cap_l2ca,
            const struct pqos_capability *cap_mba,
            const struct pqos_cpuinfo *cpu,
            const struct pqos_devinfo *dev)
{
        if (cap_l3ca != NULL || cap_l2ca != NULL || cap_mba != NULL) {
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
                ret_assoc = set_allocation_assoc(dev);
                if (ret_assoc < 0) {
                        printf("Allocation association error!\n");
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
                if (sel_assoc_core_num > 0 || sel_alloc_opt_num > 0 ||
                    sel_assoc_pid_num > 0 || sel_assoc_channel_num > 0) {
                        printf("Allocation capability not detected!\n");
                        return -1;
                }
        }

        return 0;
}
