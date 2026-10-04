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
 * The command lines the utility refuses before it does anything: nothing is
 * reset, printed or applied and the library is not opened, so each case drives
 * the whole utility and reads the report it printed.
 *
 * These cases have a binary of their own because the option selections are held
 * in variables that a command line only ever adds to. Every case here therefore
 * runs with what the ones above it selected, from the emptiest command line to
 * the most complete one. The interface is named in the first case only, and the
 * rest inherit it: a second interface selection is refused as such, and naming
 * it at all is what lets resolve_interface() settle on one without asking the
 * platform, so a machine with no MMIO runs these too.
 */

#include "common.h"
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

int appmain(int argc, char **argv);

/**
 * @brief Run the utility with the given command line
 *
 * @param [out] ret where the status the utility returned is stored
 * @param [in] argc number of arguments, including the utility name
 * @param [in] ... the arguments after the utility name
 */
static void
run_pqos(int *ret, int argc, ...)
{
        char *argv[8] = {NULL};
        va_list args;
        int i;

        assert_true(argc > 0);
        assert_true((size_t)argc <= DIM(argv));

        /* strdup() because getopt_long() and the parsers write to argv */
        argv[0] = strdup("pqos");
        assert_non_null(argv[0]);

        va_start(args, argc);
        for (i = 1; i < argc; i++) {
                argv[i] = strdup(va_arg(args, const char *));
                assert_non_null(argv[i]);
        }
        va_end(args);

        optind = 1;
        opterr = 0;
        run_function(appmain, *ret, argc, argv);

        for (i = 0; i < argc; i++)
                free(argv[i]);
}

/* ======== a print mode together with an allocation ======== */

/* The control: a print mode on its own is what it always was, so the check
 * cannot be what rejects it. What the platform then makes of the command is
 * left alone, since a machine with no MMIO interface stops at the library and
 * one with it prints its configuration
 */
static void
test_print_mode_alone_is_accepted(void **state)
{
        int ret = EXIT_SUCCESS;

        UNUSED_ARG(state);

        run_pqos(&ret, 3, "--iface=mmio", "-s");

        assert_false(output_has_text("prints and exits before an allocation"));
}

/* A profile with a print mode: the profile becomes class definitions in
 * profile_l3ca_apply(), which is below every print mode, so it was carried to
 * a return that never read it
 */
static void
test_print_mode_with_a_profile_returns_failure(void **state)
{
        int ret = EXIT_SUCCESS;

        UNUSED_ARG(state);

        run_pqos(&ret, 4, "-s", "-c", "CFG0");

        assert_int_equal(ret, EXIT_FAILURE);
        assert_true(output_has_text("-s/--show prints and exits before an "
                                    "allocation is applied"));
        assert_true(output_has_text("-c/--profile-set given with it would be "
                                    "dropped"));

        /* the command line was refused before the library was opened, which is
         * what makes it free of a side effect
         */
        assert_false(output_has_text("Error initializing PQoS library"));
        assert_false(output_has_text("Allocation configuration altered"));
}

/* An association with a print mode, which is reported the same way. The print
 * mode is the -s of the case above, still selected, and the association is
 * named ahead of the profile because it is the one that would have been applied
 * first. A channel is what is associated, because a core association is an
 * MSR and OS option and resolve_interface() would refuse it here before this
 * check is reached
 */
static void
test_print_mode_with_an_assoc_returns_failure(void **state)
{
        int ret = EXIT_SUCCESS;

        UNUSED_ARG(state);

        run_pqos(&ret, 4, "-s", "-a", "channel:1=0x10000");

        assert_int_equal(ret, EXIT_FAILURE);
        assert_true(output_has_text("-a/--alloc-assoc given with it would be "
                                    "dropped"));
        assert_false(output_has_text("Error initializing PQoS library"));
}

/* A class definition with a print mode, the last of the three allocation
 * options, with the print mode named by the option earliest on the list rather
 * than by the one this command line added
 */
static void
test_print_mode_with_a_class_returns_failure(void **state)
{
        int ret = EXIT_SUCCESS;

        UNUSED_ARG(state);

        run_pqos(&ret, 4, "--print-topology", "-e", "llc:1=0xf");

        assert_int_equal(ret, EXIT_FAILURE);
        assert_true(output_has_text("-e/--alloc-class given with it would be "
                                    "dropped"));
        assert_false(output_has_text("Error initializing PQoS library"));
}

/* The list of allocation profiles is printed once the command line has been
 * read, rather than where the option is parsed, so that a command line which
 * also asks for an allocation is reported instead of the allocation being
 * dropped. Both orders are covered, because the option used to end the parse
 * where it was found: -e before -H was dropped, and -e after -H was never even
 * looked at
 */
