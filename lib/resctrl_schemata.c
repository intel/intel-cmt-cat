/*
 * BSD LICENSE
 *
 * Copyright(c) 2019-2020 Intel Corporation. All rights reserved.
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

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "resctrl_schemata.h"
#include "resctrl_utils.h"
#include "types.h"
#include "log.h"

/*
 * @brief Structure to hold parsed schemata
 */
struct resctrl_schemata {
        unsigned l3ids_num; /**< Number of L3 */
        unsigned *l3ids;
        struct pqos_l3ca *l3ca; /**< L3 COS definitions */

        unsigned mbaids_num; /**< Number of mba ids */
        unsigned *mbaids;
        struct pqos_mba *mba; /**< MBA COS definitions */

        unsigned l2ids_num; /**< Number of L2 clusters */
        unsigned *l2ids;
        struct pqos_l2ca *l2ca; /**< L2 COS definitions */
};

void
resctrl_schemata_free(struct resctrl_schemata *schemata)
{
        if (schemata == NULL)
                return;

        if (schemata->l2ca != NULL)
                free(schemata->l2ca);
        if (schemata->l3ca != NULL)
                free(schemata->l3ca);
        if (schemata->mba != NULL)
                free(schemata->mba);
        if (schemata->l2ids != NULL)
                free(schemata->l2ids);
        if (schemata->l3ids != NULL)
                free(schemata->l3ids);
        if (schemata->mbaids != NULL)
                free(schemata->mbaids);
        free(schemata);
}

struct resctrl_schemata *
resctrl_schemata_alloc(const struct pqos_cap *cap,
                       const struct pqos_cpuinfo *cpu)
{
        struct resctrl_schemata *schemata;
        int retval;
        const struct pqos_capability *cap_l2ca;
        const struct pqos_capability *cap_l3ca;
        const struct pqos_capability *cap_mba;
        unsigned i;

        schemata = (struct resctrl_schemata *)calloc(1, sizeof(*schemata));
        if (schemata == NULL)
                return NULL;

        /* L2 */
        retval = pqos_cap_get_type(cap, PQOS_CAP_TYPE_L2CA, &cap_l2ca);
        if (retval == PQOS_RETVAL_OK && cap_l2ca != NULL) {
                schemata->l2ids = pqos_cpu_get_l2ids(cpu, &schemata->l2ids_num);
                if (schemata->l2ids == NULL)
                        goto resctrl_schemata_alloc_error;

                schemata->l2ca =
                    calloc(schemata->l2ids_num, sizeof(struct pqos_l2ca));
                if (schemata->l2ca == NULL)
                        goto resctrl_schemata_alloc_error;
        }

        /* L3 */
        retval = pqos_cap_get_type(cap, PQOS_CAP_TYPE_L3CA, &cap_l3ca);
        if (retval == PQOS_RETVAL_OK && cap_l3ca != NULL) {
                schemata->l3ids =
                    pqos_cpu_get_l3cat_ids(cpu, &schemata->l3ids_num);
                if (schemata->l3ids == NULL)
                        goto resctrl_schemata_alloc_error;

                schemata->l3ca =
                    calloc(schemata->l3ids_num, sizeof(struct pqos_l3ca));
                if (schemata->l3ca == NULL)
                        goto resctrl_schemata_alloc_error;
        }

        /* MBA */
        retval = pqos_cap_get_type(cap, PQOS_CAP_TYPE_MBA, &cap_mba);
        if (retval == PQOS_RETVAL_OK && cap_mba != NULL) {
                int ctrl_enabled;

                if (schemata->mbaids == NULL) {
                        schemata->mbaids =
                            pqos_cpu_get_mba_ids(cpu, &schemata->mbaids_num);
                        if (schemata->mbaids == NULL)
                                goto resctrl_schemata_alloc_error;
                }

                schemata->mba =
                    calloc(schemata->mbaids_num, sizeof(struct pqos_mba));
                if (schemata->mba == NULL)
                        goto resctrl_schemata_alloc_error;

                retval = pqos_mba_ctrl_enabled(cap, NULL, &ctrl_enabled);
                if (retval != PQOS_RETVAL_OK)
                        goto resctrl_schemata_alloc_error;

                /* fill ctrl and class_id */
                for (i = 0; i < schemata->mbaids_num; i++)
                        schemata->mba[i].ctrl = ctrl_enabled;
        }

        return schemata;

resctrl_schemata_alloc_error:
        resctrl_schemata_free(schemata);
        return NULL;
}

