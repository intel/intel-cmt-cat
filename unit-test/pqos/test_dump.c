/*
 * BSD LICENSE
 *
 * Copyright(c) 2022-2026 Intel Corporation. All rights reserved.
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
#include "test_dump.h"

#include "common.h"
#include "dump.h"
#include "dump_rmids.h"
#include "output.h"
#include "pqos.h"

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
/* clang-format off */
#include <cmocka.h>
/* clang-format on */

/* Both dump entry points check the ERDT information first, so a system
 * configuration that carries one is enough to reach the option checks these
 * test cases are about. Most cases are command lines that have to be
 * rejected; the --socket cases that are accepted stop at a wrapped
 * pqos_dump(), which checks the domains the selection resolved to, so no
 * register is read either way.
 *
 * They run after pqos_init(), so a refusal is a return: an exit would leave
 * the library initialised, with no pqos_fini() to remove its lock file. Each
 * case asserts the status and that the process was not ended.
 */
static struct pqos_erdt_info test_erdt;
static struct pqos_sysconfig test_sys = {.erdt = &test_erdt};

/* A CACD that enumerates the IDs of a table. The IDs stand for the APIC IDs of
 * the CPUs, and CPU n has ID n throughout.
 */
#define CACD(ids)                                                              \
        {                                                                      \
                .enumeration_ids = (ids), .enum_ids_length = DIM(ids)          \
        }

/* CPU domains that neither start at 0 nor fill CPU_AGENTS_PER_SOCKET: 1 and 3
 * on socket 0, the way a part with two CBBs numbers them, and 4 on socket 1.
 * ERDT lists domain 4 between the other two, so a socket's domains cannot be
 * told by their position.
 *
 * The core to domain mapping is indexed by logical CPU, and the topology lists
 * the CPUs of socket 1 first, so a core's position in it is not its logical
 * CPU: reading the mapping by position gives socket 1 the wrong domains. Each
 * CACD enumerates the two cores of its domain. The topologies are built by the
 * group setup, since struct pqos_cpuinfo ends in a flexible array.
 */
static uint32_t cbb_ids_1[] = {0, 1};
static uint32_t cbb_ids_3[] = {2, 3};
static uint32_t cbb_ids_4[] = {4, 5};
static struct pqos_cpu_agent_info cbb_agents[] = {
    {.rmdd = {.domain_id = 1}, .cacd = CACD(cbb_ids_1)},
    {.rmdd = {.domain_id = 4}, .cacd = CACD(cbb_ids_4)},
    {.rmdd = {.domain_id = 3}, .cacd = CACD(cbb_ids_3)}};
static struct pqos_erdt_info cbb_erdt = {.num_cpu_agents = DIM(cbb_agents),
                                         .cpu_agents = cbb_agents};
static uint16_t cbb_core_domains[] = {1, 1, 3, 3, 4, 4};
static const unsigned cbb_lcores[] = {4, 5, 0, 1, 2, 3};
static const unsigned cbb_sockets[] = {1, 1, 0, 0, 0, 0};
static struct pqos_cores_domains cbb_cores_domains = {
    .num_cores = DIM(cbb_core_domains),
    .domains = cbb_core_domains};
static struct pqos_sysconfig cbb_sys = {.erdt = &cbb_erdt,
                                        .cores_domains = &cbb_cores_domains};

/* Domain 0 on socket 0 and domain 4 on socket 1, where CPU 3, on socket 1,
 * is in no CACD. The mapping is zero-filled, so it reads as domain 0, which
 * the mapping then has on both sockets. Domain 0's CACD enumerates CPUs 0 and
 * 1, and domain 4's CPU 2.
 */
static uint32_t stray_ids_0[] = {0, 1};
static uint32_t stray_ids_4[] = {2};
static struct pqos_cpu_agent_info stray_agents[] = {
    {.rmdd = {.domain_id = 0}, .cacd = CACD(stray_ids_0)},
    {.rmdd = {.domain_id = 4}, .cacd = CACD(stray_ids_4)}};
static struct pqos_erdt_info stray_erdt = {.num_cpu_agents = DIM(stray_agents),
                                           .cpu_agents = stray_agents};
static uint16_t stray_core_domains[] = {0, 0, 4, 0};
static const unsigned stray_lcores[] = {0, 1, 2, 3};
static const unsigned stray_sockets[] = {0, 0, 1, 1};
static struct pqos_cores_domains stray_cores_domains = {
    .num_cores = DIM(stray_core_domains),
    .domains = stray_core_domains};
static struct pqos_sysconfig stray_sys = {.erdt = &stray_erdt,
                                          .cores_domains =
                                              &stray_cores_domains};

/* The agents of cbb_erdt and a fourth, domain 5, whose CACD enumerates no
 * core: its socket cannot be told either. The cases using it take the
 * topology and core map of cbb_sys.
 */