static void
test_profile_list_after_a_class_returns_failure(void **state)
{
        int ret = EXIT_SUCCESS;

        UNUSED_ARG(state);

        run_pqos(&ret, 4, "-e", "llc:1=0xf", "-H");

        assert_int_equal(ret, EXIT_FAILURE);
        assert_true(output_has_text("-H/--profile-list prints and exits before "
                                    "an allocation is applied"));
        assert_true(output_has_text("-e/--alloc-class given with it would be "
                                    "dropped"));

        /* the profiles themselves are not printed, so the command line is
         * refused rather than half served
         */
        assert_false(output_has_text("Config ID"));
}

static void
test_profile_list_before_a_class_returns_failure(void **state)
{
        int ret = EXIT_SUCCESS;

        UNUSED_ARG(state);

        run_pqos(&ret, 4, "-H", "-e", "llc:1=0xf");

        assert_int_equal(ret, EXIT_FAILURE);
        assert_true(output_has_text("-H/--profile-list prints and exits before "
                                    "an allocation is applied"));
        assert_false(output_has_text("Config ID"));
}

/* ======== a value that begins with a dash ======== */

/* -R, -r and -p take a value, and a value that begins with a dash is read as
 * the next switch handed to them by mistake, so optind is rewound to it. That
 * is only right when the value was its own word. Attached to the option, the
 * value is part of the word just consumed, and the rewind used to make getopt
 * return the same option with the same value without end. The handlers end in
 * exit(), which the harness turns into a longjmp, so the status is read from
 * the output module rather than from the return value.
 */
static void
test_reset_alloc_attached_dash_returns_failure(void **state)
{
        int ret = EXIT_SUCCESS;

        UNUSED_ARG(state);

        run_pqos(&ret, 2, "-R-");

        assert_true(output_exit_was_called());
        assert_int_equal(output_get_exit_status(), EXIT_FAILURE);
        assert_true(
            output_has_text("Unrecognized '-' allocation reset option"));
}

static void
test_reset_alloc_long_option_dash_returns_failure(void **state)
{
        int ret = EXIT_SUCCESS;

        UNUSED_ARG(state);

        run_pqos(&ret, 2, "--alloc-reset=-");

        assert_true(output_exit_was_called());
        assert_int_equal(output_get_exit_status(), EXIT_FAILURE);
        assert_true(
            output_has_text("Unrecognized '-' allocation reset option"));
}

static void
test_reset_mon_attached_dash_returns_failure(void **state)
{
        int ret = EXIT_SUCCESS;

        UNUSED_ARG(state);

        run_pqos(&ret, 2, "-r-");

        assert_true(output_exit_was_called());
        assert_int_equal(output_get_exit_status(), EXIT_FAILURE);
        assert_true(output_has_text("Unrecognized '-' monitoring"));
}

/* Only the status here: which message rejects "-" depends on the interface an
 * earlier case left selected. Under mmio, narrow_iface() refuses -p itself, on
 * stderr, before the value is parsed; otherwise parse_monitor_pids() rejects
 * the value. Either way the utility exits with a failure instead of looping.
 */
static void
test_monitor_pids_attached_dash_returns_failure(void **state)
{
        int ret = EXIT_SUCCESS;

        UNUSED_ARG(state);

        run_pqos(&ret, 2, "-p-");

        assert_true(output_exit_was_called());
        assert_int_equal(output_get_exit_status(), EXIT_FAILURE);
}

/* The control: the next switch as its own word is still given back to getopt,
 * so -R followed by -s is a reset and a show, not a reset of "-s". What the
 * platform then makes of the command is left alone, as in the print-mode tests
 */
static void
test_reset_alloc_next_switch_is_not_its_value(void **state)
{
        int ret = EXIT_SUCCESS;

        UNUSED_ARG(state);

        run_pqos(&ret, 3, "-R", "-s");

        assert_false(output_has_text("Unrecognized"));
}

static void
test_reset_mon_next_switch_is_not_its_value(void **state)
{
        int ret = EXIT_SUCCESS;

        UNUSED_ARG(state);

        run_pqos(&ret, 3, "-r", "-s");

        assert_false(output_has_text("Unrecognized"));
}

int
main(void)
{
        const struct CMUnitTest tests_cmdline[] = {
            cmocka_unit_test(test_print_mode_alone_is_accepted),
            cmocka_unit_test(test_print_mode_with_a_profile_returns_failure),
            cmocka_unit_test(test_print_mode_with_an_assoc_returns_failure),
            cmocka_unit_test(test_print_mode_with_a_class_returns_failure),
            cmocka_unit_test(test_profile_list_after_a_class_returns_failure),
            cmocka_unit_test(test_profile_list_before_a_class_returns_failure),
            cmocka_unit_test(test_reset_alloc_attached_dash_returns_failure),
            cmocka_unit_test(test_reset_alloc_long_option_dash_returns_failure),
            cmocka_unit_test(test_reset_mon_attached_dash_returns_failure),
            cmocka_unit_test(test_monitor_pids_attached_dash_returns_failure),
            cmocka_unit_test(test_reset_alloc_next_switch_is_not_its_value),
            cmocka_unit_test(test_reset_mon_next_switch_is_not_its_value)};

        return cmocka_run_group_tests(tests_cmdline, NULL, NULL);
}
