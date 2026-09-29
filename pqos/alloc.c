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
 */

/**
 * @brief Platform QoS utility - allocation module
 *
 */
#include "alloc.h"

#include "common.h"
#include "main.h"
#include "pqos.h"
#include "utils.h"

#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

/**
 * Defines used to identify CAT mask definitions
 */
#define CAT_UPDATE_SCOPE_BOTH 0 /**< update CLOS code & data masks */
#define CAT_UPDATE_SCOPE_DATA 1 /**< update CLOS data mask */
#define CAT_UPDATE_SCOPE_CODE 2 /**< update CLOS code mask */

/**
 * Length of max unit64_t value in DEC string
 * + single char for code or data + '\0'
 */
#define MAX_CLOS_MASK_STR_LEN 22

/**
 * Allocation type selected
 */
enum sel_alloc_type { L3CA, L2CA, MBA, MBA_CTRL, SMBA };

/**
 * Array to store allocation options
 */
static char *alloc_opts[32];

/**
 * Memory regions info for allocation
 */
static struct sel_alloc_mem_regions_info {
        int opt_bw_limit_flag;
        int min_bw_limit_flag;
        int max_bw_limit_flag;
        int num_mem_regions;
        uint64_t region_num[PQOS_MAX_MEM_REGIONS];
} sel_alloc_mem_regions;

/**
 * Domain IDs info for allocation
 */
static struct sel_alloc_domain_ids_info {
        uint64_t domain_ids[MAX_DOMAIN_IDS];
        int num_domain_ids;
} sel_alloc_domain_id;

/**
 * Number of allocation options selected
 */
static unsigned sel_alloc_opt_num = 0;

/**
 * Number of allocation settings successfully modified
 */
static unsigned sel_alloc_mod = 0;

/**
 * Number of core to CLOS associations to be done
 */
static int sel_assoc_core_num = 0;

/**
 * Max number of elements in sel_assoc_tab array
 */
static unsigned sel_assoc_tab_size = 128;

/**
 * Core to CLOS associations details
 */
static struct {
        unsigned core;
        unsigned class_id;
} *sel_assoc_tab = NULL;

/**
 * Number of Task ID to CLOS associations to be done
 */
static int sel_assoc_pid_num = 0;

/**
 * Task ID to CLOS associations details
 */
static struct {
        pid_t task_id;
        unsigned class_id;
} sel_assoc_pid_tab[128];

/**
 * Number of IO RDT Channel to CLOS associations to be done
 */
static int sel_assoc_channel_num = 0;

/**
 * IO RDT Channel to CLOS associations details
 */
static struct {
        pqos_channel_t channel;
        unsigned class_id;
} sel_assoc_channel_tab[128];

/**
 * Number of IO RDT Device to CLOS associations to be done
 */
static int sel_assoc_dev_num = 0;

/**
 * IO RDT Device to CLOS associations details
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
 * @brief Whether a string is a single number, decimal or hexadecimal
 *
 * The class id of a class definition takes one number. The two options beside
 * it on the same command line - --alloc-domain-id and --alloc-mem-regions -
 * take a list or a range, so reaching for one here is an easy mistake, and the
 * number helper's own message names neither the option nor what it wanted.
 *
 * @param [in] str string to check
 *
 * @retval 1 the whole string is one number
 * @retval 0 it is not
 */
static int
is_a_single_number(const char *str)
{
        size_t digits;

        if (str == NULL || *str == '\0')
                return 0;

        if (strncmp(str, "0x", 2) == 0 || strncmp(str, "0X", 2) == 0) {
                digits = strspn(str + 2, "0123456789abcdefABCDEF");

                return digits > 0 && str[2 + digits] == '\0';
        }

        digits = strspn(str, "0123456789");

        return str[digits] == '\0';
}

/**
 * @brief Converts string describing allocation CLOS into ID and mask scope
 *
 * Current string format is: <ID>[CcDd]
 *
 * Some examples:
 *  1d  - class 1, data mask
 *  5C  - class 5, code mask
 *  0   - class 0, common mask for code & data
 *
 * @param [in] str string describing allocation CLOS. Function may modify
 *             the last character of the string in some conditions.
 * @param [out] scope indicates if string \a str refers to both CLOS masks
 *              or just one of them
 * @param [out] id class ID referred to in the string \a str
 */
static void
parse_clos_mask_type(char *str, int *scope, unsigned *id)
{
        size_t len = 0;
        char given[MAX_CLOS_MASK_STR_LEN];

        ASSERT(str != NULL);
        ASSERT(scope != NULL);
        ASSERT(id != NULL);

        len = strnlen(str, (size_t)MAX_CLOS_MASK_STR_LEN);
        if (len == MAX_CLOS_MASK_STR_LEN) {
                printf("Error converting allocation CLOS string!\n");
                exit(EXIT_FAILURE);
        }

        /* kept for the message below, which quotes what was typed rather than
         * what is left of it once a scope suffix has been taken off: "abc"
         * reaches the check as "ab", and a reader shown "ab" would look for a
         * mistake they did not make
         */
        strncpy(given, str, sizeof(given) - 1);
        given[sizeof(given) - 1] = '\0';

        if (len > 1 && (str[len - 1] == 'c' || str[len - 1] == 'C')) {
                *scope = CAT_UPDATE_SCOPE_CODE;
                str[len - 1] = '\0';
        } else if (len > 1 && (str[len - 1] == 'd' || str[len - 1] == 'D')) {
                *scope = CAT_UPDATE_SCOPE_DATA;
                str[len - 1] = '\0';
        } else {
                *scope = CAT_UPDATE_SCOPE_BOTH;
        }
        if (!is_a_single_number(str)) {
                printf("Invalid class of service id \"%s\": a class "
                       "definition takes a single number, as in \"mba:1=50\". "
                       "A list or a range is accepted by --alloc-domain-id and "
                       "--alloc-mem-regions, not here.\n",
                       given);
                exit(EXIT_FAILURE);
        }

        *id = (unsigned)strtouint64(str);
}

/**
 * @brief Check whether the platform exposes multiple L3 CAT IDs per socket.
 *
 * On such platforms the IDs used by the MSR allocation interface are L3
 * CAT/Domain IDs rather than socket IDs, so output should be labelled
 * accordingly.
 *
 * @param cpu pqos_cpuinfo list containing all the cores
 *
 * @retval 1 when L3 CAT ID count differs from socket count
 * @retval 0 otherwise (or on lookup failure)
 */
