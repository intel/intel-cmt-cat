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
#include "mock_test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void *mock_calloc(size_t nmemb, size_t size);
FILE *mock_pqos_fopen(const char *name, const char *mode);
static int mock_pqos_fclose(FILE *stream);

#define calloc      mock_calloc
#define pqos_fopen  mock_pqos_fopen
#define pqos_fclose mock_pqos_fclose
/* clang-format off */
#include "monitor_utils.c"
/* clang-format on */
#undef calloc
#undef pqos_fopen
#undef pqos_fclose

struct pid_stat_fixture {
        pid_t pid;
        int available;
        char stat[512];
};

static struct pid_stat_fixture fixtures[8];
static size_t fixture_count;
static int calloc_should_fail;

static void
append_stat_field(char **ptr, size_t *remaining, const char *format, ...)
{
        va_list args;
        int ret;

        assert_non_null(ptr);
        assert_non_null(*ptr);
        assert_non_null(remaining);

        va_start(args, format);
        ret = vsnprintf(*ptr, *remaining, format, args);
        va_end(args);

        assert_true(ret >= 0);
        assert_true((size_t)ret < *remaining);

        *ptr += ret;
        *remaining -= (size_t)ret;
}

static void
reset_fixtures(void)
{
        memset(fixtures, 0, sizeof(fixtures));
        fixture_count = 0;
        calloc_should_fail = 0;
}

static void
set_pid_core(const pid_t pid, const unsigned core)
{
        unsigned i;
        struct pid_stat_fixture *fixture;
        char *ptr;
        size_t remaining;

        assert_true(fixture_count < DIM(fixtures));

        fixture = &fixtures[fixture_count];
        ptr = fixture->stat;
        remaining = sizeof(fixture->stat);

        fixture->pid = pid;
        fixture->available = 1;

        append_stat_field(&ptr, &remaining, "%d (test) S", pid);
        for (i = 4; i < PID_COL_CORE; i++)
                append_stat_field(&ptr, &remaining, " 0");
        append_stat_field(&ptr, &remaining, " %u 0\n", core);

        fixture_count++;
}

static void
set_pid_core_raw(const pid_t pid, const char *core_field)
{
        unsigned i;
        struct pid_stat_fixture *fixture;
        char *ptr;
        size_t remaining;

        assert_true(fixture_count < DIM(fixtures));

        fixture = &fixtures[fixture_count];
        ptr = fixture->stat;
        remaining = sizeof(fixture->stat);

        fixture->pid = pid;
        fixture->available = 1;

        append_stat_field(&ptr, &remaining, "%d (test) S", pid);
        for (i = 4; i < PID_COL_CORE; i++)
                append_stat_field(&ptr, &remaining, " 0");
        append_stat_field(&ptr, &remaining, " %s 0\n", core_field);

        fixture_count++;
}

static void
set_pid_missing(const pid_t pid)
{
        struct pid_stat_fixture *fixture;

        assert_true(fixture_count < DIM(fixtures));

        fixture = &fixtures[fixture_count];

        fixture->pid = pid;
        fixture->available = 0;
        fixture_count++;
}

static void
set_pid_stat(const pid_t pid, const char *comm)
{
        unsigned i;
        struct pid_stat_fixture *fixture;
        char *ptr;
        size_t remaining;

        assert_true(fixture_count < DIM(fixtures));
        assert_non_null(comm);

        fixture = &fixtures[fixture_count];
        ptr = fixture->stat;
        remaining = sizeof(fixture->stat);

        fixture->pid = pid;
        fixture->available = 1;

        append_stat_field(&ptr, &remaining, "%d (%s) S", pid, comm);
        for (i = 4; i <= 52; i++)
                append_stat_field(&ptr, &remaining, " %u", i);
        append_stat_field(&ptr, &remaining, "\n");

        fixture_count++;
}

static struct pid_stat_fixture *
find_fixture(const pid_t pid)
{
        size_t i;

        for (i = 0; i < fixture_count; i++)
                if (fixtures[i].pid == pid)
                        return &fixtures[i];

        return NULL;
}

void *
mock_calloc(const size_t nmemb, const size_t size)
{
        void *ptr;

        if (calloc_should_fail)
                return NULL;

        ptr = malloc(nmemb * size);
        if (ptr != NULL)
                memset(ptr, 0, nmemb * size);

        return ptr;
}

/* The stream the opener below hands out comes from fmemopen(), and fclose() is
 * what closes one. pqos_fclose() is the library's name for that call, and the
 * library is not linked into this test.
 */
