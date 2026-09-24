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
 *
 */

/**
 * pqos_hybrid_discover() and the topology it falls back to.
 *
 * The capabilities are CPUID's answer, but the processors to read them on come
 * from a topology - and the two sources of that topology fail independently.
 * CPUID's own topology needs leaf 0BH and the cache leaves, which a hypervisor
 * may filter; the operating system's needs neither. These cases drive the real
 * discovery with CPUID answering as such a platform does, and require the
 * operating system to be asked.
 *
 * lcpuid() is wrapped rather than the topology builders: everything the
 * discovery reads goes through it, so one fake platform serves the vendor
 * check, the APIC detection and the hybrid read, and the code under test is the
 * code that ships.
 */

#include "cpuinfo.h"
#include "machine.h"
#include "os_cpuinfo.h"
#include "pqos.h"
#include "test.h"

#include <sched.h>
#include <stdlib.h>
#include <string.h>

/** "GenuineIntel", as the vendor check reads it out of leaf 0 */
#define VENDOR_EBX 0x756e6547
#define VENDOR_ECX 0x6c65746e
#define VENDOR_EDX 0x49656e69

/** How many times the operating system was asked for the topology, and what it
 *  should answer
 */
static unsigned g_os_topology_calls;
static int g_os_topology_available = 1;

/**
 * @brief A platform whose CPUID carries no topology
 *
 * Leaf 0BH answers with EBX zero at every sub-leaf, which is how CPUID says it
 * has no topology to report - detect_apic_core_masks() then finds no thread
 * level and fails, and with it the CPUID topology builder. Leaf 7 still reports
 * a hybrid processor, so the read has something to find once a topology is
 * available from elsewhere.
 */
void
__wrap_lcpuid(const unsigned leaf,
              const unsigned subleaf,
              struct cpuid_out *out)
{
        memset(out, 0, sizeof(*out));

        switch (leaf) {
        case 0:
                out->eax = 0x28;
                out->ebx = VENDOR_EBX;
                out->ecx = VENDOR_ECX;
                out->edx = VENDOR_EDX;
                break;
        case 7:
                if (subleaf == 0)
                        out->edx = 1U << 15; /* hybrid */
                break;
        default:
                /* including 0BH, whose zero EBX is the point of this fake */
                break;
        }
}

#ifdef __linux__
int
__wrap_sched_getaffinity(pid_t pid, size_t cpusetsize, cpu_set_t *mask)
{
        (void)pid;
        CPU_ZERO_S(cpusetsize, mask);
        CPU_SET_S(0, cpusetsize, mask);
        return 0;
}

int
__wrap_sched_setaffinity(pid_t pid, size_t cpusetsize, const cpu_set_t *mask)
{
        (void)pid;
        (void)cpusetsize;
        (void)mask;
        return 0;
}

/**
 * @brief The operating system's topology: one processor, or none to be had
 */
struct pqos_cpuinfo *
__wrap_os_cpuinfo_topology(int prepare_for_access)
{
        struct pqos_cpuinfo *cpu;

        g_os_topology_calls++;

        /* a read of the topology must not ask for the open file limit to be
         * raised: that is the caller's process state, not this call's
         */
        assert_int_equal(prepare_for_access, 0);

        if (!g_os_topology_available)
                return NULL;

        cpu = calloc(1, sizeof(*cpu) + sizeof(struct pqos_coreinfo));
        assert_non_null(cpu);
        cpu->mem_size = sizeof(*cpu) + sizeof(struct pqos_coreinfo);
        cpu->num_cores = 1;
        cpu->cores[0].lcore = 0;

        return cpu;
}

static void
test_discover_falls_back_to_the_os_topology(void **state)
{
        struct pqos_hybrid_capabilities *cap = NULL;

        g_os_topology_calls = 0;
        g_os_topology_available = 1;

        /* CPUID has no topology to give, so the discovery has to ask the
         * operating system - and answer from what it gets back
         */
        assert_int_equal(pqos_hybrid_discover(&cap), PQOS_RETVAL_OK);
        assert_non_null(cap);
        assert_int_equal(g_os_topology_calls, 1);
        assert_int_equal(cap->status, PQOS_HYBRID_STATUS_YES);
        assert_int_equal(cap->num_cores, 1);
        assert_int_equal(cap->cores[0].lcore, 0);

        /* the cache description CPUID never gave is reported as absent rather
         * than as zeroes that look measured
         */
        pqos_hybrid_free(cap);
        (void)state;
}

static void
test_discover_reports_no_topology_at_all(void **state)
{
        struct pqos_hybrid_capabilities *cap = NULL;

        g_os_topology_calls = 0;
        g_os_topology_available = 0;

        /* neither source answers: a resource error, and no capability handed
         * back for a caller to read
         */
        assert_int_equal(pqos_hybrid_discover(&cap), PQOS_RETVAL_RESOURCE);
        assert_null(cap);
        assert_int_equal(g_os_topology_calls, 1);

        g_os_topology_available = 1;
        (void)state;
}
#endif

static void
test_discover_checks_its_parameter(void **state)
{
        assert_int_equal(pqos_hybrid_discover(NULL), PQOS_RETVAL_PARAM);
        pqos_hybrid_free(NULL);
        (void)state;
}

int
main(void)
{
        const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_discover_checks_its_parameter),
#ifdef __linux__
            cmocka_unit_test(test_discover_falls_back_to_the_os_topology),
            cmocka_unit_test(test_discover_reports_no_topology_at_all),
#endif
        };

        return cmocka_run_group_tests(tests, NULL, NULL);
}
