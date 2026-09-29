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
#include "common.h"
#include "main.h"
#include "output.h"
#include "utils.h"

#include <getopt.h>
#include <limits.h>
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

static void
test_realloc_and_init_grows_and_zeroes_new_elements(void **state)
{
        unsigned count = 2;
        uint32_t *tab = calloc(count, sizeof(*tab));

        assert_non_null(tab);

        tab[0] = 0xAAAAAAAAU;
        tab[1] = 0x55555555U;

        tab = realloc_and_init(tab, &count, sizeof(*tab));

        assert_non_null(tab);
        assert_int_equal(count, 4);
        assert_int_equal(tab[0], 0xAAAAAAAAU);
        assert_int_equal(tab[1], 0x55555555U);
        assert_int_equal(tab[2], 0);
        assert_int_equal(tab[3], 0);

        free(tab);
        (void)state; /* unused */
}

static void
test_realloc_and_init_initializes_empty_array(void **state)
{
        unsigned count = 0;
        uint32_t *tab = NULL;

        tab = realloc_and_init(tab, &count, sizeof(*tab));

        assert_non_null(tab);
        assert_int_equal(count, 1);
        assert_int_equal(tab[0], 0);

        free(tab);
        (void)state; /* unused */
}

static void
test_realloc_and_init_rejects_element_count_overflow(void **state)
{
        unsigned count = (UINT_MAX / 2U) + 1U;
        unsigned prev_count = count;
        uint8_t data = 0xA5;

        assert_null(realloc_and_init(&data, &count, sizeof(data)));
        assert_int_equal(count, prev_count);

        (void)state; /* unused */
}

static void
test_realloc_and_init_rejects_byte_size_overflow(void **state)
{
        unsigned count = 1;
        unsigned prev_count = count;
        uint8_t data = 0x5A;

        assert_null(realloc_and_init(&data, &count, SIZE_MAX));
        assert_int_equal(count, prev_count);

        (void)state; /* unused */
}

static void
test_strlisttotabrealloc_grows_array(void **state)
{
        unsigned count = 2;
        char input[] = "1,2,3,4,5";
        uint64_t *tab = calloc(count, sizeof(*tab));
        unsigned parsed;

        assert_non_null(tab);

        parsed = strlisttotabrealloc(input, &tab, &count);
        assert_int_equal(parsed, 5);
        assert_true(count >= 5);
        assert_int_equal(tab[0], 1);
        assert_int_equal(tab[1], 2);
        assert_int_equal(tab[2], 3);
        assert_int_equal(tab[3], 4);
        assert_int_equal(tab[4], 5);

        free(tab);
        (void)state; /* unused */
}

static void
test_strlisttotabrealloc_ignores_duplicates(void **state)
{
        unsigned count = 2;
        char input[] = "1,1,2,2,3";
        uint64_t *tab = calloc(count, sizeof(*tab));
        unsigned parsed;

        assert_non_null(tab);

        parsed = strlisttotabrealloc(input, &tab, &count);
        assert_int_equal(parsed, 3);
        assert_int_equal(tab[0], 1);
        assert_int_equal(tab[1], 2);
        assert_int_equal(tab[2], 3);

        free(tab);
        (void)state; /* unused */
}

static void
test_strlisttotabrealloc_large_range(void **state)
{
        unsigned count = 4;
        char input[] = "1-65535";
        uint64_t *tab = calloc(count, sizeof(*tab));
        unsigned parsed;

        assert_non_null(tab);

        parsed = strlisttotabrealloc(input, &tab, &count);
        assert_int_equal(parsed, 65535);
        assert_true(count >= 65535);
        assert_int_equal(tab[0], 1);
        assert_int_equal(tab[65534], 65535);

        free(tab);
        (void)state; /* unused */
}

static void
test_strlisttotabrealloc_range_fills_the_table(void **state)
{
        unsigned count = 128;
        char input[] = "0-127,200";
        uint64_t *tab = calloc(count, sizeof(*tab));
        unsigned parsed;

        assert_non_null(tab);

        /* A first range that fills the table exactly, and a token after it. The
         * first range is stored by a path of its own - no duplicates to check
         * against - which used to grow the table to the range's length and go
         * straight back to the top of the loop, so the token after it wrote one
         * element past the end. AddressSanitizer is what sees that; what this
         * asserts is that both are stored and counted.
         */
        parsed = strlisttotabrealloc(input, &tab, &count);
        assert_int_equal(parsed, 129);
        assert_true(count > 129);
        assert_int_equal(tab[0], 0);
        assert_int_equal(tab[127], 127);
        assert_int_equal(tab[128], 200);

        free(tab);
        (void)state; /* unused */
}

