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

#include "cap.h"
#include "common_monitoring.h"
#include "mmio_monitoring.h"
#include "monitoring.h"
#include "test.h"

#include <string.h>

/* ======== mock ======== */

/* one CPU agent, of domain 0, whose CACD lists cores 0-3 of the test's
 * topology: cores 4-7 are in no CPU domain
 */
static struct pqos_cpu_agent_info cpu_agent = {
    .rmdd = {.domain_id = 0, .max_rmids = 32}};
static struct pqos_erdt_info erdt = {.num_cpu_agents = 1,
                                     .cpu_agents = &cpu_agent};
static uint16_t domains[8] = {0,
                              0,
                              0,
                              0,
                              PQOS_CORES_DOMAINS_NONE,
                              PQOS_CORES_DOMAINS_NONE,
                              PQOS_CORES_DOMAINS_NONE,
                              PQOS_CORES_DOMAINS_NONE};
static const struct pqos_cores_domains cores_domains = {.num_cores = 8,
                                                        .domains = domains};

const struct pqos_erdt_info *
__wrap__pqos_get_erdt(void)
{
        return &erdt;
}

const struct pqos_cores_domains *
__wrap__pqos_get_cores_domains(void)
{
        return &cores_domains;
}

/* every core is monitored by no one: RMID0 */
int
__wrap_mon_assoc_read(const unsigned lcore __attribute__((unused)),
                      pqos_rmid_t *rmid)
{
        *rmid = RMID0;

        return PQOS_RETVAL_OK;
}

/* the start of the performance counters, which sets those of every core of
 * the group: a case expects it, or not
 */
int
__wrap_mon_start_perf(struct pqos_mon_data *group __attribute__((unused)),
                      enum pqos_mon_event event __attribute__((unused)))
{
        function_called();

        return mock_type(int);
}

int
__wrap_mon_stop_perf(struct pqos_mon_data *group __attribute__((unused)))
{
        return PQOS_RETVAL_OK;
}

/* ======== mmio_mon_start_cores ======== */

static int
start(const unsigned num_cores, const unsigned *cores, void **state)
{
        struct test_data *data = (struct test_data *)*state;
        struct pqos_mon_data_internal intl;
        struct pqos_mon_mem_region region;
        struct pqos_mon_options opt;
        struct pqos_mon_data group;

        memset(&intl, 0, sizeof(intl));
        memset(&region, 0, sizeof(region));
        memset(&opt, 0, sizeof(opt));
        memset(&group, 0, sizeof(group));
        group.intl = &intl;
        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);

        return mmio_mon_start_cores(
            num_cores, cores,
            (enum pqos_mon_event)(PQOS_MON_EVENT_L3_OCCUP |
                                  PQOS_PERF_EVENT_LLC_MISS),
            NULL, &region, &group, &opt);
}

/* a core that no CACD lists is refused before the start of the performance
 * counters, which would have set those of the group's other cores too
 */
static void
test_mmio_mon_start_cores_core_in_no_domain(void **state)
{
        const unsigned cores[] = {1, 5};

        assert_int_equal(start(DIM(cores), cores, state), PQOS_RETVAL_PARAM);
}

/* cores that the CACD lists get as far as that start, here made to fail */
static void
test_mmio_mon_start_cores_listed_cores(void **state)
{
        const unsigned cores[] = {1, 2};

        expect_function_call(__wrap_mon_start_perf);
        will_return(__wrap_mon_start_perf, PQOS_RETVAL_ERROR);
        assert_int_equal(start(DIM(cores), cores, state), PQOS_RETVAL_ERROR);
}

int
main(void)
{
        int result = 0;

        const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_mmio_mon_start_cores_core_in_no_domain),
            cmocka_unit_test(test_mmio_mon_start_cores_listed_cores)};

        result += cmocka_run_group_tests(tests, test_init_mon, test_fini);

        return result;
}
