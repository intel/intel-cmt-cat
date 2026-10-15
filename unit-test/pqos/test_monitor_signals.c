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
 * What a signal that ends monitoring does when it arrives while the groups are
 * starting, and while the loop runs: either way the loop polls no more, and
 * every group that started is stopped. Before the handlers were set ahead of
 * the start, a signal in that window ended the process with its groups running.
 *
 * monitor.c is included rather than linked, because what the cases check and
 * reset is private to it: the flag the handler sets, and the selection and
 * timeout that monitor_setup(), monitor_loop() and monitor_stop() work from. A
 * case adds a group of this process as the -p option does and runs the three
 * as main() does. The library's start, poll and stop are mocked, and the
 * signal is raised from inside the mocked call - where a Ctrl-C lands in the
 * window this is about.
 */

#include "test_monitor_signals.h"

#include "monitor.c"
#include "output.h"

#include <errno.h>
#include <setjmp.h>
#include <signal.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
/* clang-format off */
#include <cmocka.h>
/* clang-format on */

/** the signal the mocked start raises, 0 for none */
static int g_signal_at_start;

/** the signal the mocked poll raises, 0 for none */
static int g_signal_at_poll;

/** the signal whose handler the wrapped sigaction() refuses, 0 for none */
static int g_sigaction_refuses;

/** how many times the mocked calls were made */
static unsigned g_starts;
static unsigned g_polls;
static unsigned g_stops;

/** how many times the loop set its timer, which it does after every return it
 *  can take before the loop proper
 */
static unsigned g_timers_set;

/** the group the mocked start hands out */
static struct pqos_mon_data g_group;

/** the topology the loop asks for, which it reads the cache size from */
static struct pqos_cpuinfo g_cpu;

int
__wrap_pqos_inter_get(enum pqos_interface *interface)
{
        *interface = PQOS_INTER_OS;
        return PQOS_RETVAL_OK;
}

int
__wrap_pqos_cap_get(const struct pqos_cap **cap,
                    const struct pqos_cpuinfo **cpu)
{
        if (cap != NULL)
                *cap = NULL;
        if (cpu != NULL)
                *cpu = &g_cpu;

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
        *group = &g_group;
        if (g_signal_at_start != 0)
                raise(g_signal_at_start);

        return PQOS_RETVAL_OK;
}

int
__wrap_pqos_mon_poll(struct pqos_mon_data **groups, const unsigned num_groups)
{
        (void)groups;
        (void)num_groups;

        g_polls++;
        if (g_signal_at_poll != 0)
                raise(g_signal_at_poll);

        return PQOS_RETVAL_OK;
}

int
__wrap_sigaction(int signum,
                 const struct sigaction *act,
                 struct sigaction *oldact)
{
        /* a query is answered whatever a case set: only installing the
         * handler is refused
         */
        if (act != NULL && signum == g_sigaction_refuses) {
                errno = EPERM;
                return -1;
        }

        return __real_sigaction(signum, act, oldact);
}

int
__wrap_pqos_mon_stop(struct pqos_mon_data *group)
{
        assert_ptr_equal(group, &g_group);
        g_stops++;

        return PQOS_RETVAL_OK;
}

#ifdef __linux__
int
__wrap_timerfd_settime(int fd,
                       int flags,
                       const struct itimerspec *new_value,
                       struct itimerspec *old_value)
{
        const int ret = __real_timerfd_settime(fd, flags, new_value, old_value);

        if (ret == 0)
                g_timers_set++;

        return ret;
}
#else
int
__wrap_timer_settime(timer_t timerid,
                     int flags,
                     const struct itimerspec *new_value,
                     struct itimerspec *old_value)
{
        const int ret =
            __real_timer_settime(timerid, flags, new_value, old_value);

        if (ret == 0)
                g_timers_set++;

        return ret;
}
#endif

/**
 * @brief Selects this process the way "-p llc:<pid>" does and starts it
 *
 * The group is added directly rather than through the option's parser, which
 * first checks the PID through /proc: not every platform the tests build on has
 * it mounted, and the check is not what these cases are about.
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
        int ret = -1;

        assert_non_null(grp_add(MON_GROUP_TYPE_PID, PQOS_MON_EVENT_L3_OCCUP,
                                uinttostr((unsigned)pid), &pid, 1));

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

/**
 * @brief Stops the selection as main() does
 *
 * monitor_stop() frees the groups and leaves the pointer to them, so it is
 * cleared at once: a check that fails after the stop then leaves the teardown
 * nothing to free a second time.
 */
