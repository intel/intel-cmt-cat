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

/**
 * @brief A monitoring run's exit status, and the cleanup that follows it
 *
 * main() makes a monitoring loop that an error ended an EXIT_FAILURE, and it
 * still stops the monitoring and finalises the library afterwards, which is
 * what the loop's returning rather than ending the process is for. main.c is
 * included, as test_print_modes.c includes it, and what main() asks of the
 * library and of the monitoring on the way is wrapped: a case runs a
 * monitoring command line through appmain() to a loop whose result it sets,
 * and records the order in which main() runs the loop, stops the monitoring
 * and finalises the library. Stopping needs the library initialised, so
 * the stop has to come between the other two.
 */

#include "test_main_monitoring.h"

#define main appmain
#include "main.c"
#undef main

#include "output.h"

#include <getopt.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* clang-format off */
#include <cmocka.h>
/* clang-format on */

/** the system main() is given: two cores, with monitoring the one capability */
static struct pqos_cap m_cap;
static struct pqos_cap_mon m_mon;
static struct pqos_capability m_cap_mon;
static struct pqos_sysconfig m_sys;

/** what the loop returns, and what main() called from the loop on, in order:
 * "loop", "stop" and "fini", each followed by a space
 */
static int m_loop_result;
static char m_calls[64];

static void
called(const char *what)
{
        const size_t used = strlen(m_calls);

        assert_true(used + strlen(what) + 1 < sizeof(m_calls));
        strcat(m_calls, what);
        strcat(m_calls, " ");
}

int
__wrap_pqos_init(const struct pqos_config *config)
{
        UNUSED_ARG(config);
        return PQOS_RETVAL_OK;
}

int
__wrap_pqos_fini(void)
{
        called("fini");
        return PQOS_RETVAL_OK;
}

int
__wrap_pqos_inter_get(enum pqos_interface *interface)
{
        *interface = PQOS_INTER_MSR;
        return PQOS_RETVAL_OK;
}

int
__wrap_pqos_sysconfig_get(const struct pqos_sysconfig **sysconf)
{
        *sysconf = &m_sys;
        return PQOS_RETVAL_OK;
}

int
__wrap_pqos_cap_get_type(const struct pqos_cap *cap,
                         const enum pqos_cap_type type,
                         const struct pqos_capability **cap_item)
{
        UNUSED_ARG(cap);
        if (type != PQOS_CAP_TYPE_MON) {
                *cap_item = NULL;
                return PQOS_RETVAL_RESOURCE;
        }
        *cap_item = &m_cap_mon;
        return PQOS_RETVAL_OK;
}

int
__wrap_alloc_apply(const struct pqos_capability *cap_l3ca,
                   const struct pqos_capability *cap_l2ca,
                   const struct pqos_capability *cap_mba,
                   const struct pqos_capability *cap_smba,
                   const struct pqos_cpuinfo *cpu,
                   const struct pqos_devinfo *dev)
{
        UNUSED_ARG(cap_l3ca);
        UNUSED_ARG(cap_l2ca);
        UNUSED_ARG(cap_mba);
        UNUSED_ARG(cap_smba);
        UNUSED_ARG(cpu);
        UNUSED_ARG(dev);
        return 0; /* nothing to apply */
}

int
__wrap_monitor_setup(const struct pqos_cpuinfo *cpu_info,
                     const struct pqos_capability *const cap_mon,
                     const struct pqos_devinfo *dev_info)
{
        UNUSED_ARG(cpu_info);
        UNUSED_ARG(cap_mon);
        UNUSED_ARG(dev_info);
        return 0;
}

int
__wrap_monitor_loop(void)
{
        called("loop");
        return m_loop_result;
}

void
__wrap_monitor_stop(void)
{
        called("stop");
}

/**
 * @brief Runs "pqos -m <groups>" to a loop that returns \a loop_result
 *
 * The groups are added to what earlier cases selected, which the parser keeps,
 * so each case names a core of its own.
 *
 * @return what appmain() returned
 */
static int
run_monitoring(const char *groups, const int loop_result)
{
        char *argv[] = {strdup("pqos"), strdup("-m"), strdup(groups), NULL};
        const int argc = (int)DIM(argv) - 1;
        int ret = -1;
        int i;

        for (i = 0; i < argc; i++)
                assert_non_null(argv[i]);

        m_loop_result = loop_result;
        m_calls[0] = '\0';
        optind = 1;
        opterr = 0;
        run_function(appmain, ret, argc, argv);

        for (i = 0; i < argc; i++)
                free(argv[i]);
        return ret;
}

static void
test_a_monitoring_loop_that_fails_makes_pqos_exit_1(void **state)
{
        UNUSED_ARG(state);

        assert_int_equal(run_monitoring("all:0", -1), EXIT_FAILURE);
        assert_int_equal(output_exit_was_called(), 0);
        /* and the monitoring is stopped, then the library finalised, all the
         * same, which is what the loop returns for
         */
        assert_string_equal(m_calls, "loop stop fini ");
}

static void
test_a_monitoring_loop_that_ends_well_makes_pqos_exit_0(void **state)
{
        UNUSED_ARG(state);

        assert_int_equal(run_monitoring("all:1", 0), EXIT_SUCCESS);
        assert_int_equal(output_exit_was_called(), 0);
        assert_string_equal(m_calls, "loop stop fini ");
}

int
main(void)
{
        const struct CMUnitTest tests[] = {
            cmocka_unit_test(
                test_a_monitoring_loop_that_fails_makes_pqos_exit_1),
            cmocka_unit_test(
                test_a_monitoring_loop_that_ends_well_makes_pqos_exit_0)};
        struct pqos_cpuinfo *cpu =
            calloc(1, sizeof(*cpu) + 2 * sizeof(struct pqos_coreinfo));
        int ret;

        if (cpu == NULL)
                return EXIT_FAILURE;
        cpu->num_cores = 2;
        cpu->cores[0].lcore = 0;
        cpu->cores[1].lcore = 1;
        m_cap_mon.type = PQOS_CAP_TYPE_MON;
        m_cap_mon.u.mon = &m_mon;
        m_sys.cap = &m_cap;
        m_sys.cpu = cpu;

        ret = cmocka_run_group_tests(tests, NULL, NULL);

        free(cpu);
        return ret;
}
