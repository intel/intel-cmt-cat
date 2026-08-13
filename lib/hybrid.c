/*
 * BSD LICENSE
 *
 * Copyright(c) 2026 Intel Corporation. All rights reserved.
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

#include "hybrid.h"

#include "log.h"
#include "machine.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#ifdef __FreeBSD__
#include <sys/cpuset.h>

typedef cpuset_t cpu_set_t;
#include <sys/param.h>
#endif
#ifdef __linux__
#include <sched.h>
#endif

#define CPUID_FEATURES        0x7U
#define CPUID_NATIVE_MODEL    0x1aU
#define CPUID_MONITORING      0xfU
#define CPUID_ALLOCATION      0x10U
#define CPUID_ASYM_MONITORING 0x27U
#define CPUID_ASYM_ALLOCATION 0x28U

#define HYBRID_BIT          (1U << 15)
#define RDT_MONITORING_BIT  (1U << 12)
#define RDT_ALLOCATION_BIT  (1U << 15)
#define ASYM_MONITORING_BIT (1U << 0)
#define ASYM_ALLOCATION_BIT (1U << 1)
#define MON_RESOURCE_MASK   (1U << 1)
#define RP_RESOURCE_MASK    (1U << 6)
#define ALLOC_RESOURCE_MASK                                                    \
        ((1U << 1) | (1U << 2) | (1U << 3) | (1U << 5) | (1U << 6))

struct field_definition {
        enum hybrid_field field;
        unsigned reg;
        uint32_t mask;
        unsigned shift;
        uint32_t add;
};

static const struct field_definition mon_l3_fields[] = {
    {HYBRID_FIELD_COUNTER_WIDTH, 0, 0xffU, 0, 24},
    {HYBRID_FIELD_OVERFLOW, 0, 1U << 8, 8, 0},
    {HYBRID_FIELD_IO_CMT, 0, 1U << 9, 9, 0},
    {HYBRID_FIELD_IO_MBM, 0, 1U << 10, 10, 0},
    {HYBRID_FIELD_CONVERSION_FACTOR, 1, UINT32_MAX, 0, 0},
    {HYBRID_FIELD_MAX_RMID, 2, UINT32_MAX, 0, 0},
    {HYBRID_FIELD_L3_OCCUP, 3, 1U, 0, 0},
    {HYBRID_FIELD_L3_TOTAL_BW, 3, 1U << 1, 1, 0},
    {HYBRID_FIELD_L3_LOCAL_BW, 3, 1U << 2, 2, 0}};

static const struct field_definition cat_l3_fields[] = {
    {HYBRID_FIELD_CBM_LENGTH, 0, 0x1fU, 0, 1},
    {HYBRID_FIELD_CONTENTION_MASK, 1, UINT32_MAX, 0, 0},
    {HYBRID_FIELD_NON_CPU_AGENT, 2, 1U << 1, 1, 0},
    {HYBRID_FIELD_CDP, 2, 1U << 2, 2, 0},
    {HYBRID_FIELD_NON_CONTIGUOUS_CBM, 2, 1U << 3, 3, 0},
    {HYBRID_FIELD_MAX_CLOS, 3, 0xffffU, 0, 0}};

static const struct field_definition cat_l2_fields[] = {
    {HYBRID_FIELD_CBM_LENGTH, 0, 0x1fU, 0, 1},
    {HYBRID_FIELD_CONTENTION_MASK, 1, UINT32_MAX, 0, 0},
    {HYBRID_FIELD_CDP, 2, 1U << 2, 2, 0},
    {HYBRID_FIELD_NON_CONTIGUOUS_CBM, 2, 1U << 3, 3, 0},
    {HYBRID_FIELD_MAX_CLOS, 3, 0xffffU, 0, 0}};

static const struct field_definition mba_fields[] = {
    {HYBRID_FIELD_MAX_THROTTLE, 0, 0xfffU, 0, 1},
    {HYBRID_FIELD_PER_THREAD_CONTROL, 2, 1U, 0, 0},
    {HYBRID_FIELD_LINEAR_RESPONSE, 2, 1U << 2, 2, 0},
    {HYBRID_FIELD_MAX_CLOS, 3, 0xffffU, 0, 0}};

static const struct field_definition cba_fields[] = {
    {HYBRID_FIELD_MAX_LEVELS, 0, 0xffU, 0, 1},
    {HYBRID_FIELD_BANDWIDTH_SCOPE, 0, 0xfU << 8, 8, 0},
    {HYBRID_FIELD_LINEAR_RESPONSE, 2, 1U << 3, 3, 0},
    {HYBRID_FIELD_MAX_CLOS, 3, 0xffffU, 0, 0}};

static const struct field_definition priority_fields[] = {
    {HYBRID_FIELD_PER_THREAD_ENABLE, 0, 1U, 0, 0},
    {HYBRID_FIELD_PER_PACKAGE_ENABLE, 0, 1U << 1, 1, 0}};

/**
 * @brief Executes CPUID on the current logical processor
 *
 * @param [in] leaf CPUID leaf
 * @param [in] subleaf CPUID subleaf
 * @param [out] out CPUID register values
 * @param [in] context Unused callback context
 *
 * @return Operation status
 * @retval 0 Success
 */
