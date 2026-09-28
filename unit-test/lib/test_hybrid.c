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

#include "cpuinfo.h"
#include "hybrid.h"
#include "test.h"

#ifdef __linux__
#include <errno.h>
#include <sched.h>
#endif

#define MAX_RESULTS 24

struct result {
        unsigned leaf;
        unsigned subleaf;
        struct pqos_hybrid_cpuid_out out;
};

struct cpuid_data {
        struct result results[MAX_RESULTS];
        unsigned count;
};

#ifdef __linux__
/* Size the kernel insists on, and the smallest buffer it has accepted. The
 * kernel compares sched_getaffinity()'s buffer with its own cpumask, which
 * follows the processors it was configured for, and answers EINVAL for anything
 * smaller. Zero means "accept any size", which is what every case that is not
 * about the mask wants.
 */
static size_t g_kernel_set_size;
static size_t g_accepted_set_size;

/* Which affinity mask allocation should fail, counted from one, and how many
 * have been asked for. Zero means every one of them succeeds, which is what
 * every case that is not about an allocation failure wants.
 */
static unsigned g_mask_alloc_fails_on;
static unsigned g_mask_allocs;

/* How many processors sched_getaffinity() reports, which restore of that mask
 * should fail, counted from one, and how many have been asked for. A restore is
 * the only affinity call made with more than one processor in the mask, which
 * is how the wrapper below tells it from the pin before each processor's read.
 *
 * One restore rather than all of them, because the cases below are about what
 * the first failure makes the caller do: a topology read and the capability
 * read that follows it each restore this mask once, and failing both would fail
 * the call whatever the caller decided in between.
 */
static unsigned g_affinity_cpus = 1;
static unsigned g_fail_restore_number;
static unsigned g_restores;

/* CPU_ALLOC() calls this, and the library reads each processor through a mask
 * it allocates here - so this is where an out of memory can be put in front of
 * the enumeration without troubling any other allocation in the process.
 */
cpu_set_t *
__wrap___sched_cpualloc(size_t count)
{
        g_mask_allocs++;
        if (g_mask_alloc_fails_on != 0 &&
            g_mask_allocs == g_mask_alloc_fails_on)
                return NULL;

        return calloc(1, CPU_ALLOC_SIZE(count));
}

int
__wrap_sched_getaffinity(pid_t pid, size_t cpusetsize, cpu_set_t *mask)
{
        unsigned i;

        (void)pid;
        if (g_kernel_set_size != 0 && cpusetsize < g_kernel_set_size) {
                errno = EINVAL;
                return -1;
        }
        g_accepted_set_size = cpusetsize;
        CPU_ZERO_S(cpusetsize, mask);
        for (i = 0; i < g_affinity_cpus; i++)
                CPU_SET_S(i, cpusetsize, mask);
        return 0;
}

int
__wrap_sched_setaffinity(pid_t pid, size_t cpusetsize, const cpu_set_t *mask)
{
        /* the cases are about which processors are enumerated and with what
         * mask, not about this thread really moving, and a real pin would make
         * the result depend on the machine the tests run on
         */
        (void)pid;

        if (CPU_COUNT_S(cpusetsize, mask) > 1) {
                g_restores++;
                if (g_fail_restore_number == g_restores) {
                        errno = EINVAL;
                        return -1;
                }
        }

        return 0;
}
#endif

static void
add(struct cpuid_data *data,
    unsigned leaf,
    unsigned subleaf,
    uint32_t eax,
    uint32_t ebx,
    uint32_t ecx,
    uint32_t edx)
{
        struct result *result = &data->results[data->count++];

        assert_true(data->count <= MAX_RESULTS);
        result->leaf = leaf;
        result->subleaf = subleaf;
        result->out.eax = eax;
        result->out.ebx = ebx;
        result->out.ecx = ecx;
        result->out.edx = edx;
}

static int
read_cpuid(unsigned leaf,
           unsigned subleaf,
           struct pqos_hybrid_cpuid_out *out,
           void *context)
{
        const struct cpuid_data *data = context;
        unsigned i;

        for (i = 0; i < data->count; i++)
                if (data->results[i].leaf == leaf &&
                    data->results[i].subleaf == subleaf) {
                        *out = data->results[i].out;
                        return 0;
                }
        return -1;
}

