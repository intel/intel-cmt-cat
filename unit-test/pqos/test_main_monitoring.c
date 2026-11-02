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
 * what the loop's returning rather than ending the process is for. The end of
 * the output, which the loop leaves unwritten, it has monitor_flush() write
 * and flush last, and makes a flush that failed an EXIT_FAILURE too. main.c
 * is included, as test_print_modes.c includes it, and what main() asks of the
 * library and of the monitoring on the way is wrapped: a case runs a
 * monitoring command line through appmain() to a loop and a flush whose
 * results it sets, and records the order in which main() runs the loop,
 * stops the monitoring, finalises the library and flushes. Stopping needs the
 * library initialised, so the stop has to come between the loop and the
 * finalising; and a flush that a reader which stopped reading blocks must
 * come after both, holding nothing.
 *
 * main() has SIGPIPE ignored by the time it initialises the library, so that a
 * reader of stdout that has gone fails the writes there with EPIPE instead of
 * ending the process with the library's lock held. Where no monitoring ran,
 * main() then says a write to stdout that failed, and makes it an
 * EXIT_FAILURE; a monitoring run's output is monitor_flush()'s to check.
 */

#include "test_main_monitoring.h"

#define main appmain
#include "main.c"
#undef main

#include "output.h"

#include <errno.h>
#include <getopt.h>
#include <setjmp.h>
#include <signal.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
/* clang-format off */
#include <cmocka.h>
/* clang-format on */

/** the system main() is given: four cores, with monitoring the one
 * capability
 */
static struct pqos_cap m_cap;
static struct pqos_cap_mon m_mon;
static struct pqos_capability m_cap_mon;
static struct pqos_sysconfig m_sys;

/** what the loop and the flush return, and what main() called from the loop
 * on, in order: "loop", "stop", "fini" and "flush", and "stdout" for a flush
 * of stdout, each followed by a space
 */
static int m_loop_result;
static int m_flush_result;
static char m_calls[64];

/** 1 where SIGPIPE was ignored when main() initialised the library */
static int m_sigpipe_ignored_at_init;

/** a line the library's initialisation logs, as libpqos says a warning there,
 * or NULL for none; and whether a failure of the log had been said by the
 * time the library was finalised
 */
static const char *m_init_log;
static int m_log_said_before_fini;

/** the write end of a pipe whose reader has gone, which a case puts in place
 * of stdout's descriptor, and that descriptor, kept meanwhile
 */
static int m_reader_gone = -1;
static int m_stdout_saved = -1;

static void
called(const char *what)
{
        const size_t used = strlen(m_calls);

        assert_true(used + strlen(what) + 1 < sizeof(m_calls));
        strcat(m_calls, what);
        strcat(m_calls, " ");
}

/**
 * Logs m_init_log as libpqos would: to the descriptor it was given, a write
 * that fails leaving the status alone, and to the callback
 */
int
__wrap_pqos_init(const struct pqos_config *config)
{
        struct sigaction sa;

        m_sigpipe_ignored_at_init =
            sigaction(SIGPIPE, NULL, &sa) == 0 && sa.sa_handler == SIG_IGN;
        if (m_init_log != NULL) {
                const size_t size = strlen(m_init_log);

                if (config->fd_log >= 0 &&
                    write(config->fd_log, m_init_log, size) < 0)
                        fprintf(stderr, "log_emit: printing to file failed\n");
                if (config->callback_log != NULL)
                        config->callback_log(config->context_log, size,
                                             m_init_log);
        }
        return PQOS_RETVAL_OK;
}

int
__wrap_pqos_fini(void)
{
        called("fini");
        m_log_said_before_fini =
            output_stderr_has_text("Failed to write the log");
        return PQOS_RETVAL_OK;
}