static int
native_cpuid(unsigned leaf,
             unsigned subleaf,
             struct hybrid_cpuid_out *out,
             void *context)
{
        struct cpuid_out result;

        (void)context;
        lcpuid(leaf, subleaf, &result);
        out->eax = result.eax;
        out->ebx = result.ebx;
        out->ecx = result.ecx;
        out->edx = result.edx;
        return 0;
}

/**
 * @brief Reads supported resource subleaves
 *
 * @param [in] cpuid CPUID reader callback
 * @param [in] context CPUID callback context
 * @param [in] leaf CPUID capability leaf
 * @param [in] resources Supported resource bitmap
 * @param [out] out CPUID values indexed by resource identifier
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK Success
 * @retval PQOS_RETVAL_ERROR CPUID read failed
 */
static int
read_subleaves(hybrid_cpuid_fn cpuid,
               void *context,
               unsigned leaf,
               uint32_t resources,
               struct hybrid_cpuid_out *out)
{
        unsigned id;

        for (id = 1; id <= PQOS_HYBRID_MAX_RESOURCE_ID; id++)
                if ((resources & (1U << id)) != 0 &&
                    cpuid(leaf, id, &out[id], context) != 0)
                        return PQOS_RETVAL_ERROR;
        return PQOS_RETVAL_OK;
}

/**
 * @brief Reads one regular or asymmetric capability leaf
 *
 * @param [in] cpuid CPUID reader callback
 * @param [in] context CPUID callback context
 * @param [in] enabled Non-zero when the leaf is supported
 * @param [in] leaf CPUID capability leaf
 * @param [in] resources_in_ebx Non-zero when EBX contains the resource bitmap
 * @param [in] resource_mask Supported resource mask
 * @param [out] resources Detected resource bitmap
 * @param [out] out CPUID values indexed by resource identifier
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK Success or unsupported leaf
 * @retval PQOS_RETVAL_ERROR CPUID read failed
 */
static int
read_capability(hybrid_cpuid_fn cpuid,
                void *context,
                int enabled,
                unsigned leaf,
                int resources_in_ebx,
                uint32_t resource_mask,
                uint32_t *resources,
                struct hybrid_cpuid_out *out)
{
        if (!enabled)
                return PQOS_RETVAL_OK;
        if (cpuid(leaf, 0, &out[0], context) != 0)
                return PQOS_RETVAL_ERROR;
        *resources =
            (resources_in_ebx ? out[0].ebx : out[0].edx) & resource_mask;
        return read_subleaves(cpuid, context, leaf, *resources, out);
}

int
hybrid_cap_read(hybrid_cpuid_fn cpuid,
                void *context,
                struct hybrid_core_capability *cap)
{
        struct hybrid_cpuid_out leaf0, leaf7 = {0}, leaf7_1 = {0};