int
resctrl_schemata_reset(struct resctrl_schemata *schmt,
                       const struct pqos_cap_l3ca *l3ca_cap,
                       const struct pqos_cap_l2ca *l2ca_cap,
                       const struct pqos_cap_mba *mba_cap)
{
        unsigned j;

        /* Reset L3 CAT */
        if (l3ca_cap != NULL) {
                uint64_t default_l3ca = ((uint64_t)1 << l3ca_cap->num_ways) - 1;

                for (j = 0; j < schmt->l3ids_num; j++) {
                        if (l3ca_cap->cdp_on) {
                                schmt->l3ca[j].cdp = 1;
                                schmt->l3ca[j].u.s.code_mask = default_l3ca;
                                schmt->l3ca[j].u.s.data_mask = default_l3ca;
                        } else {
                                schmt->l3ca[j].cdp = 0;
                                schmt->l3ca[j].u.ways_mask = default_l3ca;
                        }
                }
        }

        /* Reset L2 CAT */
        if (l2ca_cap != NULL) {
                uint64_t default_l2ca = ((uint64_t)1 << l2ca_cap->num_ways) - 1;

                for (j = 0; j < schmt->l2ids_num; j++) {
                        if (l2ca_cap->cdp_on) {
                                schmt->l2ca[j].cdp = 1;
                                schmt->l2ca[j].u.s.code_mask = default_l2ca;
                                schmt->l2ca[j].u.s.data_mask = default_l2ca;
                        } else {
                                schmt->l2ca[j].cdp = 0;
                                schmt->l2ca[j].u.ways_mask = default_l2ca;
                        }
                }
        }

        /* Reset MBA */
        if (mba_cap != NULL) {
                uint32_t default_mba = 100;

                /* kernel always rounds up value to MBA granularity */
                if (mba_cap->ctrl_on)
                        default_mba =
                            UINT32_MAX - UINT32_MAX % mba_cap->throttle_step;

                for (j = 0; j < schmt->mbaids_num; j++)
                        schmt->mba[j].mb_max = default_mba;
        }

        return PQOS_RETVAL_OK;
}

static int
get_l2_index(const struct resctrl_schemata *schemata, unsigned resource_id)
{
        unsigned i;

        ASSERT(schemata->l2ids != NULL);

        for (i = 0; i < schemata->l2ids_num; ++i)
                if (schemata->l2ids[i] == resource_id)
                        return i;

        return -1;
}

int
resctrl_schemata_l2ca_get(const struct resctrl_schemata *schemata,
                          unsigned resource_id,
                          struct pqos_l2ca *ca)
{
        int index;

        ASSERT(schemata != NULL);
        ASSERT(ca != NULL);
        ASSERT(schemata->l2ca != NULL);

        index = get_l2_index(schemata, resource_id);
        if (index < 0)
                return PQOS_RETVAL_ERROR;

        *ca = schemata->l2ca[index];

        return PQOS_RETVAL_OK;
}

int
resctrl_schemata_l2ca_set(struct resctrl_schemata *schemata,
                          unsigned resource_id,
                          const struct pqos_l2ca *ca)
{
        int index;

        ASSERT(schemata != NULL);
        ASSERT(ca != NULL);
        ASSERT(schemata->l2ca != NULL);

        index = get_l2_index(schemata, resource_id);
        if (index < 0)
                return PQOS_RETVAL_ERROR;

        schemata->l2ca[index] = *ca;

        return PQOS_RETVAL_OK;
}

static int
get_l3_index(const struct resctrl_schemata *schemata, unsigned resource_id)
{
        unsigned i;

        ASSERT(schemata->l3ids != NULL);

        for (i = 0; i < schemata->l3ids_num; ++i)
                if (schemata->l3ids[i] == resource_id)
                        return i;

        return -1;
}

int
resctrl_schemata_l3ca_get(const struct resctrl_schemata *schemata,
                          unsigned resource_id,
                          struct pqos_l3ca *ca)
{
        int index;

        ASSERT(schemata != NULL);
        ASSERT(ca != NULL);
        ASSERT(schemata->l3ca != NULL);

        index = get_l3_index(schemata, resource_id);
        if (index < 0)
                return PQOS_RETVAL_ERROR;

        *ca = schemata->l3ca[index];

