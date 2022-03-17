/*
 * BSD LICENSE
 *
 * Copyright(c) 2020-2022 Intel Corporation. All rights reserved.
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

#include "mock_common.h"
#include "os_cap.h"
#include "test.h"

/* ======== os_cap_mon_resctrl_support ======== */

static void
test_os_cap_mon_resctrl_support_llc(void **state __attribute__((unused)))
{
        int ret;
        int supported;
        uint32_t scale;

        expect_string(__wrap_pqos_dir_exists, path,
                      "/sys/fs/resctrl/info/L3_MON");
        will_return(__wrap_pqos_dir_exists, 1);

        expect_string(__wrap_pqos_file_contains, fname,
                      "/sys/fs/resctrl/info/L3_MON/mon_features");
        expect_string(__wrap_pqos_file_contains, str, "llc_occupancy");
        will_return(__wrap_pqos_file_contains, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_file_contains, 1);

        ret = os_cap_mon_resctrl_support(PQOS_MON_EVENT_L3_OCCUP, &supported,
                                         &scale);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(supported, 1);
        assert_int_equal(scale, 1);
}

static void
test_os_cap_mon_resctrl_support_llc_unsupported(void **state
                                                __attribute__((unused)))
{
        int ret;
        int supported;
        uint32_t scale;

        /* resctrl mon unsupported */
        expect_string(__wrap_pqos_dir_exists, path,
                      "/sys/fs/resctrl/info/L3_MON");
        will_return(__wrap_pqos_dir_exists, 0);

        ret = os_cap_mon_resctrl_support(PQOS_MON_EVENT_L3_OCCUP, &supported,
                                         &scale);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(supported, 0);

        /* event unsupported */
        expect_string(__wrap_pqos_dir_exists, path,
                      "/sys/fs/resctrl/info/L3_MON");
        will_return(__wrap_pqos_dir_exists, 1);

        expect_string(__wrap_pqos_file_contains, fname,
                      "/sys/fs/resctrl/info/L3_MON/mon_features");
        expect_string(__wrap_pqos_file_contains, str, "llc_occupancy");
        will_return(__wrap_pqos_file_contains, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_file_contains, 0);

        ret = os_cap_mon_resctrl_support(PQOS_MON_EVENT_L3_OCCUP, &supported,
                                         &scale);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(supported, 0);
}

static void
test_os_cap_mon_resctrl_support_lmem(void **state __attribute__((unused)))
{
        int ret;
        int supported;
        uint32_t scale;

        expect_string(__wrap_pqos_dir_exists, path,
                      "/sys/fs/resctrl/info/L3_MON");
        will_return(__wrap_pqos_dir_exists, 1);

        expect_string(__wrap_pqos_file_contains, fname,
                      "/sys/fs/resctrl/info/L3_MON/mon_features");
        expect_string(__wrap_pqos_file_contains, str, "mbm_local_bytes");
        will_return(__wrap_pqos_file_contains, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_file_contains, 1);

        ret = os_cap_mon_resctrl_support(PQOS_MON_EVENT_LMEM_BW, &supported,
                                         &scale);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(supported, 1);
        assert_int_equal(scale, 1);
}

static void
test_os_cap_mon_resctrl_support_lmem_unsupported(void **state
                                                 __attribute__((unused)))
{
        int ret;
        int supported;
        uint32_t scale;

        /* resctrl mon unsupported */
        expect_string(__wrap_pqos_dir_exists, path,
                      "/sys/fs/resctrl/info/L3_MON");
        will_return(__wrap_pqos_dir_exists, 0);

        ret = os_cap_mon_resctrl_support(PQOS_MON_EVENT_LMEM_BW, &supported,
                                         &scale);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(supported, 0);

        /* event unsupported */
        expect_string(__wrap_pqos_dir_exists, path,
                      "/sys/fs/resctrl/info/L3_MON");
        will_return(__wrap_pqos_dir_exists, 1);

        expect_string(__wrap_pqos_file_contains, fname,
                      "/sys/fs/resctrl/info/L3_MON/mon_features");
        expect_string(__wrap_pqos_file_contains, str, "mbm_local_bytes");
        will_return(__wrap_pqos_file_contains, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_file_contains, 0);

        ret = os_cap_mon_resctrl_support(PQOS_MON_EVENT_LMEM_BW, &supported,
                                         &scale);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(supported, 0);
}

static void
test_os_cap_mon_resctrl_support_tmem(void **state __attribute__((unused)))
{
        int ret;
        int supported;
        uint32_t scale;

        expect_string(__wrap_pqos_dir_exists, path,
                      "/sys/fs/resctrl/info/L3_MON");
        will_return(__wrap_pqos_dir_exists, 1);

        expect_string(__wrap_pqos_file_contains, fname,
                      "/sys/fs/resctrl/info/L3_MON/mon_features");
        expect_string(__wrap_pqos_file_contains, str, "mbm_total_bytes");
        will_return(__wrap_pqos_file_contains, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_file_contains, 1);

        ret = os_cap_mon_resctrl_support(PQOS_MON_EVENT_TMEM_BW, &supported,
                                         &scale);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(supported, 1);
        assert_int_equal(scale, 1);
}

