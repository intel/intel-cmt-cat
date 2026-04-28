/*
 * BSD LICENSE
 *
 * Copyright(c) 2026 Intel Corporation. All rights reserved.
 */

#include "resctrl_monitoring.h"
#include "test.h"

FILE *
__wrap_pqos_fopen(const char *name, const char *mode)
{
        check_expected(name);
        check_expected(mode);

        return mock_ptr_type(FILE *);
}

/*
int
__wrap_pqos_fclose(FILE *fd)
{
        function_called();
        assert_non_null(fd);

        return fclose(fd);
}
*/

static FILE *
create_perf_pkg_mon_features(const int core_energy, const int activity)
{
        FILE *fd = tmpfile();

        assert_non_null(fd);

        if (core_energy)
                fprintf(fd, "core_energy\n");
        if (activity)
                fprintf(fd, "activity\n");

        fseek(fd, 0, SEEK_SET);

        return fd;
}

static void
test_resctrl_aet_init_not_supported(void **state __attribute__((unused)))
{
        int ret;
        struct pqos_cpuinfo cpu;
        struct pqos_cap cap;

        expect_string(__wrap_pqos_dir_exists, path,
                      "/sys/fs/resctrl/info/L3_MON");
        will_return(__wrap_pqos_dir_exists, 0);
        expect_string(__wrap_pqos_dir_exists, path,
                      "/sys/fs/resctrl/info/PERF_PKG_MON");
        will_return(__wrap_pqos_dir_exists, 0);

        ret = resctrl_mon_init(&cpu, &cap);
        assert_int_equal(ret, PQOS_RETVAL_OK);

        assert_false(resctrl_mon_is_supported());
        assert_false(
            resctrl_mon_is_event_supported(PQOS_MON_EVENT_CORE_ENERGY));
        assert_false(resctrl_mon_is_event_supported(PQOS_MON_EVENT_ACTIVITY));
        assert_false(resctrl_mon_is_event_supported(PQOS_MON_EVENT_POWER));

        ret = resctrl_mon_fini();
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_resctrl_aet_init_core_energy(void **state __attribute__((unused)))
{
        int ret;
        struct pqos_cpuinfo cpu;
        struct pqos_cap cap;
        FILE *fd = create_perf_pkg_mon_features(1, 0);

        expect_string(__wrap_pqos_dir_exists, path,
                      "/sys/fs/resctrl/info/L3_MON");
        will_return(__wrap_pqos_dir_exists, 0);
        expect_string(__wrap_pqos_dir_exists, path,
                      "/sys/fs/resctrl/info/PERF_PKG_MON");
        will_return(__wrap_pqos_dir_exists, 1);
        expect_string(__wrap_pqos_dir_exists, path,
                      "/sys/fs/resctrl/info/L3_MON");
        will_return(__wrap_pqos_dir_exists, 0);
        expect_string(__wrap_pqos_dir_exists, path,
                      "/sys/fs/resctrl/info/PERF_PKG_MON");
        will_return(__wrap_pqos_dir_exists, 1);

        expect_string(__wrap_pqos_fopen, name,
                      "/sys/fs/resctrl/info/PERF_PKG_MON/mon_features");
        expect_string(__wrap_pqos_fopen, mode, "r");
        will_return(__wrap_pqos_fopen, fd);

        ret = resctrl_mon_init(&cpu, &cap);
        assert_int_equal(ret, PQOS_RETVAL_OK);

        assert_true(resctrl_mon_is_supported());
        assert_true(resctrl_mon_is_event_supported(PQOS_MON_EVENT_CORE_ENERGY));
        assert_false(resctrl_mon_is_event_supported(PQOS_MON_EVENT_ACTIVITY));
        assert_true(resctrl_mon_is_event_supported(PQOS_MON_EVENT_POWER));

        ret = resctrl_mon_fini();
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_resctrl_aet_init_activity(void **state __attribute__((unused)))
{
        int ret;
        struct pqos_cpuinfo cpu;
        struct pqos_cap cap;
        FILE *fd = create_perf_pkg_mon_features(0, 1);

        expect_string(__wrap_pqos_dir_exists, path,
                      "/sys/fs/resctrl/info/L3_MON");
        will_return(__wrap_pqos_dir_exists, 0);
        expect_string(__wrap_pqos_dir_exists, path,
                      "/sys/fs/resctrl/info/PERF_PKG_MON");
        will_return(__wrap_pqos_dir_exists, 1);
        expect_string(__wrap_pqos_dir_exists, path,
                      "/sys/fs/resctrl/info/L3_MON");
        will_return(__wrap_pqos_dir_exists, 0);
        expect_string(__wrap_pqos_dir_exists, path,
                      "/sys/fs/resctrl/info/PERF_PKG_MON");
        will_return(__wrap_pqos_dir_exists, 1);

        expect_string(__wrap_pqos_fopen, name,
                      "/sys/fs/resctrl/info/PERF_PKG_MON/mon_features");
        expect_string(__wrap_pqos_fopen, mode, "r");
        will_return(__wrap_pqos_fopen, fd);

        ret = resctrl_mon_init(&cpu, &cap);
        assert_int_equal(ret, PQOS_RETVAL_OK);

        assert_true(resctrl_mon_is_supported());
        assert_false(
            resctrl_mon_is_event_supported(PQOS_MON_EVENT_CORE_ENERGY));
        assert_true(resctrl_mon_is_event_supported(PQOS_MON_EVENT_ACTIVITY));
        assert_false(resctrl_mon_is_event_supported(PQOS_MON_EVENT_POWER));

        ret = resctrl_mon_fini();
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_resctrl_aet_init_both(void **state __attribute__((unused)))
{
        int ret;
        struct pqos_cpuinfo cpu;
        struct pqos_cap cap;
        FILE *fd = create_perf_pkg_mon_features(1, 1);

        expect_string(__wrap_pqos_dir_exists, path,
                      "/sys/fs/resctrl/info/L3_MON");
        will_return(__wrap_pqos_dir_exists, 0);
        expect_string(__wrap_pqos_dir_exists, path,
                      "/sys/fs/resctrl/info/PERF_PKG_MON");
        will_return(__wrap_pqos_dir_exists, 1);
        expect_string(__wrap_pqos_dir_exists, path,
                      "/sys/fs/resctrl/info/L3_MON");
        will_return(__wrap_pqos_dir_exists, 0);
        expect_string(__wrap_pqos_dir_exists, path,
                      "/sys/fs/resctrl/info/PERF_PKG_MON");
        will_return(__wrap_pqos_dir_exists, 1);

        expect_string(__wrap_pqos_fopen, name,
                      "/sys/fs/resctrl/info/PERF_PKG_MON/mon_features");
        expect_string(__wrap_pqos_fopen, mode, "r");
        will_return(__wrap_pqos_fopen, fd);

        ret = resctrl_mon_init(&cpu, &cap);
        assert_int_equal(ret, PQOS_RETVAL_OK);

        assert_true(resctrl_mon_is_supported());
        assert_true(resctrl_mon_is_event_supported(PQOS_MON_EVENT_CORE_ENERGY));
        assert_true(resctrl_mon_is_event_supported(PQOS_MON_EVENT_ACTIVITY));
        assert_true(resctrl_mon_is_event_supported(PQOS_MON_EVENT_POWER));

        ret = resctrl_mon_fini();
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

int
main(void)
{
        const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_resctrl_aet_init_not_supported),
            cmocka_unit_test(test_resctrl_aet_init_core_energy),
            cmocka_unit_test(test_resctrl_aet_init_activity),
            cmocka_unit_test(test_resctrl_aet_init_both),
        };

        return cmocka_run_group_tests(tests, NULL, NULL);
}