static int
msr_l3_id_differs_from_sockets(const struct pqos_cpuinfo *cpu)
{
        unsigned sc = 0, l3c = 0;
        unsigned *s_tmp = NULL, *l3_tmp = NULL;
        int differs = 0;

        if (cpu == NULL)
                return 0;

        s_tmp = pqos_cpu_get_sockets(cpu, &sc);
        l3_tmp = pqos_cpu_get_l3cat_ids(cpu, &l3c);

        if (s_tmp != NULL && l3_tmp != NULL && sc != l3c)
                differs = 1;

        free(s_tmp);
        free(l3_tmp);
        return differs;
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
set_l3_clos(const unsigned class_id,
            const uint64_t mask,
            const unsigned *sock_ids,
            const unsigned sock_num,
            const unsigned scope,
            const struct pqos_cpuinfo *cpu)
{
        unsigned i, set = 0;
        const char *package;
        unsigned count;
        int ret = PQOS_RETVAL_OK;
        enum pqos_interface interface;
        unsigned id = 0;

        ret = pqos_inter_get(&interface);
        if (ret != PQOS_RETVAL_OK) {
                printf("%s(): Failed to get interface!\n", __func__);
                return -1;
        }

        if (sock_ids == NULL || (mask == 0 && interface != PQOS_INTER_MMIO)) {
                printf("Failed to set L3 CAT configuration!\n");
                return -1;
        }

        if (interface == PQOS_INTER_MMIO) {
                if (sel_alloc_domain_id.num_domain_ids == 0) {
                        printf("--alloc-domain-id option is missing in command "
                               "line. Failed to set IORDT L3 CAT "
                               "configuration!\n");
                        return -1;
                }
                count = sel_alloc_domain_id.num_domain_ids;
        } else
                count = sock_num;

        /**
         * Loop through each socket and set selected classes
         */
        for (i = 0; i < count; i++) {
                unsigned j, num_ca;
                struct pqos_l3ca ca, sock_l3ca[PQOS_MAX_L3CA_CLOS];

                ret = PQOS_RETVAL_OK;
                memset(&ca, 0, sizeof(ca));
                memset(sock_l3ca, 0, sizeof(sock_l3ca));

                if (interface == PQOS_INTER_MMIO) {
                        for (j = 0; j < PQOS_MAX_L3CA_CLOS; j++) {
                                sock_l3ca[j].domain_id =
                                    sel_alloc_domain_id.domain_ids[i];
                        }
                }

                /* get the current L3 definitions of this socket, or of this
                 * domain on the MMIO interface, where a socket does not select
                 * the L3 CAT registers
                 */
                ret = pqos_l3ca_get(sock_ids[i], DIM(sock_l3ca), &num_ca,
                                    sock_l3ca);
                if (ret != PQOS_RETVAL_OK) {
                        if (interface == PQOS_INTER_MMIO)
                                printf("Failed to retrieve L3 classes of "
                                       "domain 0x%" PRIx64 "!\n",
                                       sel_alloc_domain_id.domain_ids[i]);
                        else
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
                if ((cpu->vendor == PQOS_VENDOR_AMD ||
                     cpu->vendor == PQOS_VENDOR_HYGON) &&
                    interface != PQOS_INTER_MMIO)
                        package = "Core Complex";
                else if (interface != PQOS_INTER_MMIO) {
                        /**
                         * On platforms where L3 cache instances do not
                         * map 1:1 to sockets (multiple L3 CAT IDs per
                         * socket), the configured IDs are L3 CAT/Domain
                         * IDs rather than socket IDs. Label the output
                         * accordingly.
                         */
                        if (msr_l3_id_differs_from_sockets(cpu))
                                package = "Domain ID ";
                        else
                                package = "SOCKET";
                } else
                        package = "Domain ID ";

                if (interface == PQOS_INTER_MMIO)
                        id = ca.domain_id;
                else
                        id = sock_ids[i];

                if (ret != PQOS_RETVAL_OK) {
                        printf("%s%u L3CA CLOS%u - FAILED!\n", package, id,
                               ca.class_id);
                        break;
                }

                if (ca.cdp)
                        printf("%s%u L3CA CLOS%u => DATA 0x%lx,CODE "
                               "0x%lx\n",
                               package, id, ca.class_id, (long)ca.u.s.data_mask,
                               (long)ca.u.s.code_mask);
                else
                        printf("%s%u L3CA CLOS%u => MASK 0x%lx\n", package, id,
                               ca.class_id, (long)ca.u.ways_mask);
                set++;
        }
        sel_alloc_mod += set;
        if (set < count)
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
set_l2_clos(const unsigned class_id,
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
                struct pqos_l2ca ca, cluster_l2ca[PQOS_MAX_L2CA_CLOS];

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
                        printf("L2ID %u L2CA CLOS%u - FAILED!\n", l2_ids[i],
                               ca.class_id);
                        break;
                }
                if (ca.cdp)
                        printf("L2ID %u L2CA CLOS%u => DATA 0x%lx,CODE 0x%lx\n",
                               l2_ids[i], ca.class_id, (long)ca.u.s.data_mask,
                               (long)ca.u.s.code_mask);
                else
                        printf("L2ID %u L2CA CLOS%u => MASK 0x%lx\n", l2_ids[i],
                               ca.class_id, (long)ca.u.ways_mask);
                set++;
        }
        sel_alloc_mod += set;
        if (set < id_num)
                return -1;

        return (int)set;
}

/**
 * @brief Print Region Aware MBA command success log
 *
 * @param mba requested table with Domain, CLOS & Memory Regions definition
 * @param actual actual table with Domain, CLOS & Memory Regions definition
 */
static void
print_mmio_mba_log(const struct pqos_mba *mba, const struct pqos_mba *actual)
{
        int region_idx = 0;
        int ctrl_type_idx = 0;
        const char *ctrl_type = NULL;

        for (region_idx = 0; region_idx < mba->num_mem_regions; region_idx++)
                for (ctrl_type_idx = 0; ctrl_type_idx < PQOS_BW_CTRL_TYPE_COUNT;
                     ctrl_type_idx++) {
                        if (mba->mem_regions[region_idx]
                                .bw_ctrl_val[ctrl_type_idx] != -1) {
                                printf("Domain ID %u MBA CLOS%u "
                                       "Memory Region %d ",
                                       mba->domain_id, actual->class_id,
                                       mba->mem_regions[region_idx].region_num);
                                if (ctrl_type_idx == PQOS_BW_CTRL_TYPE_OPT_IDX)
                                        ctrl_type = "Optimal Bandwidth";
                                else if (ctrl_type_idx ==
                                         PQOS_BW_CTRL_TYPE_MIN_IDX)
                                        ctrl_type = "Minimum Bandwidth";
                                else if (ctrl_type_idx ==
                                         PQOS_BW_CTRL_TYPE_MAX_IDX)
                                        ctrl_type = "Maximum Bandwidth";
                                printf("%s=> ", ctrl_type);
                                printf("0x%x requested, 0x%x applied\n",
                                       mba->mem_regions[region_idx]
                                           .bw_ctrl_val[ctrl_type_idx],
                                       actual->mem_regions[region_idx]
                                           .bw_ctrl_val[ctrl_type_idx]);
                        }
                }
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
set_mba_clos(const unsigned class_id,
             const uint64_t available_bw,
             const unsigned *sock_ids,
             const unsigned sock_num,
             int ctrl,
             struct sel_alloc_mem_regions_info *mem_regions,
             const struct pqos_cpuinfo *cpu)
{
        unsigned i, set = 0;
        int idx = 0;
        struct pqos_mba mba, actual;
        unsigned int count = 0;
        unsigned num_mem_regions;
        int ret = PQOS_RETVAL_OK;
        enum pqos_interface interface;

        if (sock_ids == NULL) {
                printf("Failed to set MBA configuration!\n");
                return -1;
        }

        if (available_bw == 0) {
                printf("MBA CLOS%u bandwidth value of 0 is not allowed. "
                       "Failed to set MBA configuration!\n",
                       class_id);
                return -1;
        }

        ret = pqos_inter_get(&interface);
        if (ret != PQOS_RETVAL_OK) {
                printf("%s(): Failed to get interface!\n", __func__);
                return -1;
        }

        if (interface == PQOS_INTER_MMIO) {
                /**
                 * The bandwidth value is copied into the int bw_ctrl_val
                 * fields below, so reject anything the MMIO registers cannot
                 * hold before the value is narrowed.
                 */
                if (available_bw > PQOS_MMIO_MBA_MAX_BW) {
                        printf("MBA CLOS%u bandwidth value %#" PRIx64
                               " is out of range (0x1-%#x). "
                               "Failed to set MBA configuration!\n",
                               class_id, available_bw, PQOS_MMIO_MBA_MAX_BW);
                        return -1;
                }

                if (sel_alloc_domain_id.num_domain_ids == 0 &&
                    mem_regions->num_mem_regions == 0) {
                        printf("--alloc-domain-id & --alloc-mem-region options "
                               "are missing in command line. "
                               "Failed to set MBA configuration!\n");
                        return -1;
                }

                if (sel_alloc_domain_id.num_domain_ids == 0) {
                        printf("--alloc-domain-id option is missing in command "
                               "line. Failed to set MBA configuration!\n");
                        return -1;
                }

                if (mem_regions->num_mem_regions == 0) {
                        printf("--alloc-mem-region option is missing in "
                               "command line. "
                               "Failed to set MBA configuration!\n");
                        return -1;
                }

                /**
                 * The parser only knows the array bound, so reject a region
                 * the platform does not implement now that MRRM is available.
                 */
                if (pqos_platform_mem_regions(&num_mem_regions) != 0)
                        return -1;

                for (idx = 0; idx < mem_regions->num_mem_regions; idx++) {
                        const uint64_t region = mem_regions->region_num[idx];

                        if (region >= num_mem_regions) {
                                printf("Memory region %" PRIu64 " is not "
                                       "supported on this platform, available "
                                       "regions are 0 to %u. "
                                       "Failed to set MBA configuration!\n",
                                       region, num_mem_regions - 1);
                                return -1;
                        }
                }

                count = sel_alloc_domain_id.num_domain_ids;
        } else
                count = sock_num;

        memset(&mba, 0, sizeof(struct pqos_mba));
        memset(&actual, 0, sizeof(struct pqos_mba));
        /* Initialize memory regions info with invalid values */
        memset(&mba.mem_regions[0], -1, sizeof(mba.mem_regions));
        memset(&actual.mem_regions[0], -1, sizeof(actual.mem_regions));

        mba.ctrl = ctrl;
        mba.class_id = class_id;
        mba.mb_max = available_bw;

        /* Copy memory regions & bandwidth control limits */
        for (idx = 0; idx < mem_regions->num_mem_regions; idx++) {
                mba.mem_regions[idx].region_num = mem_regions->region_num[idx];
                /* Check optimal bandwidth limit selection */
                if (mem_regions->opt_bw_limit_flag == 1)
                        /* Assign provided Q value to optimal bandwidth limit */
                        mba.mem_regions[idx]
                            .bw_ctrl_val[PQOS_BW_CTRL_TYPE_OPT_IDX] =
                            available_bw;
                /* Check minimum bandwidth limit selection */
                if (mem_regions->min_bw_limit_flag == 1)
                        /* Assign provided Q value to minimum bandwidth limit */
                        mba.mem_regions[idx]
                            .bw_ctrl_val[PQOS_BW_CTRL_TYPE_MIN_IDX] =
                            available_bw;
                /* Check maximum bandwidth limit selection */
                if (mem_regions->max_bw_limit_flag == 1)
                        /* Assign provided Q value to maximum bandwidth limit */
                        mba.mem_regions[idx]
                            .bw_ctrl_val[PQOS_BW_CTRL_TYPE_MAX_IDX] =
                            available_bw;

                /**
                 * Assign provided Q value to all bandwidth limits,
                 * if no specific bandwidth limit selection
                 */
                if (mem_regions->opt_bw_limit_flag == 0 &&
                    mem_regions->min_bw_limit_flag == 0 &&
                    mem_regions->max_bw_limit_flag == 0) {
                        mba.mem_regions[idx]
                            .bw_ctrl_val[PQOS_BW_CTRL_TYPE_OPT_IDX] =
                            available_bw;
                        mba.mem_regions[idx]
                            .bw_ctrl_val[PQOS_BW_CTRL_TYPE_MIN_IDX] =
                            available_bw;
                        mba.mem_regions[idx]
                            .bw_ctrl_val[PQOS_BW_CTRL_TYPE_MAX_IDX] =
                            available_bw;
                }
        }
        /* Copy memory regions count */
        mba.num_mem_regions = mem_regions->num_mem_regions;

        /**
         * Set all selected classes
         */
        for (i = 0; i < count; i++) {
                const char *unit;
                const char *package;
                int ret = 0;

                if (interface == PQOS_INTER_MMIO) {
                        mba.domain_id = sel_alloc_domain_id.domain_ids[i];
                        ret = pqos_mba_set(sel_alloc_domain_id.domain_ids[i], 1,
                                           &mba, &actual);
                } else
                        ret = pqos_mba_set(sock_ids[i], 1, &mba, &actual);

                if ((cpu->vendor == PQOS_VENDOR_AMD ||
                     cpu->vendor == PQOS_VENDOR_HYGON) &&
                    interface != PQOS_INTER_MMIO) {
                        package = "Core Complex";
                        unit = "";
                } else if (interface != PQOS_INTER_MMIO) {
                        /**
                         * Label the output as "Domain ID" when the
                         * platform exposes multiple L3 CAT IDs per
                         * socket; otherwise keep the legacy "SOCKET"
                         * label.
                         */
                        if (msr_l3_id_differs_from_sockets(cpu))
                                package = "Domain ID";
                        else
                                package = "SOCKET";
                        unit = "%";
                } else
                        package = "Domain ID";

                if (ret != PQOS_RETVAL_OK && interface == PQOS_INTER_MMIO) {
                        printf("%s %u MBA CLOS%u - FAILED!\n", package,
                               mba.domain_id, mba.class_id);
                        break;
                } else if (ret != PQOS_RETVAL_OK) {
                        printf("%s %u MBA CLOS%u - FAILED!\n", package,
                               sock_ids[i], mba.class_id);
                        break;
                }

                if (interface == PQOS_INTER_MMIO)
                        print_mmio_mba_log(&mba, &actual);
                else {
                        printf("%s %u MBA CLOS%u => ", package, sock_ids[i],
                               actual.class_id);
                        if (ctrl == 1)
                                printf("%u MBps\n", mba.mb_max);
                        else
                                printf("%u%s requested, %u%s applied\n",
                                       mba.mb_max, unit, actual.mb_max, unit);
                }

                set++;
        }
        sel_alloc_mod += set;
        if (set < count)
                return -1;

        return (int)set;
}

/**
 * @brief Set SMBA class definitions on selected sockets
 *
 * @param class_id SMBA class ID to set
 * @param available_bw to set
 * @param sock_ids Array of socket ID's to set class definition
 * @param sock_num Number of socket ID's in the array
 * @param ctrl Flag indicating a use of MBA controller
 * @param cpu pqos_cpuinfo list containing all the cores
 *
 * @return Number of classes set
 * @retval -1 on error
 */
static int
set_smba_clos(const unsigned class_id,
              const uint64_t available_bw,
              const unsigned *sock_ids,
              const unsigned sock_num,
              int ctrl,
              const struct pqos_cpuinfo *cpu)
{
        unsigned i, set = 0;
        struct pqos_mba smba, actual;

        if (sock_ids == NULL || available_bw == 0) {
                printf("Failed to set MBA configuration!\n");
                return -1;
        }
        smba.ctrl = ctrl;
        smba.class_id = class_id;
        smba.mb_max = available_bw;
        smba.smba = 1;

        /**
         * Set all selected classes
         */
        for (i = 0; i < sock_num; i++) {
                const char *unit;
                const char *package;
                int ret = pqos_mba_set(sock_ids[i], 1, &smba, &actual);

                if (cpu->vendor == PQOS_VENDOR_AMD) {
                        package = "Core Complex";
                        unit = "";
                } else {
                        package = "SOCKET";
                        unit = "%";
                }

                if (ret != PQOS_RETVAL_OK) {
                        printf("%s %u SMBA CLOS%u - FAILED!\n", package,
                               sock_ids[i], smba.class_id);
                        break;
                }

                printf("%s %u SMBA CLOS%u => ", package, sock_ids[i],
                       actual.class_id);

                if (ctrl == 1)
                        printf("%u MBps\n", smba.mb_max);
                else
                        printf("%u%s requested, %u%s applied\n", smba.mb_max,
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
 * @param res_ids array of resource ID's to set CLOS definition
 * @param res_num number of resource ID's in array
 * @param type allocation type (L2/L3/MBA/MBA CTRL)
 * @param cpu pointer to cpu topology structure
 *
 * @return Number of classes set
 * @retval Positive on success
 * @retval Negative on error
 */
static int
set_allocation_clos(char *str,
                    unsigned *res_ids,
                    const unsigned res_num,
                    const enum sel_alloc_type type,
                    struct sel_alloc_mem_regions_info *mem_regions,
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

        parse_clos_mask_type(str, &update_scope, &class_id);
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
                ret = set_mba_clos(class_id, mask, ids, n, ctrl, mem_regions,
                                   cpu);
                if (res_ids == NULL && ids != NULL)
                        free(ids);
                return ret;
        }

        /* if SMBA selected, set SMBA classes */
        if (type == SMBA) {
                int ctrl = 0;

                if (ids == NULL)
                        ids = pqos_cpu_get_smba_ids(cpu, &n);
                if (ids == NULL) {
                        printf("Failed to retrieve socket info!\n");
                        return -1;
                }
                ret = set_smba_clos(class_id, mask, ids, n, ctrl, cpu);
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
                ret = set_l2_clos(class_id, mask, ids, n, update_scope);
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
        ret = set_l3_clos(class_id, mask, ids, n, update_scope, cpu);
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
set_allocation_class(char *str,
                     struct sel_alloc_mem_regions_info *mem_regions,
                     const struct pqos_cpuinfo *cpu)
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
        else if (strcasecmp(str, "smba") == 0)
                type = SMBA;
        else if (strcasecmp(str, "mba_max") == 0)
                type = MBA_CTRL;
        else {
                printf("Unrecognized allocation type: %s\n", s);
                free(s);
                return ret;
        }
        /**
         * Parse CLOS masks and apply to selected res_ids
         */
        for (++p;; p = NULL) {
                char *token = NULL;

                token = strtok_r(p, ",", &saveptr);
                if (token == NULL)
                        break;
                ret = set_allocation_clos(token, sp, n, type, mem_regions, cpu);
                if (ret <= 0)
                        break;
        }
        free(s);
        return ret;
}

/**
 * The options that only say where and how a class definition given with -e
 * applies, each next to where the parser records that it was given. Naming them
 * once keeps what is detected and what is reported from drifting apart, and
 * lets the report name the options that were actually on the command line.
 */
static const struct {
        const char *name;    /**< as it is written on the command line */
        const int *selected; /**< non-zero once the option has been parsed */
} alloc_class_options[] = {
    {"--alloc-domain-id", &sel_alloc_domain_id.num_domain_ids},
    {"--alloc-mem-regions", &sel_alloc_mem_regions.num_mem_regions},
    {"--alloc-opt-bw", &sel_alloc_mem_regions.opt_bw_limit_flag},
    {"--alloc-min-bw", &sel_alloc_mem_regions.min_bw_limit_flag},
    {"--alloc-max-bw", &sel_alloc_mem_regions.max_bw_limit_flag}};

/**
 * Room for every name in alloc_class_options with ", " between them. The five
 * of them take 90 bytes, and a name added to the table that no longer fits
 * shortens the list rather than running past its end.
 */
#define ALLOC_CLASS_OPTIONS_SIZE 128

/**
 * @brief Whether any of the class-dependent allocation options was given
 *
 * @return whether one of them selects anything
 */
static int
alloc_options_selected(void)
{
        unsigned i;

        for (i = 0; i < DIM(alloc_class_options); i++)
                if (*alloc_class_options[i].selected != 0)
                        return 1;

        return 0;
}

const char *
alloc_requested_option(void)
{
        if (sel_alloc_opt_num > 0)
                return "-e/--alloc-class";

        if (sel_assoc_core_num > 0 || sel_assoc_pid_num > 0 ||
            sel_assoc_dev_num > 0 || sel_assoc_channel_num > 0)
                return "-a/--alloc-assoc";

        /* On its own this is rejected by alloc_check_options() before anyone
         * asks, so it is reached with a class definition beside it, and it
         * names the group rather than the option. Without an article, like the
         * names above it, so that it reads as written wherever a caller puts it
         */
        if (alloc_options_selected())
                return "--alloc-* option";

        return NULL;
}

int
alloc_check_options(const int class_pending)
{
        char given[ALLOC_CLASS_OPTIONS_SIZE] = {0};
        size_t used = 0;
        unsigned i;

        if (class_pending || sel_alloc_opt_num > 0)
                return 0;

        for (i = 0; i < DIM(alloc_class_options); i++) {
                int len;

                if (*alloc_class_options[i].selected == 0)
                        continue;

                len =
                    snprintf(given + used, sizeof(given) - used, "%s%s",
                             used > 0 ? ", " : "", alloc_class_options[i].name);
                if (len < 0 || (size_t)len >= sizeof(given) - used)
                        break;

                used += (size_t)len;
        }

        if (used == 0)
                return 0;

        printf("-e/--alloc-class option is missing in command line. Nothing "
               "reads %s without it: an allocation option only says where and "
               "how a class definition applies!\n",
               given);

        return -1;
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
                ret = set_allocation_class(alloc_opts[i],
                                           &sel_alloc_mem_regions, cpu);
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
        int ret = PQOS_RETVAL_OK;
        enum pqos_interface interface;

        ret = pqos_inter_get(&interface);
        if (ret != PQOS_RETVAL_OK) {
                printf("%s(): Failed to get interface!\n", __func__);
                return -1;
        }

        if (interface == PQOS_INTER_MMIO &&
            (sel_assoc_core_num != 0 || sel_assoc_pid_num != 0)) {
                printf("CAT is not supported in MMIO interface. The clos, llc, "
                       "core and pid options are not available in MMIO "
                       "interface.\nOnly I/O RDT CAT is supported. "
                       "The available options are dev and channel.\n"
                       "Possible Cache Allocation Commands in MMIO: "
                       "\n\tpqos --iface=mmio -a \"dev:<CLOS>=<SEGMENT:"
                       "BUS:DEVICE.FUNCTION@VIRTUAL CHANNEL NUMBERS>\""
                       "\n\tpqos --iface=mmio -a \"channel:"
                       "<CLOS>=<CHANNEL NUMBERS>\"\n");
                return -1;
        }

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
                unsigned clos = sel_assoc_dev_tab[i].class_id;

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
                                                               clos);
                                if (ret != PQOS_RETVAL_OK)
                                        break;
                        }
                } else
                        ret = pqos_alloc_assoc_set_dev(segment, bdf, vc, clos);

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
        long max_cores_count = sysconf(_SC_NPROCESSORS_CONF);
        uint64_t *cores = NULL;
        unsigned i = 0, n = 0, clos = 0;
        char *p = NULL;

        if (max_cores_count <= 0) {
                printf("Failed to get processor count from "
                       "sysconf(_SC_NPROCESSORS_CONF): returned %ld\n",
                       max_cores_count);
                goto error_exit;
        }

        cores = calloc(max_cores_count, sizeof(uint64_t));
        if (cores == NULL) {
                printf("Failed to allocate memory for cores array!\n");
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

        if (strncasecmp(str, "clos:", 4) == 0)
                str += strlen("clos:");
        else if (strncasecmp(str, "llc:", 4) == 0)
                str += strlen("llc:");
        else
                str += strlen("core:");

        p = strchr(str, '=');
        if (p == NULL)
                parse_error(str, "Invalid allocation class of service "
                                 "association format");
        *p = '\0';

        clos = (unsigned)strtouint64(str);
        n = strlisttotab(p + 1, cores, max_cores_count);

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
                        sel_assoc_tab[i].class_id = clos;
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
                         * - update CLOS but warn about it
                         */
                        printf(
                            "warn: updating CLOS for core %u from %u to %u\n",
                            (unsigned)cores[i], sel_assoc_tab[j].class_id,
                            clos);
                        sel_assoc_tab[j].class_id = clos;
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
                        sel_assoc_tab[k].class_id = clos;
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
        unsigned i = 0, n = 0, clos = 0;
        char *p = NULL;

        str += strlen("pid:");
        p = strchr(str, '=');
        if (p == NULL)
                parse_error(str, "Invalid allocation class of service "
                                 "association format");
        *p = '\0';

        clos = (unsigned)strtouint64(str);

        n = strlisttotab(p + 1, tasks, DIM(tasks));
        if (n == 0)
                return;

        if (sel_assoc_pid_num <= 0) {
                for (i = 0; i < n; i++) {
                        if (i >= DIM(sel_assoc_pid_tab))
                                parse_error(str, "too many tasks selected for "
                                                 "allocation association");
                        sel_assoc_pid_tab[i].task_id = (pid_t)tasks[i];
                        sel_assoc_pid_tab[i].class_id = clos;
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
                         * - update CLOS but warn about it
                         */
                        printf(
                            "warn: updating CLOS for task %u from %u to %u\n",
                            (unsigned)tasks[i], sel_assoc_pid_tab[j].class_id,
                            clos);
                        sel_assoc_pid_tab[j].class_id = clos;
                } else {
                        /**
                         * New task is selected - extend the list
                         */
                        unsigned k = (unsigned)sel_assoc_pid_num;

                        if (k >= DIM(sel_assoc_pid_tab))
                                parse_error(str, "too many tasks selected for "
                                                 "allocation association");

                        sel_assoc_pid_tab[k].task_id = (pid_t)tasks[i];
                        sel_assoc_pid_tab[k].class_id = clos;
                        sel_assoc_pid_num++;
                }
        }
}

/**
 * @brief Adds channel and CLOS to internal channel table.
 *
 * @param channel channel to be added
 * @param clos CLOS for channel
 */
static void
add_channel_channel_tab(const pqos_channel_t channel, const unsigned clos)
{
        ASSERT(channel);

        if (sel_assoc_channel_num <= 0) {
                sel_assoc_channel_num = 0;
                sel_assoc_channel_tab[sel_assoc_channel_num].channel = channel;
                sel_assoc_channel_tab[sel_assoc_channel_num].class_id = clos;
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
                 * - update CLOS but warn about it
                 */
                printf("warn: updating CLOS for channel 0x%" PRIx64
                       " from %u to %u\n",
                       channel, sel_assoc_channel_tab[i].class_id, clos);

                sel_assoc_channel_tab[i].class_id = clos;
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
                sel_assoc_channel_tab[j].class_id = clos;
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
        unsigned clos = 0;
        uint16_t segment = 0;
        uint16_t bdf = 0;
        char *vc_str = NULL;
        unsigned vc = DEV_ALL_VCS; /* All channels by default */
        char *p = NULL;

        str += strlen("dev:");
        p = strchr(str, '=');
        if (p == NULL)
                parse_error(str, "Invalid allocation class of service "
                                 "association format.");
        *p = '\0';

        clos = (unsigned)strtouint64(str);

        str = ++p;

        if (pqos_parse_pci_id(str, 1, &segment, &bdf, &vc_str) != 0)
                parse_error(str, "Invalid PCI ID");

        if (vc_str != NULL) {
                uint64_t value;

                if (pqos_parse_uint64(vc_str, &value) != 0)
                        parse_error(vc_str, "Invalid virtual channel");
                if (value >= PQOS_DEV_MAX_CHANNELS) {
                        fprintf(stderr,
                                "Virtual channel %" PRIu64
                                " is out of range [0, %u]\n",
                                value, PQOS_DEV_MAX_CHANNELS - 1);
                        parse_error(vc_str, "Invalid virtual channel");
                }
                vc = (unsigned)value;
        }

        if (sel_assoc_dev_num <= 0) {
                sel_assoc_dev_num = 0;
                sel_assoc_dev_tab[sel_assoc_dev_num].segment = segment;
                sel_assoc_dev_tab[sel_assoc_dev_num].bdf = bdf;
                sel_assoc_dev_tab[sel_assoc_dev_num].vc = vc;
                sel_assoc_dev_tab[sel_assoc_dev_num].class_id = clos;
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
                 * - update CLOS but warn about it
                 */
                printf("warn: updating CLOS for dev %.4x:%.2x:%.2x.%x", segment,
                       BDF_BUS(bdf), BDF_DEV(bdf), BDF_FUNC(bdf));
                if (vc != DEV_ALL_VCS)
                        printf("@%u", vc);
                printf(" from %u to %u.\n", sel_assoc_dev_tab[i].class_id,
                       clos);

                sel_assoc_dev_tab[i].class_id = clos;
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
                sel_assoc_dev_tab[i].class_id = clos;
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
        unsigned clos = 0;
        char *p = NULL;

        str += strlen("channel:");
        p = strchr(str, '=');
        if (p == NULL)
                parse_error(str, "Invalid allocation class of service "
                                 "association format");
        *p = '\0';

        clos = (unsigned)strtouint64(str);
        channel = (pqos_channel_t)strtouint64(p + 1);

        add_channel_channel_tab(channel, clos);
}

/**
 * @brief Verifies allocation association config string.
 *
 * @param str string passed to -a command line option
 */
static void
parse_allocation_assoc(char *str)
{
        if ((strncasecmp(str, "clos:", 4) == 0) ||
            (strncasecmp(str, "llc:", 4) == 0) ||
            (strncasecmp(str, "core:", 5) == 0)) {
                alloc_pid_flag = 0;
                narrow_iface(IFACE_MSR | IFACE_OS, "-a/--alloc-assoc (core)");
                fill_core_tab(str);
        } else if (strncasecmp(str, "pid:", 4) == 0) {
                alloc_pid_flag = 1;
                narrow_iface(IFACE_OS, "-a/--alloc-assoc (pid)");
                fill_pid_tab(str);
        } else if (strncasecmp(str, "dev:", 4) == 0) {
                alloc_pid_flag = 0;
                narrow_iface(IFACE_MSR | IFACE_MMIO, "-a/--alloc-assoc (dev)");
                fill_dev_tab(str);
        } else if (strncasecmp(str, "channel:", 8) == 0) {
                alloc_pid_flag = 0;
                narrow_iface(IFACE_MSR | IFACE_MMIO,
                             "-a/--alloc-assoc (channel)");
                fill_channel_tab(str);
        } else if (strncasecmp(str, "cos:", 4) == 0) {
                printf("Error parsing \"%s\" command line argument.\n", str);
                printf("Allocation type \"cos\" is deprecated. "
                       "Use \"clos\" instead.\n");
                exit(EXIT_FAILURE);
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
print_l3ca_config(const struct pqos_l3ca *ca,
                  const int is_error,
                  const int io_l3ca)
{
        const char *indent = io_l3ca ? "" : "    ";

        if (is_error) {
                printf("%sL3CA CLOS%u => ERROR\n", indent, ca->class_id);
                return;
        }

        if (ca->cdp) {
                printf("%sL3CA CLOS%u => DATA 0x%llx, CODE 0x%llx\n", indent,
                       ca->class_id, (unsigned long long)ca->u.s.data_mask,
                       (unsigned long long)ca->u.s.code_mask);
        } else {
                printf("%sL3CA CLOS%u => MASK 0x%llx\n", indent, ca->class_id,
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
                printf("    L2CA CLOS%u => ERROR\n", ca->class_id);
                return;
        }

        if (ca->cdp) {
                printf("    L2CA CLOS%u => DATA 0x%llx, CODE 0x%llx\n",
                       ca->class_id, (unsigned long long)ca->u.s.data_mask,
                       (unsigned long long)ca->u.s.code_mask);
        } else {
                printf("    L2CA CLOS%u => MASK 0x%llx\n", ca->class_id,
                       (unsigned long long)ca->u.ways_mask);
        }
}

/**
 * @brief Per socket L3 CAT, MBA and SMBA class definition printing
 *
 * If new per socket technologies appear they should be added here, too.
 *
 * @param [in] cap_l3ca pointer to L3 CAT capability structure
 * @param [in] cap_mba pointer to MBA capability structure
 * @param [in] cap_smba pointer to SMBA capability structure
 * @param [in] sock_count number of IDs in \a sockets
 * @param [in] sockets array of IDs (socket ids or L3 CAT/domain ids)
 * @param [in] id_label label used to describe the id (e.g. "Socket" or
 *                      "Domain ID")
 */
static void
print_per_socket_config(const struct pqos_capability *cap_l3ca,
                        const struct pqos_capability *cap_mba,
                        const struct pqos_capability *cap_smba,
                        const struct pqos_cpuinfo *cpu_info,
                        const unsigned sock_count,
                        const unsigned *sockets,
                        const char *id_label)
{
        int ret;
        unsigned i;

        if (cpu_info == NULL || sockets == NULL || id_label == NULL)
                return;

        if (cap_l3ca == NULL && cap_mba == NULL && cap_smba == NULL)
                return;

        for (i = 0; i < sock_count; i++) {

                printf("%s%s%s CLOS definitions for %s %u:\n",
                       cap_l3ca != NULL ? "L3CA" : "",
                       (cap_l3ca != NULL && cap_mba != NULL) ? "/" : "",
                       cap_mba != NULL ? "MBA" : "", id_label, sockets[i]);

                if (cap_l3ca != NULL) {
                        const struct pqos_cap_l3ca *l3ca = cap_l3ca->u.l3ca;
                        struct pqos_l3ca tab[l3ca->num_classes];
                        unsigned num = 0;
                        unsigned n = 0;

                        ret = pqos_l3ca_get(sockets[i], l3ca->num_classes, &num,
                                            tab);

                        if (ret == PQOS_RETVAL_OK) {
                                for (n = 0; n < num; n++)
                                        print_l3ca_config(&tab[n], 0, 0);
                        } else {
                                printf("L3CA: Couldn't obtain info.\n");
                        }
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
                                if (cpu_info->vendor == PQOS_VENDOR_AMD ||
                                    cpu_info->vendor == PQOS_VENDOR_HYGON)
                                        unit = "";
                                else
                                        unit = "%";
                        }

                        memset(&tab, 0,
                               sizeof(struct pqos_mba) * mba->num_classes);

                        ret = pqos_mba_get(sockets[i], mba->num_classes, &num,
                                           tab);

                        if (ret == PQOS_RETVAL_OK) {
                                for (n = 0; n < num; n++)
                                        printf("    MBA CLOS%u => %u%s%s\n",
                                               tab[n].class_id, tab[n].mb_max,
                                               unit, available);
                        } else {
                                printf("MBA: Couldn't obtain info.\n");
                        }
                }

                if (cap_smba != NULL) {
                        const struct pqos_cap_mba *smba = cap_smba->u.smba;
                        struct pqos_mba tab[smba->num_classes];
                        unsigned num = 0;
                        unsigned n = 0;
                        const char *unit;
                        const char *available;

                        if (smba->ctrl_on == 1) {
                                unit = " MBps";
                                available = "";
                        } else {
                                available = " available";
                                if (cpu_info->vendor == PQOS_VENDOR_AMD)
                                        unit = "";
                                else
                                        unit = "%";
                        }

                        for (n = 0; n < smba->num_classes; n++)
                                tab[n].smba = 1;

                        ret = pqos_mba_get(sockets[i], smba->num_classes, &num,
                                           tab);

                        if (ret == PQOS_RETVAL_OK) {
                                for (n = 0; n < num; n++)
                                        printf("    SMBA CLOS%u => %u%s%s\n",
                                               tab[n].class_id, tab[n].mb_max,
                                               unit, available);
                        } else {
                                printf("SMBA: Couldn't obtain info.\n");
                        }
                }
        }
}

/**
 * @brief Per L3 cluster L3 CAT, MBA and SMBA class definition printing
 *
 * If new per L3 cluster technologies appear they should be added here, too.
 *
 * @param [in] cap_l3ca pointer to L3 CAT capability structure
 * @param [in] cap_mba pointer to MBA capability structure
 * @param [in] cap_smba pointer to SMBA capability structure
 * @param [in] l3c_count number of L3 cluster id's in \a l3_clusters
 * @param [in] l3_clusters arrays of L3 cluster id's
 */
static void
print_per_l3_cluster_config(const struct pqos_capability *cap_l3ca,
                            const struct pqos_capability *cap_mba,
                            const struct pqos_capability *cap_smba,
                            const struct pqos_cpuinfo *cpu_info,
                            const unsigned l3c_count,
                            const unsigned *l3_clusters)
{
        int ret;
        unsigned i;

        if (cpu_info == NULL || l3_clusters == NULL)
                return;

        if (cap_l3ca == NULL && cap_mba == NULL && cap_smba == NULL)
                return;

        for (i = 0; i < l3c_count; i++) {

                printf("%s%s%s CLOS definitions for L3 Cluster"
                       " (or Core Complex) %u:\n",
                       cap_l3ca != NULL ? "L3CA" : "",
                       (cap_l3ca != NULL && cap_mba != NULL) ? "/" : "",
                       cap_mba != NULL ? "MBA" : "", l3_clusters[i]);

                if (cap_l3ca != NULL) {
                        const struct pqos_cap_l3ca *l3ca = cap_l3ca->u.l3ca;
                        struct pqos_l3ca tab[l3ca->num_classes];
                        unsigned num = 0;
                        unsigned n = 0;

                        ret = pqos_l3ca_get(l3_clusters[i], l3ca->num_classes,
                                            &num, tab);

                        if (ret == PQOS_RETVAL_OK) {
                                for (n = 0; n < num; n++)
                                        print_l3ca_config(&tab[n], 0, 0);
                        } else {
                                printf("L3CA: Couldn't obtain info.\n");
                        }
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
                                if (cpu_info->vendor == PQOS_VENDOR_AMD ||
                                    cpu_info->vendor == PQOS_VENDOR_HYGON)
                                        unit = "";
                                else
                                        unit = "%";
                        }

                        memset(&tab, 0,
                               sizeof(struct pqos_mba) * mba->num_classes);

                        ret = pqos_mba_get(l3_clusters[i], mba->num_classes,
                                           &num, tab);

                        if (ret == PQOS_RETVAL_OK) {
                                for (n = 0; n < num; n++)
                                        printf("    MBA CLOS%u => %u%s%s\n",
                                               tab[n].class_id, tab[n].mb_max,
                                               unit, available);
                        } else {
                                printf("MBA: Couldn't obtain info.\n");
                        }
                }

                if (cap_smba != NULL) {
                        const struct pqos_cap_mba *smba = cap_smba->u.smba;
                        struct pqos_mba tab[smba->num_classes];
                        unsigned num = 0;
                        unsigned n = 0;
                        const char *unit;
                        const char *available;

                        if (smba->ctrl_on == 1) {
                                unit = " MBps";
                                available = "";
                        } else {
                                available = " available";
                                if (cpu_info->vendor == PQOS_VENDOR_AMD)
                                        unit = "";
                                else
                                        unit = "%";
                        }

                        for (n = 0; n < smba->num_classes; n++)
                                tab[n].smba = 1;

                        ret = pqos_mba_get(l3_clusters[i], smba->num_classes,
                                           &num, tab);

                        if (ret == PQOS_RETVAL_OK) {
                                for (n = 0; n < num; n++)
                                        printf("    SMBA CLOS%u => %u%s%s\n",
                                               tab[n].class_id, tab[n].mb_max,
                                               unit, available);
                        } else {
                                printf("SMBA: Couldn't obtain info.\n");
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
                 const struct pqos_coreinfo *ci,
                 const struct pqos_cores_domains *cores_domains)
{
        unsigned class_id = 0;
        pqos_rmid_t rmid = 0;
        int ret = PQOS_RETVAL_OK;
        enum pqos_interface interface;

        ret = pqos_inter_get(&interface);
        if (ret != PQOS_RETVAL_OK) {
                printf("%s(): Failed to get interface!\n", __func__);
                return;
        }

        if (is_alloc)
                ret = pqos_alloc_assoc_get(ci->lcore, &class_id);

        if (is_mon && ret == PQOS_RETVAL_OK &&
            (interface == PQOS_INTER_MSR || interface == PQOS_INTER_MMIO))
                ret = pqos_mon_assoc_get(ci->lcore, &rmid);

        if (ret == PQOS_RETVAL_OK && interface == PQOS_INTER_MMIO &&
            cores_domains != NULL && cores_domains->domains != NULL) {
                if (ci->lcore < cores_domains->num_cores)
                        printf("    Domain ID  0x%x => ",
                               cores_domains->domains[ci->lcore]);
                else
                        printf("    Domain ID  [INVALID lcore %u] => ",
                               ci->lcore);
        }

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
                printf("CLOS%u", class_id);

        if (is_mon &&
            (interface == PQOS_INTER_MSR || interface == PQOS_INTER_MMIO))
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
                const struct pqos_devinfo *devinfo,
                const struct pqos_channels_domains *channels_domains,
                enum pqos_interface interface)
{
        unsigned vc;
        unsigned int idx;

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

                if (interface == PQOS_INTER_MMIO && channels_domains != NULL)
                        for (idx = 0; idx < channels_domains->num_channel_ids;
                             idx++)
                                if (channel->channel_id ==
                                    channels_domains->channel_ids[idx])
                                        printf(
                                            "    Domain ID  0x%x => ",
                                            channels_domains->domain_ids[idx]);

                printf("    Device %.4x:%.2x:%.2x.%x@%u, Channel 0x%" PRIx64
                       " => ",
                       dev->segment, BDF_BUS(dev->bdf), BDF_DEV(dev->bdf),
                       BDF_FUNC(dev->bdf), vc, dev->channel[vc]);

                if (print_clos) {
                        unsigned class_id = 0;
                        int ret = pqos_alloc_assoc_get_dev(
                            dev->segment, dev->bdf, vc, &class_id);

                        if (ret == PQOS_RETVAL_OK)
                                printf("CLOS%u", class_id);
                        else if (ret == PQOS_RETVAL_RESOURCE)
                                printf("NOCLOS");
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
                    const struct pqos_channel *channel,
                    const struct pqos_channels_domains *channels_domains,
                    enum pqos_interface interface)
{
        pqos_rmid_t rmid = 0;
        int ret = PQOS_RETVAL_OK;
        unsigned int idx;

        if (!channel)
                return;

        const int print_clos = is_alloc && channel->clos_tagging;
        const int print_rmid = is_mon && channel->rmid_tagging;

        if (!(print_clos || print_rmid))
                return;

        if (interface == PQOS_INTER_MMIO && channels_domains != NULL)
                for (idx = 0; idx < channels_domains->num_channel_ids; idx++)
                        if (channel->channel_id ==
                            channels_domains->channel_ids[idx])
                                printf("    Domain ID  0x%x => ",
                                       channels_domains->domain_ids[idx]);

        printf("    Channel 0x%" PRIx64 " => ", channel->channel_id);

        if (print_clos) {
                unsigned class_id = 0;
                int ret = pqos_alloc_assoc_get_channel(channel->channel_id,
                                                       &class_id);

                if (ret == PQOS_RETVAL_OK)
                        printf("CLOS%u", class_id);
                else if (ret == PQOS_RETVAL_RESOURCE)
                        printf("NOCLOS");
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
                  const struct pqos_sysconfig *sys)
{
        size_t i;
        const int is_iordt_alloc = cap_l3ca && cap_l3ca->u.l3ca->iordt_on;
        const int is_iordt_mon = cap_mon && cap_mon->u.mon->iordt_on;
        const struct pqos_devinfo *dev_info = sys ? sys->dev : NULL;
        const struct pqos_channels_domains *channels_domains =
            sys ? sys->channels_domains : NULL;
        enum pqos_interface interface;
        int ret;

        ret = pqos_inter_get(&interface);
        if (ret != PQOS_RETVAL_OK) {
                printf("%s(): Failed to get interface!\n", __func__);
                return;
        }

        if (!(is_iordt_alloc || is_iordt_mon) || !dev_info)
                return;

        printf("Device information:\n");
        for (i = 0; i < dev_info->num_devs; ++i)
                print_dev_assoc(is_iordt_alloc, is_iordt_mon,
                                &dev_info->devs[i], dev_info, channels_domains,
                                interface);

        printf("Control channel information:\n");
        for (i = 0; i < dev_info->num_channels; ++i)
                print_channel_assoc(is_iordt_alloc, is_iordt_mon,
                                    &dev_info->channels[i], channels_domains,
                                    interface);
}

void
alloc_print_config(const struct pqos_capability *cap_mon,
                   const struct pqos_capability *cap_l3ca,
                   const struct pqos_capability *cap_l2ca,
                   const struct pqos_capability *cap_mba,
                   const struct pqos_capability *cap_smba,
                   const struct pqos_sysconfig *sys,
                   const int verbose)
{
        int ret;
        unsigned i;
        unsigned sock_count, *sockets = NULL;
        unsigned l3c_count, *l3_clusters = NULL;
        unsigned l3cat_id_count = 0, *l3cat_ids = NULL;

        if (!sys) {
                printf("Error: 'sys' (pqos_sysconfig) is not available!\n");
                return;
        }

        sockets = pqos_cpu_get_sockets(sys->cpu, &sock_count);
        if (sockets == NULL) {
                printf("Error retrieving information for Sockets\n");
                return;
        }

        l3_clusters = pqos_cpu_get_l3_clusters(sys->cpu, &l3c_count);
        if (l3_clusters == NULL) {
                printf("Error retrieving information for L3 clusters\n");
                goto free_sockets;
        }

        /**
         * AMD and Hygon Platform QoS allocation configuration is
         * per L3 cluster (or Core Complex - CCX).
         *
         * Each L3 cluster (CCX) has one L3 cluster ID (l3_id)
         * which is used for both CAT and MBA ids.
         */
        if (sys->cpu->vendor == PQOS_VENDOR_AMD ||
            sys->cpu->vendor == PQOS_VENDOR_HYGON) {
                print_per_l3_cluster_config(cap_l3ca, cap_mba, cap_smba,
                                            sys->cpu, l3c_count, l3_clusters);
        } else {
                /**
                 * Some platforms expose multiple L3 cache instances
                 * (multiple L3 CAT IDs) per socket. In that case the
                 * CLOS definitions are reported per L3 CAT/Domain ID
                 * instead of per Socket.
                 */
                l3cat_ids = pqos_cpu_get_l3cat_ids(sys->cpu, &l3cat_id_count);
                if (l3cat_ids != NULL && l3cat_id_count != sock_count)
                        print_per_socket_config(cap_l3ca, cap_mba, cap_smba,
                                                sys->cpu, l3cat_id_count,
                                                l3cat_ids, "Domain ID");
                else
                        print_per_socket_config(cap_l3ca, cap_mba, cap_smba,
                                                sys->cpu, sock_count, sockets,
                                                "Socket");
        }

        if (cap_l2ca != NULL) {
                /* Print L2 CAT class definitions per L2 cluster */
                unsigned *l2id, count = 0;

                l2id = pqos_cpu_get_l2ids(sys->cpu, &count);
                if (l2id == NULL) {
                        printf("Error retrieving information for L2\n");
                        goto free_and_return;
                }

                for (i = 0; i < count; i++) {
                        struct pqos_l2ca tab[PQOS_MAX_L2CA_CLOS];
                        unsigned num = 0, n = 0;

                        ret = pqos_l2ca_get(l2id[i], PQOS_MAX_L2CA_CLOS, &num,
                                            tab);
                        if (ret != PQOS_RETVAL_OK)
                                continue;

                        printf("L2CA CLOS definitions for L2ID %u:\n", l2id[i]);
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

                lcores = pqos_cpu_get_cores(sys->cpu, sockets[i], &lcount);
                if (lcores == NULL) {
                        printf("Error retrieving core information!\n");
                        goto free_and_return;
                }
                printf("Core information for socket %u:\n", sockets[i]);
                for (n = 0; n < lcount; n++) {
                        const struct pqos_coreinfo *core_info = NULL;

                        core_info = pqos_cpu_get_core_info(sys->cpu, lcores[n]);
                        if (core_info == NULL) {
                                printf("Error retrieving information "
                                       "for core %u!\n",
                                       lcores[n]);
                                free(lcores);
                                goto free_and_return;
                        }

                        print_core_assoc((cap_l3ca != NULL) ||
                                             (cap_l2ca != NULL) ||
                                             (cap_mba != NULL) /* is_alloc */,
                                         sys->cpu->l3.detected /* is_l3 */,
                                         (cap_mon != NULL) /* is_mon */,
                                         core_info, sys->cores_domains);
                }
                free(lcores);
        }

        if (sel_interface == PQOS_INTER_OS) {
                unsigned max_clos = UINT_MAX;

                if (cap_l2ca != NULL)
                        max_clos = max_clos < cap_l2ca->u.l2ca->num_classes
                                       ? max_clos
                                       : cap_l2ca->u.l2ca->num_classes;
                if (cap_l3ca != NULL)
                        max_clos = max_clos < cap_l3ca->u.l3ca->num_classes
                                       ? max_clos
                                       : cap_l3ca->u.l3ca->num_classes;
                if (cap_mba != NULL)
                        max_clos = max_clos < cap_mba->u.mba->num_classes
                                       ? max_clos
                                       : cap_mba->u.mba->num_classes;

                ASSERT(max_clos < UINT_MAX);

                printf("PID association information:\n");

                for (i = !verbose; i < max_clos; i++) {
                        unsigned *tasks = NULL;
                        unsigned tcount = 0, j;

                        tasks = pqos_pid_get_pid_assoc(i, &tcount);
                        if (tasks == NULL) {
                                printf("Error retrieving PID information!\n");
                                goto free_and_return;
                        }
                        printf("    CLOS%u => ", i);
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
        print_iordt_alloc(cap_mon, cap_l3ca, sys);

free_and_return:
        free(l3cat_ids);
        free(l3_clusters);
free_sockets:
        free(sockets);
}

static void
print_mba(const struct pqos_mba *mba)
{
        int region_idx = 0;
        int ctrl_type_idx = 0;
        const char *ctrl_type = NULL;

        for (region_idx = 0; region_idx < mba->num_mem_regions; region_idx++)
                for (ctrl_type_idx = 0; ctrl_type_idx < PQOS_BW_CTRL_TYPE_COUNT;
                     ctrl_type_idx++) {
                        if (mba->mem_regions[region_idx]
                                .bw_ctrl_val[ctrl_type_idx] != -1) {
                                printf("Domain ID %u MBA CLOS%u "
                                       "Memory Region %d ",
                                       mba->domain_id, mba->class_id,
                                       mba->mem_regions[region_idx].region_num);
                                if (ctrl_type_idx == PQOS_BW_CTRL_TYPE_OPT_IDX)
                                        ctrl_type = "Optimal Bandwidth";
                                else if (ctrl_type_idx ==
                                         PQOS_BW_CTRL_TYPE_MIN_IDX)
                                        ctrl_type = "Minimum Bandwidth";
                                else if (ctrl_type_idx ==
                                         PQOS_BW_CTRL_TYPE_MAX_IDX)
                                        ctrl_type = "Maximum Bandwidth";
                                printf("%s=> ", ctrl_type);
                                printf("0x%x\n",
                                       mba->mem_regions[region_idx]
                                           .bw_ctrl_val[ctrl_type_idx]);
                        }
                }
}

/**
 * @brief Whether a CPU domain has MBA registers to display
 *
 * The MBA registers of a domain are in the MARC block of its CPU agent, and a
 * CPU agent is valid without one. This mirrors the library's own rule - a copy
 * of it, not a call, so the two have to agree - all three bandwidth control
 * windows, because pqos_mba_get() reads all three and refuses a domain that
 * cannot serve them. Admitting a domain the library will refuse would put the
 * abort below back, which is what this predicate exists to avoid.
 *
 * @param [in] cpu_agent CPU agent to examine
 *
 * @return whether the domain has MBA registers
 */
static int
domain_has_mba(const struct pqos_cpu_agent_info *cpu_agent)
{
        return cpu_agent->marc.reg_block_size != 0 &&
               cpu_agent->marc.opt_bw_reg_block_base_addr != 0 &&
               cpu_agent->marc.min_bw_reg_block_base_addr != 0 &&
               cpu_agent->marc.max_bw_reg_block_base_addr != 0;
}

/**
 * @brief Whether a device domain has I/O L3 CAT registers to display
 *
 * The I/O L3 CAT registers of a domain are in the CARD block of its device
 * agent, and a device agent enumerated for monitoring alone leaves that block
 * zeroed. This mirrors the library's own predicate, dev_agent_has_l3ca() in
 * lib/mmio_allocation.c - it is a copy, not a call, and the two have to agree -
 * because pqos_l3ca_get() refuses a domain that fails it, and admitting one
 * here would put the abort below back.
 *
 * @param [in] dev_agent device agent to examine
 *
 * @return whether the domain has I/O L3 CAT registers
 */
static int
domain_has_l3ca(const struct pqos_device_agent_info *dev_agent)
{
        return dev_agent->card.reg_base_addr != 0 &&
               dev_agent->card.reg_block_size != 0;
}

void
print_domain_alloc_config(const struct pqos_capability *cap_mon,
                          const struct pqos_capability *cap_l3ca,
                          const struct pqos_capability *cap_l2ca,
                          const struct pqos_capability *cap_mba,
                          const struct pqos_sysconfig *sys)
{
        int ret;
        unsigned i;
        unsigned idx;
        unsigned clos_idx;
        unsigned num_ca;
        unsigned sock_count;
        unsigned *sockets = NULL;
        struct pqos_mba *mba_tab = NULL;
        struct pqos_l3ca l3ca[PQOS_MAX_L3CA_CLOS];

        if (!sys) {
                printf("Error: 'sys' (pqos_sysconfig) is not available!\n");
                return;
        }

        if (!sys->erdt) {
                printf("Error: 'erdt' acpi table is not available in 'sys'!\n");
                return;
        }

        if (!sys->mrrm) {
                printf("Error: 'mrrm' acpi table is not available in 'sys'!\n");
                return;
        }

        sockets = pqos_cpu_get_sockets(sys->cpu, &sock_count);
        if (sockets == NULL) {
                printf("Error retrieving information for Sockets\n");
                return;
        }

        for (idx = 0; idx < sys->erdt->num_dev_agents; idx++) {
                memset(l3ca, 0, sizeof(l3ca));
                for (clos_idx = 0; clos_idx < sys->erdt->max_clos; clos_idx++)
                        l3ca[clos_idx].domain_id =
                            sys->erdt->dev_agents[idx].rmdd.domain_id;

                /* A device agent enumerated for monitoring alone has no
                 * I/O L3 CAT register to show, and pqos_l3ca_get() refuses
                 * such a domain. That is right for a domain the user named,
                 * but here the domains come from the platform, so skip it:
                 * failing would hide the configuration of every domain
                 * enumerated after it, the MBA of every CPU domain, and the
                 * core associations printed below.
                 */
                if (!domain_has_l3ca(&sys->erdt->dev_agents[idx]))
                        continue;

                ret = pqos_l3ca_get(sys->erdt->dev_agents[idx].rmdd.domain_id,
                                    DIM(l3ca), &num_ca, l3ca);
                if (ret != PQOS_RETVAL_OK) {
                        printf("Error retrieving I/O L3CA configuration for "
                               "Domain ID 0x%x: pqos_l3ca_get() returned %d\n",
                               sys->erdt->dev_agents[idx].rmdd.domain_id, ret);
                        goto free_and_return;
                }

                for (clos_idx = 0; clos_idx < num_ca; clos_idx++) {
                        printf("Domain ID 0x%x I/O ", l3ca[clos_idx].domain_id);
                        print_l3ca_config(&l3ca[clos_idx], 0, 1);
                }

                printf("\n");
        }

        mba_tab = calloc(sys->erdt->max_clos, sizeof(*mba_tab));
        if (mba_tab == NULL) {
                printf("Error allocating memory for MBA configuration\n");
                goto free_and_return;
        }

        for (idx = 0; idx < sys->erdt->num_cpu_agents; idx++) {
                unsigned supported;
                unsigned num_mba = 0;

                /* A CPU agent enumerated without a MARC block has no MBA
                 * register to show, and pqos_mba_get() refuses such a domain.
                 * That is right for a domain the user named, but here the
                 * domains come from the platform, so skip it: failing would
                 * hide the MBA of every domain enumerated after it, and the
                 * core associations printed below.
                 */
                if (!domain_has_mba(&sys->erdt->cpu_agents[idx]))
                        continue;

                if (pqos_platform_mem_regions(&supported) != 0)
                        goto free_and_return;

                mba_tab[0].domain_id =
                    sys->erdt->cpu_agents[idx].rmdd.domain_id;
                mba_tab[0].num_mem_regions = (int)supported;

                ret = pqos_mba_get(sys->erdt->cpu_agents[idx].rmdd.domain_id,
                                   sys->erdt->max_clos, &num_mba, mba_tab);
                if (ret != PQOS_RETVAL_OK) {
                        printf("Error retrieving MMIO registers for Domain "
                               "ID %x: pqos_mba_get() returned %d\n",
                               sys->erdt->cpu_agents[idx].rmdd.domain_id, ret);
                        goto free_and_return;
                }

                for (clos_idx = 0; clos_idx < num_mba; clos_idx++) {
                        print_mba(&mba_tab[clos_idx]);
                        printf("\n");
                }
                printf("\n");
        }

        /* Print core to class associations */
        for (i = 0; i < sock_count; i++) {
                unsigned *lcores = NULL;
                unsigned lcount = 0, n = 0;

                lcores = pqos_cpu_get_cores(sys->cpu, sockets[i], &lcount);
                if (lcores == NULL) {
                        printf("Error retrieving core information!\n");
                        goto free_and_return;
                }
                printf("Core information for socket %u:\n", sockets[i]);
                for (n = 0; n < lcount; n++) {
                        const struct pqos_coreinfo *core_info = NULL;

                        core_info = pqos_cpu_get_core_info(sys->cpu, lcores[n]);
                        if (core_info == NULL) {
                                printf("Error retrieving information "
                                       "for core %u!\n",
                                       lcores[n]);
                                free(lcores);
                                goto free_and_return;
                        }

                        print_core_assoc((cap_l3ca != NULL) ||
                                             (cap_l2ca != NULL) ||
                                             (cap_mba != NULL) /* is_alloc */,
                                         sys->cpu->l3.detected /* is_l3 */,
                                         (cap_mon != NULL) /* is_mon */,
                                         core_info, sys->cores_domains);
                }
                free(lcores);
        }

        /* Print IO RDT associations */
        print_iordt_alloc(cap_mon, cap_l3ca, sys);

free_and_return:
        free(mba_tab);
        free(sockets);
}

/**
 * @brief Selects the memory regions an allocation applies to
 *
 * @param [in] arg the list given to --alloc-mem-regions: region numbers in any
 *             form strlisttotab() reads - commas, ranges, decimal or
 *             hexadecimal, and duplicates dropped as it reads them - which this
 *             then checks for range, exiting where one is out of it
 */
void
selfn_alloc_mem_regions(const char *arg)
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

        sel_alloc_mem_regions.num_mem_regions = 0;
        for (i = 0; i < PQOS_MAX_MEM_REGIONS; i++)
                sel_alloc_mem_regions.region_num[i] = -1;

        n = strlisttotab(str, sel_alloc_mem_regions.region_num,
                         PQOS_MAX_MEM_REGIONS);
        if (n == 0) {
                printf("No Mem Region specified: %s\n", str);
                exit(EXIT_FAILURE);
        }

        sel_alloc_mem_regions.num_mem_regions = n;

        /* check for invalid memory region */
        for (i = 0; i < n; i++) {
                if (sel_alloc_mem_regions.region_num[i] >=
                    PQOS_MAX_MEM_REGIONS) {
                        printf("Mem Region out of range: %s\n", str);
                        exit(EXIT_FAILURE);
                }
        }

        /* Check duplicate memory region entry */
        for (i = 0; i < n; i++) {
                for (j = i + 1; j < n; j++) {
                        if (sel_alloc_mem_regions.region_num[i] ==
                            sel_alloc_mem_regions.region_num[j]) {
                                parse_error(str,
                                            "Duplicate Mem Region selection");
                                printf("The Mem Region %ld is entered two "
                                       "times\n",
                                       sel_alloc_mem_regions.region_num[i]);
                                exit(EXIT_FAILURE);
                        }
                }
        }

        free(str);
}

/**
 * @brief Selects optimal bandwidth in memory regions for allocation
 *
 * @param arg not used
 */
void
selfn_alloc_opt_bw(const char *arg)
{
        UNUSED_ARG(arg);
        sel_alloc_mem_regions.opt_bw_limit_flag = 1;
}

/**
 * @brief Selects minimum bandwidth in memory regions for allocation
 *
 * @param arg not used
 */
void
selfn_alloc_min_bw(const char *arg)
{
        UNUSED_ARG(arg);
        sel_alloc_mem_regions.min_bw_limit_flag = 1;
}

/**
 * @brief Selects maximum bandwidth in memory regions for allocation
 *
 * @param arg not used
 */
void
selfn_alloc_max_bw(const char *arg)
{
        UNUSED_ARG(arg);
        sel_alloc_mem_regions.max_bw_limit_flag = 1;
}

/**
 * @brief Selects the domains an allocation applies to
 *
 * @param [in] arg the list given to --alloc-domain-id: domain identifiers in
 *             any form strlisttotab() reads - commas, ranges, decimal or
 *             hexadecimal, and duplicates dropped as it reads them - which
 *             selfn_domain_id_list() then checks for range, exiting where one
 *             is out of it
 */
void
selfn_alloc_domain_id(const char *arg)
{
        sel_alloc_domain_id.num_domain_ids = selfn_domain_id_list(
            arg, sel_alloc_domain_id.domain_ids, MAX_DOMAIN_IDS);
}

int
alloc_apply(const struct pqos_capability *cap_l3ca,
            const struct pqos_capability *cap_l2ca,
            const struct pqos_capability *cap_mba,
            const struct pqos_capability *cap_smba,
            const struct pqos_cpuinfo *cpu,
            const struct pqos_devinfo *dev)
{
        if (cap_l3ca != NULL || cap_l2ca != NULL || cap_mba != NULL ||
            cap_smba != NULL) {
                /**
                 * If allocation config changed then exit.
                 * For monitoring, start the program again unless
                 * config file was provided
                 */
                int ret_assoc = 0, ret_clos = 0;

                ret_clos = set_alloc(cpu);
                if (ret_clos < 0) {
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
                if (ret_assoc > 0 || ret_clos > 0) {
                        printf("Allocation configuration altered.\n");
                        return 1;
                }
        } else {
                /* the same question as the one asked of the command line
                 * before anything ran, so that an allocation option added to
                 * one of them is reported by both
                 */
                if (alloc_requested_option() != NULL) {
                        printf("Allocation capability not detected!\n");
                        return -1;
                }
        }

        return 0;
}