static struct pqos_cpu_agent_info unplaced_agents[] = {
    {.rmdd = {.domain_id = 1}, .cacd = CACD(cbb_ids_1)},
    {.rmdd = {.domain_id = 4}, .cacd = CACD(cbb_ids_4)},
    {.rmdd = {.domain_id = 3}, .cacd = CACD(cbb_ids_3)},
    {.rmdd = {.domain_id = 5}, .cacd = {.enum_ids_length = 0}}};
static struct pqos_erdt_info unplaced_erdt = {.num_cpu_agents =
                                                  DIM(unplaced_agents),
                                              .cpu_agents = unplaced_agents};

/* Domain 0's CACD enumerates no core, and CPU 1, on socket 1, is in no CACD:
 * the zero-filled mapping reads it as domain 0, which would put domain 0 on
 * socket 1 by that CPU alone.
 */
static uint32_t empty0_ids_1[] = {0};
static struct pqos_cpu_agent_info empty0_agents[] = {
    {.rmdd = {.domain_id = 0}, .cacd = {.enum_ids_length = 0}},
    {.rmdd = {.domain_id = 1}, .cacd = CACD(empty0_ids_1)}};
static struct pqos_erdt_info empty0_erdt = {.num_cpu_agents =
                                                DIM(empty0_agents),
                                            .cpu_agents = empty0_agents};
static uint16_t empty0_core_domains[] = {1, 0};
static const unsigned empty0_lcores[] = {0, 1};
static const unsigned empty0_sockets[] = {0, 1};
static struct pqos_cores_domains empty0_cores_domains = {
    .num_cores = DIM(empty0_core_domains),
    .domains = empty0_core_domains};
static struct pqos_sysconfig empty0_sys = {.erdt = &empty0_erdt,
                                           .cores_domains =
                                               &empty0_cores_domains};

/* Domain 0's CACD enumerates CPUs 0 and 1, and CPU 2, on the same socket, is
 * in no CACD: the mapping gives domain 0 three cores where its CACD lists two,
 * so the mapping cannot be told from ERDT for it.
 */
static uint32_t extra0_ids_0[] = {0, 1};
static uint32_t extra0_ids_4[] = {3};
static struct pqos_cpu_agent_info extra0_agents[] = {
    {.rmdd = {.domain_id = 0}, .cacd = CACD(extra0_ids_0)},
    {.rmdd = {.domain_id = 4}, .cacd = CACD(extra0_ids_4)}};
static struct pqos_erdt_info extra0_erdt = {.num_cpu_agents =
                                                DIM(extra0_agents),
                                            .cpu_agents = extra0_agents};
static uint16_t extra0_core_domains[] = {0, 0, 0, 4};
static const unsigned extra0_lcores[] = {0, 1, 2, 3};
static const unsigned extra0_sockets[] = {0, 0, 0, 1};
static struct pqos_cores_domains extra0_cores_domains = {
    .num_cores = DIM(extra0_core_domains),
    .domains = extra0_core_domains};
static struct pqos_sysconfig extra0_sys = {.erdt = &extra0_erdt,
                                           .cores_domains =
                                               &extra0_cores_domains};

/* Domains 0 and 1 both enumerate CPUs 0 and 1, on socket 0, and CPUs 2 and 3,
 * on socket 1, are in no CACD. The mapping gives a CPU two CACDs list to the
 * one written last, domain 1, and reads CPUs 2 and 3 as domain 0, so each
 * domain has as many cores as its CACD lists, and domain 0's are on socket 1.
 */
static uint32_t overlap_ids[] = {0, 1};
static struct pqos_cpu_agent_info overlap_agents[] = {
    {.rmdd = {.domain_id = 0}, .cacd = CACD(overlap_ids)},
    {.rmdd = {.domain_id = 1}, .cacd = CACD(overlap_ids)}};
static struct pqos_erdt_info overlap_erdt = {.num_cpu_agents =
                                                 DIM(overlap_agents),
                                             .cpu_agents = overlap_agents};
static uint16_t overlap_core_domains[] = {1, 1, 0, 0};
static const unsigned overlap_lcores[] = {0, 1, 2, 3};
static const unsigned overlap_sockets[] = {0, 0, 1, 1};
static struct pqos_cores_domains overlap_cores_domains = {
    .num_cores = DIM(overlap_core_domains),
    .domains = overlap_core_domains};
static struct pqos_sysconfig overlap_sys = {.erdt = &overlap_erdt,
                                            .cores_domains =
                                                &overlap_cores_domains};

