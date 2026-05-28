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
#include "output.h"

#include <limits.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
/* clang-format off */
#include <cmocka.h>
/* clang-format on */

void *realloc_and_init(void *ptr, unsigned *elem_count, const size_t elem_size);
unsigned strlisttotab(char *s, uint64_t *tab, const unsigned max);

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
test_strlisttotab_single_values(void **state)
{
        char s[] = "1,2,3,5,7";
        uint64_t tab[10] = {0};
        unsigned n;

        n = strlisttotab(s, tab, 10);
        assert_int_equal(n, 5);
        assert_int_equal(tab[0], 1);
        assert_int_equal(tab[1], 2);
        assert_int_equal(tab[2], 3);
        assert_int_equal(tab[3], 5);
        assert_int_equal(tab[4], 7);

        (void)state; /* unused */
}

static void
test_strlisttotab_range_values(void **state)
{
        char s[] = "1-3,5-7";
        uint64_t tab[10] = {0};
        unsigned n;

        n = strlisttotab(s, tab, 10);
        assert_int_equal(n, 6);
        assert_int_equal(tab[0], 1);
        assert_int_equal(tab[1], 2);
        assert_int_equal(tab[2], 3);
        assert_int_equal(tab[3], 5);
        assert_int_equal(tab[4], 6);
        assert_int_equal(tab[5], 7);

        (void)state; /* unused */
}

static void
test_strlisttotab_too_many_single_values_reports_original_arg(void **state)
{
        char s[] = "1,2,3,4,5,6,7,8,9,10";
        uint64_t tab[3];

        run_void_function(strlisttotab, s, tab, 3);
        assert_int_equal(output_exit_was_called(), 1);
        assert_int_equal(output_get_exit_status(), EXIT_FAILURE);
        assert_int_equal(output_has_text("Too many groups selected"), 1);
        /* Must report original argument, never "<null>" */
        assert_int_equal(output_has_text("<null>"), 0);
        assert_int_equal(output_has_text("1,2,3,4,5,6,7,8,9,10"), 1);

        (void)state; /* unused */
}

static void
test_strlisttotab_long_arg_reports_original_arg(void **state)
{
        /* Build a comma-separated list longer than 256 chars (old BUF_SIZE) */
        char s[1024] = {0};
        uint64_t tab[3];
        size_t pos = 0;
        int i;

        for (i = 1; i <= 100; i++) {
                if (i > 1)
                        s[pos++] = ',';
                pos += (size_t)snprintf(s + pos, sizeof(s) - pos, "%d", i);
        }

        run_void_function(strlisttotab, s, tab, 3);
        assert_int_equal(output_exit_was_called(), 1);
        assert_int_equal(output_get_exit_status(), EXIT_FAILURE);
        assert_int_equal(output_has_text("Too many groups selected"), 1);
        /* Must not report "<null>" even for strings longer than the old 256-byte buffer */
        assert_int_equal(output_has_text("<null>"), 0);
        assert_int_equal(output_has_text("1,2,3"), 1);

        (void)state; /* unused */
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
            cmocka_unit_test(test_strlisttotab_single_values),
            cmocka_unit_test(test_strlisttotab_range_values),
            cmocka_unit_test(
                test_strlisttotab_too_many_single_values_reports_original_arg),
            cmocka_unit_test(
                test_strlisttotab_long_arg_reports_original_arg)};

        return cmocka_run_group_tests(tests, NULL, NULL);
}