        if (cpuid == NULL || cap == NULL)
                return PQOS_RETVAL_PARAM;
        memset(cap, 0, sizeof(*cap));
        cap->mem_size = sizeof(*cap);
        if (cpuid(0, 0, &leaf0, context) != 0)
                return PQOS_RETVAL_ERROR;
        cap->max_leaf = leaf0.eax;
        if (cap->max_leaf < CPUID_FEATURES)
                return PQOS_RETVAL_RESOURCE;
        if (cpuid(CPUID_FEATURES, 0, &leaf7, context) != 0)
                return PQOS_RETVAL_ERROR;

        if ((leaf7.edx & HYBRID_BIT) == 0)
                return PQOS_RETVAL_RESOURCE;
        if (cap->max_leaf >= CPUID_NATIVE_MODEL) {
                struct hybrid_cpuid_out model;

                if (cpuid(CPUID_NATIVE_MODEL, 0, &model, context) != 0)
                        return PQOS_RETVAL_ERROR;
                if (model.eax != 0) {
                        cap->core_type_valid = 1;
                        cap->core_type = (uint8_t)(model.eax >> 24);
                        cap->native_model_id = model.eax & 0xffffffU;
                }
        }
        if (leaf7.eax >= 1 && cpuid(CPUID_FEATURES, 1, &leaf7_1, context) != 0)
                return PQOS_RETVAL_ERROR;

        cap->regular_mon_supported = cap->max_leaf >= CPUID_MONITORING &&
                                     (leaf7.ebx & RDT_MONITORING_BIT) != 0;
        cap->regular_alloc_supported = cap->max_leaf >= CPUID_ALLOCATION &&
                                       (leaf7.ebx & RDT_ALLOCATION_BIT) != 0;
        cap->mon_supported = cap->max_leaf >= CPUID_ASYM_MONITORING &&
                             (leaf7_1.ecx & ASYM_MONITORING_BIT) != 0;
        cap->alloc_supported = cap->max_leaf >= CPUID_ASYM_ALLOCATION &&
                               (leaf7_1.ecx & ASYM_ALLOCATION_BIT) != 0;

        if (read_capability(cpuid, context, cap->regular_mon_supported,
                            CPUID_MONITORING, 0, MON_RESOURCE_MASK,
                            &cap->regular_mon_resources,
                            cap->regular_mon) != PQOS_RETVAL_OK ||
            read_capability(cpuid, context, cap->regular_alloc_supported,
                            CPUID_ALLOCATION, 1, ALLOC_RESOURCE_MASK,
                            &cap->regular_alloc_resources,
                            cap->regular_alloc) != PQOS_RETVAL_OK ||
            read_capability(cpuid, context, cap->mon_supported,
                            CPUID_ASYM_MONITORING, 0, MON_RESOURCE_MASK,
                            &cap->mon_resources, cap->mon) != PQOS_RETVAL_OK ||
            read_capability(cpuid, context, cap->alloc_supported,
                            CPUID_ASYM_ALLOCATION, 1, ALLOC_RESOURCE_MASK,
                            &cap->alloc_resources,
                            cap->alloc) != PQOS_RETVAL_OK)
                return PQOS_RETVAL_ERROR;
        return PQOS_RETVAL_OK;
}

/**
 * @brief Selects a register value from CPUID output
 *
 * @param [in] out CPUID register values
 * @param [in] reg Register index from EAX through EDX
 *
 * @return Selected register value
 */
static uint32_t
register_value(const struct hybrid_cpuid_out *out, unsigned reg)
{
        if (reg == 0)
                return out->eax;
        if (reg == 1)
                return out->ebx;
        if (reg == 2)
                return out->ecx;
        return out->edx;
}

/**
 * @brief Decodes a capability field from CPUID output
 *
 * @param [in] out CPUID register values
 * @param [in] field Field decoding definition
 *
 * @return Decoded field value
 */
static uint32_t
field_value(const struct hybrid_cpuid_out *out,
            const struct field_definition *field)
{
        return ((register_value(out, field->reg) & field->mask) >>
                field->shift) +
               field->add;
}

/**
 * @brief Records a difference between regular and asymmetric values
 *
 * @param [in,out] cap Logical processor hybrid capability
 * @param [in] resource Resource containing the field
 * @param [in] field Compared capability field
 * @param [in] regular Regular CPUID value
 * @param [in] asymmetric Asymmetric CPUID value
 */