/* A topology that a cpuset can leave without CPU 0, whose CACD is domain
 * 0's: domains 0-3 each enumerate one CPU, 0-3, and CPU 4, on socket 1, is in
 * no CACD, so the mapping reads it as domain 0. CPU 4 then stands in for CPU 0,
 * one core for the one domain 0 lists, and only a check that the topology has
 * every CPU the mapping covers tells the two apart. The second topology also
 * lists CPU 5, which the mapping does not cover, as the review's case has it.
 */
static uint32_t cpuset_ids_0[] = {0};
static uint32_t cpuset_ids_1[] = {1};
static uint32_t cpuset_ids_2[] = {2};
static uint32_t cpuset_ids_3[] = {3};
static struct pqos_cpu_agent_info cpuset_agents[] = {
    {.rmdd = {.domain_id = 0}, .cacd = CACD(cpuset_ids_0)},
    {.rmdd = {.domain_id = 1}, .cacd = CACD(cpuset_ids_1)},
    {.rmdd = {.domain_id = 2}, .cacd = CACD(cpuset_ids_2)},
    {.rmdd = {.domain_id = 3}, .cacd = CACD(cpuset_ids_3)}};
static struct pqos_erdt_info cpuset_erdt = {.num_cpu_agents =
                                                DIM(cpuset_agents),
                                            .cpu_agents = cpuset_agents};
static uint16_t cpuset_core_domains[] = {0, 1, 2, 3, 0};
static struct pqos_cores_domains cpuset_cores_domains = {
    .num_cores = DIM(cpuset_core_domains),
    .domains = cpuset_core_domains};
static const unsigned cpuset_lcores[] = {1, 2, 3, 4};
static const unsigned cpuset_sockets[] = {0, 0, 0, 1};
static struct pqos_sysconfig cpuset_sys = {.erdt = &cpuset_erdt,
                                           .cores_domains =
                                               &cpuset_cores_domains};
static const unsigned cpuset5_lcores[] = {1, 2, 3, 4, 5};
static const unsigned cpuset5_sockets[] = {0, 0, 0, 1, 1};
static struct pqos_sysconfig cpuset5_sys = {.erdt = &cpuset_erdt,
                                            .cores_domains =
                                                &cpuset_cores_domains};

/* Domain 0's CACD repeats enumeration ID 0 four times, and domain 1's lists
 * it once too. The mapping gives CPU 0 to domain 1 and reads CPUs 1 and 2,
 * in no CACD, as domain 0: two cores where domain 0's CACD lists four. The
 * counts are checked first, which bounds the IDs left to compare to the
 * cores, so this ERDT is refused for its counts and not for its shared ID.
 */
static uint32_t repeat_ids_0[] = {0, 0, 0, 0};
static uint32_t repeat_ids_1[] = {0};
static struct pqos_cpu_agent_info repeat_agents[] = {
    {.rmdd = {.domain_id = 0}, .cacd = CACD(repeat_ids_0)},
    {.rmdd = {.domain_id = 1}, .cacd = CACD(repeat_ids_1)}};
static struct pqos_erdt_info repeat_erdt = {.num_cpu_agents =
                                                DIM(repeat_agents),
                                            .cpu_agents = repeat_agents};
static uint16_t repeat_core_domains[] = {1, 0, 0};
static const unsigned repeat_lcores[] = {0, 1, 2};
static const unsigned repeat_sockets[] = {0, 0, 0};
static struct pqos_cores_domains repeat_cores_domains = {
    .num_cores = DIM(repeat_core_domains),
    .domains = repeat_core_domains};
static struct pqos_sysconfig repeat_sys = {.erdt = &repeat_erdt,
                                           .cores_domains =
                                               &repeat_cores_domains};

/* malloc() fails at its m_fail_malloc'th call, counting from 1, and 0 leaves
 * it alone. The dumps allocate their domain and RMID tables once the options
 * are checked, so a case with a complete selection can fail each of them.
 */
static int m_fail_malloc;

/* calloc() fails the same way, at its m_fail_calloc'th call: the CPU topology
 * check takes its table from it, once the dump's domain table is allocated
 */
static int m_fail_calloc;

/* the block a case watches: with m_track_malloc set, malloc() records the
 * first block it hands out, and free() notes the release of that block
 */
static int m_track_malloc;
static void *m_tracked;
static int m_tracked_freed;

void *
__wrap_malloc(size_t size)
{
        void *ptr;

        if (m_fail_malloc > 0 && --m_fail_malloc == 0)
                return NULL;
        ptr = __real_malloc(size);
        if (m_track_malloc && m_tracked == NULL)
                m_tracked = ptr;
        return ptr;
}

void *
__wrap_calloc(size_t nmemb, size_t size)
{
        if (m_fail_calloc > 0 && --m_fail_calloc == 0)
                return NULL;
        return __real_calloc(nmemb, size);
}

void
__wrap_free(void *ptr)
{
        if (ptr != NULL && ptr == m_tracked)
                m_tracked_freed = 1;
        __real_free(ptr);
}