static void
test_cap_read_checks_max_leaf(void **state)
{
        struct cpuid_data data = {0};
        struct pqos_hybrid_core_capability cap;

        add(&data, 0, 0, 0x10, 0, 0, 0);
        add(&data, 7, 0, 1, 0, 0, 1U << 15);
        add(&data, 7, 1, 0, 0, 3, 0);
        assert_int_equal(hybrid_cap_read(read_cpuid, &data, &cap),
                         PQOS_RETVAL_OK);
        assert_false(cap.mon_supported);
        assert_false(cap.alloc_supported);
        (void)state;
}

static void
test_cap_read_decodes_asymmetric_leaves(void **state)
{
        struct cpuid_data data = {0};
        struct pqos_hybrid_core_capability cap;

        add(&data, 0, 0, 0x28, 0, 0, 0);
        add(&data, 7, 0, 1, 0, 0, 1U << 15);
        add(&data, 7, 1, 0, 0, 3, 0);
        add(&data, 0x1a, 0, 0x40123456, 0, 0, 0);
        add(&data, 0x27, 0, 0, 31, 0, 1U << 1);
        add(&data, 0x27, 1, 8, 64, 15, 7);
        add(&data, 0x28, 0, 0, (1U << 2) | (1U << 3), 0, 0);
        add(&data, 0x28, 2, 15, 0, 12, 7);
        add(&data, 0x28, 3, 99, 0, 5, 7);

        assert_int_equal(hybrid_cap_read(read_cpuid, &data, &cap),
                         PQOS_RETVAL_OK);
        assert_true(cap.mon_supported);
        assert_true(cap.alloc_supported);
        assert_int_equal(cap.core_type, 0x40);
        assert_int_equal(cap.native_model_id, 0x123456);
        assert_int_equal(cap.mon[1].ebx, 64);
        assert_int_equal(cap.alloc[2].eax, 15);
        (void)state;
}

static void
add_matching_data(struct cpuid_data *data)
{
        const uint32_t features = (1U << 12) | (1U << 15);

        add(data, 0, 0, 0x28, 0, 0, 0);
        add(data, 7, 0, 1, features, 0, 1U << 15);
        add(data, 7, 1, 0, 0, 3, 0);
        add(data, 0x1a, 0, 0, 0, 0, 0);
        add(data, 0xf, 0, 0, 7, 0, 0);
        add(data, 0x10, 0, 0, 1U << 2, 0, 0);
        add(data, 0x10, 2, 15, 0x1234, 12, 7);
        add(data, 0x27, 0, 0, 7, 0, 0);
        add(data, 0x28, 0, UINT32_MAX, 1U << 2, UINT32_MAX, UINT32_MAX);
        add(data, 0x28, 2, 15 | 0xffffffe0U, 0x1234, 12 | 0xfffffff0U,
            7 | 0xffff0000U);
}

/**
 * @brief Replace what a leaf and subleaf answer, or add it where it is absent
 *
 * read_cpuid() answers with the first entry that matches, so a second entry for
 * the same leaf and subleaf would never be read: a case changing one platform
 * into another has to overwrite.
 */
static void
override(struct cpuid_data *data,
         unsigned leaf,
         unsigned subleaf,
         uint32_t eax,
         uint32_t ebx,
         uint32_t ecx,
         uint32_t edx)
{
        unsigned i;

        for (i = 0; i < data->count; i++)
                if (data->results[i].leaf == leaf &&
                    data->results[i].subleaf == subleaf) {
                        data->results[i].out.eax = eax;
                        data->results[i].out.ebx = ebx;
                        data->results[i].out.ecx = ecx;
                        data->results[i].out.edx = edx;
                        return;
                }

        add(data, leaf, subleaf, eax, ebx, ecx, edx);
}

