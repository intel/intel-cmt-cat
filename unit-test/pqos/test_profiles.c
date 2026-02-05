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
#include "caps_gen.h"
#include "mock_alloc.h"
#include "output.h"

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>
/* clang-format off */
#include <cmocka.h>
#include "profiles.c"
/* clang-format on */

static void
test_profile_l3ca_list(void **state)
{
        run_void_function(profile_l3ca_list);
        assert_true(output_has_text(
            "1)\n"
            "      Config ID: CFG0\n"
            "    Description: non-overlapping, ways equally divided\n"
            " Configurations:\n"
            "\tnumber of classes = 4, number of cache ways = 11\n"
            "\tnumber of classes = 4, number of cache ways = 12\n"
            "\tnumber of classes = 4, number of cache ways = 15\n"
            "\tnumber of classes = 4, number of cache ways = 16\n"
            "\tnumber of classes = 4, number of cache ways = 20\n"
            "2)\n"
            "      Config ID: CFG1\n"
            "    Description: non-overlapping, ways unequally divided\n"
            " Configurations:\n"
            "\tnumber of classes = 4, number of cache ways = 11\n"
            "\tnumber of classes = 4, number of cache ways = 12\n"
            "\tnumber of classes = 4, number of cache ways = 15\n"
            "\tnumber of classes = 4, number of cache ways = 16\n"
            "\tnumber of classes = 4, number of cache ways = 20\n"
            "3)\n"
            "      Config ID: CFG2\n"
            "    Description: overlapping, ways unequally divided, class 0 can "
            "access all ways\n"
            " Configurations:\n"
            "\tnumber of classes = 4, number of cache ways = 11\n"
            "\tnumber of classes = 4, number of cache ways = 12\n"
            "\tnumber of classes = 4, number of cache ways = 15\n"
            "\tnumber of classes = 4, number of cache ways = 16\n"
            "\tnumber of classes = 4, number of cache ways = 20\n"
            "4)\n"
            "      Config ID: CFG3\n"
            "    Description: ways unequally divided, overlapping access for "
            "higher classes\n"
            " Configurations:\n"
            "\tnumber of classes = 4, number of cache ways = 11\n"
            "\tnumber of classes = 4, number of cache ways = 12\n"
            "\tnumber of classes = 4, number of cache ways = 15\n"
            "\tnumber of classes = 4, number of cache ways = 16\n"
            "\tnumber of classes = 4, number of cache ways = 20"));
        (void)state; /* unused */
}

static void
test_profile_l3ca_apply_no_capability(void **state)
{
        int ret = 0;

        run_function(profile_l3ca_apply, ret, "CFG3", NULL);
        assert_int_equal(ret, -1);
        assert_true(output_has_text("Allocation profile 'CFG3' not found or "
                                    "cache allocation not supported!"));
        (void)state; /* unused */
}

static void
test_profile_l3ca_apply(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret = 0;

        // mock selfn_allocation_class
        expect_memory(__wrap_selfn_allocation_class, arg, "llc:0=0xFFF",
                      sizeof(char) * 11);
        expect_memory(__wrap_selfn_allocation_class, arg, "llc:1=0xFF0",
                      sizeof(char) * 11);
        expect_memory(__wrap_selfn_allocation_class, arg, "llc:2=0xF00",
                      sizeof(char) * 11);
        expect_memory(__wrap_selfn_allocation_class, arg, "llc:3=0xC00",
                      sizeof(char) * 11);

        run_function(profile_l3ca_apply, ret, "CFG3", data->cap_l3ca);
        assert_int_equal(ret, 0);
}

static void
test_profile_l3ca_apply_no_name(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret = 0;

        run_function(profile_l3ca_apply, ret, NULL, data->cap_l3ca);
        assert_int_equal(ret, -1);
        assert_true(output_has_text("Allocation profile '(null)' not found or "
                                    "cache allocation not supported!"));
}

static void
test_profile_l3ca_apply_bad_num_classes(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret = 0;

        data->cap_l3ca->u.l3ca->num_classes = 0;
        run_function(profile_l3ca_apply, ret, "CFG3", data->cap_l3ca);
        assert_int_equal(ret, -1);
        assert_true(output_has_text("Allocation profile 'CFG3' not found or "
                                    "cache allocation not supported!"));
}

static int
init_l3_cap(void **state)
{
        return init_caps(state, 1 << GENERATE_CAP_L3CA);
}

int
main(void)
{
        int result = 0;

        const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_profile_l3ca_list),
            cmocka_unit_test(test_profile_l3ca_apply_no_capability)};

        const struct CMUnitTest tests_need_l3_cap[] = {
            cmocka_unit_test(test_profile_l3ca_apply),
            cmocka_unit_test(test_profile_l3ca_apply_no_name),
            cmocka_unit_test(test_profile_l3ca_apply_bad_num_classes)};

        result += cmocka_run_group_tests(tests, NULL, NULL);
        result +=
            cmocka_run_group_tests(tests_need_l3_cap, init_l3_cap, fini_caps);

        return result;
}