/* the domains a selection resolved to, which is all a case here asks of the
 * dump itself
 */
int
__wrap_pqos_dump(const struct pqos_mmio_dump *dump_cfg)
{
        const unsigned num_domain_ids = dump_cfg->topology.num_domain_ids;
        const uint16_t *domain_ids = dump_cfg->topology.domain_ids;

        check_expected(num_domain_ids);
        check_expected_ptr(domain_ids);

        return PQOS_RETVAL_OK;
}

static struct pqos_cpuinfo *
make_cpu(const unsigned *lcores, const unsigned *sockets, unsigned num_cores)
{
        struct pqos_cpuinfo *cpu;

        cpu = calloc(1, sizeof(*cpu) + num_cores * sizeof(cpu->cores[0]));
        if (cpu == NULL)
                return NULL;

        cpu->num_cores = num_cores;
        for (unsigned i = 0; i < num_cores; i++) {
                cpu->cores[i].lcore = lcores[i];
                cpu->cores[i].socket = sockets[i];
        }

        return cpu;
}

static int
cbb_setup(void **state)
{
        UNUSED_ARG(state);

        cbb_sys.cpu = make_cpu(cbb_lcores, cbb_sockets, DIM(cbb_lcores));
        stray_sys.cpu =
            make_cpu(stray_lcores, stray_sockets, DIM(stray_lcores));
        empty0_sys.cpu =
            make_cpu(empty0_lcores, empty0_sockets, DIM(empty0_lcores));
        extra0_sys.cpu =
            make_cpu(extra0_lcores, extra0_sockets, DIM(extra0_lcores));
        overlap_sys.cpu =
            make_cpu(overlap_lcores, overlap_sockets, DIM(overlap_lcores));
        cpuset_sys.cpu =
            make_cpu(cpuset_lcores, cpuset_sockets, DIM(cpuset_lcores));
        cpuset5_sys.cpu =
            make_cpu(cpuset5_lcores, cpuset5_sockets, DIM(cpuset5_lcores));
        repeat_sys.cpu =
            make_cpu(repeat_lcores, repeat_sockets, DIM(repeat_lcores));

        return cbb_sys.cpu != NULL && stray_sys.cpu != NULL &&
                       empty0_sys.cpu != NULL && extra0_sys.cpu != NULL &&
                       overlap_sys.cpu != NULL && cpuset_sys.cpu != NULL &&
                       cpuset5_sys.cpu != NULL && repeat_sys.cpu != NULL
                   ? 0
                   : -1;
}

static int
cbb_teardown(void **state)
{
        UNUSED_ARG(state);

        free(cbb_sys.cpu);
        free(stray_sys.cpu);
        free(empty0_sys.cpu);
        free(extra0_sys.cpu);
        free(overlap_sys.cpu);
        free(cpuset_sys.cpu);
        free(cpuset5_sys.cpu);
        free(repeat_sys.cpu);

        return 0;
}

/* ======== dump_mmio_regs ======== */

static void
test_dump_mmio_regs_requires_a_domain(void **state)
{
        int ret = PQOS_RETVAL_OK;

        UNUSED_ARG(state);

        run_function(dump_mmio_regs, ret, &test_sys);

        assert_int_equal(output_exit_was_called(), 0);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
        assert_int_equal(output_has_text("Provide either --socket or "
                                         "--dump-domain-id"),
                         1);
}

static void
test_dump_mmio_regs_requires_a_space(void **state)
{
        int ret = PQOS_RETVAL_OK;

        UNUSED_ARG(state);

        /* a complete selection but for --space, which used to dump CMRC and
         * report success, because the space selector defaults to zero. A
         * socket rather than a domain id, which would take its place in the
         * case after this one
         */
        selfn_dump_socket("1");

        run_function(dump_mmio_regs, ret, &test_sys);

        assert_int_equal(output_exit_was_called(), 0);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
        assert_int_equal(output_has_text("Provide --space"), 1);
        assert_int_equal(output_has_text("MMIO space dump"), 0);
}

static void
test_dump_mmio_regs_refuses_a_socket_without_agents(void **state)
{
        int ret = PQOS_RETVAL_OK;
        int unallocated;

        UNUSED_ARG(state);

        /* the socket the case above selected, and a space now: the ERDT here
         * lists no CPU agents, so no socket has the domains a CMRC dump reads,
         * and that is said before a table is sized for none. The allocator is
         * armed, so an allocation tried all the same fails the case, and a
         * charge it still holds afterwards shows none was tried.
         */
        selfn_dump_space("cmrc");

        m_fail_malloc = 1;
        run_function(dump_mmio_regs, ret, &test_sys);
        unallocated = m_fail_malloc;
        m_fail_malloc = 0;

        assert_int_equal(unallocated, 1);
        assert_int_equal(output_exit_was_called(), 0);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
        assert_int_equal(output_has_text("ERDT lists no CPU Agents"), 1);
        assert_int_equal(
            output_has_text("Error with memory allocation for domain_ids!"), 0);
}

