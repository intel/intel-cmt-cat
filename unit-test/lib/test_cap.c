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

#include "test_cap.h"

#include "cap.h"
#include "log.h"
#include "mock_cap.h"
#include "test.h"

#include <fcntl.h>
#include <sys/stat.h>

#define wrap_lockf(cmdpar)                                                     \
        do {                                                                   \
                expect_value(__wrap_lockf, fd, LOCKFILENO);                    \
                expect_value(__wrap_lockf, cmd, cmdpar);                       \
                expect_value(__wrap_lockf, len, 0);                            \
                expect_function_call(__wrap_lockf);                            \
        } while (0)

#define wrap_lock_and_unlock()                                                 \
        do {                                                                   \
                wrap_lockf(F_LOCK);                                            \
                wrap_lockf(F_ULOCK);                                           \
        } while (0)

/* ======== mock ========*/

int
__wrap_open(const char *path, int oflags, int mode)
{
        if (strcmp(path, LOCKFILE))
                return __real_open(path, oflags, mode);

        function_called();
        check_expected(path);
        check_expected(oflags);
        check_expected(mode);

        return LOCKFILENO;
}

int
__wrap_close(int fildes)
{
        if (fildes != LOCKFILENO)
                return __real_close(fildes);

        function_called();
        check_expected(fildes);

        return 0;
}

int
__wrap_lockf(int fd, int cmd, off_t len)
{
        if (fd != LOCKFILENO)
                return __real_lockf(fd, cmd, len);

        function_called();
        check_expected(fd);
        check_expected(cmd);
        check_expected(len);
        return 0;
}

int
__wrap_cpuinfo_init(enum pqos_interface interface __attribute__((unused)),
                    const struct pqos_cpuinfo **topology)
{
        assert_non_null(topology);
        *topology = mock_type(const struct pqos_cpuinfo *);

        return 0;
}

int
__wrap_cpuinfo_fini(void)
{
        return 0;
};

int
__wrap_hw_cap_mon_discover(struct pqos_cap_mon **r_cap,
                           const struct pqos_cpuinfo *cpu
                           __attribute__((unused)))
{
        function_called();
        assert_non_null(r_cap);
        *r_cap = mock_type(struct pqos_cap_mon *);

        return 0;
}

int
__wrap_os_cap_mon_discover(struct pqos_cap_mon **r_cap,
                           const struct pqos_cpuinfo *cpu
                           __attribute__((unused)))
{
        function_called();
        assert_non_null(r_cap);
        *r_cap = mock_type(struct pqos_cap_mon *);

        return 0;
}

int
__wrap_hw_cap_l3ca_discover(struct pqos_cap_l3ca *cap,
                            const struct pqos_cpuinfo *cpu
                            __attribute__((unused)))
{
        assert_non_null(cap);
        memcpy(cap, mock_type(struct pqos_cap_l3ca *), sizeof(*cap));

        return 0;
}

int
__wrap_os_cap_l3ca_discover(struct pqos_cap_l3ca *cap,
                            const struct pqos_cpuinfo *cpu
                            __attribute__((unused)))
{
        assert_non_null(cap);
        memcpy(cap, mock_type(struct pqos_cap_l3ca *), sizeof(*cap));

        return 0;
}

int
__wrap_hw_cap_l2ca_discover(struct pqos_cap_l2ca *cap,
                            const struct pqos_cpuinfo *cpu
                            __attribute__((unused)))
{
        assert_non_null(cap);
        memcpy(cap, mock_type(struct pqos_cap_l2ca *), sizeof(*cap));

        return 0;
}

int
__wrap_os_cap_l2ca_discover(struct pqos_cap_l2ca *cap,
                            const struct pqos_cpuinfo *cpu
                            __attribute__((unused)))
{
        assert_non_null(cap);
        memcpy(cap, mock_type(struct pqos_cap_l2ca *), sizeof(*cap));

        return 0;
}

int
__wrap_hw_cap_mba_discover(struct pqos_cap_mba *cap,
                           const struct pqos_cpuinfo *cpu
                           __attribute__((unused)))
{
        assert_non_null(cap);
        memcpy(cap, mock_type(struct pqos_cap_mba *), sizeof(*cap));

        return 0;
}