static int
mock_pqos_fclose(FILE *stream)
{
        assert_non_null(stream);

        return fclose(stream);
}

FILE *
mock_pqos_fopen(const char *name, const char *mode)
{
        struct pid_stat_fixture *fixture;
        pid_t pid = -1;

        assert_non_null(name);
        assert_non_null(mode);
        assert_string_equal(mode, "r");
        int matched = sscanf(name, "/proc/%d/stat", &pid);

        assert_int_equal(matched, 1);

        fixture = find_fixture(pid);
        if (fixture == NULL || !fixture->available)
                return NULL;

        return fmemopen(fixture->stat, strlen(fixture->stat), mode);
}

int
monitor_get_interval(void)
{
        return 1;
}

enum monitor_llc_format
monitor_get_llc_format(void)
{
        return LLC_FORMAT_KILOBYTES;
}

int
pqos_mon_get_value(const struct pqos_mon_data *const group,
                   const enum pqos_mon_event event_id,
                   uint64_t *value,
                   uint64_t *delta)
{
        (void)group;
        (void)event_id;
        (void)value;
        (void)delta;

        return PQOS_RETVAL_OK;
}

int
pqos_mon_get_region_value(const struct pqos_mon_data *const group,
                          const enum pqos_mon_event event_id,
                          const int region_num,
                          uint64_t *value,
                          uint64_t *delta)
{
        (void)group;
        (void)event_id;
        (void)region_num;
        (void)value;
        (void)delta;

        return PQOS_RETVAL_OK;
}

int
pqos_mon_get_ipc(const struct pqos_mon_data *const group, double *value)
{
        (void)group;

        if (value != NULL)
                *value = 0.0;

        return PQOS_RETVAL_OK;
}

int
pqos_mon_get_tel_value(const struct pqos_mon_data *group,
                       const enum pqos_mon_event event,
                       double *value)
{
        (void)group;
        (void)event;

        if (value != NULL)
                *value = 0.0;

        return PQOS_RETVAL_OK;
}

int
pqos_cap_get(const struct pqos_cap **cap, const struct pqos_cpuinfo **cpu)
{
        (void)cap;
        (void)cpu;

        return PQOS_RETVAL_OK;
}

static void
test_monitor_utils_get_pid_stat_handles_complex_comm(void **state)
{
        char pid[4];
        char comm[32];
        char status[2];
        char user_ticks[3];
        char core[3];
        char too_small[1];

        reset_fixtures();
        set_pid_stat(801, "worker name) child");

        assert_int_equal(monitor_utils_get_pid_stat("801", 1, sizeof(pid), pid),
                         0);
        assert_string_equal(pid, "801");
        assert_int_equal(
            monitor_utils_get_pid_stat("801", 2, sizeof(comm), comm), 0);
        assert_string_equal(comm, "(worker name) child)");
        assert_int_equal(
            monitor_utils_get_pid_stat("801", 3, sizeof(status), status), 0);
        assert_string_equal(status, "S");
        assert_int_equal(monitor_utils_get_pid_stat(
                             "801", 14, sizeof(user_ticks), user_ticks),
                         0);
        assert_string_equal(user_ticks, "14");
        assert_int_equal(
            monitor_utils_get_pid_stat("801", 39, sizeof(core), core), 0);
        assert_string_equal(core, "39");
        assert_int_equal(
            monitor_utils_get_pid_stat("801", 3, sizeof(too_small), too_small),
            -1);
        assert_int_equal(
            monitor_utils_get_pid_stat("801", 53, sizeof(core), core), -1);
        (void)state;
}

static void
test_monitor_utils_get_pid_cores_all_resolved(void **state)
{
        pid_t tids[] = {101, 102, 103, 104};
        struct pqos_mon_data mon_data = {0};
        char cores[16];

        reset_fixtures();
        set_pid_core(101, 7);
        set_pid_core(102, 3);
        set_pid_core(103, 7);
        set_pid_core(104, 15);

        mon_data.tid_nr = DIM(tids);
        mon_data.tid_map = tids;

        assert_int_equal(
            monitor_utils_get_pid_cores(&mon_data, cores, sizeof(cores)), 0);
        assert_string_equal(cores, "3,7,15");
        (void)state;
}

static void
test_monitor_utils_get_pid_cores_partial_resolved(void **state)
{
        pid_t tids[] = {201, 202, 203, 204, 205};
        struct pqos_mon_data mon_data = {0};
        char cores[16];

        reset_fixtures();
        set_pid_missing(201);
        set_pid_core(202, 11);
        set_pid_core(203, 11);
        set_pid_missing(204);
        set_pid_core(205, 13);

        mon_data.tid_nr = DIM(tids);
        mon_data.tid_map = tids;

        assert_int_equal(
            monitor_utils_get_pid_cores(&mon_data, cores, sizeof(cores)), 0);
        assert_string_equal(cores, "11,13");
        (void)state;
}