static void
test_one_asymmetric_leaf_without_the_other(void **state)
{
        struct cpuid_data monitoring = {0}, allocation = {0};
        struct pqos_hybrid_core_capability cap;

        /* CPUID.7.1:ECX bit 0 alone: monitoring is enumerated and allocation is
         * not, and the leaf that is not supported is never read - a subleaf for
         * it is not in this data, so reading one would fail the call
         */
        add(&monitoring, 0, 0, 0x28, 0, 0, 0);
        add(&monitoring, 7, 0, 1, 0, 0, 1U << 15);
        add(&monitoring, 7, 1, 0, 0, 1, 0);
        add(&monitoring, 0x1a, 0, 0, 0, 0, 0);
        add(&monitoring, 0x27, 0, 0, 31, 0, 1U << 1);
        add(&monitoring, 0x27, 1, 8, 64, 15, 7);
        assert_int_equal(hybrid_cap_read(read_cpuid, &monitoring, &cap),
                         PQOS_RETVAL_OK);
        assert_true(cap.mon_supported);
        assert_false(cap.alloc_supported);
        assert_int_equal(cap.mon[1].ebx, 64);

        /* and bit 1 alone, which is the shape of the board this is tested on:
         * allocation is enumerated, monitoring is not
         */
        add(&allocation, 0, 0, 0x28, 0, 0, 0);
        add(&allocation, 7, 0, 1, 0, 0, 1U << 15);
        add(&allocation, 7, 1, 0, 0, 2, 0);
        add(&allocation, 0x1a, 0, 0, 0, 0, 0);
        add(&allocation, 0x28, 0, 0, 1U << 6, 0, 0);
        add(&allocation, 0x28, 6, 3, 0, 0, 0);
        assert_int_equal(hybrid_cap_read(read_cpuid, &allocation, &cap),
                         PQOS_RETVAL_OK);
        assert_false(cap.mon_supported);
        assert_true(cap.alloc_supported);
        assert_int_equal(cap.alloc[6].eax, 3);
        (void)state;
}

static void
test_only_the_named_subleaves_are_read(void **state)
{
        struct cpuid_data data = {0};
        struct pqos_hybrid_core_capability cap;

        /* the resource mask names subleaves 2 and 3, so those two are read and
         * the others are not: subleaves 1, 5 and 6 are absent from this data
         * and reading one would fail the call
         */
        add(&data, 0, 0, 0x28, 0, 0, 0);
        add(&data, 7, 0, 1, 0, 0, 1U << 15);
        add(&data, 7, 1, 0, 0, 2, 0);
        add(&data, 0x1a, 0, 0, 0, 0, 0);
        add(&data, 0x28, 0, 0, (1U << 2) | (1U << 3), 0, 0);
        add(&data, 0x28, 2, 15, 0, 12, 7);
        add(&data, 0x28, 3, 99, 0, 5, 7);
        assert_int_equal(hybrid_cap_read(read_cpuid, &data, &cap),
                         PQOS_RETVAL_OK);
        assert_int_equal(cap.alloc_resources, (1U << 2) | (1U << 3));
        assert_int_equal(cap.alloc[2].eax, 15);
        assert_int_equal(cap.alloc[3].eax, 99);
        (void)state;
}

static void
test_a_subleaf_that_cannot_be_read_fails_the_call(void **state)
{
        struct cpuid_data data = {0};
        struct pqos_hybrid_core_capability cap;

        /* the mask names subleaf 2 and the platform does not answer for it:
         * malformed enumeration, and half a capability is not published
         */
        add(&data, 0, 0, 0x28, 0, 0, 0);
        add(&data, 7, 0, 1, 0, 0, 1U << 15);
        add(&data, 7, 1, 0, 0, 2, 0);
        add(&data, 0x1a, 0, 0, 0, 0, 0);
        add(&data, 0x28, 0, 0, 1U << 2, 0, 0);
        assert_int_equal(hybrid_cap_read(read_cpuid, &data, &cap),
                         PQOS_RETVAL_ERROR);
        (void)state;
}

/**
 * @brief Find a difference of a resource and field in a capability
 *
 * @param [in] cap capability to search
 * @param [in] resource resource the difference is about
 * @param [in] field field the difference is about
 *
 * @return The difference, or NULL where there is none
 */