static void
test_strlisttotabrealloc_range_after_a_grown_range(void **state)
{
        unsigned count = 4;
        char input[] = "0-7,8-15,100";
        uint64_t *tab = calloc(count, sizeof(*tab));
        unsigned parsed;

        assert_non_null(tab);

        /* the same thing where the first range had to grow the table to reach
         * its own length: a second range then writes past the end, at the other
         * of the two places that trusted there to be an element free
         */
        parsed = strlisttotabrealloc(input, &tab, &count);
        assert_int_equal(parsed, 17);
        assert_true(count > 17);
        assert_int_equal(tab[0], 0);
        assert_int_equal(tab[15], 15);
        assert_int_equal(tab[16], 100);

        free(tab);
        (void)state; /* unused */
}

static void
test_strlisttotabrealloc_range_ending_at_the_top(void **state)
{
        unsigned count = 4;
        char input[] = "18446744073709551615-18446744073709551615";
        uint64_t *tab = calloc(count, sizeof(*tab));
        unsigned parsed;

        assert_non_null(tab);

        /* An inclusive range of one, at the top of the width. "n <= end" is
         * true again once n has wrapped past UINT64_MAX, so this used to fill
         * the table with wrapped values and report a capacity error - or, in
         * the reallocating parser, grow it until the allocation failed -
         * instead of storing the one value asked for.
         */
        parsed = strlisttotabrealloc(input, &tab, &count);
        assert_int_equal(parsed, 1);
        assert_true(tab[0] == UINT64_MAX);

        free(tab);
        (void)state; /* unused */
}

static void
test_strlisttotab_range_ending_at_the_top(void **state)
{
        uint64_t tab[4] = {0};
        char input[] = "18446744073709551614-18446744073709551615";
        unsigned parsed;

        /* the same endpoint in the parser that does not reallocate, and a range
         * of two so the loop has to advance once before it stops
         */
        parsed = strlisttotab(input, tab, 4);
        assert_int_equal(parsed, 2);
        assert_true(tab[0] == UINT64_MAX - 1);
        assert_true(tab[1] == UINT64_MAX);

        (void)state; /* unused */
}

static void
test_strlisttotab_range_of_duplicates_fits_a_full_table(void **state)
{
        unsigned parsed;
        uint64_t tab[8] = {0};
        char full[] = "0-7";
        char single[] = "0-7,0";
        char range[] = "0-7,0-5";

        /* A duplicate is not stored, so it does not need room. The capacity
         * check used to come first in the range branch and second in the
         * single-number branch, which refused a range naming values the table
         * already held while accepting the same value written on its own.
         */
        parsed = strlisttotab(full, tab, 8);
        assert_int_equal(parsed, 8);

        memset(tab, 0, sizeof(tab));
        parsed = strlisttotab(single, tab, 8);
        assert_int_equal(parsed, 8);

        memset(tab, 0, sizeof(tab));
        parsed = strlisttotab(range, tab, 8);
        assert_int_equal(parsed, 8);
        assert_int_equal(tab[0], 0);
        assert_int_equal(tab[7], 7);

        (void)state; /* unused */
}

static void
test_parse_uint64_formats_and_errors(void **state)
{
        uint64_t value = 0;

        (void)state;

        assert_int_equal(pqos_parse_uint64("08", &value), 0);
        assert_int_equal(value, 8);
        assert_int_equal(pqos_parse_uint64("010", &value), 0);
        assert_int_equal(value, 10);
        assert_int_equal(pqos_parse_uint64("0x10", &value), 0);
        assert_int_equal(value, 16);
        assert_int_equal(pqos_parse_uint64(" 10 ", &value), 0);
        assert_int_equal(value, 10);

        assert_int_equal(pqos_parse_uint64("", &value), -1);
        assert_int_equal(pqos_parse_uint64("-1", &value), -1);
        assert_int_equal(pqos_parse_uint64(" -1", &value), -1);
        assert_int_equal(pqos_parse_uint64("+1", &value), -1);
        assert_int_equal(pqos_parse_uint64(" +1", &value), -1);
        assert_int_equal(pqos_parse_uint64(" ", &value), -1);
        assert_int_equal(pqos_parse_uint64("0x", &value), -1);
        assert_int_equal(pqos_parse_uint64("08x", &value), -1);
        assert_int_equal(pqos_parse_uint64("18446744073709551616", &value), -1);
        assert_int_equal(pqos_parse_uint64(NULL, &value), -1);
        assert_int_equal(pqos_parse_uint64("1", NULL), -1);
}

