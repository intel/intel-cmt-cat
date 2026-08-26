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
            cmocka_unit_test(test_parse_uint64_formats_and_errors),
            cmocka_unit_test(test_parse_mem_regions_replaces_previous_values),
            cmocka_unit_test(test_parse_mem_regions_rejects_invalid_lists),
            cmocka_unit_test(
                test_parse_mem_regions_separates_id_range_from_capacity),
            cmocka_unit_test(test_parse_pci_id_accepts_valid_fields),
            cmocka_unit_test(test_parse_pci_id_rejects_invalid_fields),
            cmocka_unit_test(test_unknown_option_returns_failure),
            /* last: its command line stays in the selection globals */
            cmocka_unit_test(test_alloc_option_without_class_returns_failure)};

        return cmocka_run_group_tests(tests, NULL, NULL);
}