static const struct pqos_hybrid_difference *
difference_of(const struct pqos_hybrid_core_capability *cap,
              enum pqos_hybrid_resource resource,
              enum pqos_hybrid_field field)
{
        unsigned i;

        for (i = 0; i < cap->num_differences; i++)
                if (cap->differences[i].resource == resource &&
                    cap->differences[i].field == field)
                        return &cap->differences[i];

        return NULL;
}

static void
test_differences_between_the_two_enumerations(void **state)
{
        struct cpuid_data data = {0};
        struct pqos_hybrid_core_capability cap;
        const struct pqos_hybrid_difference *diff;

        /* the matching platform first, so that each difference below is the one
         * the case introduced and not one that was always there
         */
        add_matching_data(&data);
        assert_int_equal(hybrid_cap_read(read_cpuid, &data, &cap),
                         PQOS_RETVAL_OK);
        assert_int_equal(hybrid_cap_compare(&cap), PQOS_RETVAL_OK);
        assert_int_equal(cap.num_differences, 0);

        /* a resource the asymmetric enumeration has and the regular one does
         * not: 0x10's mask carries L2 CAT, 0x28's carries L2 CAT and MBA
         */
        override(&data, 0x28, 0, UINT32_MAX, (1U << 2) | (1U << 3), UINT32_MAX,
                 UINT32_MAX);
        override(&data, 0x28, 3, 99, 0, 5, 7);
        assert_int_equal(hybrid_cap_read(read_cpuid, &data, &cap),
                         PQOS_RETVAL_OK);
        assert_int_equal(hybrid_cap_compare(&cap), PQOS_RETVAL_OK);
        diff = difference_of(&cap, PQOS_HYBRID_RESOURCE_MBA,
                             PQOS_HYBRID_FIELD_SUPPORT);
        assert_non_null(diff);
        assert_int_equal(diff->regular, 0);
        assert_int_equal(diff->asymmetric, 1);

        /* a class count that differs, which is EDX[15:0] of the L2 subleaf */
        data.count = 0;
        add_matching_data(&data);
        override(&data, 0x28, 2, 15 | 0xffffffe0U, 0x1234, 12 | 0xfffffff0U,
                 9 | 0xffff0000U);
        assert_int_equal(hybrid_cap_read(read_cpuid, &data, &cap),
                         PQOS_RETVAL_OK);
        assert_int_equal(hybrid_cap_compare(&cap), PQOS_RETVAL_OK);
        diff = difference_of(&cap, PQOS_HYBRID_RESOURCE_L2_CAT,
                             PQOS_HYBRID_FIELD_MAX_CLOS);
        assert_non_null(diff);
        assert_int_equal(diff->regular, 7);
        assert_int_equal(diff->asymmetric, 9);

        /* and CDP, which is ECX[2] of the same subleaf */
        data.count = 0;
        add_matching_data(&data);
        override(&data, 0x28, 2, 15 | 0xffffffe0U, 0x1234, 8 | 0xfffffff0U,
                 7 | 0xffff0000U);
        assert_int_equal(hybrid_cap_read(read_cpuid, &data, &cap),
                         PQOS_RETVAL_OK);
        assert_int_equal(hybrid_cap_compare(&cap), PQOS_RETVAL_OK);
        diff = difference_of(&cap, PQOS_HYBRID_RESOURCE_L2_CAT,
                             PQOS_HYBRID_FIELD_CDP);
        assert_non_null(diff);
        assert_int_equal(diff->regular, 1);
        assert_int_equal(diff->asymmetric, 0);
        (void)state;
}