static void
test_monitor_utils_get_pid_cores_all_missing(void **state)
{
        pid_t tids[] = {301, 302};
        struct pqos_mon_data mon_data = {0};
        char cores[16];

        reset_fixtures();
        set_pid_missing(301);
        set_pid_missing(302);

        mon_data.tid_nr = DIM(tids);
        mon_data.tid_map = tids;

        assert_int_equal(
            monitor_utils_get_pid_cores(&mon_data, cores, sizeof(cores)), 0);
        assert_string_equal(cores, "-");
        (void)state;
}

static void
test_monitor_utils_get_pid_cores_negative_core_skipped(void **state)
{
        pid_t tids[] = {501, 502};
        struct pqos_mon_data mon_data = {0};
        char cores[16];

        reset_fixtures();
        set_pid_core_raw(501, "-1");
        set_pid_core(502, 5);

        mon_data.tid_nr = DIM(tids);
        mon_data.tid_map = tids;

        assert_int_equal(
            monitor_utils_get_pid_cores(&mon_data, cores, sizeof(cores)), 0);
        assert_string_equal(cores, "5");
        assert_null(strstr(cores, "429496"));
        assert_null(strstr(cores, "4294967295"));
        (void)state;
}

static void
test_monitor_utils_get_pid_cores_all_negative(void **state)
{
        pid_t tids[] = {601, 602};
        struct pqos_mon_data mon_data = {0};
        char cores[16];

        reset_fixtures();
        set_pid_core_raw(601, "-1");
        set_pid_core_raw(602, "-1");

        mon_data.tid_nr = DIM(tids);
        mon_data.tid_map = tids;

        assert_int_equal(
            monitor_utils_get_pid_cores(&mon_data, cores, sizeof(cores)), 0);
        assert_string_equal(cores, "-");
        (void)state;
}

static void
test_monitor_utils_get_pid_cores_mixed_negative_and_missing(void **state)
{
        pid_t tids[] = {701, 702, 703};
        struct pqos_mon_data mon_data = {0};
        char cores[16];

        reset_fixtures();
        set_pid_missing(701);
        set_pid_core_raw(702, "-1");
        set_pid_core(703, 9);

        mon_data.tid_nr = DIM(tids);
        mon_data.tid_map = tids;

        assert_int_equal(
            monitor_utils_get_pid_cores(&mon_data, cores, sizeof(cores)), 0);
        assert_string_equal(cores, "9");
        (void)state;
}

static void
test_monitor_utils_get_pid_cores_null_map(void **state)
{
        struct pqos_mon_data mon_data = {0};
        char cores[16];

        reset_fixtures();
        mon_data.tid_nr = 1;
        mon_data.tid_map = NULL;

        assert_int_equal(
            monitor_utils_get_pid_cores(&mon_data, cores, sizeof(cores)), -1);
        (void)state;
}

static void
test_monitor_utils_get_pid_cores_calloc_failure(void **state)
{
        pid_t tids[] = {401};
        struct pqos_mon_data mon_data = {0};
        char cores[16];

        reset_fixtures();
        calloc_should_fail = 1;

        mon_data.tid_nr = DIM(tids);
        mon_data.tid_map = tids;

        assert_int_equal(
            monitor_utils_get_pid_cores(&mon_data, cores, sizeof(cores)), -1);
        (void)state;
}

int
main(void)
{
        const struct CMUnitTest tests[] = {
            cmocka_unit_test(
                test_monitor_utils_get_pid_stat_handles_complex_comm),
            cmocka_unit_test(test_monitor_utils_get_pid_cores_all_resolved),
            cmocka_unit_test(test_monitor_utils_get_pid_cores_partial_resolved),
            cmocka_unit_test(test_monitor_utils_get_pid_cores_all_missing),
            cmocka_unit_test(
                test_monitor_utils_get_pid_cores_negative_core_skipped),
            cmocka_unit_test(test_monitor_utils_get_pid_cores_all_negative),
            cmocka_unit_test(
                test_monitor_utils_get_pid_cores_mixed_negative_and_missing),
            cmocka_unit_test(test_monitor_utils_get_pid_cores_null_map),
            cmocka_unit_test(test_monitor_utils_get_pid_cores_calloc_failure),
        };

        return cmocka_run_group_tests(tests, NULL, NULL);
}
