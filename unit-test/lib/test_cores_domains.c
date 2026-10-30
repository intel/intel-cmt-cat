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

#include "cores_domains.h"
#include "test.h"

#include <string.h>

/* a /proc/cpuinfo of five CPUs, each one's APIC ID twice its number */
static const char cpuinfo[] = "processor\t: 0\napicid\t\t: 0\n\n"
                              "processor\t: 1\napicid\t\t: 2\n\n"
                              "processor\t: 2\napicid\t\t: 4\n\n"
                              "processor\t: 3\napicid\t\t: 6\n\n"
                              "processor\t: 4\napicid\t\t: 8\n\n";

/* a CPU agent of domain dm whose CACD enumerates the APIC IDs of a table */
#define AGENT(dm, ids)                                                         \
        {                                                                      \
                .rmdd = {.domain_id = (dm)}, .cacd = {                         \
                        .rmdd_domain_id = (dm),                                \
                        .enumeration_ids = (ids),                              \
                        .enum_ids_length = DIM(ids)                            \
                }                                                              \
        }

/* maps the five CPUs by the agents given; returns cores_domains_init()'s
 * status, with the map in map
 */
static int
map_cores(struct pqos_cpu_agent_info *agents,
          const unsigned num_agents,
          struct pqos_cores_domains **map)
{
        struct pqos_erdt_info erdt;

        memset(&erdt, 0, sizeof(erdt));
        erdt.num_cpu_agents = num_agents;
        erdt.cpu_agents = agents;
        *map = NULL;

        expect_string(__wrap_pqos_fopen, name, "/proc/cpuinfo");
        expect_string(__wrap_pqos_fopen, mode, "r");
        will_return(__wrap_pqos_fopen, cpuinfo);

        return cores_domains_init(5, &erdt, map);
}

/* a CPU that no CACD lists is in no domain, rather than in domain 0 */
static void
test_cores_domains_cpu_in_no_cacd(void **state __attribute__((unused)))
{
        static uint32_t ids_1[] = {0, 2};
        static uint32_t ids_3[] = {4, 6};
        struct pqos_cpu_agent_info agents[] = {AGENT(1, ids_1),
                                               AGENT(3, ids_3)};
        const uint16_t expected[] = {1, 1, 3, 3, PQOS_CORES_DOMAINS_NONE};
        struct pqos_cores_domains *map;

        assert_int_equal(map_cores(agents, DIM(agents), &map), PQOS_RETVAL_OK);
        assert_non_null(map);
        assert_int_equal(map->num_cores, DIM(expected));
        assert_memory_equal(map->domains, expected, sizeof(expected));

        cores_domains_fini();
}

/* domain 0 is a domain like any other: the CPUs its CACD lists are in it, and
 * the ones no CACD lists are not
 */
static void
test_cores_domains_domain_0(void **state __attribute__((unused)))
{
        static uint32_t ids_0[] = {0};
        static uint32_t ids_1[] = {2};
        struct pqos_cpu_agent_info agents[] = {AGENT(0, ids_0),
                                               AGENT(1, ids_1)};
        const uint16_t expected[] = {0, 1, PQOS_CORES_DOMAINS_NONE,
                                     PQOS_CORES_DOMAINS_NONE,
                                     PQOS_CORES_DOMAINS_NONE};
        struct pqos_cores_domains *map;

        assert_int_equal(map_cores(agents, DIM(agents), &map), PQOS_RETVAL_OK);
        assert_non_null(map);
        assert_memory_equal(map->domains, expected, sizeof(expected));

        cores_domains_fini();
}

/* a CACD of the domain that marks a core in no domain is refused, saying so:
 * its cores could not be told from those of no domain
 */
static void
test_cores_domains_refuses_the_none_domain(void **state __attribute__((unused)))
{
        static uint32_t ids[] = {0};
        struct pqos_cpu_agent_info agents[] = {
            AGENT(PQOS_CORES_DOMAINS_NONE, ids)};
        struct pqos_cores_domains *map;
        int ret;

        test_log_keep_start();
        ret = map_cores(agents, DIM(agents), &map);
        assert_int_equal(test_log_count_stop(), 1);

        assert_int_equal(ret, PQOS_RETVAL_ERROR);
        assert_null(map);
        assert_non_null(strstr(test_log_said(), "CACD domain ID 0xffff"));
}

/* a CACD that lists an APIC ID no CPU has is refused, as before */
static void
test_cores_domains_refuses_an_unknown_apic_id(void **state
                                              __attribute__((unused)))
{
        static uint32_t ids[] = {0, 10};
        struct pqos_cpu_agent_info agents[] = {AGENT(1, ids)};
        struct pqos_cores_domains *map;

        assert_int_equal(map_cores(agents, DIM(agents), &map),
                         PQOS_RETVAL_ERROR);
        assert_null(map);
}

int
main(void)
{
        const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_cores_domains_cpu_in_no_cacd),
            cmocka_unit_test(test_cores_domains_domain_0),
            cmocka_unit_test(test_cores_domains_refuses_the_none_domain),
            cmocka_unit_test(test_cores_domains_refuses_an_unknown_apic_id),
        };

        return cmocka_run_group_tests(tests, NULL, NULL);
}