static void
test_monitoring_differences_between_the_two_enumerations(void **state)
{
        struct cpuid_data data = {0};
        struct pqos_hybrid_core_capability cap;
        const struct pqos_hybrid_difference *diff;

        /* the RMID limit of the main subleaf, reported by both enumerations */
        add_matching_data(&data);
        override(&data, 0x27, 0, 0, 15, 0, 0);
        assert_int_equal(hybrid_cap_read(read_cpuid, &data, &cap),
                         PQOS_RETVAL_OK);
        assert_int_equal(hybrid_cap_compare(&cap), PQOS_RETVAL_OK);
        diff = difference_of(&cap, PQOS_HYBRID_RESOURCE_MONITORING,
                             PQOS_HYBRID_FIELD_MAX_RMID);
        assert_non_null(diff);
        assert_int_equal(diff->regular, 7);
        assert_int_equal(diff->asymmetric, 15);

        /* monitoring enumerated by one mechanism and not the other: the regular
         * leaf is in range and its feature bit is clear
         */
        data.count = 0;
        add_matching_data(&data);
        override(&data, 7, 0, 1, 1U << 15, 0, 1U << 15);
        assert_int_equal(hybrid_cap_read(read_cpuid, &data, &cap),
                         PQOS_RETVAL_OK);
        assert_int_equal(hybrid_cap_compare(&cap), PQOS_RETVAL_OK);
        diff = difference_of(&cap, PQOS_HYBRID_RESOURCE_MONITORING,
                             PQOS_HYBRID_FIELD_ENUMERATION_SUPPORT);
        assert_non_null(diff);
        assert_int_equal(diff->regular, 0);
        assert_int_equal(diff->asymmetric, 1);
        (void)state;
}

static void
test_physical_core_from_the_topology_leaf(void **state)
{
        struct cpuid_data first = {0}, sibling = {0}, older = {0};
        struct pqos_hybrid_core_capability cap;

        /* subleaf 0 of 1FH describes the logical processor domain (ECX[15:8] is
         * 1), its EAX[4:0] shift is one, and EDX carries the x2APIC identifier
         * of the processor the read runs on. 5 >> 1 names core 2
         */
        add(&first, 0, 0, 0x1f, 0, 0, 0);
        add(&first, 7, 0, 1, 0, 0, 1U << 15);
        add(&first, 7, 1, 0, 0, 0, 0);
        add(&first, 0x1a, 0, 0, 0, 0, 0);
        add(&first, 0x1f, 0, 1, 0, 0x0100, 5);
        assert_int_equal(hybrid_cap_read(read_cpuid, &first, &cap),
                         PQOS_RETVAL_OK);
        assert_true(cap.physical_core_valid);
        assert_int_equal(cap.physical_core, 2);

        /* its SMT sibling has the other x2APIC identifier of the same core and
         * has to answer with the same core
         */
        add(&sibling, 0, 0, 0x1f, 0, 0, 0);
        add(&sibling, 7, 0, 1, 0, 0, 1U << 15);
        add(&sibling, 7, 1, 0, 0, 0, 0);
        add(&sibling, 0x1a, 0, 0, 0, 0, 0);
        add(&sibling, 0x1f, 0, 1, 0, 0x0100, 4);
        assert_int_equal(hybrid_cap_read(read_cpuid, &sibling, &cap),
                         PQOS_RETVAL_OK);
        assert_true(cap.physical_core_valid);
        assert_int_equal(cap.physical_core, 2);

        /* a processor with 0BH and not 1FH is read the same way, and a shift of
         * zero is what a core carrying one processor reports
         */
        add(&older, 0, 0, 0x0b, 0, 0, 0);
        add(&older, 7, 0, 1, 0, 0, 1U << 15);
        add(&older, 7, 1, 0, 0, 0, 0);
        add(&older, 0x0b, 0, 0, 0, 0x0100, 7);
        assert_int_equal(hybrid_cap_read(read_cpuid, &older, &cap),
                         PQOS_RETVAL_OK);
        assert_true(cap.physical_core_valid);
        assert_int_equal(cap.physical_core, 7);
        (void)state;
}