static void
test_parse_mem_regions_replaces_previous_values(void **state)
{
        int regions[PQOS_MAX_MEM_REGIONS];
        int count;

        count = pqos_parse_mem_regions("0,2", regions, DIM(regions));
        assert_int_equal(count, 2);
        assert_int_equal(regions[0], 0);
        assert_int_equal(regions[1], 2);

        count = pqos_parse_mem_regions("1", regions, DIM(regions));
        assert_int_equal(count, 1);
        assert_int_equal(regions[0], 1);
        assert_int_equal(regions[1], -1);

        (void)state;
}

static void
test_parse_mem_regions_rejects_invalid_lists(void **state)
{
        int regions[PQOS_MAX_MEM_REGIONS];
        int short_regions[2];

        assert_int_equal(pqos_parse_mem_regions("0,,1", regions, DIM(regions)),
                         -1);
        assert_int_equal(pqos_parse_mem_regions(",0", regions, DIM(regions)),
                         -1);
        assert_int_equal(pqos_parse_mem_regions("0,", regions, DIM(regions)),
                         -1);
        assert_int_equal(pqos_parse_mem_regions("1,1", regions, DIM(regions)),
                         -1);
        assert_int_equal(
            pqos_parse_mem_regions("1,invalid", regions, DIM(regions)), -1);
        assert_int_equal(pqos_parse_mem_regions("4", regions, DIM(regions)),
                         -1);
        assert_int_equal(
            pqos_parse_mem_regions("0,1,2", short_regions, DIM(short_regions)),
            -1);

        (void)state;
}

static void
test_parse_mem_regions_separates_id_range_from_capacity(void **state)
{
        int region[1];

        assert_int_equal(pqos_parse_mem_regions("3", region, DIM(region)), 1);
        assert_int_equal(region[0], 3);

        (void)state;
}

static void
test_parse_pci_id_accepts_valid_fields(void **state)
{
        char full_id[] = "abcd:fe:1f.7@3";
        char short_id[] = "02:03.1";
        char *vc = NULL;
        uint16_t segment;
        uint16_t bdf;

        assert_int_equal(pqos_parse_pci_id(full_id, 1, &segment, &bdf, &vc), 0);
        assert_int_equal(segment, 0xabcd);
        assert_int_equal(bdf, 0xfeff);
        assert_string_equal(vc, "3");

        assert_int_equal(pqos_parse_pci_id(short_id, 0, &segment, &bdf, &vc),
                         0);
        assert_int_equal(segment, 0);
        assert_int_equal(bdf, 0x0219);
        assert_null(vc);

        (void)state;
}

static void
test_parse_pci_id_rejects_invalid_fields(void **state)
{
        char invalid[][32] = {"10000:00:00.0", "00:100:00.0", "00:20.0",
                              "00:00.8",       "00:00",       "00:00.0@",
                              "00:00.0@1@2"};
        char vc_not_allowed[] = "00:00.0@1";
        char *vc = NULL;
        uint16_t segment;
        uint16_t bdf;
        unsigned i;

        for (i = 0; i < DIM(invalid); i++)
                assert_int_equal(
                    pqos_parse_pci_id(invalid[i], 1, &segment, &bdf, &vc), -1);
        assert_int_equal(
            pqos_parse_pci_id(vc_not_allowed, 0, &segment, &bdf, &vc), -1);

        (void)state;
}

static void
test_unknown_option_returns_failure(void **state)
{
        char **argv = calloc(3, sizeof(*argv));

        assert_non_null(argv);
        argv[0] = strdup("pqos");
        argv[1] = strdup("--not-a-pqos-option");
        assert_non_null(argv[0]);
        assert_non_null(argv[1]);

        optind = 1;
        opterr = 0;
        assert_int_equal(appmain(2, argv), EXIT_FAILURE);

        free(argv[0]);
        free(argv[1]);
        free(argv);
        (void)state;
}

/**
 * Where the missing class is caught is the guarantee, so this drives the
 * whole utility rather than the check on its own: the interface is named
 * explicitly, which is what lets resolve_interface() take the override path
 * and settle on MMIO without asking the platform, so a machine with no MMIO
 * runs this too. That the library was never opened is asserted through the
 * initialization error it would otherwise report here, since the check would
 * still reject this command line from further down - only by then -R has
 * reset the configuration and the print-and-exit options have printed.
 */
