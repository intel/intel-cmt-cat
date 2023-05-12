/*
 * BSD LICENSE
 *
 * Copyright(c) 2022-2023 Intel Corporation. All rights reserved.
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

#include <sys/stat.h>

/* ======== mock ======== */

int
__wrap_mkdir(const char *path, mode_t mode)
{
        check_expected(path);
        check_expected(mode);

        return mock_type(int);
}

int
__wrap_rmdir(const char *path)
{
        check_expected(path);

        return mock_type(int);
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

        /* invalid value */
        expect_string(__wrap_pqos_fopen, name,
                      "/sys/fs/resctrl/COS1/mon_groups/test/mon_data/mon_L3_00/"
                      "llc_occupancy");
        expect_string(__wrap_pqos_fopen, mode, "r");
        will_return(__wrap_pqos_fopen, ";invalid");

        ret = resctrl_mon_read_counter(1, "test", 0, PQOS_MON_EVENT_L3_OCCUP,
                                       &value);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(value, 0);
}

int
main(void)
{
        const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_resctrl_mon_mkdir),
            cmocka_unit_test(test_resctrl_mon_rmdir),
            cmocka_unit_test(test_resctrl_mon_read_counter),
            cmocka_unit_test(test_resctrl_mon_read_counter_error),
        };

        cmocka_run_group_tests(tests, NULL, NULL);
}
