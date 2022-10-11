/*
 * BSD LICENSE
 *
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
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

#include "resctrl_monitoring.h"
#include "test.h"

static FILE *
create_mon_features(enum pqos_mon_event events)
{
        FILE *fd = tmpfile();

        assert_non_null(fd);

        if (events & PQOS_MON_EVENT_L3_OCCUP)
                fprintf(fd, "llc_occupancy\n");
        if (events & PQOS_MON_EVENT_LMEM_BW)
                fprintf(fd, "mbm_local_bytes\n");
        if (events & PQOS_MON_EVENT_TMEM_BW)
                fprintf(fd, "mbm_total_bytes\n");

        fseek(fd, 0, SEEK_SET);

        return fd;
}

int
__wrap_pqos_fclose(FILE *fd)
{
        function_called();
        assert_non_null(fd);

        return fclose(fd);
}

static void
test_resctrl_mon_init_not_supported(void **state __attribute__((unused)))
{
        int ret;
        struct pqos_cpuinfo cpu;
        struct pqos_cap cap;

        expect_string(__wrap_pqos_dir_exists, path,
                      "/sys/fs/resctrl/info/L3_MON");
        will_return(__wrap_pqos_dir_exists, 0);

        ret = resctrl_mon_init(&cpu, &cap);
        assert_int_equal(ret, PQOS_RETVAL_OK);

        assert_false(resctrl_mon_is_supported());
        assert_false(resctrl_mon_is_event_supported(PQOS_MON_EVENT_L3_OCCUP));
        assert_false(resctrl_mon_is_event_supported(PQOS_MON_EVENT_LMEM_BW));
        assert_false(resctrl_mon_is_event_supported(PQOS_MON_EVENT_TMEM_BW));
        assert_false(resctrl_mon_is_event_supported(PQOS_MON_EVENT_RMEM_BW));

        ret = resctrl_mon_fini();
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_resctrl_mon_init_error(void **state __attribute__((unused)))
{
        int ret;
        struct pqos_cpuinfo cpu;
        struct pqos_cap cap;

        expect_string(__wrap_pqos_dir_exists, path,
                      "/sys/fs/resctrl/info/L3_MON");
        will_return(__wrap_pqos_dir_exists, 1);

        expect_string(__wrap_pqos_fopen, name,
                      "/sys/fs/resctrl/info/L3_MON/mon_features");
        expect_string(__wrap_pqos_fopen, mode, "r");
        will_return(__wrap_pqos_fopen, NULL);

        ret = resctrl_mon_init(&cpu, &cap);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
}

static void
test_resctrl_mon_init_llc(void **state __attribute__((unused)))
{
        int ret;
        struct pqos_cpuinfo cpu;
        struct pqos_cap cap;
        FILE *fd;

        fd = create_mon_features(PQOS_MON_EVENT_L3_OCCUP);

        expect_string(__wrap_pqos_dir_exists, path,
                      "/sys/fs/resctrl/info/L3_MON");
        will_return(__wrap_pqos_dir_exists, 1);

        expect_string(__wrap_pqos_fopen, name,
                      "/sys/fs/resctrl/info/L3_MON/mon_features");
        expect_string(__wrap_pqos_fopen, mode, "r");
        will_return(__wrap_pqos_fopen, fd);
        expect_function_call(__wrap_pqos_fclose);

        ret = resctrl_mon_init(&cpu, &cap);
        assert_int_equal(ret, PQOS_RETVAL_OK);

        assert_true(resctrl_mon_is_supported());
        assert_true(resctrl_mon_is_event_supported(PQOS_MON_EVENT_L3_OCCUP));
        assert_false(resctrl_mon_is_event_supported(PQOS_MON_EVENT_LMEM_BW));
        assert_false(resctrl_mon_is_event_supported(PQOS_MON_EVENT_TMEM_BW));
        assert_false(resctrl_mon_is_event_supported(PQOS_MON_EVENT_RMEM_BW));
}

static void
test_resctrl_mon_init_lmem(void **state __attribute__((unused)))
{
        int ret;
        struct pqos_cpuinfo cpu;
        struct pqos_cap cap;
        FILE *fd;

        fd = create_mon_features(PQOS_MON_EVENT_LMEM_BW);

        expect_string(__wrap_pqos_dir_exists, path,
                      "/sys/fs/resctrl/info/L3_MON");
        will_return(__wrap_pqos_dir_exists, 1);

        expect_string(__wrap_pqos_fopen, name,
                      "/sys/fs/resctrl/info/L3_MON/mon_features");
        expect_string(__wrap_pqos_fopen, mode, "r");
        will_return(__wrap_pqos_fopen, fd);
        expect_function_call(__wrap_pqos_fclose);

        ret = resctrl_mon_init(&cpu, &cap);
        assert_int_equal(ret, PQOS_RETVAL_OK);

        assert_true(resctrl_mon_is_supported());
        assert_false(resctrl_mon_is_event_supported(PQOS_MON_EVENT_L3_OCCUP));
        assert_true(resctrl_mon_is_event_supported(PQOS_MON_EVENT_LMEM_BW));
        assert_false(resctrl_mon_is_event_supported(PQOS_MON_EVENT_TMEM_BW));
        assert_false(resctrl_mon_is_event_supported(PQOS_MON_EVENT_RMEM_BW));
}

static void
test_resctrl_mon_init_tmem(void **state __attribute__((unused)))
{
        int ret;
        struct pqos_cpuinfo cpu;
        struct pqos_cap cap;
        FILE *fd;

        fd = create_mon_features(PQOS_MON_EVENT_TMEM_BW);

        expect_string(__wrap_pqos_dir_exists, path,
                      "/sys/fs/resctrl/info/L3_MON");
        will_return(__wrap_pqos_dir_exists, 1);

        expect_string(__wrap_pqos_fopen, name,
                      "/sys/fs/resctrl/info/L3_MON/mon_features");
        expect_string(__wrap_pqos_fopen, mode, "r");
        will_return(__wrap_pqos_fopen, fd);
        expect_function_call(__wrap_pqos_fclose);

        ret = resctrl_mon_init(&cpu, &cap);
        assert_int_equal(ret, PQOS_RETVAL_OK);

        assert_true(resctrl_mon_is_supported());
        assert_false(resctrl_mon_is_event_supported(PQOS_MON_EVENT_L3_OCCUP));
        assert_false(resctrl_mon_is_event_supported(PQOS_MON_EVENT_LMEM_BW));
        assert_true(resctrl_mon_is_event_supported(PQOS_MON_EVENT_TMEM_BW));
        assert_false(resctrl_mon_is_event_supported(PQOS_MON_EVENT_RMEM_BW));
}

static void
test_resctrl_mon_init_rmem(void **state __attribute__((unused)))
{
        int ret;
        struct pqos_cpuinfo cpu;
        struct pqos_cap cap;
        FILE *fd;

        fd = create_mon_features(PQOS_MON_EVENT_TMEM_BW |
                                 PQOS_MON_EVENT_LMEM_BW);

        expect_string(__wrap_pqos_dir_exists, path,
                      "/sys/fs/resctrl/info/L3_MON");
        will_return(__wrap_pqos_dir_exists, 1);

        expect_string(__wrap_pqos_fopen, name,
                      "/sys/fs/resctrl/info/L3_MON/mon_features");
        expect_string(__wrap_pqos_fopen, mode, "r");
        will_return(__wrap_pqos_fopen, fd);
        expect_function_call(__wrap_pqos_fclose);

        ret = resctrl_mon_init(&cpu, &cap);
        assert_int_equal(ret, PQOS_RETVAL_OK);

        assert_true(resctrl_mon_is_supported());
        assert_false(resctrl_mon_is_event_supported(PQOS_MON_EVENT_L3_OCCUP));
        assert_true(resctrl_mon_is_event_supported(PQOS_MON_EVENT_LMEM_BW));
        assert_true(resctrl_mon_is_event_supported(PQOS_MON_EVENT_TMEM_BW));
        assert_true(resctrl_mon_is_event_supported(PQOS_MON_EVENT_RMEM_BW));
}

int
main(void)
{
        const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_resctrl_mon_init_not_supported),
            cmocka_unit_test(test_resctrl_mon_init_error),
            cmocka_unit_test(test_resctrl_mon_init_llc),
            cmocka_unit_test(test_resctrl_mon_init_lmem),
            cmocka_unit_test(test_resctrl_mon_init_tmem),
            cmocka_unit_test(test_resctrl_mon_init_rmem),
        };

        cmocka_run_group_tests(tests, NULL, NULL);
}