static void
test_physical_core_left_unavailable(void **state)
{
        struct cpuid_data no_leaf = {0}, other_domain = {0};
        struct pqos_hybrid_core_capability cap;

        /* neither topology leaf is in range */
        add(&no_leaf, 0, 0, 0x0a, 0, 0, 0);
        add(&no_leaf, 7, 0, 1, 0, 0, 1U << 15);
        add(&no_leaf, 7, 1, 0, 0, 0, 0);
        assert_int_equal(hybrid_cap_read(read_cpuid, &no_leaf, &cap),
                         PQOS_RETVAL_OK);
        assert_false(cap.physical_core_valid);

        /* and a subleaf 0 describing some other domain is not read as a
         * processor domain: the identifier stays unavailable rather than being
         * shifted by a number that means something else
         */
        add(&other_domain, 0, 0, 0x1f, 0, 0, 0);
        add(&other_domain, 7, 0, 1, 0, 0, 1U << 15);
        add(&other_domain, 7, 1, 0, 0, 0, 0);
        add(&other_domain, 0x1a, 0, 0, 0, 0, 0);
        add(&other_domain, 0x1f, 0, 1, 0, 0x0200, 5);
        add(&other_domain, 0x0b, 0, 1, 0, 0x0200, 5);
        assert_int_equal(hybrid_cap_read(read_cpuid, &other_domain, &cap),
                         PQOS_RETVAL_OK);
        assert_false(cap.physical_core_valid);
        (void)state;
}

static void
test_compare_ignores_reserved_and_reports_cbm(void **state)
{
        struct cpuid_data data = {0};
        struct pqos_hybrid_core_capability cap;

        add_matching_data(&data);
        assert_int_equal(hybrid_cap_read(read_cpuid, &data, &cap),
                         PQOS_RETVAL_OK);
        assert_int_equal(hybrid_cap_compare(&cap), PQOS_RETVAL_OK);
        assert_int_equal(cap.num_differences, 0);

        data.results[data.count - 1].out.eax = 7;
        assert_int_equal(hybrid_cap_read(read_cpuid, &data, &cap),
                         PQOS_RETVAL_OK);
        assert_int_equal(hybrid_cap_compare(&cap), PQOS_RETVAL_OK);
        assert_int_equal(cap.num_differences, 1);
        assert_int_equal(cap.differences[0].resource,
                         PQOS_HYBRID_RESOURCE_L2_CAT);
        assert_int_equal(cap.differences[0].field,
                         PQOS_HYBRID_FIELD_CBM_LENGTH);
        assert_int_equal(cap.differences[0].regular, 16);
        assert_int_equal(cap.differences[0].asymmetric, 8);
        (void)state;
}

static void
test_resource_priority_support(void **state)
{
        const size_t size = sizeof(struct pqos_hybrid_capabilities) +
                            sizeof(struct pqos_hybrid_core_capability);
        struct pqos_hybrid_capabilities *cap = calloc(1, size);

        assert_non_null(cap);
        cap->status = PQOS_HYBRID_STATUS_YES;
        cap->num_cores = 1;
        assert_false(hybrid_cap_rp_supported(cap));
        cap->cores[0].alloc_supported = 1;
        assert_false(hybrid_cap_rp_supported(cap));
        cap->cores[0].alloc_resources = 1U << 6;
        assert_true(hybrid_cap_rp_supported(cap));
        free(cap);
        (void)state;
}

#ifdef __linux__
static void
test_discover_skips_inaccessible_topology_cpus(void **state)
{
        const size_t size =
            sizeof(struct pqos_cpuinfo) + sizeof(struct pqos_coreinfo);
        struct pqos_cpuinfo *cpu = calloc(1, size);
        struct pqos_hybrid_capabilities *cap = NULL;

        assert_non_null(cpu);
        cpu->num_cores = 1;
        cpu->cores[0].lcore = 1;
        /* the vendor is named because discovery is gated on it: the leaves this
         * reads are Intel definitions
         */
        cpu->vendor = PQOS_VENDOR_INTEL;
        assert_int_equal(hybrid_cap_discover(&cap, cpu),
                         PQOS_RETVAL_UNAVAILABLE);
        assert_null(cap);
        free(cpu);
        (void)state;
}
#endif