static void
test_dump_mmio_regs_without_memory_for_socket_domains_returns(void **state)
{
        int ret = PQOS_RETVAL_OK;

        UNUSED_ARG(state);

        /* the socket and the space the cases above selected, on a platform
         * with CPU agents: the table of the socket's domains is the
         * allocation that fails, before the ERDT is read for them
         */
        m_fail_malloc = 1;
        run_function(dump_mmio_regs, ret, &cbb_sys);
        m_fail_malloc = 0;

        assert_int_equal(output_exit_was_called(), 0);
        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);
        assert_int_equal(
            output_has_text("Error with memory allocation for domain_ids!"), 1);
        assert_int_equal(output_has_text("Unable to get domain ids for socket"),
                         0);
}

/* the sockets a --socket case dumps, with the space it reads: each case
 * selects both, so that it does not depend on the cases run before it
 */
static void
select_socket(const char *sockets)
{
        selfn_dump_socket(sockets);
        selfn_dump_space("cmrc");
}

static void
test_dump_mmio_regs_socket_takes_the_cpu_domains_of_its_cores(void **state)
{
        static const uint16_t domain_ids[] = {1, 3};
        int ret = PQOS_RETVAL_ERROR;

        UNUSED_ARG(state);

        /* socket 0 holds two CPU domains where CPU_AGENTS_PER_SOCKET says
         * four, and used to be refused
         */
        select_socket("0");

        expect_value(__wrap_pqos_dump, num_domain_ids, DIM(domain_ids));
        expect_memory(__wrap_pqos_dump, domain_ids, domain_ids,
                      sizeof(domain_ids));

        run_function(dump_mmio_regs, ret, &cbb_sys);

        assert_int_equal(output_exit_was_called(), 0);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_dump_mmio_regs_sockets_take_the_cpu_domains_in_turn(void **state)
{
        static const uint16_t domain_ids[] = {1, 3, 4};
        int ret = PQOS_RETVAL_ERROR;

        UNUSED_ARG(state);

        select_socket("0,1");

        expect_value(__wrap_pqos_dump, num_domain_ids, DIM(domain_ids));
        expect_memory(__wrap_pqos_dump, domain_ids, domain_ids,
                      sizeof(domain_ids));

        run_function(dump_mmio_regs, ret, &cbb_sys);

        assert_int_equal(output_exit_was_called(), 0);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_dump_mmio_regs_refuses_a_socket_without_cpu_domains(void **state)
{
        int ret = PQOS_RETVAL_OK;

        UNUSED_ARG(state);

        /* the platform has CPU agents, none of them with cores on socket 2;
         * the dump is not reached, which the wrapper would fail on
         */
        select_socket("2");

        run_function(dump_mmio_regs, ret, &cbb_sys);

        assert_int_equal(output_exit_was_called(), 0);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
        assert_int_equal(
            output_has_text("Socket 2 holds none of the 3 CPU Agents"), 1);
        assert_int_equal(
            output_has_text("Unable to get domain ids for socket 2"), 1);
}

static void
test_dump_mmio_regs_refuses_a_domain_on_two_sockets(void **state)
{
        int ret = PQOS_RETVAL_OK;

        UNUSED_ARG(state);

        /* socket 1 holds domain 4, and holds domain 0 only by the CPU no CACD
         * lists; which socket domain 0 is on cannot be told, so neither is
         * taken for it, and the dump is not reached
         */
        select_socket("1");

        run_function(dump_mmio_regs, ret, &stray_sys);

        assert_int_equal(output_exit_was_called(), 0);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
        assert_int_equal(output_has_text("Domain ID 0 has cores on sockets 0 "
                                         "and 1, so its socket is unknown"),
                         1);
        assert_int_equal(
            output_has_text("Unable to get domain ids for socket 1"), 1);
}

static void
test_dump_mmio_regs_refuses_a_domain_on_no_socket(void **state)
{
        struct pqos_sysconfig sys = cbb_sys;
        int ret = PQOS_RETVAL_OK;

        UNUSED_ARG(state);

        /* socket 0 holds domains 1 and 3, and may hold domain 5, whose CACD
         * enumerates no core; a dump of 1 and 3 alone would pass for the
         * socket's, so the dump is not reached
         */
        sys.erdt = &unplaced_erdt;
        select_socket("0");

        run_function(dump_mmio_regs, ret, &sys);

        assert_int_equal(output_exit_was_called(), 0);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
        assert_int_equal(output_has_text("Domain ID 5 has no core in its CACD, "
                                         "so its socket is unknown"),
                         1);
        assert_int_equal(
            output_has_text("Unable to get domain ids for socket 0"), 1);
}

static void
test_dump_mmio_regs_refuses_a_domain_with_an_empty_cacd(void **state)
{
        int ret = PQOS_RETVAL_OK;

        UNUSED_ARG(state);

        /* domain 0 enumerates no core, and the only core the mapping gives it
         * is the CPU that no CACD lists, on socket 1: that is no evidence it
         * is on socket 1, so the dump is not reached
         */
        select_socket("1");

        run_function(dump_mmio_regs, ret, &empty0_sys);

        assert_int_equal(output_exit_was_called(), 0);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
        assert_int_equal(output_has_text("Domain ID 0 has no core in its CACD, "
                                         "so its socket is unknown"),
                         1);
        assert_int_equal(
            output_has_text("Unable to get domain ids for socket 1"), 1);
}

static void
test_dump_mmio_regs_refuses_a_domain_with_extra_cores(void **state)
{
        int ret = PQOS_RETVAL_OK;

        UNUSED_ARG(state);

        /* the mapping gives domain 0 a third core, the CPU no CACD lists; the
         * mapping and ERDT disagree about domain 0, so it is not placed, though
         * the stray core is on its socket
         */
        select_socket("0");

        run_function(dump_mmio_regs, ret, &extra0_sys);

        assert_int_equal(output_exit_was_called(), 0);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
        assert_int_equal(output_has_text("Domain ID 0 has 3 cores in the CPU "
                                         "topology where its CACD lists 2, so "
                                         "its socket is unknown"),
                         1);
        assert_int_equal(
            output_has_text("Unable to get domain ids for socket 0"), 1);
}

static void
test_dump_mmio_regs_refuses_overlapping_cacds(void **state)
{
        int ret = PQOS_RETVAL_OK;

        UNUSED_ARG(state);

        /* domain 0 has as many cores as its CACD lists, but they are not the
         * ones it lists: those went to domain 1, which lists them too, so the
         * dump is not reached
         */
        select_socket("1");

        run_function(dump_mmio_regs, ret, &overlap_sys);

        assert_int_equal(output_exit_was_called(), 0);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
        assert_int_equal(output_has_text("Domain IDs 0 and 1 both list "
                                         "enumeration ID 0x0 in their CACDs"),
                         1);
        assert_int_equal(
            output_has_text("Unable to get domain ids for socket 1"), 1);
}

static void
test_dump_mmio_regs_refuses_a_cpu_the_topology_lacks(void **state)
{
        int ret = PQOS_RETVAL_OK;

        UNUSED_ARG(state);

        /* CPU 0, domain 0's, is not in the topology, and CPU 4, in no CACD,
         * would have put domain 0 on socket 1; the dump is not reached
         */
        select_socket("1");

        run_function(dump_mmio_regs, ret, &cpuset_sys);

        assert_int_equal(output_exit_was_called(), 0);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
        assert_int_equal(output_has_text("CPU 0 of the core to domain mapping "
                                         "is not in the CPU topology"),
                         1);
        assert_int_equal(
            output_has_text("Unable to get domain ids for socket 1"), 1);
}

static void
test_dump_mmio_regs_refuses_a_cpu_the_mapping_lacks(void **state)
{
        int ret = PQOS_RETVAL_OK;

        UNUSED_ARG(state);

        /* the review's case: CPU 5 is in the topology but not in the mapping,
         * which covers CPUs 0-4
         */
        select_socket("0");

        run_function(dump_mmio_regs, ret, &cpuset5_sys);

        assert_int_equal(output_exit_was_called(), 0);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
        assert_int_equal(output_has_text("CPU 5 of the CPU topology is outside "
                                         "the core to domain mapping"),
                         1);
        assert_int_equal(
            output_has_text("Unable to get domain ids for socket 0"), 1);
}

static void
test_dump_mmio_regs_checks_counts_before_comparing_ids(void **state)
{
        const long page = sysconf(_SC_PAGESIZE);
        uint32_t *guard;
        int ret = PQOS_RETVAL_OK;

        UNUSED_ARG(state);

        /* the CACDs' IDs are on a page that cannot be read: the counts refuse
         * this ERDT without looking at them, so a comparison made before the
         * counts fails the case, whatever is done with its result
         */
        guard = mmap(NULL, (size_t)page, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS,
                     -1, 0);
        assert_true(guard != MAP_FAILED);
        repeat_agents[0].cacd.enumeration_ids = guard;
        repeat_agents[1].cacd.enumeration_ids = guard;

        select_socket("0");

        run_function(dump_mmio_regs, ret, &repeat_sys);

        repeat_agents[0].cacd.enumeration_ids = repeat_ids_0;
        repeat_agents[1].cacd.enumeration_ids = repeat_ids_1;
        munmap(guard, (size_t)page);

        assert_int_equal(output_exit_was_called(), 0);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
        assert_int_equal(output_has_text("Domain ID 0 has 2 cores in the CPU "
                                         "topology where its CACD lists 4"),
                         1);
        assert_int_equal(output_has_text("both list enumeration ID"), 0);
}

static void
test_dump_mmio_regs_topology_check_without_memory_returns(void **state)
{
        int ret = PQOS_RETVAL_OK;

        UNUSED_ARG(state);

        /* the domain table is allocated, and the topology check's calloc()
         * fails: the call returns, frees the table and does not reach the
         * dump, which the wrapper would fail on
         */
        select_socket("0");

        m_tracked = NULL;
        m_tracked_freed = 0;
        m_track_malloc = 1;
        m_fail_calloc = 1;
        run_function(dump_mmio_regs, ret, &cbb_sys);
        m_fail_calloc = 0;
        m_track_malloc = 0;

        assert_int_equal(output_exit_was_called(), 0);
        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);
        assert_int_equal(output_has_text("Error with memory allocation for the "
                                         "CPU topology check!"),
                         1);
        assert_int_equal(
            output_has_text("Unable to get domain ids for socket 0"), 1);
        assert_non_null(m_tracked);
        assert_int_equal(m_tracked_freed, 1);
        m_tracked = NULL;
}

/* socket 0 of a platform that is complete but for the one piece the caller
 * removed, which has to be refused rather than read through
 */
static void
check_socket_without_core_domains(const struct pqos_sysconfig *sys)
{
        int ret = PQOS_RETVAL_OK;

        select_socket("0");

        run_function(dump_mmio_regs, ret, sys);

        assert_int_equal(output_exit_was_called(), 0);
        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);
        assert_int_equal(
            output_has_text("Core to domain mapping not available"), 1);
}