        return PQOS_RETVAL_OK;
}

int
resctrl_schemata_l3ca_set(struct resctrl_schemata *schemata,
                          unsigned resource_id,
                          const struct pqos_l3ca *ca)
{
        int index;

        ASSERT(schemata != NULL);
        ASSERT(ca != NULL);
        ASSERT(schemata->l3ca != NULL);

        index = get_l3_index(schemata, resource_id);
        if (index < 0)
                return PQOS_RETVAL_ERROR;

        schemata->l3ca[index] = *ca;

        return PQOS_RETVAL_OK;
}

static int
get_mba_index(const struct resctrl_schemata *schemata, unsigned resource_id)
{
        unsigned i;

        ASSERT(schemata->mbaids != NULL);

        for (i = 0; i < schemata->mbaids_num; ++i)
                if (schemata->mbaids[i] == resource_id)
                        return i;

        return -1;
}

int
resctrl_schemata_mba_get(const struct resctrl_schemata *schemata,
                         unsigned resource_id,
                         struct pqos_mba *ca)
{
        int index;

        ASSERT(schemata != NULL);
        ASSERT(ca != NULL);
        ASSERT(schemata->mba != NULL);

        index = get_mba_index(schemata, resource_id);
        if (index < 0)
                return PQOS_RETVAL_ERROR;

        *ca = schemata->mba[index];

        return PQOS_RETVAL_OK;
}

int
resctrl_schemata_mba_set(struct resctrl_schemata *schemata,
                         unsigned resource_id,
                         const struct pqos_mba *ca)
{
        int index;

        ASSERT(schemata != NULL);
        ASSERT(ca != NULL);
        ASSERT(schemata->mba != NULL);

        index = get_mba_index(schemata, resource_id);
        if (index < 0)
                return PQOS_RETVAL_ERROR;

        schemata->mba[index] = *ca;

        return PQOS_RETVAL_OK;
}

/**
 * @brief Schemata type
 */
enum resctrl_schemata_type {
        RESCTRL_SCHEMATA_TYPE_NONE,   /**< unknown */
        RESCTRL_SCHEMATA_TYPE_L2,     /**< L2 CAT without CDP */
        RESCTRL_SCHEMATA_TYPE_L2CODE, /**< L2 CAT code */
        RESCTRL_SCHEMATA_TYPE_L2DATA, /**< L2 CAT data */
        RESCTRL_SCHEMATA_TYPE_L3,     /**< L3 CAT without CDP */
        RESCTRL_SCHEMATA_TYPE_L3CODE, /**< L3 CAT code */
        RESCTRL_SCHEMATA_TYPE_L3DATA, /**< L3 CAT data */
        RESCTRL_SCHEMATA_TYPE_MB,     /**< MBA data */
};

/**
 * @brief Determine allocation type
 *
 * @param [in] str resctrl label
 *
 * @return Allocation type
 */
static int
resctrl_schemata_type_get(const char *str)
{
        int type = RESCTRL_SCHEMATA_TYPE_NONE;

        if (strcasecmp(str, "L2") == 0)
                type = RESCTRL_SCHEMATA_TYPE_L2;
        else if (strcasecmp(str, "L2CODE") == 0)
                type = RESCTRL_SCHEMATA_TYPE_L2CODE;
        else if (strcasecmp(str, "L2DATA") == 0)
                type = RESCTRL_SCHEMATA_TYPE_L2DATA;
        else if (strcasecmp(str, "L3") == 0)
                type = RESCTRL_SCHEMATA_TYPE_L3;
        else if (strcasecmp(str, "L3CODE") == 0)
                type = RESCTRL_SCHEMATA_TYPE_L3CODE;
        else if (strcasecmp(str, "L3DATA") == 0)
                type = RESCTRL_SCHEMATA_TYPE_L3DATA;
        else if (strcasecmp(str, "MB") == 0)
                type = RESCTRL_SCHEMATA_TYPE_MB;

        return type;
}

/**
 * @brief Fill schemata structure
 *
 * @param [in] res_id Resource id
 * @param [in] value Ways mask/Memory B/W rate
 * @param [in] type Schemata type
 * @param [out] schemata Schemata structure
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK on success
 */
static int
resctrl_schemata_set(const unsigned res_id,
                     const uint64_t value,
                     const int type,
                     struct resctrl_schemata *schemata)
{
        int index = -1;