static void
test_os_cap_mon_resctrl_support_tmem_unsupported(void **state
                                                 __attribute__((unused)))
{
        int ret;
        int supported;
        uint32_t scale;

        /* resctrl mon unsupported */
        expect_string(__wrap_pqos_dir_exists, path,
                      "/sys/fs/resctrl/info/L3_MON");
        will_return(__wrap_pqos_dir_exists, 0);

        ret = os_cap_mon_resctrl_support(PQOS_MON_EVENT_TMEM_BW, &supported,
                                         &scale);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(supported, 0);

        /* event unsupported */
        expect_string(__wrap_pqos_dir_exists, path,
                      "/sys/fs/resctrl/info/L3_MON");
        will_return(__wrap_pqos_dir_exists, 1);

        expect_string(__wrap_pqos_file_contains, fname,
                      "/sys/fs/resctrl/info/L3_MON/mon_features");
        expect_string(__wrap_pqos_file_contains, str, "mbm_total_bytes");
        will_return(__wrap_pqos_file_contains, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_file_contains, 0);

        ret = os_cap_mon_resctrl_support(PQOS_MON_EVENT_TMEM_BW, &supported,
                                         &scale);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(supported, 0);
}

/* ======== os_cap_mon_perf_support ======== */

static void
test_os_cap_mon_perf_support_llc_miss(void **state __attribute__((unused)))
{
        int ret;
        int supported;
        uint32_t scale;

        ret = os_cap_mon_perf_support(PQOS_PERF_EVENT_LLC_MISS, &supported,
                                      &scale);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(supported, 1);
        assert_int_equal(scale, 1);
}

static void
test_os_cap_mon_perf_support_llc_ref(void **state __attribute__((unused)))
{
        int ret;
        int supported;
        uint32_t scale;

        ret = os_cap_mon_perf_support(PQOS_PERF_EVENT_LLC_REF, &supported,
                                      &scale);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(supported, 1);
        assert_int_equal(scale, 1);
}

static void
test_os_cap_mon_perf_support_ipc(void **state __attribute__((unused)))
{
        int ret;
        int supported;
        uint32_t scale;

        ret = os_cap_mon_perf_support(PQOS_PERF_EVENT_IPC, &supported, &scale);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(supported, 1);
        assert_int_equal(scale, 1);
}

static void
test_os_cap_mon_perf_support_llc_unsupported(void **state
                                             __attribute__((unused)))
{
        int ret;
        int supported;
        uint32_t scale;

        expect_string(__wrap_pqos_file_exists, path,
                      "/sys/devices/intel_cqm/events/llc_occupancy");
        will_return(__wrap_pqos_file_exists, 0);

        ret = os_cap_mon_perf_support(PQOS_MON_EVENT_L3_OCCUP, &supported,
                                      &scale);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(supported, 0);
}

static void
test_os_cap_mon_perf_support_lmem_unsupported(void **state
                                              __attribute__((unused)))
{
        int ret;
        int supported;
        uint32_t scale;

        expect_string(__wrap_pqos_file_exists, path,
                      "/sys/devices/intel_cqm/events/local_bytes");
        will_return(__wrap_pqos_file_exists, 0);

        ret =
            os_cap_mon_perf_support(PQOS_MON_EVENT_LMEM_BW, &supported, &scale);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(supported, 0);
}

static void
test_os_cap_mon_perf_support_tmem_unsupported(void **state
                                              __attribute__((unused)))
{
        int ret;
        int supported;
        uint32_t scale;

        expect_string(__wrap_pqos_file_exists, path,
                      "/sys/devices/intel_cqm/events/total_bytes");
        will_return(__wrap_pqos_file_exists, 0);

        ret =
            os_cap_mon_perf_support(PQOS_MON_EVENT_TMEM_BW, &supported, &scale);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(supported, 0);
}

int
main(void)
{
        int result = 0;

        const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_os_cap_mon_resctrl_support_llc),
            cmocka_unit_test(test_os_cap_mon_resctrl_support_llc_unsupported),
            cmocka_unit_test(test_os_cap_mon_resctrl_support_lmem),
            cmocka_unit_test(test_os_cap_mon_resctrl_support_lmem_unsupported),
            cmocka_unit_test(test_os_cap_mon_resctrl_support_tmem),
            cmocka_unit_test(test_os_cap_mon_resctrl_support_tmem_unsupported),
            cmocka_unit_test(test_os_cap_mon_perf_support_llc_miss),
            cmocka_unit_test(test_os_cap_mon_perf_support_llc_ref),
            cmocka_unit_test(test_os_cap_mon_perf_support_ipc),
            cmocka_unit_test(test_os_cap_mon_perf_support_llc_unsupported),
            cmocka_unit_test(test_os_cap_mon_perf_support_lmem_unsupported),
            cmocka_unit_test(test_os_cap_mon_perf_support_tmem_unsupported)};

        result += cmocka_run_group_tests(tests, NULL, NULL);

        return result;
}
