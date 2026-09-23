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

int
__wrap_sched_getaffinity(pid_t pid, size_t cpusetsize, cpu_set_t *mask)
{
        (void)pid;
        if (g_kernel_set_size != 0 && cpusetsize < g_kernel_set_size) {
                errno = EINVAL;
                return -1;
        }
        g_accepted_set_size = cpusetsize;
        CPU_ZERO_S(cpusetsize, mask);
        CPU_SET_S(0, cpusetsize, mask);
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
        (void)cpusetsize;
        (void)mask;
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
        assert_int_equal(hybrid_cap_discover(&cap, cpu),
                         PQOS_RETVAL_UNAVAILABLE);
        assert_null(cap);
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
            cmocka_unit_test(test_compare_ignores_reserved_and_reports_cbm),
            cmocka_unit_test(test_resource_priority_support),
#ifdef __linux__
            cmocka_unit_test(test_discover_skips_inaccessible_topology_cpus),
#ifdef __linux__
            cmocka_unit_test(test_discover_grows_the_affinity_mask),
#endif
#endif
            cmocka_unit_test(test_non_hybrid_is_not_a_hybrid_capability)};

        return cmocka_run_group_tests(tests, NULL, NULL);
}