static void
test_dump_mmio_regs_socket_without_cpu_topology_returns(void **state)
{
        struct pqos_sysconfig sys = cbb_sys;

        UNUSED_ARG(state);

        sys.cpu = NULL;
        check_socket_without_core_domains(&sys);
}

static void
test_dump_mmio_regs_socket_without_core_domains_returns(void **state)
{
        struct pqos_sysconfig sys = cbb_sys;

        UNUSED_ARG(state);

        sys.cores_domains = NULL;
        check_socket_without_core_domains(&sys);
}

static void
test_dump_mmio_regs_socket_without_core_domain_table_returns(void **state)
{
        struct pqos_cores_domains no_table = {
            .num_cores = DIM(cbb_core_domains), .domains = NULL};
        struct pqos_sysconfig sys = cbb_sys;

        UNUSED_ARG(state);

        sys.cores_domains = &no_table;
        check_socket_without_core_domains(&sys);
}

static void
test_dump_mmio_regs_without_memory_for_given_domains_returns(void **state)
{
        int ret = PQOS_RETVAL_OK;

        UNUSED_ARG(state);

        /* a domain id now, which takes the socket's place: the table of the
         * ids given is the allocation that fails
         */
        selfn_dump_domain_id("0");

        m_fail_malloc = 1;
        run_function(dump_mmio_regs, ret, &test_sys);
        m_fail_malloc = 0;

        assert_int_equal(output_exit_was_called(), 0);
        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);
        assert_int_equal(output_has_text("--socket is ignored"), 1);
        assert_int_equal(
            output_has_text("Error with memory allocation for domain_ids!"), 1);
}

