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
 * Every print-and-exit mode of the utility, one case per entry of
 * print_and_exit_options.
 *
 * A command line only ever adds to the option selections, and the report names
 * the first mode of the list that is selected, so a case that drives the whole
 * utility can only ever cover the first mode it selects - see test_cmdline.c,
 * which does that for the command lines as a whole. Here main.c is included
 * rather than linked instead: the flags the parser sets and the check that
 * reads them are both private to it, so a case can select one mode, ask the
 * check and put the flag back, and the modes are covered one at a time. That is
 * what makes a mode dropped from the list fail a case of its own. main() is
 * renamed on the way in, since this binary has one of its own.
 *
 * The allocation side is wrapped. alloc_requested_option() answers for the
 * options of alloc.c, whose selections are private to that file, and every case
 * here is about the print side of the same question.
 */

#include "test_print_modes.h"

#define main appmain
#include "main.c"
#undef main

#include "output.h"

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

/** What the allocation side of the question answers, NULL for nothing asked */
static const char *test_allocating;

const char *
__wrap_alloc_requested_option(void)
{
        return test_allocating;
}

/**
 * @brief Assert that one print mode is refused when an allocation is asked for
 *
 * @param [in] selected the flag the parser sets for the mode
 * @param [in] name the mode as the report has to name it
 */
static void
assert_mode_refuses_an_allocation(int *selected, const char *name)
{
        int ret = 0;

        test_allocating = "-e/--alloc-class";
        *selected = 1;

        run_function(check_print_and_exit_options, ret);

        *selected = 0;
        test_allocating = NULL;

        assert_int_equal(ret, -1);
        assert_true(
            output_has_text("%s prints and exits before an allocation", name));
        assert_true(output_has_text("-e/--alloc-class given with it"));
}

/* ======== the question with one side missing ======== */

static void
test_an_allocation_alone_is_accepted(void **state)
{
        int ret = -1;

        UNUSED_ARG(state);

        test_allocating = "-e/--alloc-class";

        run_function(check_print_and_exit_options, ret);

        test_allocating = NULL;

        assert_int_equal(ret, 0);
        assert_false(output_has_text("prints and exits"));
}

static void
test_a_print_mode_alone_is_accepted(void **state)
{
        int ret = -1;

        UNUSED_ARG(state);

        sel_dump = 1;

        run_function(check_print_and_exit_options, ret);

        sel_dump = 0;

        assert_int_equal(ret, 0);
        assert_false(output_has_text("prints and exits"));
}

/* A profile is an allocation the utility has not turned into class definitions
 * yet, so it is asked about here rather than through alloc_requested_option()
 */
static void
test_a_profile_counts_as_an_allocation(void **state)
{
        int ret = 0;
        char profile[] = "CFG0";

        UNUSED_ARG(state);

        sel_allocation_profile = profile;
        sel_show_allocation_config = 1;

        run_function(check_print_and_exit_options, ret);

        sel_show_allocation_config = 0;
        sel_allocation_profile = NULL;

        assert_int_equal(ret, -1);
        assert_true(output_has_text("-c/--profile-set given with it"));
}

/* ======== one case per mode of the list ======== */

static void
test_profile_list_is_a_print_mode(void **state)
{
        UNUSED_ARG(state);
        assert_mode_refuses_an_allocation(&sel_profile_list,
                                          "-H/--profile-list");
}

static void
test_show_is_a_print_mode(void **state)
{
        UNUSED_ARG(state);
        assert_mode_refuses_an_allocation(&sel_show_allocation_config,
                                          "-s/--show");
}

static void
test_display_is_a_print_mode(void **state)
{
        UNUSED_ARG(state);
        assert_mode_refuses_an_allocation(&sel_display, "-d/--display");
}

static void
test_display_verbose_is_a_print_mode(void **state)
{
        UNUSED_ARG(state);
        assert_mode_refuses_an_allocation(&sel_display_verbose,
                                          "-D/--display-verbose");
}

static void
test_version_is_a_print_mode(void **state)
{
        UNUSED_ARG(state);
        assert_mode_refuses_an_allocation(&sel_print_version, "--version");
}