int
__wrap_os_cap_mba_discover(struct pqos_cap_mba *cap,
                           const struct pqos_cpuinfo *cpu
                           __attribute__((unused)))
{
        assert_non_null(cap);
        memcpy(cap, mock_type(struct pqos_cap_mba *), sizeof(*cap));

        return 0;
}

int
__wrap_pqos_mon_init(const struct pqos_cpuinfo *cpu __attribute__((unused)),
                     const struct pqos_cap *cap __attribute__((unused)),
                     const struct pqos_config *cfg __attribute__((unused)))
{
        return PQOS_RETVAL_OK;
}

int
__wrap_os_cap_init(const enum pqos_interface inter)
{
        const LargestIntegralType valid_inters[] = {PQOS_INTER_OS,
                                                    PQOS_INTER_OS_RESCTRL_MON};

        function_called();
        assert_in_set(inter, valid_inters, DIM(valid_inters));
        return PQOS_RETVAL_OK;
}

/* ======== setup ========*/
static int
setup_cap_init_msr(void **state __attribute__((unused)))
{
        struct test_data *data;

        test_init_all(state);

        data = (struct test_data *)*state;
        data->interface = PQOS_INTER_MSR;

        return 0;
}

static int
setup_cap_init_os(void **state __attribute__((unused)))
{
        struct test_data *data;

        test_init_all(state);

        data = (struct test_data *)*state;
        data->interface = PQOS_INTER_OS;

        return 0;
}

static int
setup_cap_init_os_resctrl_mon(void **state __attribute__((unused)))
{
        struct test_data *data;

        test_init_all(state);

        data = (struct test_data *)*state;
        data->interface = PQOS_INTER_OS_RESCTRL_MON;

        return 0;
}

/* ======== _pqos_check_init  ======== */

static void
test__pqos_check_init_before_init(void **state __attribute__((unused)))
{
        int ret;

        ret = _pqos_check_init(0);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        ret = _pqos_check_init(1);
        assert_int_not_equal(ret, PQOS_RETVAL_OK);
}