        switch (type) {
        case RESCTRL_SCHEMATA_TYPE_L2:
        case RESCTRL_SCHEMATA_TYPE_L2CODE:
        case RESCTRL_SCHEMATA_TYPE_L2DATA:
                index = get_l2_index(schemata, res_id);
                break;
        case RESCTRL_SCHEMATA_TYPE_L3:
        case RESCTRL_SCHEMATA_TYPE_L3CODE:
        case RESCTRL_SCHEMATA_TYPE_L3DATA:
                index = get_l3_index(schemata, res_id);
                break;
        case RESCTRL_SCHEMATA_TYPE_MB:
                index = get_mba_index(schemata, res_id);
                break;
        }

        if (index < 0)
                return PQOS_RETVAL_ERROR;

        switch (type) {
        case RESCTRL_SCHEMATA_TYPE_L2:
                schemata->l2ca[index].cdp = 0;
                schemata->l2ca[index].u.ways_mask = value;
                break;
        case RESCTRL_SCHEMATA_TYPE_L2CODE:
                schemata->l2ca[index].cdp = 1;
                schemata->l2ca[index].u.s.code_mask = value;
                break;
        case RESCTRL_SCHEMATA_TYPE_L2DATA:
                schemata->l2ca[index].cdp = 1;
                schemata->l2ca[index].u.s.data_mask = value;
                break;
        case RESCTRL_SCHEMATA_TYPE_L3:
                schemata->l3ca[index].cdp = 0;
                schemata->l3ca[index].u.ways_mask = value;
                break;
        case RESCTRL_SCHEMATA_TYPE_L3CODE:
                schemata->l3ca[index].cdp = 1;
                schemata->l3ca[index].u.s.code_mask = value;
                break;
        case RESCTRL_SCHEMATA_TYPE_L3DATA:
                schemata->l3ca[index].cdp = 1;
                schemata->l3ca[index].u.s.data_mask = value;
                break;
        case RESCTRL_SCHEMATA_TYPE_MB:
                schemata->mba[index].mb_max = value;
                break;
        }

        return PQOS_RETVAL_OK;
}

int
resctrl_schemata_read(FILE *fd, struct resctrl_schemata *schemata)
{
        int ret = PQOS_RETVAL_OK;
        int type = RESCTRL_SCHEMATA_TYPE_NONE;
        char *p = NULL, *q = NULL, *saveptr = NULL;
        const size_t buf_size = 16 * 1024;
        char *buf = calloc(buf_size, sizeof(*buf));

        if (buf == NULL) {
                ret = PQOS_RETVAL_ERROR;
                goto resctrl_schemata_read_exit;
        }

        while (fgets(buf, buf_size, fd) != NULL) {
                q = buf;
                /**
                 * Trim white spaces
                 */
                while (isspace(*q))
                        q++;

                /**
                 * Determine allocation type
                 */
                p = strchr(q, ':');
                if (p == NULL) {
                        ret = PQOS_RETVAL_ERROR;
                        break;
                }
                *p = '\0';
                type = resctrl_schemata_type_get(q);

                /* Skip unknown label */
                if (type == RESCTRL_SCHEMATA_TYPE_NONE)
                        continue;

                /**
                 * Parse COS masks
                 */
                for (++p;; p = NULL) {
                        char *token = NULL;
                        uint64_t id = 0;
                        uint64_t value = 0;
                        unsigned base =
                            (type == RESCTRL_SCHEMATA_TYPE_MB ? 10 : 16);

                        token = strtok_r(p, ";", &saveptr);
                        if (token == NULL)
                                break;

                        q = strchr(token, '=');
                        if (q == NULL) {
                                ret = PQOS_RETVAL_ERROR;
                                goto resctrl_schemata_read_exit;
                        }
                        *q = '\0';

                        ret = resctrl_utils_strtouint64(token, 10, &id);
                        if (ret != PQOS_RETVAL_OK)
                                goto resctrl_schemata_read_exit;

                        ret = resctrl_utils_strtouint64(q + 1, base, &value);
                        if (ret != PQOS_RETVAL_OK)
                                goto resctrl_schemata_read_exit;

                        ret = resctrl_schemata_set(id, value, type, schemata);
                        if (ret != PQOS_RETVAL_OK)
                                goto resctrl_schemata_read_exit;
                }
        }

resctrl_schemata_read_exit:
        if (buf != NULL)
                free(buf);

        return ret;
}

