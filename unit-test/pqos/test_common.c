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
#include "common.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
/* clang-format off */
#include <cmocka.h>
/* clang-format on */

/* safe_open() and safe_fopen() are gone: the tool calls the library's
 * pqos_open() and pqos_fopen(), and the cases that covered them live in
 * unit-test/lib/test_common.c beside the functions they test. What is left here
 * belongs to this file's own helpers.
 */

/* ======== pqos_filter_cpu / pqos_cpu_sort ======== */

/* The two are the scandir() callbacks for /sys/devices/system/cpu, so a
 * directory entry carrying the name is all they are given.
 */
static const struct dirent *
cpu_entry(struct dirent *entry, const char *name)
{
        memset(entry, 0, sizeof(*entry));
        snprintf(entry->d_name, sizeof(entry->d_name), "%s", name);

        return entry;
}

static int
filter_name(const char *name)
{
        struct dirent entry;

        return pqos_filter_cpu(cpu_entry(&entry, name));
}

static int
sort_names(const char *name1, const char *name2)
{
        struct dirent entry1;
        struct dirent entry2;
        const struct dirent *dir1 = cpu_entry(&entry1, name1);
        const struct dirent *dir2 = cpu_entry(&entry2, name2);

        return pqos_cpu_sort(&dir1, &dir2);
}

static void
test_filter_cpu_accepts_a_cpu_number(void **state)
{
        UNUSED_ARG(state);

        assert_int_equal(filter_name("cpu0"), 1);
        assert_int_equal(filter_name("cpu7"), 1);
        assert_int_equal(filter_name("cpu4294967295"), 1);
}

static void
test_filter_cpu_rejects_everything_else(void **state)
{
        UNUSED_ARG(state);

        /* the entries a "cpu[0-9]*" glob used to let through */
        assert_int_equal(filter_name("cpu0abc"), 0);
        assert_int_equal(filter_name("cpu1_2"), 0);

        /* the two scandir() calls the filter with first */
        assert_int_equal(filter_name("."), 0);
        assert_int_equal(filter_name(".."), 0);

        assert_int_equal(filter_name("cpu"), 0);
        assert_int_equal(filter_name("cpufreq"), 0);
        assert_int_equal(filter_name("cpuidle"), 0);
        assert_int_equal(filter_name("possible"), 0);
        assert_int_equal(filter_name("cpu-1"), 0);
        assert_int_equal(filter_name("cpu4294967296"), 0);
        assert_int_equal(filter_name("cpu99999999999999999999"), 0);

        /* the forms strtoul() would take for a number the kernel never writes
         * that way, each of which would be a second entry for a CPU that
         * already has one
         */
        assert_int_equal(filter_name("cpu+7"), 0);
        assert_int_equal(filter_name("cpu 7"), 0);
        assert_int_equal(filter_name("cpu7\n"), 0);
        assert_int_equal(filter_name("cpu0x10"), 0);
        assert_int_equal(filter_name("cpu0X10"), 0);
}

/* A leading zero is still a decimal number: base 0 would read cpu010 as octal
 * 8 and order it in front of cpu9.
 */
static void
test_cpu_names_are_read_as_decimal(void **state)
{
        UNUSED_ARG(state);

        assert_int_equal(filter_name("cpu010"), 1);
        assert_true(sort_names("cpu010", "cpu9") > 0);
        assert_true(sort_names("cpu9", "cpu010") < 0);
        assert_int_equal(sort_names("cpu010", "cpu10"), 0);
}

static void
test_cpu_sort_orders_by_number(void **state)
{
        UNUSED_ARG(state);

        /* by number rather than by name, so cpu2 comes before cpu10 */
        assert_true(sort_names("cpu2", "cpu10") < 0);
        assert_true(sort_names("cpu10", "cpu2") > 0);
        assert_int_equal(sort_names("cpu5", "cpu5"), 0);
}

/* The comparator returns int, so a difference of the two unsigned numbers is
 * only the answer while it stays inside int. Beyond that the sign is whatever
 * the conversion happens to produce.
 */
static void
test_cpu_sort_orders_numbers_beyond_int_range(void **state)
{
        UNUSED_ARG(state);

        assert_true(sort_names("cpu0", "cpu4000000000") < 0);
        assert_true(sort_names("cpu4000000000", "cpu0") > 0);
}

/* pqos_filter_cpu() keeps these away from the comparator, so this is about the
 * comparator being an ordering on its own: a name it cannot read a number out
 * of has one place, not the place a number nobody parsed would have given it.
 */
static void
test_cpu_sort_puts_unreadable_names_last(void **state)
{
        UNUSED_ARG(state);

        assert_true(sort_names("cpu1", "cpu0abc") < 0);
        assert_true(sort_names("cpu0abc", "cpu1") > 0);
        assert_true(sort_names("cpu0", "cpu0abc") < 0);

        /* two of them are ordered against each other by name */
        assert_true(sort_names("cpu0abc", "cpuxyz") < 0);
        assert_true(sort_names("cpuxyz", "cpu0abc") > 0);
        assert_int_equal(sort_names("cpu0abc", "cpu0abc"), 0);
}

int
main(void)
{
        const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_filter_cpu_accepts_a_cpu_number),
            cmocka_unit_test(test_filter_cpu_rejects_everything_else),
            cmocka_unit_test(test_cpu_names_are_read_as_decimal),
            cmocka_unit_test(test_cpu_sort_orders_by_number),
            cmocka_unit_test(test_cpu_sort_orders_numbers_beyond_int_range),
            cmocka_unit_test(test_cpu_sort_puts_unreadable_names_last),
        };

        return cmocka_run_group_tests(tests, NULL, NULL);
}