static void
test__pqos_check_init_after_init(void **state __attribute__((unused)))
{
        int ret;

        ret = _pqos_check_init(0);
        assert_int_not_equal(ret, PQOS_RETVAL_OK);
        ret = _pqos_check_init(1);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

/* ======== _pqos_get_inter  ======== */

static void
test__pqos_get_inter(void **state __attribute__((unused)))
{
        enum pqos_interface ret;
        struct test_data *data;

        data = (struct test_data *)*state;

        ret = _pqos_get_inter();
        assert_int_equal(ret, data->interface);
}

/* ======== _pqos_get_cap  ======== */

static void
test__pqos_get_cap_before_init(void **state __attribute__((unused)))
{
        const struct pqos_cap *ret;

        ret = _pqos_get_cap();
        assert_null(ret);
}

static void
test__pqos_get_cap_after_init(void **state __attribute__((unused)))
{
        const struct pqos_cap *ret;

        ret = _pqos_get_cap();
        assert_non_null(ret);
}

/* ======== _pqos_get_cpu  ======== */

static void
test__pqos_get_cpu_before_init(void **state __attribute__((unused)))
{
        const struct pqos_cpuinfo *ret;

        ret = _pqos_get_cpu();
        assert_null(ret);
}

static void
test__pqos_get_cpu_after_init(void **state __attribute__((unused)))
{
        const struct pqos_cpuinfo *ret;

        ret = _pqos_get_cpu();
        assert_non_null(ret);
}

/* ======== _pqos_cap_get_type ======== */

static void
test__pqos_cap_get_type_param(void **state __attribute__((unused)))
{
        int ret;
        const struct pqos_capability *p_cap_item;

        ret = _pqos_cap_get_type(-1, NULL);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
        ret = _pqos_cap_get_type(-1, &p_cap_item);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
        ret = _pqos_cap_get_type(PQOS_CAP_TYPE_MON, NULL);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
        ret = _pqos_cap_get_type(PQOS_CAP_TYPE_L3CA, NULL);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
        ret = _pqos_cap_get_type(PQOS_CAP_TYPE_L2CA, NULL);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
        ret = _pqos_cap_get_type(PQOS_CAP_TYPE_MBA, NULL);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
}

/* ======== pqos_init ======== */

static void
test_pqos_init_param(void **state __attribute__((unused)))
{
        int ret;

        ret = pqos_init(NULL);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
}

static void
test_pqos_init(void **state __attribute__((unused)))
{
        int ret;
        struct pqos_config cfg;
        struct test_data *data;

        data = (struct test_data *)*state;

        assert_non_null(data);

        memset(&cfg, 0, sizeof(cfg));
        cfg.verbose = LOG_VER_SILENT;
        cfg.fd_log = -1;
        cfg.interface = data->interface;

        will_return(__wrap_cpuinfo_init, data->cpu);
        if (data->interface == PQOS_INTER_MSR) {
                will_return(__wrap_hw_cap_mon_discover, data->cap_mon);
                will_return(__wrap_hw_cap_l3ca_discover, &(data->cap_l3ca));
                will_return(__wrap_hw_cap_l2ca_discover, &(data->cap_l2ca));
                will_return(__wrap_hw_cap_mba_discover, &(data->cap_mba));
        }
#ifdef __linux__
        else if (data->interface == PQOS_INTER_OS ||
                 data->interface == PQOS_INTER_OS_RESCTRL_MON) {
                will_return(__wrap_os_cap_mon_discover, data->cap_mon);
                will_return(__wrap_os_cap_l3ca_discover, &(data->cap_l3ca));
                will_return(__wrap_os_cap_l2ca_discover, &(data->cap_l2ca));
                will_return(__wrap_os_cap_mba_discover, &(data->cap_mba));
        };
#endif
        expect_string(__wrap_open, path, LOCKFILE);
        expect_value(__wrap_open, oflags, O_WRONLY | O_CREAT);
        expect_value(__wrap_open, mode, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        expect_function_call(__wrap_open);
        wrap_lockf(F_LOCK);
#ifdef __linux__
        if (data->interface != PQOS_INTER_MSR)
                expect_function_call(__wrap_os_cap_init);
#endif
        if (data->interface == PQOS_INTER_MSR)
                expect_function_call(__wrap_hw_cap_mon_discover);
#ifdef __linux__
        else if (data->interface == PQOS_INTER_OS ||
                 data->interface == PQOS_INTER_OS_RESCTRL_MON)
                expect_function_call(__wrap_os_cap_mon_discover);
#endif
        wrap_lockf(F_ULOCK);
        ret = pqos_init(&cfg);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

/* ======== pqos_fini ======== */

static void
test_pqos_fini(void **state __attribute__((unused)))
{
        int ret;

        wrap_lock_and_unlock();
        expect_function_call(__wrap_close);
        expect_value(__wrap_close, fildes, LOCKFILENO);
        ret = pqos_fini();
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

/* ======== pqos_inter_get ======== */

static void
test_pqos_inter_get_param(void **state __attribute__((unused)))
{
        int ret;

        ret = pqos_inter_get(NULL);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
}

static void
test_pqos_inter_get_before_init(void **state __attribute__((unused)))
{
        int ret;
        enum pqos_interface interface;

        ret = pqos_inter_get(&interface);
        assert_int_not_equal(ret, PQOS_RETVAL_OK);
}

static void
test_pqos_inter_get_after_init(void **state __attribute__((unused)))
{
        int ret;
        enum pqos_interface interface;

        wrap_lock_and_unlock();
        ret = pqos_inter_get(&interface);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

/* ======== pqos_cap_get ======== */

static void
test_pqos_cap_get_before_init(void **state __attribute__((unused)))
{
        int ret;
        const struct pqos_cap *p_cap;
        const struct pqos_cpuinfo *p_cpu;

        ret = pqos_cap_get(NULL, NULL);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
        ret = pqos_cap_get(&p_cap, &p_cpu);
        assert_int_not_equal(ret, PQOS_RETVAL_OK);
        ret = pqos_cap_get(&p_cap, NULL);
        assert_int_not_equal(ret, PQOS_RETVAL_OK);
        ret = pqos_cap_get(NULL, &p_cpu);
        assert_int_not_equal(ret, PQOS_RETVAL_OK);
}

static void
test_pqos_cap_get_after_init(void **state __attribute__((unused)))
{
        int ret;
        const struct pqos_cap *p_cap;
        const struct pqos_cpuinfo *p_cpu;

        ret = pqos_cap_get(NULL, NULL);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
        wrap_lock_and_unlock();
        ret = pqos_cap_get(&p_cap, &p_cpu);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        wrap_lock_and_unlock();
        ret = pqos_cap_get(&p_cap, NULL);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        wrap_lock_and_unlock();
        ret = pqos_cap_get(NULL, &p_cpu);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

/* ======== _pqos_cap_l3cdp_change ======== */

static void
test__pqos_cap_l3cdp_change_msr(void **state __attribute__((unused)))
{
        struct test_data *data;

        data = (struct test_data *)*state;

        if (data->interface == PQOS_INTER_MSR) {
                will_return(__wrap_hw_cap_l3ca_discover, &(data->cap_l3ca));
                _pqos_cap_l3cdp_change(PQOS_REQUIRE_CDP_OFF);
                will_return(__wrap_hw_cap_l3ca_discover, &(data->cap_l3ca));
                _pqos_cap_l3cdp_change(PQOS_REQUIRE_CDP_ON);
                will_return(__wrap_hw_cap_l3ca_discover, &(data->cap_l3ca));
                _pqos_cap_l3cdp_change(PQOS_REQUIRE_CDP_ANY);
                will_return(__wrap_hw_cap_l3ca_discover, &(data->cap_l3ca));
                _pqos_cap_l3cdp_change(-1);
        }
}

#ifdef __linux__
static void
test__pqos_cap_l3cdp_change_os(void **state __attribute__((unused)))
{
        struct test_data *data;

        data = (struct test_data *)*state;

        if (data->interface == PQOS_INTER_OS) {
                will_return(__wrap_os_cap_l3ca_discover, &(data->cap_l3ca));
                _pqos_cap_l3cdp_change(PQOS_REQUIRE_CDP_OFF);
                will_return(__wrap_os_cap_l3ca_discover, &(data->cap_l3ca));
                _pqos_cap_l3cdp_change(PQOS_REQUIRE_CDP_ON);
                will_return(__wrap_os_cap_l3ca_discover, &(data->cap_l3ca));
                _pqos_cap_l3cdp_change(PQOS_REQUIRE_CDP_ANY);
                will_return(__wrap_os_cap_l3ca_discover, &(data->cap_l3ca));
                _pqos_cap_l3cdp_change(-1);
        }
}

static void
test__pqos_cap_l3cdp_change_os_resctrl_mon(void **state __attribute__((unused)))
{
        struct test_data *data;

        data = (struct test_data *)*state;

        if (data->interface == PQOS_INTER_OS_RESCTRL_MON) {
                will_return(__wrap_os_cap_l3ca_discover, &(data->cap_l3ca));
                _pqos_cap_l3cdp_change(PQOS_REQUIRE_CDP_OFF);
                will_return(__wrap_os_cap_l3ca_discover, &(data->cap_l3ca));
                _pqos_cap_l3cdp_change(PQOS_REQUIRE_CDP_ON);
                will_return(__wrap_os_cap_l3ca_discover, &(data->cap_l3ca));
                _pqos_cap_l3cdp_change(PQOS_REQUIRE_CDP_ANY);
                will_return(__wrap_os_cap_l3ca_discover, &(data->cap_l3ca));
                _pqos_cap_l3cdp_change(-1);
        }
}

#endif
/* ======== _pqos_cap_l2cdp_change ======== */

static void
test__pqos_cap_l2cdp_change_msr(void **state __attribute__((unused)))
{
        struct test_data *data;

        data = (struct test_data *)*state;

        if (data->interface == PQOS_INTER_MSR) {
                will_return(__wrap_hw_cap_l2ca_discover, &(data->cap_l2ca));
                _pqos_cap_l2cdp_change(PQOS_REQUIRE_CDP_OFF);
                will_return(__wrap_hw_cap_l2ca_discover, &(data->cap_l2ca));
                _pqos_cap_l2cdp_change(PQOS_REQUIRE_CDP_ON);
                will_return(__wrap_hw_cap_l2ca_discover, &(data->cap_l2ca));
                _pqos_cap_l2cdp_change(PQOS_REQUIRE_CDP_ANY);
                will_return(__wrap_hw_cap_l2ca_discover, &(data->cap_l2ca));
                _pqos_cap_l2cdp_change(-1);
        }
}

#ifdef __linux__
static void
test__pqos_cap_l2cdp_change_os(void **state __attribute__((unused)))
{
        struct test_data *data;

        data = (struct test_data *)*state;

        if (data->interface == PQOS_INTER_OS) {
                will_return(__wrap_os_cap_l2ca_discover, &(data->cap_l2ca));
                _pqos_cap_l2cdp_change(PQOS_REQUIRE_CDP_OFF);
                will_return(__wrap_os_cap_l2ca_discover, &(data->cap_l2ca));
                _pqos_cap_l2cdp_change(PQOS_REQUIRE_CDP_ON);
                will_return(__wrap_os_cap_l2ca_discover, &(data->cap_l2ca));
                _pqos_cap_l2cdp_change(PQOS_REQUIRE_CDP_ANY);
                will_return(__wrap_os_cap_l2ca_discover, &(data->cap_l2ca));
                _pqos_cap_l2cdp_change(-1);
        }
}

static void
test__pqos_cap_l2cdp_change_os_resctrl_mon(void **state __attribute__((unused)))
{
        struct test_data *data;

        data = (struct test_data *)*state;

        if (data->interface == PQOS_INTER_OS_RESCTRL_MON) {
                will_return(__wrap_os_cap_l2ca_discover, &(data->cap_l2ca));
                _pqos_cap_l2cdp_change(PQOS_REQUIRE_CDP_OFF);
                will_return(__wrap_os_cap_l2ca_discover, &(data->cap_l2ca));
                _pqos_cap_l2cdp_change(PQOS_REQUIRE_CDP_ON);
                will_return(__wrap_os_cap_l2ca_discover, &(data->cap_l2ca));
                _pqos_cap_l2cdp_change(PQOS_REQUIRE_CDP_ANY);
                will_return(__wrap_os_cap_l2ca_discover, &(data->cap_l2ca));
                _pqos_cap_l2cdp_change(-1);
        }
}
#endif

/* ======== _pqos_cap_mba_change ======== */

static void
test__pqos_cap_mba_change(void **state __attribute__((unused)))
{
        _pqos_cap_mba_change(-1);
        _pqos_cap_mba_change(PQOS_MBA_ANY);
        _pqos_cap_mba_change(PQOS_MBA_DEFAULT);
        _pqos_cap_mba_change(PQOS_MBA_CTRL);
}

int
main(void)
{
        int result = 0;

        const struct CMUnitTest tests_cap_param[] = {
            cmocka_unit_test(test_pqos_inter_get_param),
            cmocka_unit_test(test__pqos_cap_get_type_param),
            cmocka_unit_test(test_pqos_init_param),
        };

        const struct CMUnitTest tests_cap_init[] = {
            cmocka_unit_test(test__pqos_check_init_before_init),
            cmocka_unit_test(test__pqos_get_cap_before_init),
            cmocka_unit_test(test__pqos_get_cpu_before_init),
            cmocka_unit_test(test_pqos_inter_get_before_init),
            cmocka_unit_test(test_pqos_cap_get_before_init),
            cmocka_unit_test(test_pqos_init),
            cmocka_unit_test(test_pqos_inter_get_after_init),
            cmocka_unit_test(test_pqos_cap_get_after_init),
            cmocka_unit_test(test__pqos_get_inter),
            cmocka_unit_test(test__pqos_get_cap_after_init),
            cmocka_unit_test(test__pqos_get_cpu_after_init),
            cmocka_unit_test(test__pqos_check_init_after_init),
            cmocka_unit_test(test__pqos_cap_l3cdp_change_msr),
            cmocka_unit_test(test__pqos_cap_l2cdp_change_msr),
#ifdef __linux__
            cmocka_unit_test(test__pqos_cap_l3cdp_change_os),
            cmocka_unit_test(test__pqos_cap_l2cdp_change_os),
            cmocka_unit_test(test__pqos_cap_l3cdp_change_os_resctrl_mon),
            cmocka_unit_test(test__pqos_cap_l2cdp_change_os_resctrl_mon),
#endif
            cmocka_unit_test(test__pqos_cap_mba_change),
            cmocka_unit_test(test_pqos_fini),
        };

        result += cmocka_run_group_tests(tests_cap_param, NULL, NULL);
        result += cmocka_run_group_tests(tests_cap_init, setup_cap_init_msr,
                                         test_fini);
#ifdef __linux__
        result += cmocka_run_group_tests(tests_cap_init, setup_cap_init_os,
                                         test_fini);
        result += cmocka_run_group_tests(
            tests_cap_init, setup_cap_init_os_resctrl_mon, test_fini);
#endif
        return result;
}