int
__wrap_fflush(FILE *stream)
{
        if (stream == stdout)
                called("stdout");
        return __real_fflush(stream);
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

int
__wrap_monitor_flush(void)
{
        called("flush");
        return m_flush_result;
}

/**
 * @brief Runs "pqos -m <groups>" to a loop that returns \a loop_result and a
 *        flush that returns \a flush_result
 *
 * The groups are added to what earlier cases selected, which the parser keeps,
 * so each case names a core of its own.
 *
 * @return what appmain() returned
 */
static int
run_monitoring(const char *groups,
               const int loop_result,
               const int flush_result)
{
        char *argv[] = {strdup("pqos"), strdup("-m"), strdup(groups), NULL};
        const int argc = (int)DIM(argv) - 1;
        int ret = -1;
        int i;

        for (i = 0; i < argc; i++)
                assert_non_null(argv[i]);

        m_loop_result = loop_result;
        m_flush_result = flush_result;
        m_calls[0] = '\0';
        optind = 1;
        opterr = 0;
        run_function(appmain, ret, argc, argv);

        for (i = 0; i < argc; i++)
                free(argv[i]);
        return ret;
}

/**
 * @brief Runs "pqos --version", which initialises the library and monitors
 *        nothing
 *
 * @return what appmain() returned
 */
static int
run_version(void)
{
        char *argv[] = {strdup("pqos"), strdup("--version"), NULL};
        const int argc = (int)DIM(argv) - 1;
        int ret = -1;
        int i;

        for (i = 0; i < argc; i++)
                assert_non_null(argv[i]);

        m_flush_result = 0;
        m_calls[0] = '\0';
        optind = 1;
        opterr = 0;
        run_function(appmain, ret, argc, argv);
        /* the parser keeps what it selected, for the next case to start
         * from
         */
        sel_print_version = 0;

        for (i = 0; i < argc; i++)
                free(argv[i]);
        return ret;
}

/** where SIGPIPE is not ignored, a case has it handled here rather than
 * ending the process
 */
static void
sigpipe_handled(int signo)
{
        UNUSED_ARG(signo);
}

/**
 * @brief Has SIGPIPE handled, not ignored, and gives a case a pipe whose
 *        reader has gone, for it to put in place of stdout
 */
static int
case_start(void **state)
{
        struct sigaction sa;
        int fds[2];

        UNUSED_ARG(state);
        memset(&sa, 0, sizeof(sa));
        sa.sa_handler = sigpipe_handled;
        sigemptyset(&sa.sa_mask);
        assert_int_equal(sigaction(SIGPIPE, &sa, NULL), 0);
        m_sigpipe_ignored_at_init = 0;
        assert_int_equal(pipe(fds), 0);
        close(fds[0]);
        m_reader_gone = fds[1];

        return 0;
}

/**
 * @brief Puts the pipe whose reader has gone in place of stdout, with \a text
 *        in the stream for a flush to write
 *
 * The descriptor is put in place, not the stream: stdout cannot be assigned
 * where the C library declares it const, as musl does. What the stream holds
 * goes where it was going first. \a text has no newline, so that the stream
 * keeps it, line buffered, as on a terminal, or not. The fixture's flushes are
 * not main()'s, and are not recorded.
 */
static void
stdout_to(const int fd, const char *text)
{
        (void)__real_fflush(stdout);
        m_stdout_saved = dup(STDOUT_FILENO);
        assert_int_not_equal(m_stdout_saved, -1);
        assert_int_not_equal(dup2(fd, STDOUT_FILENO), -1);
        (void)fputs(text, stdout);
}

static void
stdout_to_the_pipe(const char *text)
{
        stdout_to(m_reader_gone, text);
}

/**
 * @brief Puts the process's stdout back: what the stream still holds is
 *        dropped by a flush to the pipe, which fails, as SIGPIPE is handled
 *        or ignored, and the stream's error indicator is cleared
 */
static void
stdout_back(void)
{
        if (m_stdout_saved == -1)
                return;
        (void)__real_fflush(stdout);
        (void)dup2(m_stdout_saved, STDOUT_FILENO);
        close(m_stdout_saved);
        m_stdout_saved = -1;
        clearerr(stdout);
}

/**
 * @brief Puts the process's stdout back, closes the pipe, and gives SIGPIPE
 *        its default action back
 */
static int
case_end(void **state)
{
        struct sigaction sa;

        UNUSED_ARG(state);
        stdout_back();
        if (m_reader_gone != -1)
                close(m_reader_gone);
        m_reader_gone = -1;
        memset(&sa, 0, sizeof(sa));
        sa.sa_handler = SIG_DFL;
        sigemptyset(&sa.sa_mask);
        (void)sigaction(SIGPIPE, &sa, NULL);

        return 0;
}

static void
test_a_monitoring_loop_that_fails_makes_pqos_exit_1(void **state)
{
        UNUSED_ARG(state);

        assert_int_equal(run_monitoring("all:0", -1, 0), EXIT_FAILURE);
        assert_int_equal(output_exit_was_called(), 0);
        /* and the monitoring is stopped, then the library finalised, all the
         * same, which is what the loop returns for
         */
        assert_string_equal(m_calls, "loop stop fini flush ");
}

static void
test_a_monitoring_loop_that_ends_well_makes_pqos_exit_0(void **state)
{
        UNUSED_ARG(state);

        assert_int_equal(run_monitoring("all:1", 0, 0), EXIT_SUCCESS);
        assert_int_equal(output_exit_was_called(), 0);
        assert_string_equal(m_calls, "loop stop fini flush ");
}

static void
test_an_output_whose_end_fails_makes_pqos_exit_1(void **state)
{
        UNUSED_ARG(state);

        /* the loop ended well, but the output did not take its end */
        assert_int_equal(run_monitoring("all:2", 0, -1), EXIT_FAILURE);
        assert_int_equal(output_exit_was_called(), 0);
        assert_string_equal(m_calls, "loop stop fini flush ");
}

/**
 * @brief The library is initialised with SIGPIPE ignored
 *
 * pqos_init() writes the library's log, on stdout by default. At its default
 * action, SIGPIPE ended the process at the first such write to a pipe whose
 * reader had gone, before its clean-up, with the lock file left behind.
 */
static void
test_the_library_is_initialised_with_sigpipe_ignored(void **state)
{
        UNUSED_ARG(state);

        assert_int_equal(run_version(), EXIT_SUCCESS);
        assert_true(m_sigpipe_ignored_at_init);
}

/**
 * @brief A write to stdout that failed, where no monitoring ran, is said
 *
 * What the tool prints is captured here, so stdout, with the pipe in place of
 * its descriptor, holds a line of the tool's for main()'s check to write; the
 * write to the pipe fails with EPIPE. The process's stdout is put back as
 * soon as main() returns.
 */
static void
test_a_failed_write_to_stdout_makes_pqos_exit_1(void **state)
{
        int ret;

        UNUSED_ARG(state);
        stdout_to_the_pipe("PQoS Library version: 26.6.0");
        ret = run_version();
        stdout_back();

        assert_int_equal(ret, EXIT_FAILURE);
        assert_int_equal(output_exit_was_called(), 0);
        assert_true(output_stderr_has_text(
            "Failed to write standard output: %s\n", strerror(EPIPE)));
        /* stdout flushed last, with the library finalised and the lock let
         * go: a reader that has stopped reading can hold the process there,
         * but nothing else
         */
        assert_string_equal(m_calls, "fini flush stdout ");
}

/**
 * @brief And so is one that failed before, whose line the stream dropped
 *
 * A stream drops what a write that failed was given, and keeps the failure in
 * its error indicator, which is then all there is to look at: the flush at
 * the end has nothing to write. What the write failed with is not known then.
 */
static void
test_an_earlier_failed_write_to_stdout_makes_pqos_exit_1(void **state)
{
        int ret;

        UNUSED_ARG(state);
        stdout_to_the_pipe("PQoS Library version: 26.6.0");
        assert_int_not_equal(__real_fflush(stdout), 0);
        ret = run_version();
        stdout_back();

        assert_int_equal(ret, EXIT_FAILURE);
        assert_true(
            output_stderr_has_text("Failed to write standard output\n"));
        assert_string_equal(m_calls, "fini flush stdout ");
}

/**
 * @brief And one that wrote is not
 */
static void
test_a_command_that_wrote_its_output_exits_0(void **state)
{
        UNUSED_ARG(state);

        assert_int_equal(run_version(), EXIT_SUCCESS);
        assert_false(output_has_text("Failed to write"));
        assert_string_equal(m_calls, "fini flush stdout ");
}

/**
 * @brief Where monitoring ran, stdout is left to monitor_flush()
 *
 * It checks the monitoring's output, which stdout is unless -o named a file,
 * and stdout where it is not, and says what failed: main() saying the failure
 * too would say it twice. monitor_flush() is wrapped here, and says nothing.
 */
static void
test_a_monitoring_run_leaves_stdout_to_its_flush(void **state)
{
        int ret;

        UNUSED_ARG(state);
        stdout_to_the_pipe("TIME 2026-10-30 16:00:00");
        ret = run_monitoring("all:3", 0, 0);
        stdout_back();

        assert_int_equal(ret, EXIT_SUCCESS);
        assert_false(output_has_text("Failed to write"));
        assert_string_equal(m_calls, "loop stop fini flush ");
}

/**
 * @brief A write of the library's log that failed makes pqos exit 1
 *
 * The library writes its log on stdout, here at its initialisation, to the
 * descriptor straight: stdout's error indicator does not see it, and a
 * monitoring run whose output goes to a file has nothing else on stdout to
 * fail. As "pqos -m llc:0 -o out.txt | head -n 1" where the reader goes before
 * the library's warning. The failure is said once the library is finalised.
 */
static void
test_a_failed_write_of_the_log_makes_pqos_exit_1(void **state)
{
        int ret;

        UNUSED_ARG(state);
        stdout_to_the_pipe("");
        m_init_log = "WARN: resctrl filesystem mounted!\n";
        ret = run_monitoring("all:0", 0, 0);
        m_init_log = NULL;
        stdout_back();

        assert_int_equal(ret, EXIT_FAILURE);
        assert_true(output_stderr_has_text(
            "Failed to write the log to standard output: %s\n",
            strerror(EPIPE)));
        assert_false(m_log_said_before_fini);
        assert_string_equal(m_calls, "loop stop fini flush ");
}

/**
 * @brief And one that wrote is not
 */
static void
test_a_log_that_was_written_exits_0(void **state)
{
        const int null = open("/dev/null", O_WRONLY);
        int ret;

        UNUSED_ARG(state);
        assert_int_not_equal(null, -1);
        stdout_to(null, "");
        close(null);
        m_init_log = "WARN: resctrl filesystem mounted!\n";
        ret = run_monitoring("all:1", 0, 0);
        m_init_log = NULL;
        stdout_back();

        assert_int_equal(ret, EXIT_SUCCESS);
        assert_false(output_has_text("Failed to write"));
        assert_false(output_has_text("printing to file failed"));
}

int
main(void)
{
        const struct CMUnitTest tests[] = {
            cmocka_unit_test_setup_teardown(
                test_the_library_is_initialised_with_sigpipe_ignored,
                case_start, case_end),
            cmocka_unit_test_setup_teardown(
                test_a_failed_write_to_stdout_makes_pqos_exit_1, case_start,
                case_end),
            cmocka_unit_test_setup_teardown(
                test_an_earlier_failed_write_to_stdout_makes_pqos_exit_1,
                case_start, case_end),
            cmocka_unit_test_setup_teardown(
                test_a_command_that_wrote_its_output_exits_0, case_start,
                case_end),
            cmocka_unit_test(
                test_a_monitoring_loop_that_fails_makes_pqos_exit_1),
            cmocka_unit_test(
                test_a_monitoring_loop_that_ends_well_makes_pqos_exit_0),
            cmocka_unit_test(test_an_output_whose_end_fails_makes_pqos_exit_1),
            cmocka_unit_test_setup_teardown(
                test_a_monitoring_run_leaves_stdout_to_its_flush, case_start,
                case_end),
            cmocka_unit_test_setup_teardown(
                test_a_failed_write_of_the_log_makes_pqos_exit_1, case_start,
                case_end),
            cmocka_unit_test_setup_teardown(test_a_log_that_was_written_exits_0,
                                            case_start, case_end)};
        struct pqos_cpuinfo *cpu =
            calloc(1, sizeof(*cpu) + 4 * sizeof(struct pqos_coreinfo));
        int ret;

        if (cpu == NULL)
                return EXIT_FAILURE;
        cpu->num_cores = 4;
        cpu->cores[0].lcore = 0;
        cpu->cores[1].lcore = 1;
        cpu->cores[2].lcore = 2;
        cpu->cores[3].lcore = 3;
        m_cap_mon.type = PQOS_CAP_TYPE_MON;
        m_cap_mon.u.mon = &m_mon;
        m_sys.cap = &m_cap;
        m_sys.cpu = cpu;

        ret = cmocka_run_group_tests(tests, NULL, NULL);

        free(cpu);
        return ret;
}