int
resctrl_schemata_write(FILE *fd, const struct resctrl_schemata *schemata)
{
        resctrl_schemata_l2ca_write(fd, schemata);
        resctrl_schemata_l3ca_write(fd, schemata);
        resctrl_schemata_mba_write(fd, schemata);

        return PQOS_RETVAL_OK;
}

int
resctrl_schemata_l3ca_write(FILE *fd, const struct resctrl_schemata *schemata)
{
        unsigned i;

        if (schemata->l3ca == NULL)
                return PQOS_RETVAL_OK;

        /* L3 without CDP */
        if (!schemata->l3ca[0].cdp) {
                fprintf(fd, "L3:");
                for (i = 0; i < schemata->l3ids_num; i++) {
                        unsigned id = schemata->l3ids[i];
                        unsigned long long mask =
                            (unsigned long long)schemata->l3ca[i].u.ways_mask;

                        if (i > 0)
                                fprintf(fd, ";");
                        fprintf(fd, "%u=%llx", id, mask);
                }
                fprintf(fd, "\n");
        }

        /* L3 with CDP */
        if (schemata->l3ca[0].cdp) {
                fprintf(fd, "L3CODE:");
                for (i = 0; i < schemata->l3ids_num; i++) {
                        unsigned id = schemata->l3ids[i];
                        unsigned long long mask =
                            (unsigned long long)schemata->l3ca[i].u.s.code_mask;

                        if (i > 0)
                                fprintf(fd, ";");
                        fprintf(fd, "%u=%llx", id, mask);
                }
                fprintf(fd, "\nL3DATA:");
                for (i = 0; i < schemata->l3ids_num; i++) {
                        unsigned id = schemata->l3ids[i];
                        unsigned long long mask =
                            (unsigned long long)schemata->l3ca[i].u.s.data_mask;

                        if (i > 0)
                                fprintf(fd, ";");
                        fprintf(fd, "%u=%llx", id, mask);
                }
                fprintf(fd, "\n");
        }

        return PQOS_RETVAL_OK;
}

int
resctrl_schemata_l2ca_write(FILE *fd, const struct resctrl_schemata *schemata)
{
        unsigned i;

        if (schemata->l2ca == NULL)
                return PQOS_RETVAL_OK;

        /* L2 without CDP */
        if (!schemata->l2ca[0].cdp) {
                fprintf(fd, "L2:");
                for (i = 0; i < schemata->l2ids_num; i++) {
                        unsigned id = schemata->l2ids[i];
                        unsigned long long mask =
                            (unsigned long long)schemata->l2ca[i].u.ways_mask;

                        if (i > 0)
                                fprintf(fd, ";");
                        fprintf(fd, "%u=%llx", id, mask);
                }
                fprintf(fd, "\n");
        }

        /* L2 with CDP */
        if (schemata->l2ca[0].cdp) {
                fprintf(fd, "L2CODE:");
                for (i = 0; i < schemata->l2ids_num; i++) {
                        unsigned id = schemata->l2ids[i];
                        unsigned long long mask =
                            (unsigned long long)schemata->l2ca[i].u.s.code_mask;

                        if (i > 0)
                                fprintf(fd, ";");
                        fprintf(fd, "%u=%llx", id, mask);
                }
                fprintf(fd, "\nL2DATA:");
                for (i = 0; i < schemata->l2ids_num; i++) {
                        unsigned id = schemata->l2ids[i];
                        unsigned long long mask =
                            (unsigned long long)schemata->l2ca[i].u.s.data_mask;

                        if (i > 0)
                                fprintf(fd, ";");
                        fprintf(fd, "%u=%llx", id, mask);
                }
                fprintf(fd, "\n");
        }

        return PQOS_RETVAL_OK;
}

int
resctrl_schemata_mba_write(FILE *fd, const struct resctrl_schemata *schemata)
{
        unsigned i;

        if (schemata->mba == NULL)
                return PQOS_RETVAL_OK;

        fprintf(fd, "MB:");
        for (i = 0; i < schemata->mbaids_num; i++) {
                unsigned id = schemata->mbaids[i];

                if (i > 0)
                        fprintf(fd, ";");
                fprintf(fd, "%u=%u", id, schemata->mba[i].mb_max);
        }
        fprintf(fd, "\n");

        return PQOS_RETVAL_OK;
}