#ifdef __linux__
static void
test_discover_reports_a_failed_mask_allocation_as_resource(void **state)
{
        const size_t size =
            sizeof(struct pqos_cpuinfo) + sizeof(struct pqos_coreinfo);
        struct pqos_cpuinfo *cpu = calloc(1, size);
        struct pqos_hybrid_capabilities *cap = NULL;

        assert_non_null(cpu);
        cpu->num_cores = 1;
        cpu->cores[0].lcore = 0;
        cpu->vendor = PQOS_VENDOR_INTEL;

        /* the first mask is the process's own affinity, the second is the one
         * the processor is read through. Both are the same kind of failure and
         * both have to be named the same way, or a caller cannot tell an out of
         * memory from a platform that could not be enumerated
         */
        g_mask_allocs = 0;
        g_mask_alloc_fails_on = 1;
        assert_int_equal(hybrid_cap_discover(&cap, cpu), PQOS_RETVAL_RESOURCE);
        assert_null(cap);

        g_mask_allocs = 0;
        g_mask_alloc_fails_on = 2;
        assert_int_equal(hybrid_cap_discover(&cap, cpu), PQOS_RETVAL_RESOURCE);
        assert_null(cap);

        g_mask_alloc_fails_on = 0;
        free(cpu);
        (void)state;
}
#endif

#ifdef __linux__
static void
test_discover_grows_the_affinity_mask(void **state)
{
        const size_t size =
            sizeof(struct pqos_cpuinfo) + sizeof(struct pqos_coreinfo);
        struct pqos_cpuinfo *cpu = calloc(1, size);
        struct pqos_hybrid_capabilities *cap = NULL;

        assert_non_null(cpu);
        cpu->num_cores = 1;
        cpu->cores[0].lcore = 0;
        cpu->vendor = PQOS_VENDOR_INTEL;

        /* a kernel configured for 512 processors with one of them online: the
         * mask the topology implies is far too small, and the old sizing made
         * hybrid discovery - and so pqos_init() - fail here before a single
         * CPUID had been executed
         */
        g_kernel_set_size = CPU_ALLOC_SIZE(512);
        g_accepted_set_size = 0;
        assert_int_equal(hybrid_cap_discover(&cap, cpu), PQOS_RETVAL_OK);
        assert_non_null(cap);
        assert_true(g_accepted_set_size >= g_kernel_set_size);
        g_kernel_set_size = 0;

        free(cap);
        free(cpu);
        (void)state;
}
#endif

static void
test_discover_leaves_a_foreign_vendor_unknown(void **state)
{
        const size_t size =
            sizeof(struct pqos_cpuinfo) + sizeof(struct pqos_coreinfo);
        struct pqos_cpuinfo *cpu = calloc(1, size);
        struct pqos_hybrid_capabilities *cap = NULL;

        assert_non_null(cpu);
        cpu->num_cores = 1;
        cpu->cores[0].lcore = 0;
        cpu->vendor = PQOS_VENDOR_AMD;

        /* the hybrid bit and the two leaves are Intel definitions, so nothing
         * is read here and the status says so rather than claiming the
         * processor is not hybrid
         */
        assert_int_equal(hybrid_cap_discover(&cap, cpu), PQOS_RETVAL_OK);
        assert_non_null(cap);
        assert_int_equal(cap->status, PQOS_HYBRID_STATUS_UNKNOWN);
        assert_int_equal(cap->num_cores, 0);

        free(cap);
        free(cpu);
        (void)state;
}

#ifdef __linux__
/**
 * @brief Skips the calling case where this platform has no CPUID topology
 *
 * @param [out] affinity_lost passed through to cpuinfo_discover()
 */
static void
cpu_topology_or_skip(int *affinity_lost)
{
        struct pqos_cpuinfo *cpu =
            cpuinfo_discover(PQOS_INTER_MSR, 0, affinity_lost);

        if (cpu == NULL)
                skip();
        free(cpu);
}
#endif