static void
add_difference(struct hybrid_core_capability *cap,
               enum hybrid_resource resource,
               enum hybrid_field field,
               uint32_t regular,
               uint32_t asymmetric)
{
        struct hybrid_difference *difference;

        if (regular == asymmetric)
                return;
        if (cap->num_differences >= PQOS_HYBRID_MAX_DIFFERENCES) {
                LOG_ERROR("Too many hybrid capability differences on "
                          "logical core %u\n",
                          cap->lcore);
                return;
        }
        difference = &cap->differences[cap->num_differences++];
        difference->resource = resource;
        difference->field = field;
        difference->regular = regular;
        difference->asymmetric = asymmetric;
}

/**
 * @brief Compares decoded fields for one resource
 *
 * @param [in,out] cap Logical processor hybrid capability
 * @param [in] regular Regular CPUID values
 * @param [in] asymmetric Asymmetric CPUID values
 * @param [in] fields Field decoding definitions
 * @param [in] num_fields Number of field definitions
 * @param [in] resource Compared resource
 */
static void
compare_fields(struct hybrid_core_capability *cap,
               const struct hybrid_cpuid_out *regular,
               const struct hybrid_cpuid_out *asymmetric,
               const struct field_definition *fields,
               unsigned num_fields,
               enum hybrid_resource resource)
{
        unsigned i;

        for (i = 0; i < num_fields; i++)
                add_difference(cap, resource, fields[i].field,
                               field_value(regular, &fields[i]),
                               field_value(asymmetric, &fields[i]));
}

/**
 * @brief Compares one allocation resource
 *
 * @param [in,out] cap Logical processor hybrid capability
 * @param [in] id CPUID resource identifier
 * @param [in] resource Compared resource
 * @param [in] fields Field decoding definitions
 * @param [in] num_fields Number of field definitions
 */
static void
compare_alloc_resource(struct hybrid_core_capability *cap,
                       unsigned id,
                       enum hybrid_resource resource,
                       const struct field_definition *fields,
                       unsigned num_fields)
{
        const int regular = (cap->regular_alloc_resources & (1U << id)) != 0;
        const int asymmetric = (cap->alloc_resources & (1U << id)) != 0;

        add_difference(cap, resource, HYBRID_FIELD_SUPPORT, regular,
                       asymmetric);
        if (regular && asymmetric)
                compare_fields(cap, &cap->regular_alloc[id], &cap->alloc[id],
                               fields, num_fields, resource);
}

int
hybrid_cap_compare(struct hybrid_core_capability *cap)
{
        int regular, asymmetric;

        if (cap == NULL)
                return PQOS_RETVAL_PARAM;
        cap->num_differences = 0;
        add_difference(cap, HYBRID_RESOURCE_MONITORING,
                       HYBRID_FIELD_ENUMERATION_SUPPORT,
                       cap->regular_mon_supported, cap->mon_supported);
        if (cap->mon_supported && cap->regular_mon_supported)
                add_difference(cap, HYBRID_RESOURCE_MONITORING,
                               HYBRID_FIELD_MAX_RMID, cap->regular_mon[0].ebx,
                               cap->mon[0].ebx);

        regular = (cap->regular_mon_resources & MON_RESOURCE_MASK) != 0;
        asymmetric = (cap->mon_resources & MON_RESOURCE_MASK) != 0;
        add_difference(cap, HYBRID_RESOURCE_L3_MON, HYBRID_FIELD_SUPPORT,
                       regular, asymmetric);
        if (regular && asymmetric)
                compare_fields(cap, &cap->regular_mon[1], &cap->mon[1],
                               mon_l3_fields,
                               sizeof(mon_l3_fields) / sizeof(mon_l3_fields[0]),
                               HYBRID_RESOURCE_L3_MON);

        add_difference(cap, HYBRID_RESOURCE_ALLOCATION,
                       HYBRID_FIELD_ENUMERATION_SUPPORT,
                       cap->regular_alloc_supported, cap->alloc_supported);
        compare_alloc_resource(cap, 1, HYBRID_RESOURCE_L3_CAT, cat_l3_fields,
                               sizeof(cat_l3_fields) /
                                   sizeof(cat_l3_fields[0]));
        compare_alloc_resource(cap, 2, HYBRID_RESOURCE_L2_CAT, cat_l2_fields,
                               sizeof(cat_l2_fields) /
                                   sizeof(cat_l2_fields[0]));
        compare_alloc_resource(cap, 3, HYBRID_RESOURCE_MBA, mba_fields,
                               sizeof(mba_fields) / sizeof(mba_fields[0]));
        compare_alloc_resource(cap, 5, HYBRID_RESOURCE_CBA, cba_fields,
                               sizeof(cba_fields) / sizeof(cba_fields[0]));
        compare_alloc_resource(
            cap, 6, HYBRID_RESOURCE_PRIORITY, priority_fields,
            sizeof(priority_fields) / sizeof(priority_fields[0]));
        return PQOS_RETVAL_OK;
}