static void
stop(void)
{
        run_void_function(monitor_stop);
        sel_monitor_group = NULL;
        sel_monitor_num = 0;
        assert_false(output_exit_was_called());
}

static int
reset(void **state)
{
        (void)state;
        signal(SIGINT, SIG_DFL);
        signal(SIGHUP, SIG_DFL);
        signal(SIGTERM, SIG_DFL);
        stop_monitoring_loop = 0;
        if (sel_monitor_group != NULL)
                stop();
        sel_monitor_type = 0;
        /* a case that fails stops the loop at its timeout, not never */
        sel_timeout = 1;
        monitor_cleanup();
        g_signal_at_start = 0;
        g_signal_at_poll = 0;
        g_sigaction_refuses = 0;
        g_starts = 0;
        g_polls = 0;
        g_stops = 0;
        g_timers_set = 0;

        return 0;
}

/**
 * @brief Whether monitoring_ctrlc() is still the handler of a signal
 *
 * @param signo signal number
 *
 * @return 1 if it is, with interrupted calls restarted and the handler not
 *         reset to the default by its first delivery, 0 otherwise
 */
static int
handler_kept(const int signo)
{
        struct sigaction sa;

        assert_int_equal(sigaction(signo, NULL, &sa), 0);

        return sa.sa_handler == monitoring_ctrlc &&
               (sa.sa_flags & SA_RESTART) != 0 &&
               (sa.sa_flags & SA_RESETHAND) == 0;
}

/**
 * @brief The window: the group has started and the loop has not
 */
static void
a_signal_during_the_start(const int signo)
{
        g_signal_at_start = signo;

        assert_int_equal(setup(), 0);
        assert_int_equal(g_starts, 1);
        assert_int_equal(stop_monitoring_loop, 1);
        /* the handler outlasts the signal, so a second one during the stop
         * sets the flag again instead of ending the process
         */
        assert_true(handler_kept(signo));

        /* it gets as far as its loop, which does not run, and returns: an
         * exit() would leave the groups running, with nothing to stop them
         */
        run_void_function(monitor_loop);
        assert_false(output_exit_was_called());
        assert_int_equal(g_timers_set, 1);
        assert_int_equal(g_polls, 0);

        stop();
        assert_int_equal(g_stops, 1);
}

static void
test_sigint_during_the_start_stops_the_group(void **state)
{
        (void)state;
        a_signal_during_the_start(SIGINT);
}

static void
test_sighup_during_the_start_stops_the_group(void **state)
{
        (void)state;
        a_signal_during_the_start(SIGHUP);
}

static void
test_sigterm_during_the_start_stops_the_group(void **state)
{
        (void)state;
        a_signal_during_the_start(SIGTERM);
}

/**
 * @brief A handler that cannot be set fails the setup before any group starts
 *
 * Left at its default action, that signal would end the process with its
 * groups running, so there must be none.
 */
static void
test_no_group_starts_without_a_handler(void **state)
{
        (void)state;
        g_sigaction_refuses = SIGHUP;

        assert_int_equal(setup(), -1);
        assert_true(output_has_text("Failed to catch SIGHUP!"));
        assert_int_equal(g_starts, 0);
}

static void
test_sigint_during_the_loop_ends_it(void **state)
{
        (void)state;
        g_signal_at_poll = SIGINT;

        assert_int_equal(setup(), 0);
        assert_int_equal(stop_monitoring_loop, 0);

        run_void_function(monitor_loop);
        assert_false(output_exit_was_called());
        assert_int_equal(g_timers_set, 1);
        assert_int_equal(g_polls, 1);

        stop();
        assert_int_equal(g_stops, 1);
}

int
main(void)
{
        const struct CMUnitTest tests[] = {
            cmocka_unit_test_setup_teardown(
                test_sigint_during_the_start_stops_the_group, reset, reset),
            cmocka_unit_test_setup_teardown(
                test_sighup_during_the_start_stops_the_group, reset, reset),
            cmocka_unit_test_setup_teardown(
                test_sigterm_during_the_start_stops_the_group, reset, reset),
            cmocka_unit_test_setup_teardown(test_sigint_during_the_loop_ends_it,
                                            reset, reset),
            cmocka_unit_test_setup_teardown(
                test_no_group_starts_without_a_handler, reset, reset),
        };

        return cmocka_run_group_tests(tests, NULL, NULL);
}