static void
test_print_mem_regions_is_a_print_mode(void **state)
{
        UNUSED_ARG(state);
        assert_mode_refuses_an_allocation(&sel_print_mem_regions,
                                          "--print-mem-regions");
}

static void
test_print_topology_is_a_print_mode(void **state)
{
        UNUSED_ARG(state);
        assert_mode_refuses_an_allocation(&sel_print_topology,
                                          "--print-topology");
}

static void
test_print_dump_info_is_a_print_mode(void **state)
{
        UNUSED_ARG(state);
        assert_mode_refuses_an_allocation(&sel_print_dump_info,
                                          "--print-dump-info");
}

static void
test_dump_is_a_print_mode(void **state)
{
        UNUSED_ARG(state);
        assert_mode_refuses_an_allocation(&sel_dump, "--dump");
}

static void
test_dump_rmid_regs_is_a_print_mode(void **state)
{
        UNUSED_ARG(state);
        assert_mode_refuses_an_allocation(&sel_dump_rmid_regs,
                                          "--dump-rmid-regs");
}

static void
test_print_io_devs_is_a_print_mode(void **state)
{
        UNUSED_ARG(state);
        assert_mode_refuses_an_allocation(&sel_print_io_devs,
                                          "--print-io-devs");
}

static void
test_print_io_dev_is_a_print_mode(void **state)
{
        UNUSED_ARG(state);
        assert_mode_refuses_an_allocation(&sel_print_io_dev, "--print-io-dev");
}

static void
test_enum_hybrid_cores_is_a_print_mode(void **state)
{
        UNUSED_ARG(state);
        assert_mode_refuses_an_allocation(&sel_enum_hybrid_cores,
                                          "--enum-hybrid-cores");
}

/* ======== the mode whose output moved ======== */

/* -H used to print the profiles where the option was parsed, which is what put
 * it out of reach of the check above. It prints them from the utility now, so
 * the one thing left to say is that it still prints them: this drives the whole
 * utility, and runs last because the selection it leaves behind is the first of
 * the list and would name itself in every case after it
 */
static void
test_profile_list_alone_prints_the_profiles(void **state)
{
        char *argv[2] = {NULL};
        int ret = EXIT_FAILURE;

        UNUSED_ARG(state);

        /* strdup() because getopt_long() and the parsers write to argv */
        argv[0] = strdup("pqos");
        argv[1] = strdup("-H");
        assert_non_null(argv[0]);
        assert_non_null(argv[1]);

        test_allocating = NULL;
        optind = 1;
        opterr = 0;
        run_function(appmain, ret, 2, argv);

        assert_int_equal(ret, EXIT_SUCCESS);
        assert_true(output_has_text("Config ID"));
        assert_false(output_has_text("prints and exits"));

        free(argv[0]);
        free(argv[1]);
}

int
main(void)
{
        const struct CMUnitTest tests_print_modes[] = {
            cmocka_unit_test(test_an_allocation_alone_is_accepted),
            cmocka_unit_test(test_a_print_mode_alone_is_accepted),
            cmocka_unit_test(test_a_profile_counts_as_an_allocation),
            cmocka_unit_test(test_profile_list_is_a_print_mode),
            cmocka_unit_test(test_show_is_a_print_mode),
            cmocka_unit_test(test_display_is_a_print_mode),
            cmocka_unit_test(test_display_verbose_is_a_print_mode),
            cmocka_unit_test(test_version_is_a_print_mode),
            cmocka_unit_test(test_print_mem_regions_is_a_print_mode),
            cmocka_unit_test(test_print_topology_is_a_print_mode),
            cmocka_unit_test(test_print_dump_info_is_a_print_mode),
            cmocka_unit_test(test_dump_is_a_print_mode),
            cmocka_unit_test(test_dump_rmid_regs_is_a_print_mode),
            cmocka_unit_test(test_print_io_devs_is_a_print_mode),
            cmocka_unit_test(test_print_io_dev_is_a_print_mode),
            cmocka_unit_test(test_enum_hybrid_cores_is_a_print_mode),
            /* last: the selection it leaves behind is the first of the list */
            cmocka_unit_test(test_profile_list_alone_prints_the_profiles)};

        return cmocka_run_group_tests(tests_print_modes, NULL, NULL);
}