int
hybrid_cap_rp_supported(const struct hybrid_capabilities *cap)
{
        unsigned i;

        if (cap == NULL || cap->status != HYBRID_STATUS_YES)
                return 0;
        for (i = 0; i < cap->num_cores; i++)
                if (cap->cores[i].alloc_supported &&
                    (cap->cores[i].alloc_resources & RP_RESOURCE_MASK) != 0)
                        return 1;
        return 0;
}

/**
 * @brief Sets affinity of the current thread
 *
 * @param [in] set CPU affinity mask
 * @param [in] max_cores Number of bits represented by the mask
 *
 * @return Operation status
 * @retval 0 Success
 * @retval -1 Failure
 */
static int
set_affinity_mask(const cpu_set_t *set, unsigned max_cores)
{
#ifdef __linux__
        return sched_setaffinity(0, CPU_ALLOC_SIZE(max_cores), set);
#elif defined(__FreeBSD__)
        (void)max_cores;
        return cpuset_setaffinity(CPU_LEVEL_WHICH, CPU_WHICH_TID, -1,
                                  sizeof(*set), set);
#else
        (void)set;
        (void)max_cores;
        return -1;
#endif
}

/**
 * @brief Gets affinity of the current thread
 *
 * @param [out] set CPU affinity mask
 * @param [in] max_cores Number of bits represented by the mask
 *
 * @return Operation status
 * @retval 0 Success
 * @retval -1 Failure
 */
static int
get_affinity_mask(cpu_set_t *set, unsigned max_cores)
{
#ifdef __linux__
        return sched_getaffinity(0, CPU_ALLOC_SIZE(max_cores), set);
#elif defined(__FreeBSD__)
        (void)max_cores;
        return cpuset_getaffinity(CPU_LEVEL_WHICH, CPU_WHICH_TID, -1,
                                  sizeof(*set), set);
#else
        (void)set;
        (void)max_cores;
        return -1;
#endif
}

/**
 * @brief Reads hybrid capabilities on one logical processor
 *
 * The original affinity mask is restored before returning.
 *
 * @param [in] lcore Logical processor identifier
 * @param [in] max_cores Number of processors represented by affinity masks
 * @param [in] original Original CPU affinity mask
 * @param [out] cap Logical processor hybrid capability
 *
 * @return PQoS operation status
 */
static int
read_core(unsigned lcore,
          unsigned max_cores,
          const cpu_set_t *original,
          struct hybrid_core_capability *cap)
{
        const size_t set_size = CPU_ALLOC_SIZE(max_cores);
        cpu_set_t *target = CPU_ALLOC(max_cores);
        int ret;

        if (target == NULL) {
                LOG_ERROR("Unable to allocate CPU affinity mask\n");
                return PQOS_RETVAL_ERROR;
        }
        CPU_ZERO_S(set_size, target);
        CPU_SET_S(lcore, set_size, target);
        if (set_affinity_mask(target, max_cores) != 0) {
                LOG_INFO("Logical core %u is unavailable to this process: "
                         "%s\n",
                         lcore, strerror(errno));
                CPU_FREE(target);
                return PQOS_RETVAL_UNAVAILABLE;
        }

