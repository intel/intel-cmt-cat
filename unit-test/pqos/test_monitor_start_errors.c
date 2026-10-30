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
 * What pqos says when a task group's monitoring cannot start. On the OS
 * interface, a group whose events resctrl counts, rather than perf, is a
 * resctrl monitoring group, whose mkdir takes an RMID: a start that finds none
 * left fails with PQOS_RETVAL_RESOURCE, after the library's own message, and
 * one that finds every free RMID in the kernel's limbo with PQOS_RETVAL_BUSY.
 * Either way pqos adds that too many groups may be selected. On the MSR
 * interface a group is no resctrl group, and the hint is not given.
 *
 * monitor.c is included rather than linked, as test_monitor_signals.c includes
 * it: a case adds a group of this process as the -p option does and runs
 * monitor_setup(), with the library's start of a task group wrapped.
 */

#include "test_monitor_start_errors.h"

#include "monitor.c"
#include "output.h"

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
/* clang-format off */
#include <cmocka.h>
/* clang-format on */

/** the interface the library is said to use */
static enum pqos_interface g_interface;

/** what the start of a task group returns */
static int g_start_ret;

/** how many groups the wrapped start and stop were asked for */
static unsigned g_starts;
static unsigned g_stops;

int
__wrap_pqos_inter_get(enum pqos_interface *interface)
{
        *interface = g_interface;
        return PQOS_RETVAL_OK;
}

int
__wrap_pqos_mon_start_pids2(const unsigned num_pids,
                            const pid_t *pids,
                            const enum pqos_mon_event event,
                            void *context,
                            struct pqos_mon_data **group)
{
        (void)num_pids;
        (void)pids;
        (void)event;
        (void)context;

        g_starts++;
        *group = NULL;
        return g_start_ret;
}

int
__wrap_pqos_mon_stop(struct pqos_mon_data *group)
{
        (void)group;
        g_stops++;
        return PQOS_RETVAL_OK;
}

/**
 * @brief Selects this process the way "-p llc:<pid>" does and starts it
 *
 * @return what monitor_setup() returned
 */
static int
setup(void)
{
        struct pqos_cpuinfo cpu;
        struct pqos_capability cap;
        struct pqos_cap_mon *mon;
        const uint64_t pid = (uint64_t)getpid();
        char *desc = uinttostr_checked((unsigned)pid);
        int ret = 0;

        assert_non_null(desc);
        assert_non_null(grp_add(MON_GROUP_TYPE_PID, PQOS_MON_EVENT_L3_OCCUP,
                                desc, &pid, 1));

        memset(&cpu, 0, sizeof(cpu));
        mon = calloc(1, sizeof(*mon) + sizeof(mon->events[0]));
        assert_non_null(mon);
        mon->max_rmid = 64;
        mon->num_events = 1;
        mon->events[0].type = PQOS_MON_EVENT_L3_OCCUP;
        cap.type = PQOS_CAP_TYPE_MON;
        cap.u.mon = mon;

        run_function(monitor_setup, ret, &cpu, &cap, NULL);
        free(mon);
        assert_false(output_exit_was_called());

        return ret;
}

static int
reset(void **state)
{
        unsigned i;

        (void)state;
        for (i = 0; i < sel_monitor_num; i++)
                grp_free(&sel_monitor_group[i]);
        free(sel_monitor_group);
        sel_monitor_group = NULL;
        sel_monitor_num = 0;
        sel_monitor_type = 0;
        monitor_cleanup();
        (void)signal(SIGINT, SIG_DFL);
        (void)signal(SIGHUP, SIG_DFL);
        (void)signal(SIGTERM, SIG_DFL);
        g_interface = PQOS_INTER_OS;
        g_start_ret = PQOS_RETVAL_OK;
        g_starts = 0;
        g_stops = 0;

        return 0;
}

/**
 * @brief A start that finds no RMID left gives the hint
 *
 * The case of tests/pqos/test_pqos_cmt.py's pids_exceed_max_number_of_groups,
 * which asks for one task group more than the RMIDs that the classes leave
 */
static void
test_a_start_with_no_rmid_left_gives_the_hint(void **state)
{
        (void)state;
        g_start_ret = PQOS_RETVAL_RESOURCE;

        assert_int_equal(setup(), -1);
        assert_int_equal(g_starts, 1);
        assert_true(output_has_text("PID %d monitoring start error: resource "
                                    "error (status %d). Too many monitoring "
                                    "groups may be selected",
                                    (int)getpid(), PQOS_RETVAL_RESOURCE));
}

/**
 * @brief So does one on the OS interface that monitors with resctrl only
 */
static void
test_a_start_with_no_rmid_left_on_resctrl_mon_gives_the_hint(void **state)
{
        (void)state;
        g_interface = PQOS_INTER_OS_RESCTRL_MON;
        g_start_ret = PQOS_RETVAL_RESOURCE;

        assert_int_equal(setup(), -1);
        assert_true(output_has_text("Too many monitoring groups may be "
                                    "selected"));
}

/**
 * @brief And one that finds every free RMID in limbo, as before
 */
static void
test_a_start_with_the_rmids_in_limbo_gives_the_hint(void **state)
{
        (void)state;
        g_start_ret = PQOS_RETVAL_BUSY;

        assert_int_equal(setup(), -1);
        assert_true(output_has_text("PID %d monitoring start error: resource "
                                    "busy while creating resctrl monitoring "
                                    "group. Too many monitoring groups may be "
                                    "selected",
                                    (int)getpid()));
}

/**
 * @brief A start on the MSR interface does not: its groups take no RMID of
 *        resctrl's
 */
static void
test_a_start_on_msr_gives_no_hint(void **state)
{
        (void)state;
        g_interface = PQOS_INTER_MSR;
        g_start_ret = PQOS_RETVAL_RESOURCE;

        assert_int_equal(setup(), -1);
        assert_true(output_has_text("PID %d monitoring start error: resource "
                                    "error (status %d)\n",
                                    (int)getpid(), PQOS_RETVAL_RESOURCE));
        assert_false(output_has_text("Too many"));
}

/**
 * @brief Nor does another failure
 */
static void
test_another_failure_gives_no_hint(void **state)
{
        (void)state;
        g_start_ret = PQOS_RETVAL_ERROR;

        assert_int_equal(setup(), -1);
        assert_true(output_has_text("PID %d monitoring start error: %s "
                                    "(status %d)\n",
                                    (int)getpid(),
                                    pqos_retval_to_string(PQOS_RETVAL_ERROR),
                                    PQOS_RETVAL_ERROR));
        assert_false(output_has_text("Too many"));
}

int
main(void)
{
        const struct CMUnitTest tests[] = {
            cmocka_unit_test_setup_teardown(
                test_a_start_with_no_rmid_left_gives_the_hint, reset, reset),
            cmocka_unit_test_setup_teardown(
                test_a_start_with_no_rmid_left_on_resctrl_mon_gives_the_hint,
                reset, reset),
            cmocka_unit_test_setup_teardown(
                test_a_start_with_the_rmids_in_limbo_gives_the_hint, reset,
                reset),
            cmocka_unit_test_setup_teardown(test_a_start_on_msr_gives_no_hint,
                                            reset, reset),
            cmocka_unit_test_setup_teardown(test_another_failure_gives_no_hint,
                                            reset, reset),
        };

        return cmocka_run_group_tests(tests, NULL, NULL);
}
