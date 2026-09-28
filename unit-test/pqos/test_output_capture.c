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
 *
 */

/**
 * The output capture the whole utility suite asserts through.
 *
 * Every wrapper writes into one buffer through one cursor, so a write that is
 * not bounded does not only lose its own text: it leaves the cursor past the
 * end of the buffer, and then the next write of any wrapper is to a pointer
 * outside it with a remaining length that has gone negative - which the copy
 * takes as a size_t. These cases fill the buffer deliberately and then keep
 * writing through each wrapper in turn, which is the state that used to
 * corrupt memory.
 *
 * The buffer's size is output.c's own, so the cases do not name it: they write
 * until what output_get() returns stops growing, and that length is what the
 * capture holds.
 */

#include "output.h"

#include <cmocka.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/** one write, big enough to fill 512 KiB in a few hundred of them */
#define CHUNK 4096

/**
 * @brief Writes through printf until the capture stops growing
 *
 * @return the length the capture holds when it is full
 */
static size_t
fill_the_buffer(void)
{
        char chunk[CHUNK + 1];
        size_t filled = 0;
        unsigned i;

        memset(chunk, 'a', CHUNK);
        chunk[CHUNK] = '\0';

        for (i = 0; i < 1024; i++) {
                size_t now;

                __wrap_printf("%s", chunk);
                now = strlen(output_get());
                if (now == filled)
                        return filled;
                filled = now;
        }

        fail_msg("the capture grew for %u writes of %d characters", i, CHUNK);

        return filled;
}

static void
test_a_full_buffer_stops_growing(void **state)
{
        size_t filled;

        output_start();
        filled = fill_the_buffer();

        /* it filled, and what it holds is a string: the terminator is inside
         * the buffer and the cursor stopped at it
         */
        assert_true(filled > 0);
        assert_int_equal(strlen(output_get()), filled);

        output_stop();
        (void)state;
}

static void
test_every_wrapper_is_bounded_when_the_buffer_is_full(void **state)
{
        size_t filled;

        output_start();
        filled = fill_the_buffer();

        /* each of these, before the bound was shared, wrote through a pointer
         * the cursor had already taken past the end of the buffer
         */
        __wrap_printf("printf after the buffer is full\n");
        assert_int_equal(strlen(output_get()), filled);

        __wrap_fprintf(stderr, "fprintf after the buffer is full\n");
        assert_int_equal(strlen(output_get()), filled);

        __wrap_puts("puts after the buffer is full");
        assert_int_equal(strlen(output_get()), filled);

        __wrap_putchar('x');
        assert_int_equal(strlen(output_get()), filled);

        output_stop();
        (void)state;
}

static void
test_a_write_that_does_not_fit_is_kept_as_far_as_it_fits(void **state)
{
        size_t filled;

        output_start();
        filled = fill_the_buffer();

        /* the buffer is full to the character, so the last write before it was
         * full is the one that was cut - and what it kept is a string of
         * exactly the capacity, not one character more
         */
        assert_int_equal(strlen(output_get()), filled);
        assert_int_equal(output_get()[filled], '\0');
        assert_int_equal(output_get()[filled - 1], 'a');

        output_stop();
        (void)state;
}

static void
test_the_wrappers_capture_what_they_are_given(void **state)
{
        /* the ordinary case the suite depends on, in one grab: each wrapper's
         * text is in the buffer, in the order it was written
         */
        output_start();
        __wrap_printf("from printf %d\n", 1);
        __wrap_fprintf(stderr, "from fprintf %d\n", 2);
        __wrap_puts("from puts");
        __wrap_putchar('!');
        output_stop();

        assert_string_equal(output_get(),
                            "from printf 1\nfrom fprintf 2\nfrom puts\n!");
        assert_int_equal(output_has_text("from fprintf 2"), 1);
        (void)state;
}

static void
test_a_grab_starts_from_nothing(void **state)
{
        output_start();
        __wrap_printf("the first grab\n");
        output_stop();
        assert_int_equal(output_has_text("the first grab"), 1);

        output_start();
        output_stop();
        assert_string_equal(output_get(), "");
        assert_int_equal(output_has_text("the first grab"), 0);
        (void)state;
}

int
main(void)
{
        const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_the_wrappers_capture_what_they_are_given),
            cmocka_unit_test(test_a_grab_starts_from_nothing),
            cmocka_unit_test(test_a_full_buffer_stops_growing),
            cmocka_unit_test(
                test_every_wrapper_is_bounded_when_the_buffer_is_full),
            cmocka_unit_test(
                test_a_write_that_does_not_fit_is_kept_as_far_as_it_fits)};

        return cmocka_run_group_tests(tests, NULL, NULL);
}
