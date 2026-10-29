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

#include "mock_resctrl_monitoring.h"
#include "resctrl_monitoring.h"
#include "test.h"

#include <errno.h>
#include <sys/stat.h>
#include <time.h>

/* ======== mock ======== */

/* a negative value queued is the errno the call fails with */
int
__wrap_mkdir(const char *path, mode_t mode)
{
        int ret;

        check_expected(path);
        check_expected(mode);

        ret = mock_type(int);
        if (ret >= 0)
                return ret;
        errno = -ret;
        return -1;
}

/* the waits between the tries of a mkdir, which take no time here */
static unsigned nanosleep_calls;

int
__wrap_nanosleep(const struct timespec *req, struct timespec *rem)
{
        (void)rem;
        assert_non_null(req);
        assert_int_equal(req->tv_sec, 0);
        assert_int_equal(req->tv_nsec, 100000000L);
        nanosleep_calls++;

        return 0;
}

/* a case that has rmdir() fail, with -1, queues the errno it fails with */
int
__wrap_rmdir(const char *path)
{
        const int ret = mock_type(int);

        check_expected(path);
        if (ret == -1)
                errno = mock_type(int);

        return ret;
}

/* ======== resctrl_mon_mkdir ======== */
static void
test_resctrl_mon_mkdir(void **state __attribute__((unused)))
{
        int ret;

        expect_string(__wrap_mkdir, path, RESCTRL_PATH "/mon_groups/test");
        expect_value(__wrap_mkdir, mode, 0755);
        will_return(__wrap_mkdir, 0);

        ret = resctrl_mon_mkdir(0, "test");
        assert_int_equal(ret, PQOS_RETVAL_OK);

        expect_string(__wrap_mkdir, path, RESCTRL_PATH "/COS1/mon_groups/test");
        expect_value(__wrap_mkdir, mode, 0755);
        will_return(__wrap_mkdir, 1);

        ret = resctrl_mon_mkdir(1, "test");
        assert_int_equal(ret, PQOS_RETVAL_OK);

        expect_string(__wrap_mkdir, path, RESCTRL_PATH "/COS2/mon_groups/test");
        expect_value(__wrap_mkdir, mode, 0755);
        will_return(__wrap_mkdir, -EEXIST);

        ret = resctrl_mon_mkdir(2, "test");
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

/* has the mkdir of group "test" in class 1 fail with each error of errors,
 * and then, if made, succeed; returns what resctrl_mon_mkdir() returns, and
 * says how many messages it logged and how many waits it made
 */
static int
mkdir_after(const int *errors,
            const unsigned num_errors,
            const int made,
            unsigned *logged,
            unsigned *waits)
{
        unsigned i;
        int ret;

        for (i = 0; i < num_errors + (made ? 1 : 0); i++) {
                expect_string(__wrap_mkdir, path,
                              RESCTRL_PATH "/COS1/mon_groups/test");
                expect_value(__wrap_mkdir, mode, 0755);
                will_return(__wrap_mkdir, i < num_errors ? -errors[i] : 0);
        }

        nanosleep_calls = 0;
        test_log_count_start();
        ret = resctrl_mon_mkdir(1, "test");
        *logged = test_log_count_stop();
        *waits = nanosleep_calls;

        return ret;
}

/* every free RMID in the kernel's limbo for a while: the mkdir is tried again
 * until one is freed, and nothing is said
 */
static void
test_resctrl_mon_mkdir_busy_for_a_while(void **state __attribute__((unused)))
{
        const int errors[] = {EBUSY, EBUSY, EBUSY};
        unsigned logged, waits;
        int ret;

        ret = mkdir_after(errors, DIM(errors), 1, &logged, &waits);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(waits, 3);
        assert_int_equal(logged, 0);
}

/* every free RMID in limbo for longer than the tries: the group is busy */
static void
test_resctrl_mon_mkdir_busy_throughout(void **state __attribute__((unused)))
{
        int errors[30];
        unsigned i, logged, waits;
        int ret;

        for (i = 0; i < DIM(errors); i++)
                errors[i] = EBUSY;

        ret = mkdir_after(errors, DIM(errors), 0, &logged, &waits);
        assert_int_equal(ret, PQOS_RETVAL_BUSY);
        assert_int_equal(waits, 29);
        assert_int_equal(logged, 1);
}

/* no RMID left at all: not tried again, and not busy */
static void
test_resctrl_mon_mkdir_no_rmid(void **state __attribute__((unused)))
{
        const int errors[] = {ENOSPC};
        unsigned logged, waits;
        int ret;

        ret = mkdir_after(errors, DIM(errors), 0, &logged, &waits);
        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);
        assert_int_equal(waits, 0);
        assert_int_equal(logged, 1);
}

/* no RMID left after waiting for one in limbo: not busy either */
static void
test_resctrl_mon_mkdir_busy_then_no_rmid(void **state __attribute__((unused)))
{
        const int errors[] = {EBUSY, ENOSPC};
        unsigned logged, waits;
        int ret;

        ret = mkdir_after(errors, DIM(errors), 0, &logged, &waits);
        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);
        assert_int_equal(waits, 1);
        assert_int_equal(logged, 1);
}