#ifdef __linux__
static void
test_a_topology_read_that_loses_the_affinity_says_so(void **state)
{
        struct pqos_cpuinfo *cpu;
        int affinity_lost = 0;

        /* The CPUID builder pins this thread to each processor in turn and puts
         * the caller's mask back at the end. Where the platform has no CPUID
         * topology to build, the read fails before any of that and there is
         * nothing here to check - so ask first, with the restore allowed.
         */
        cpu = cpuinfo_discover(PQOS_INTER_MSR, 0, &affinity_lost);
        if (cpu == NULL)
                skip();
        assert_int_equal(affinity_lost, 0);
        free(cpu);

        g_affinity_cpus = 2;
        g_restores = 0;
        g_fail_restore_number = 1;
        affinity_lost = 0;

        cpu = cpuinfo_discover(PQOS_INTER_MSR, 0, &affinity_lost);
        assert_null(cpu);
        assert_int_equal(affinity_lost, 1);

        g_fail_restore_number = 0;
        g_affinity_cpus = 1;
        (void)state;
}

static void
test_a_lost_affinity_is_not_answered_from_the_next_source(void **state)
{
        struct pqos_hybrid_capabilities *cap = NULL;
        int affinity_lost = 0;

        cpu_topology_or_skip(&affinity_lost);

        g_affinity_cpus = 2;
        g_restores = 0;
        g_fail_restore_number = 1; /* the topology read's, and only it */

        /* The operating system's topology needs no affinity call, so the next
         * source would answer here and the capability read after it would
         * succeed - reporting success from a thread the failed read has left
         * pinned. The failure is the answer instead, and the count shows the
         * capability read was never reached.
         */
        assert_int_equal(pqos_hybrid_discover(&cap), PQOS_RETVAL_ERROR);
        assert_null(cap);
        assert_int_equal(g_restores, 1);

        g_fail_restore_number = 0;
        g_affinity_cpus = 1;

        /* and with the restore allowed the same call answers, so what the case
         * above met was the lost affinity and not a platform with no topology
         */
        assert_int_equal(pqos_hybrid_discover(&cap), PQOS_RETVAL_OK);
        assert_non_null(cap);
        pqos_hybrid_free(cap);
        (void)state;
}
#endif

static void
test_non_hybrid_is_not_a_hybrid_capability(void **state)
{
        struct cpuid_data data = {0};
        struct pqos_hybrid_core_capability cap;

        add(&data, 0, 0, 7, 0, 0, 0);
        add(&data, 7, 0, 0, 0, 0, 0);
        assert_int_equal(hybrid_cap_read(read_cpuid, &data, &cap),
                         PQOS_RETVAL_RESOURCE);
        (void)state;
}

int
main(void)
{
        const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_cap_read_checks_max_leaf),
            cmocka_unit_test(test_cap_read_decodes_asymmetric_leaves),
            cmocka_unit_test(test_one_asymmetric_leaf_without_the_other),
            cmocka_unit_test(test_only_the_named_subleaves_are_read),
            cmocka_unit_test(test_a_subleaf_that_cannot_be_read_fails_the_call),
            cmocka_unit_test(test_differences_between_the_two_enumerations),
            cmocka_unit_test(
                test_monitoring_differences_between_the_two_enumerations),
            cmocka_unit_test(test_physical_core_from_the_topology_leaf),
            cmocka_unit_test(test_physical_core_left_unavailable),
            cmocka_unit_test(test_compare_ignores_reserved_and_reports_cbm),
            cmocka_unit_test(test_resource_priority_support),
            /* the vendor gate answers before any affinity call, so this one
             * runs wherever the tests are built; the two below need the
             * wrapped Linux syscalls
             */
            cmocka_unit_test(test_discover_leaves_a_foreign_vendor_unknown),
#ifdef __linux__
            cmocka_unit_test(test_discover_skips_inaccessible_topology_cpus),
            cmocka_unit_test(
                test_discover_reports_a_failed_mask_allocation_as_resource),
            cmocka_unit_test(test_discover_grows_the_affinity_mask),
            cmocka_unit_test(
                test_a_topology_read_that_loses_the_affinity_says_so),
            cmocka_unit_test(
                test_a_lost_affinity_is_not_answered_from_the_next_source),
#endif
            cmocka_unit_test(test_non_hybrid_is_not_a_hybrid_capability)};

        return cmocka_run_group_tests(tests, NULL, NULL);
}
