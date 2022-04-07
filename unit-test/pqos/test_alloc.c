/*
 * BSD LICENSE
 *
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
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
#include "output.h"

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>
/* clang-format off */
#include <cmocka.h>
#include "alloc.c"
/* clang-format on */

static void
test_selfn_allocation_assoc_negative(void **state)
{
        run_void_function(selfn_allocation_assoc, NULL);
        assert_int_equal(output_exit_was_called(), 1);
        assert_int_equal(output_get_exit_status(), EXIT_FAILURE);
        assert_string_equal(
            output_get(),
            "Error parsing \"<null>\" command line argument. NULL pointer!\n");

        run_void_function(selfn_allocation_assoc, "");
        assert_int_equal(output_exit_was_called(), 1);
        assert_int_equal(output_get_exit_status(), EXIT_FAILURE);
        assert_string_equal(
            output_get(),
            "Error parsing \"\" command line argument. Empty string!\n");

        run_void_function(selfn_allocation_assoc, "badalloctype:1=0,3-5;");
        assert_int_equal(output_exit_was_called(), 1);
        assert_int_equal(output_get_exit_status(), EXIT_FAILURE);
        assert_string_equal(
            output_get(),
            "Error parsing \"badalloctype:1=0,3-5\" command line argument. "
            "Unrecognized allocation type\n");

        run_void_function(selfn_allocation_assoc, "core:0,3-5;");
        assert_int_equal(output_exit_was_called(), 1);
        assert_int_equal(output_get_exit_status(), EXIT_FAILURE);
        assert_string_equal(
            output_get(),
            "Error parsing \"0,3-5\" command line argument. Invalid allocation "
            "class of service association format\n");

        run_void_function(selfn_allocation_assoc, "pid:0,3-5;");
        assert_int_equal(output_exit_was_called(), 1);
        assert_int_equal(output_get_exit_status(), EXIT_FAILURE);
        assert_string_equal(
            output_get(),
            "Error parsing \"0,3-5\" command line argument. Invalid allocation "
            "class of service association format\n");

        (void)state; /* unused */
}

static void
test_selfn_allocation_assoc_llc(void **state)
{
        run_void_function(selfn_allocation_assoc, "llc:1=0,3-5;llc:2=1,2;");
        assert_int_equal(output_exit_was_called(), 0);
        assert_int_equal(sel_assoc_pid_num, 0);
        assert_int_equal(sel_assoc_core_num, 6);
        assert_int_equal(alloc_pid_flag, 0);
        assert_int_equal(sel_assoc_tab[0].core, 0);
        assert_int_equal(sel_assoc_tab[0].class_id, 1);
        assert_int_equal(sel_assoc_tab[1].core, 3);
        assert_int_equal(sel_assoc_tab[1].class_id, 1);
        assert_int_equal(sel_assoc_tab[2].core, 4);
        assert_int_equal(sel_assoc_tab[2].class_id, 1);
        assert_int_equal(sel_assoc_tab[3].core, 5);
        assert_int_equal(sel_assoc_tab[3].class_id, 1);
        assert_int_equal(sel_assoc_tab[4].core, 1);
        assert_int_equal(sel_assoc_tab[4].class_id, 2);
        assert_int_equal(sel_assoc_tab[5].core, 2);
        assert_int_equal(sel_assoc_tab[4].class_id, 2);
        (void)state; /* unused */
}

static int
cleanup_assoc_core_and_pid_tabs(void **state)
{
        int i;

        for (i = 0; i < sel_assoc_core_num; i++) {
                sel_assoc_tab[i].core = 0;
                sel_assoc_tab[i].class_id = 0;
        }
        sel_assoc_core_num = 0;
        for (i = 0; i < sel_assoc_pid_num; i++) {
                sel_assoc_pid_tab[i].task_id = 0;
                sel_assoc_pid_tab[i].class_id = 0;
        }
        sel_assoc_pid_num = 0;
        alloc_pid_flag = 0;
        (void)state; /* unused */
        return 0;
}

int
main(void)
{
        const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_selfn_allocation_assoc_negative),
            cmocka_unit_test_teardown(test_selfn_allocation_assoc_llc,
                                      cleanup_assoc_core_and_pid_tabs)};
        return cmocka_run_group_tests(tests, NULL, NULL);
}