/* another failure is neither: not tried again */
static void
test_resctrl_mon_mkdir_error(void **state __attribute__((unused)))
{
        const int errors[] = {EACCES};
        unsigned logged, waits;
        int ret;

        ret = mkdir_after(errors, DIM(errors), 0, &logged, &waits);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
        assert_int_equal(waits, 0);
        assert_int_equal(logged, 1);
}

/* a group made by another while its mkdir waited is there all the same */
static void
test_resctrl_mon_mkdir_busy_then_made_by_another(void **state
                                                 __attribute__((unused)))
{
        const int errors[] = {EBUSY, EEXIST};
        unsigned logged, waits;
        int ret;

        ret = mkdir_after(errors, DIM(errors), 0, &logged, &waits);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(waits, 1);
        assert_int_equal(logged, 0);
}

/* ======== resctrl_mon_rmdir ======== */

static void
test_resctrl_mon_rmdir(void **state __attribute__((unused)))
{
        int ret;

        expect_string(__wrap_rmdir, path, RESCTRL_PATH "/mon_groups/test");
        will_return(__wrap_rmdir, 0);

        ret = resctrl_mon_rmdir(0, "test");
        assert_int_equal(ret, PQOS_RETVAL_OK);

        expect_string(__wrap_rmdir, path, RESCTRL_PATH "/COS1/mon_groups/test");
        will_return(__wrap_rmdir, 1);

        ret = resctrl_mon_rmdir(1, "test");
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

/* a directory that is not there is told apart from one removed, and any
 * other failure is an error
 */
static void
test_resctrl_mon_rmdir_not_there(void **state __attribute__((unused)))
{
        expect_string(__wrap_rmdir, path, RESCTRL_PATH "/mon_groups/test");
        will_return(__wrap_rmdir, -1);
        will_return(__wrap_rmdir, ENOENT);
        assert_int_equal(resctrl_mon_rmdir(0, "test"), PQOS_RETVAL_RESOURCE);

        expect_string(__wrap_rmdir, path, RESCTRL_PATH "/mon_groups/test");
        will_return(__wrap_rmdir, -1);
        will_return(__wrap_rmdir, EBUSY);
        assert_int_equal(resctrl_mon_rmdir(0, "test"), PQOS_RETVAL_ERROR);
}

/* ======== resctrl_mon_read_counter ======== */

static void
test_resctrl_mon_read_counter(void **state __attribute__((unused)))
{
        int ret;
        uint64_t value;

        /* llc */
        expect_string(__wrap_pqos_fopen, name,
                      "/sys/fs/resctrl/COS1/mon_groups/test/mon_data/mon_L3_00/"
                      "llc_occupancy");
        expect_string(__wrap_pqos_fopen, mode, "r");
        will_return(__wrap_pqos_fopen, "1");

        ret = resctrl_mon_read_counter(1, "test", 0, PQOS_MON_EVENT_L3_OCCUP,
                                       &value);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(value, 1);

        /* lmem */
        expect_string(__wrap_pqos_fopen, name,
                      "/sys/fs/resctrl/COS1/mon_groups/test/mon_data/mon_L3_00/"
                      "mbm_local_bytes");
        expect_string(__wrap_pqos_fopen, mode, "r");
        will_return(__wrap_pqos_fopen, "2");

        ret = resctrl_mon_read_counter(1, "test", 0, PQOS_MON_EVENT_LMEM_BW,
                                       &value);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(value, 2);

        /* tmem */
        expect_string(__wrap_pqos_fopen, name,
                      "/sys/fs/resctrl/COS1/mon_groups/test/mon_data/mon_L3_00/"
                      "mbm_total_bytes");
        expect_string(__wrap_pqos_fopen, mode, "r");
        will_return(__wrap_pqos_fopen, "3");

        ret = resctrl_mon_read_counter(1, "test", 0, PQOS_MON_EVENT_TMEM_BW,
                                       &value);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(value, 3);
}

static void
test_resctrl_mon_read_counter_error(void **state __attribute__((unused)))
{
        int ret;
        uint64_t value = 0;

        /* file doesn't exists*/
        expect_string(__wrap_pqos_fopen, name,
                      "/sys/fs/resctrl/COS1/mon_groups/test/mon_data/mon_L3_00/"
                      "llc_occupancy");
        expect_string(__wrap_pqos_fopen, mode, "r");
        will_return(__wrap_pqos_fopen, NULL);

        ret = resctrl_mon_read_counter(1, "test", 0, PQOS_MON_EVENT_L3_OCCUP,
                                       &value);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);

        /* malformed/invalid value - must return error, not OK with zero */
        expect_string(__wrap_pqos_fopen, name,
                      "/sys/fs/resctrl/COS1/mon_groups/test/mon_data/mon_L3_00/"
                      "llc_occupancy");
        expect_string(__wrap_pqos_fopen, mode, "r");
        will_return(__wrap_pqos_fopen, ";invalid");

        ret = resctrl_mon_read_counter(1, "test", 0, PQOS_MON_EVENT_L3_OCCUP,
                                       &value);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
}