static void
test_alloc_option_without_class_returns_failure(void **state)
{
        char *argv[4] = {NULL};
        int ret = 0;

        /* strdup() because getopt_long() and the parsers write to argv */
        argv[0] = strdup("pqos");
        argv[1] = strdup("--iface=mmio");
        argv[2] = strdup("--alloc-domain-id=0");
        assert_non_null(argv[0]);
        assert_non_null(argv[1]);
        assert_non_null(argv[2]);

        optind = 1;
        opterr = 0;
        run_function(appmain, ret, 3, argv);

        assert_int_equal(ret, EXIT_FAILURE);
        assert_true(output_has_text(
            "-e/--alloc-class option is missing in command line"));
        assert_true(output_has_text("--alloc-domain-id"));
        assert_false(output_has_text("Error initializing PQoS library"));

        free(argv[0]);
        free(argv[1]);
        free(argv[2]);
        (void)state;
}

static void
test_parse_mem_regions_accepts_ranges(void **state)
{
        int regions[PQOS_MAX_MEM_REGIONS];

        /* a range covers every region it spans, in order */
        assert_int_equal(pqos_parse_mem_regions("0-2", regions, DIM(regions)),
                         3);
        assert_int_equal(regions[0], 0);
        assert_int_equal(regions[1], 1);
        assert_int_equal(regions[2], 2);

        /* a range of one region is the region */
        assert_int_equal(pqos_parse_mem_regions("1-1", regions, DIM(regions)),
                         1);
        assert_int_equal(regions[0], 1);

        /* ranges and single values mix in one list */
        assert_int_equal(pqos_parse_mem_regions("0,2-3", regions, DIM(regions)),
                         3);
        assert_int_equal(regions[0], 0);
        assert_int_equal(regions[1], 2);
        assert_int_equal(regions[2], 3);

        (void)state;
}

static void
test_parse_mem_regions_rejects_invalid_ranges(void **state)
{
        int regions[PQOS_MAX_MEM_REGIONS];
        int short_regions[2];

        /* a range that ends before it starts */
        assert_int_equal(pqos_parse_mem_regions("2-1", regions, DIM(regions)),
                         -1);
        /* a range that leaves the region space */
        assert_int_equal(pqos_parse_mem_regions("0-4", regions, DIM(regions)),
                         -1);
        /* an overlap between a range and a value repeats a region */
        assert_int_equal(pqos_parse_mem_regions("0-1,1", regions, DIM(regions)),
                         -1);
        /* neither end may be missing or unparsable */
        assert_int_equal(pqos_parse_mem_regions("0-", regions, DIM(regions)),
                         -1);
        assert_int_equal(pqos_parse_mem_regions("-1", regions, DIM(regions)),
                         -1);
        assert_int_equal(
            pqos_parse_mem_regions("0-invalid", regions, DIM(regions)), -1);
        /* a range that does not fit the caller's array */
        assert_int_equal(
            pqos_parse_mem_regions("0-2", short_regions, DIM(short_regions)),
            -1);

        (void)state;
}

int
main(void)
{
        const struct CMUnitTest tests[] = {
            cmocka_unit_test(
                test_realloc_and_init_grows_and_zeroes_new_elements),
            cmocka_unit_test(test_realloc_and_init_initializes_empty_array),
            cmocka_unit_test(
                test_realloc_and_init_rejects_element_count_overflow),
            cmocka_unit_test(test_realloc_and_init_rejects_byte_size_overflow),
            cmocka_unit_test(test_strlisttotabrealloc_grows_array),
            cmocka_unit_test(test_strlisttotabrealloc_ignores_duplicates),
            cmocka_unit_test(test_strlisttotabrealloc_large_range),
            cmocka_unit_test(test_strlisttotabrealloc_range_fills_the_table),
            cmocka_unit_test(
                test_strlisttotabrealloc_range_after_a_grown_range),
            cmocka_unit_test(
                test_strlisttotab_range_of_duplicates_fits_a_full_table),
            cmocka_unit_test(test_strlisttotabrealloc_range_ending_at_the_top),
            cmocka_unit_test(test_strlisttotab_range_ending_at_the_top),
            cmocka_unit_test(test_parse_uint64_formats_and_errors),
            cmocka_unit_test(test_parse_mem_regions_replaces_previous_values),
            cmocka_unit_test(test_parse_mem_regions_rejects_invalid_lists),
            cmocka_unit_test(test_parse_mem_regions_accepts_ranges),
            cmocka_unit_test(test_parse_mem_regions_rejects_invalid_ranges),
            cmocka_unit_test(
                test_parse_mem_regions_separates_id_range_from_capacity),
            cmocka_unit_test(test_parse_pci_id_accepts_valid_fields),
            cmocka_unit_test(test_parse_pci_id_rejects_invalid_fields),
            cmocka_unit_test(test_unknown_option_returns_failure),
            /* last: its command line stays in the selection globals */
            cmocka_unit_test(test_alloc_option_without_class_returns_failure)};

        return cmocka_run_group_tests(tests, NULL, NULL);
}