        ret = hybrid_cap_read(native_cpuid, NULL, cap);
        if (set_affinity_mask(original, max_cores) != 0) {
                LOG_ERROR("Unable to restore CPU affinity after logical core "
                          "%u: %s\n",
                          lcore, strerror(errno));
                ret = PQOS_RETVAL_ERROR;
        }
        CPU_FREE(target);
        return ret;
}

int
hybrid_cap_discover(struct hybrid_capabilities **cap,
                    const struct pqos_cpuinfo *cpu)
{
        struct hybrid_capabilities *hybrid = NULL;
        cpu_set_t *original = NULL;
        unsigned max_cores = 0, i;
        size_t size;
        int ret;

        if (cap == NULL || cpu == NULL || cpu->num_cores == 0)
                return PQOS_RETVAL_PARAM;
        *cap = NULL;
        for (i = 0; i < cpu->num_cores; i++) {
                if (cpu->cores[i].lcore == UINT_MAX) {
                        LOG_ERROR("Logical core identifier is too large\n");
                        return PQOS_RETVAL_RESOURCE;
                }
                if (max_cores <= cpu->cores[i].lcore)
                        max_cores = cpu->cores[i].lcore + 1;
        }
        if (cpu->num_cores >
            (UINT_MAX - sizeof(*hybrid)) / sizeof(hybrid->cores[0]))
                return PQOS_RETVAL_RESOURCE;
        size =
            sizeof(*hybrid) + (size_t)cpu->num_cores * sizeof(hybrid->cores[0]);
        hybrid = calloc(1, size);
        original = CPU_ALLOC(max_cores);
        if (hybrid == NULL || original == NULL) {
                ret = PQOS_RETVAL_RESOURCE;
                goto error;
        }
        CPU_ZERO_S(CPU_ALLOC_SIZE(max_cores), original);
        if (get_affinity_mask(original, max_cores) != 0) {
                LOG_ERROR("Unable to retrieve CPU affinity: %s\n",
                          strerror(errno));
                ret = PQOS_RETVAL_UNAVAILABLE;
                goto error;
        }

        hybrid->mem_size = size;
        hybrid->status = HYBRID_STATUS_YES;
        hybrid->num_cores = 0;
        for (i = 0; i < cpu->num_cores; i++) {
                struct hybrid_core_capability *core;

                if (!CPU_ISSET_S(cpu->cores[i].lcore, CPU_ALLOC_SIZE(max_cores),
                                 original)) {
                        LOG_INFO("Logical core %u is unavailable to this "
                                 "process\n",
                                 cpu->cores[i].lcore);
                        continue;
                }
                core = &hybrid->cores[hybrid->num_cores];
                ret = read_core(cpu->cores[i].lcore, max_cores, original, core);
                if (ret == PQOS_RETVAL_UNAVAILABLE)
                        continue;
                if (ret != PQOS_RETVAL_OK) {
                        if (ret == PQOS_RETVAL_RESOURCE &&
                            hybrid->num_cores == 0) {
                                hybrid->status = HYBRID_STATUS_NO;
                                hybrid->num_cores = 0;
                                break;
                        }
                        LOG_ERROR("Hybrid capability enumeration failed for "
                                  "logical core %u\n",
                                  cpu->cores[i].lcore);
                        goto error;
                }
                core->lcore = cpu->cores[i].lcore;
                core->socket = cpu->cores[i].socket;
                ret = hybrid_cap_compare(core);
                if (ret != PQOS_RETVAL_OK)
                        goto error;
                hybrid->num_cores++;
        }
        if (hybrid->status == HYBRID_STATUS_YES && hybrid->num_cores == 0) {
                LOG_INFO("No topology CPUs are available to this process\n");
                ret = PQOS_RETVAL_UNAVAILABLE;
                goto error;
        }

        CPU_FREE(original);
        *cap = hybrid;
        return PQOS_RETVAL_OK;

error:
        if (original != NULL)
                CPU_FREE(original);
        free(hybrid);
        return ret;
}