static void
test_dump_mmio_regs_without_erdt_returns(void **state)
{
        int ret = PQOS_RETVAL_OK;
        struct pqos_sysconfig no_erdt = {.erdt = NULL};

        UNUSED_ARG(state);

        run_function(dump_mmio_regs, ret, &no_erdt);

        assert_int_equal(output_exit_was_called(), 0);
        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);
        assert_int_equal(output_has_text("ERDT info not available!"), 1);
}

/* ======== dump_rmid_regs ======== */

static void
test_dump_rmid_regs_requires_a_domain(void **state)
{
        int ret = PQOS_RETVAL_OK;

        UNUSED_ARG(state);

        run_function(dump_rmid_regs, ret, &test_sys);

        assert_int_equal(output_exit_was_called(), 0);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
        assert_int_equal(
            output_has_text("Missing --dump-rmid-domain-ids option"), 1);
}

static void
test_dump_rmid_regs_requires_rmids(void **state)
{
        int ret = PQOS_RETVAL_OK;

        UNUSED_ARG(state);

        selfn_dump_rmid_domain_ids("0");

        run_function(dump_rmid_regs, ret, &test_sys);

        assert_int_equal(output_exit_was_called(), 0);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
        assert_int_equal(output_has_text("Missing --dump-rmids option"), 1);
}