/* ======== resctrl_mon_read_counter: resctrl status strings ======== */

static void
test_resctrl_mon_read_counter_zero(void **state __attribute__((unused)))
{
        int ret;
        uint64_t value = 99;

        /* numeric zero is a valid measurement */
        expect_string(__wrap_pqos_fopen, name,
                      "/sys/fs/resctrl/COS1/mon_groups/test/mon_data/mon_L3_00/"
                      "llc_occupancy");
        expect_string(__wrap_pqos_fopen, mode, "r");
        will_return(__wrap_pqos_fopen, "0\n");

        ret = resctrl_mon_read_counter(1, "test", 0, PQOS_MON_EVENT_L3_OCCUP,
                                       &value);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(value, 0);
}

static void
test_resctrl_mon_read_counter_unassigned(void **state __attribute__((unused)))
{
        int ret;
        uint64_t value = 99;

        expect_string(__wrap_pqos_fopen, name,
                      "/sys/fs/resctrl/COS1/mon_groups/test/mon_data/mon_L3_00/"
                      "mbm_local_bytes");
        expect_string(__wrap_pqos_fopen, mode, "r");
        will_return(__wrap_pqos_fopen, "Unassigned\n");

        ret = resctrl_mon_read_counter(1, "test", 0, PQOS_MON_EVENT_LMEM_BW,
                                       &value);
        assert_int_equal(ret, PQOS_RETVAL_UNAVAILABLE);
        /* value must not be updated */
        assert_int_equal(value, 99);
}

static void
test_resctrl_mon_read_counter_unavailable(void **state __attribute__((unused)))
{
        int ret;
        uint64_t value = 99;

        expect_string(__wrap_pqos_fopen, name,
                      "/sys/fs/resctrl/COS1/mon_groups/test/mon_data/mon_L3_00/"
                      "mbm_total_bytes");
        expect_string(__wrap_pqos_fopen, mode, "r");
        will_return(__wrap_pqos_fopen, "Unavailable\n");

        ret = resctrl_mon_read_counter(1, "test", 0, PQOS_MON_EVENT_TMEM_BW,
                                       &value);
        assert_int_equal(ret, PQOS_RETVAL_UNAVAILABLE);
        assert_int_equal(value, 99);
}

static void
test_resctrl_mon_read_counter_error_state(void **state __attribute__((unused)))
{
        int ret;
        uint64_t value = 99;

        expect_string(__wrap_pqos_fopen, name,
                      "/sys/fs/resctrl/COS1/mon_groups/test/mon_data/mon_L3_00/"
                      "llc_occupancy");
        expect_string(__wrap_pqos_fopen, mode, "r");
        will_return(__wrap_pqos_fopen, "Error\n");

        ret = resctrl_mon_read_counter(1, "test", 0, PQOS_MON_EVENT_L3_OCCUP,
                                       &value);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
        assert_int_equal(value, 99);
}

static void
test_resctrl_mon_read_counter_trailing_junk(void **state
                                            __attribute__((unused)))
{
        int ret;
        uint64_t value = 99;

        /* trailing non-numeric characters after a valid number */
        expect_string(__wrap_pqos_fopen, name,
                      "/sys/fs/resctrl/COS1/mon_groups/test/mon_data/mon_L3_00/"
                      "llc_occupancy");
        expect_string(__wrap_pqos_fopen, mode, "r");
        will_return(__wrap_pqos_fopen, "123abc\n");

        ret = resctrl_mon_read_counter(1, "test", 0, PQOS_MON_EVENT_L3_OCCUP,
                                       &value);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
        assert_int_equal(value, 99);
}

int
main(void)
{
        const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_resctrl_mon_mkdir),
            cmocka_unit_test(test_resctrl_mon_mkdir_busy_for_a_while),
            cmocka_unit_test(test_resctrl_mon_mkdir_busy_throughout),
            cmocka_unit_test(test_resctrl_mon_mkdir_no_rmid),
            cmocka_unit_test(test_resctrl_mon_mkdir_busy_then_no_rmid),
            cmocka_unit_test(test_resctrl_mon_mkdir_error),
            cmocka_unit_test(test_resctrl_mon_mkdir_busy_then_made_by_another),
            cmocka_unit_test(test_resctrl_mon_rmdir),
            cmocka_unit_test(test_resctrl_mon_rmdir_not_there),
            cmocka_unit_test(test_resctrl_mon_read_counter),
            cmocka_unit_test(test_resctrl_mon_read_counter_error),
            cmocka_unit_test(test_resctrl_mon_read_counter_zero),
            cmocka_unit_test(test_resctrl_mon_read_counter_unassigned),
            cmocka_unit_test(test_resctrl_mon_read_counter_unavailable),
            cmocka_unit_test(test_resctrl_mon_read_counter_error_state),
            cmocka_unit_test(test_resctrl_mon_read_counter_trailing_junk),
        };

        return cmocka_run_group_tests(tests, NULL, NULL);
}
