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
 * @brief  Set of utility functions to list and retrieve L3CA setting profiles
 */
#include <stdio.h>
#include <string.h>
#ifdef DEBUG
#include <assert.h>
#endif
#include "profiles.h"
#include "pqos.h"

#ifndef DIM
#define DIM(x) (sizeof(x)/sizeof(x[0]))
#endif

#ifdef DEBUG
#define ASSERT assert
#else
#define ASSERT(x)
#endif

/**
 * 12-cache ways
 */
static const char * const classes_way12_overlapN_equalY[] = {
        "0=0x007",
        "1=0x038",
        "2=0x1C0",
        "3=0xE00"
};

static const char * const classes_way12_overlapN_equalN[] = {
        "0=0x03F",
        "1=0x0C0",
        "2=0x300",
        "3=0xC00"
};

static const char * const classes_way12_overlapP0_equalN[] = {
        "0=0xFFF",
        "1=0x0C0",
        "2=0x300",
        "3=0xC00"
};

static const char * const classes_way12_overlapY_equalN[] = {
        "0=0xFFF",
        "1=0xFF0",
        "2=0xF00",
        "3=0xC00"
};

/**
 * 16-cache ways
 */
static const char * const classes_way16_overlapN_equalY[] = {
        "0=0x000F",
        "1=0x00F0",
        "2=0x0F00",
        "3=0xF000"
};

static const char * const classes_way16_overlapN_equalN[] = {
        "0=0x03FF",
        "1=0x0C00",
        "2=0x3000",
        "3=0xC000"
};

static const char * const classes_way16_overlapP0_equalN[] = {
        "0=0xFFFF",
        "1=0x0C00",
        "2=0x3000",
        "3=0xC000"
};

static const char * const classes_way16_overlapY_equalN[] = {
        "0=0xFFFF",
        "1=0xFF00",
        "2=0xF000",
        "3=0xC000"
};

/**
 * 20-cache ways
 */
static const char * const classes_way20_overlapN_equalY[] = {
        "0=0x0001F",
        "1=0x003E0",
        "2=0x07C00",
        "3=0xF8000"
};

static const char * const classes_way20_overlapN_equalN[] = {
        "0=0x000FF",
        "1=0x00F00",
        "2=0x0F000",
        "3=0xF000"
};

static const char * const classes_way20_overlapP0_equalN[] = {
        "0=0xFFFFF",
        "1=0x0C000",
        "2=0x30000",
        "3=0xC0000"
};

static const char * const classes_way20_overlapY_equalN[] = {
        "0=0xFFFFF",
        "1=0xFF000",
        "2=0xF0000",
        "3=0xC0000"
};

/**
 * meat and potatos now :)
 */
struct llc_allocation_config {
        unsigned num_ways;
        unsigned num_classes;
        const char * const *tab;
};

static const struct llc_allocation_config config_cfg0[] = {
        { .num_ways = 12,
          .num_classes = 4,
          .tab = classes_way12_overlapN_equalY,
        },
        { .num_ways = 16,
          .num_classes = 4,
          .tab = classes_way16_overlapN_equalY,
        },
        { .num_ways = 20,
          .num_classes = 4,
          .tab = classes_way20_overlapN_equalY,
        }
};

static const struct llc_allocation_config config_cfg1[] = {
        { .num_ways = 12,
          .num_classes = 4,
          .tab = classes_way12_overlapN_equalN,
        },
        { .num_ways = 16,
          .num_classes = 4,
          .tab = classes_way16_overlapN_equalN,
        },
        { .num_ways = 20,
          .num_classes = 4,
          .tab = classes_way20_overlapN_equalN,
        }
};

static const struct llc_allocation_config config_cfg2[] = {
        { .num_ways = 12,
          .num_classes = 4,
          .tab = classes_way12_overlapP0_equalN,
        },
        { .num_ways = 16,
          .num_classes = 4,
          .tab = classes_way16_overlapP0_equalN,
        },
        { .num_ways = 20,
          .num_classes = 4,
          .tab = classes_way20_overlapP0_equalN,
        },
};

static const struct llc_allocation_config config_cfg3[] = {
        { .num_ways = 12,
          .num_classes = 4,
          .tab = classes_way12_overlapY_equalN,
        },
        { .num_ways = 16,
          .num_classes = 4,
          .tab = classes_way16_overlapY_equalN,
        },
        { .num_ways = 20,
          .num_classes = 4,
          .tab = classes_way20_overlapY_equalN,
        },
};

struct llc_allocation {
        const char *id;
        const char *descr;
        unsigned num_config;
        const struct llc_allocation_config *config;
};

static const struct llc_allocation allocation_tab[] = {
        { .id = "CFG0",
          .descr = "non-overlapping, ways equally divided",
          .num_config = DIM(config_cfg0),
          .config = config_cfg0,
        },
        { .id = "CFG1",
          .descr = "non-overlapping, ways unequally divided",
          .num_config = DIM(config_cfg1),
          .config = config_cfg1,
        },
        { .id = "CFG2",
          .descr = "overlapping, ways unequally divided, "
          "class 0 can access all ways",
          .num_config = DIM(config_cfg2),
          .config = config_cfg2,
        },
        { .id = "CFG3",
          .descr = "ways unequally divided, overlapping access "
          "for higher classes",
          .num_config = DIM(config_cfg3),
          .config = config_cfg3,
        },
};

void profile_l3ca_list(FILE *fp)
{
        unsigned i = 0, j = 0;

        ASSERT(fp != NULL);
        if (fp == NULL)
                return;

        for (i = 0; i < DIM(allocation_tab); i++) {
                const struct llc_allocation *ap = &allocation_tab[i];

                fprintf(fp,
                        "%u)\n"
                        "      Config ID: %s\n"
                        "    Description: %s\n"
                        " Configurations:\n",
                        i+1, ap->id, ap->descr);
                for (j = 0; j < ap->num_config; j++) {
                        fprintf(fp,
                                "\tnumber of classes = %u, number of cache ways = %u\n",
                                (unsigned) ap->config[j].num_classes,
                                (unsigned) ap->config[j].num_ways);
                }
        }
}

int profile_l3ca_get(const char *id, const struct pqos_cap_l3ca *l3ca,
                     unsigned *p_num, const char * const **p_tab)
{
        unsigned i = 0, j = 0;

        ASSERT(id != NULL);
        ASSERT(l3ca != NULL);
        ASSERT(p_tab != NULL);
        ASSERT(p_num != NULL);

        if (id == NULL || l3ca == NULL || p_tab == NULL || p_num == NULL)
                return PQOS_RETVAL_PARAM;

        for (i = 0; i < DIM(allocation_tab); i++) {
                const struct llc_allocation *ap = &allocation_tab[i];

                if (strcasecmp(id, ap->id) != 0)
                        continue;
                for (j = 0; j < ap->num_config; j++) {
                        if (ap->config[j].num_classes != l3ca->num_classes ||
                            ap->config[j].num_ways != l3ca->num_ways)
                                continue;
                        *p_num = ap->config[j].num_classes;
                        *p_tab = ap->config[j].tab;
                        return PQOS_RETVAL_OK;
                }
        }

        return PQOS_RETVAL_RESOURCE;
}