static void
test_dump_rmid_regs_requires_a_type(void **state)
{
        int ret = PQOS_RETVAL_OK;

        UNUSED_ARG(state);

        /* a complete selection but for --dump-rmid-type, which used to dump the
         * MBM registers and report success, because the type selector defaults
         * to zero
         */
        selfn_dump_rmid_domain_ids("0");
        selfn_dump_rmids("0");

        run_function(dump_rmid_regs, ret, &test_sys);

        assert_int_equal(output_exit_was_called(), 0);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
        assert_int_equal(output_has_text("Missing --dump-rmid-type option"), 1);
        assert_int_equal(output_has_text("RMID MBM DUMP"), 0);
}

static void
test_dump_rmid_regs_without_memory_returns(void **state)
{
        static const struct {
                int fail;
                const char *said;
        } tables[] = {{1, "Error with memory allocation for domain_ids!"},
                      {2, "Error with memory allocation for rmids!"}};
        unsigned i;

        UNUSED_ARG(state);

        /* a complete selection now, of a type that reads no memory regions:
         * the first allocation the dump makes is the domain table, and the
         * second the RMID table
         */
        selfn_dump_rmid_type("l3");

        for (i = 0; i < DIM(tables); i++) {
                int ret = PQOS_RETVAL_OK;

                m_fail_malloc = tables[i].fail;
                run_function(dump_rmid_regs, ret, &test_sys);
                m_fail_malloc = 0;

                assert_int_equal(output_exit_was_called(), 0);
                assert_int_equal(ret, PQOS_RETVAL_RESOURCE);
                assert_int_equal(output_has_text(tables[i].said), 1);
        }
}

static void
test_dump_rmid_regs_without_erdt_returns(void **state)
{
        int ret = PQOS_RETVAL_OK;
        struct pqos_sysconfig no_erdt = {.erdt = NULL};

        UNUSED_ARG(state);

        run_function(dump_rmid_regs, ret, &no_erdt);

        assert_int_equal(output_exit_was_called(), 0);
        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);
        assert_int_equal(output_has_text("ERDT info not available!"), 1);
}

int
main(void)
{
        /* the selections are static in the modules under test and are only
         * ever added to, so the cases run from the emptiest command line to
         * the most complete one
         */
        const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_dump_mmio_regs_without_erdt_returns),
            cmocka_unit_test(test_dump_mmio_regs_requires_a_domain),
            cmocka_unit_test(test_dump_mmio_regs_requires_a_space),
            cmocka_unit_test(
                test_dump_mmio_regs_refuses_a_socket_without_agents),
            cmocka_unit_test(
                test_dump_mmio_regs_without_memory_for_socket_domains_returns),
            cmocka_unit_test(
                test_dump_mmio_regs_socket_takes_the_cpu_domains_of_its_cores),
            cmocka_unit_test(
                test_dump_mmio_regs_sockets_take_the_cpu_domains_in_turn),
            cmocka_unit_test(
                test_dump_mmio_regs_refuses_a_socket_without_cpu_domains),
            cmocka_unit_test(
                test_dump_mmio_regs_refuses_a_domain_on_two_sockets),
            cmocka_unit_test(test_dump_mmio_regs_refuses_a_domain_on_no_socket),
            cmocka_unit_test(
                test_dump_mmio_regs_refuses_a_domain_with_an_empty_cacd),
            cmocka_unit_test(
                test_dump_mmio_regs_refuses_a_domain_with_extra_cores),
            cmocka_unit_test(test_dump_mmio_regs_refuses_overlapping_cacds),
            cmocka_unit_test(
                test_dump_mmio_regs_refuses_a_cpu_the_topology_lacks),
            cmocka_unit_test(
                test_dump_mmio_regs_refuses_a_cpu_the_mapping_lacks),
            cmocka_unit_test(
                test_dump_mmio_regs_checks_counts_before_comparing_ids),
            cmocka_unit_test(
                test_dump_mmio_regs_topology_check_without_memory_returns),
            cmocka_unit_test(
                test_dump_mmio_regs_socket_without_cpu_topology_returns),
            cmocka_unit_test(
                test_dump_mmio_regs_socket_without_core_domains_returns),
            cmocka_unit_test(
                test_dump_mmio_regs_socket_without_core_domain_table_returns),
            cmocka_unit_test(
                test_dump_mmio_regs_without_memory_for_given_domains_returns),
            cmocka_unit_test(test_dump_rmid_regs_without_erdt_returns),
            cmocka_unit_test(test_dump_rmid_regs_requires_a_domain),
            cmocka_unit_test(test_dump_rmid_regs_requires_rmids),
            cmocka_unit_test(test_dump_rmid_regs_requires_a_type),
            cmocka_unit_test(test_dump_rmid_regs_without_memory_returns)};

        return cmocka_run_group_tests(tests, cbb_setup, cbb_teardown);
}
