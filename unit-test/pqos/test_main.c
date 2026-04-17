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
#include <limits.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
/* clang-format off */
#include <cmocka.h>
/* clang-format on */

void *realloc_and_init(void *ptr, unsigned *elem_count, const size_t elem_size);

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

int
main(void)
{
        const struct CMUnitTest tests[] = {
            cmocka_unit_test(
                test_realloc_and_init_grows_and_zeroes_new_elements),
            cmocka_unit_test(test_realloc_and_init_initializes_empty_array),
            cmocka_unit_test(
                test_realloc_and_init_rejects_element_count_overflow),
            cmocka_unit_test(test_realloc_and_init_rejects_byte_size_overflow)};

        return cmocka_run_group_tests(tests, NULL, NULL);
}
