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

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>

/* clang-format off */
#include <cmocka.h>
#include "cap.c"
/* clang-format on */
#include "test_cap.h"

/* ======== helpers ======= */

static int check_log_printf = 0;
static void
enable_check_log_printf(void)
{
        check_log_printf = 1;
};

static void
disable_check_log_printf(void)
{
        check_log_printf = 0;
};

static int
check_log_printf_enabled(void)
{
        return check_log_printf;
}

static int malloc_force_fail = 0;
static void
enable_malloc_force_fail(void)
{
        malloc_force_fail = 1;
};

/*
 * force call to malloc to fail
 * starting at n-th call
 */
static void
enable_malloc_force_fail_n(int n)
{
        malloc_force_fail = n;
};

static void
disable_malloc_force_fail(void)
{
        malloc_force_fail = 0;
};

static int
check_malloc_force_fail(void)
{
        if (malloc_force_fail > 1) {
                malloc_force_fail--;
                return 0;
        };
        return malloc_force_fail;
}

/* ======== mock ======== */

void *__real_malloc(size_t size);
void *
__wrap_malloc(size_t size)
{
        return check_malloc_force_fail() ? NULL : __real_malloc(size);
}

const char *RDT_IFACE = "RDT_IFACE";

char *__real_getenv(const char *name);
char *
__wrap_getenv(const char *name)
{
        assert_non_null(name);
        if (strcmp(name, RDT_IFACE))
                return __real_getenv(name);
        function_called();
        return mock_ptr_type(char *);
}

int
__wrap_pthread_mutex_init(pthread_mutex_t *restrict mutex,
                          const pthread_mutexattr_t *restrict attr
                          __attribute__((unused)))
{
        assert_non_null(mutex);
        function_called();
        return mock();
}
int
__wrap_pthread_mutex_destroy(pthread_mutex_t *mutex)
{
        assert_non_null(mutex);
        function_called();
        return mock();
};

int __real_open(const char *path, int oflags, int mode);
int
__wrap_open(const char *path, int oflags, int mode)
{
        if (strcmp(path, LOCKFILE))
                return __real_open(path, oflags, mode);

        function_called();
        check_expected(path);
        check_expected(oflags);
        check_expected(mode);

        return mock();
}

int __real_close(int fildes);
int
__wrap_close(int fildes)
{
        if (fildes != LOCKFILENO)
                return __real_close(fildes);

        function_called();
        check_expected(fildes);

        return mock();
}

int
log_init(int fd_log __attribute__((unused)),
         void (*callback_log)(void *, const size_t, const char *)
             __attribute__((unused)),
         void *context_log __attribute__((unused)),
         int verbosity __attribute__((unused)))
{
        function_called();

        return 0;
}

int
log_fini(void)
{
        function_called();

        return 0;
}

void
log_printf(int type __attribute__((unused)),
           const char *str __attribute__((unused)),
           ...)
{
        if (check_log_printf_enabled())
                function_called();
}

#define MAKE_CAP_DIS_WRAPPER(name, str)                                        \
        int name(struct str *cap,                                              \
                 const struct pqos_cpuinfo *cpu __attribute__((unused)))       \
        {                                                                      \
                int ret = mock();                                              \
                size_t sz = sizeof(*cap);                                      \
                                                                               \
                function_called();                                             \
                if (ret == PQOS_RETVAL_OK) {                                   \
                        assert_non_null(cap);                                  \
                        if (cap)                                               \
                                memset(cap, 0, sz);                            \
                }                                                              \
                return ret;                                                    \
        }

MAKE_CAP_DIS_WRAPPER(__wrap_hw_cap_l3ca_discover, pqos_cap_l3ca)
MAKE_CAP_DIS_WRAPPER(__wrap_os_cap_l3ca_discover, pqos_cap_l3ca)
MAKE_CAP_DIS_WRAPPER(__wrap_hw_cap_l2ca_discover, pqos_cap_l2ca)
MAKE_CAP_DIS_WRAPPER(__wrap_os_cap_l2ca_discover, pqos_cap_l2ca)
MAKE_CAP_DIS_WRAPPER(__wrap_hw_cap_mba_discover, pqos_cap_mba)
MAKE_CAP_DIS_WRAPPER(__wrap_amd_cap_mba_discover, pqos_cap_mba)
MAKE_CAP_DIS_WRAPPER(__wrap_os_cap_mba_discover, pqos_cap_mba)

#define MAKE_CAP_MON_WRAPPER(name)                                             \
        int name(struct pqos_cap_mon **r_mon,                                  \
                 const struct pqos_cpuinfo *cpu __attribute__((unused)))       \
        {                                                                      \
                int ret = mock();                                              \
                size_t sz = sizeof(struct pqos_cap_mon);                       \
                                                                               \
                function_called();                                             \
                if (ret == PQOS_RETVAL_OK) {                                   \
                        assert_non_null(r_mon);                                \
                        if (r_mon) {                                           \
                                *r_mon = (struct pqos_cap_mon *)malloc(sz);    \
                                assert_non_null(*r_mon);                       \
                                if (*r_mon)                                    \
                                        memset(*r_mon, 0, sz);                 \
                        }                                                      \
                }                                                              \
                return ret;                                                    \
        }

MAKE_CAP_MON_WRAPPER(__wrap_hw_cap_mon_discover)
MAKE_CAP_MON_WRAPPER(__wrap_os_cap_mon_discover)

int
__wrap_os_cap_get_mba_ctrl(const struct pqos_cap *cap,
                           const struct pqos_cpuinfo *cpu,
                           int *supported __attribute__((unused)),
                           int *enabled __attribute__((unused)))
{
        function_called();
        assert_non_null(cap);
        assert_non_null(cpu);
        assert_non_null(supported);
        assert_non_null(enabled);

        return mock();
}

int
pqos_cap_get_type(const struct pqos_cap *cap __attribute__((unused)),
                  const enum pqos_cap_type type __attribute__((unused)),
                  const struct pqos_capability **cap_item
                  __attribute__((unused)))
{
        function_called();

        return 0;
}

int
resctrl_alloc_get_num_closids(unsigned *num_closids __attribute__((unused)))
{
        function_called();

        return 0;
}

int
resctrl_is_supported(void)
{
        function_called();

        return mock();
}

int
cpuinfo_init(enum pqos_interface interface __attribute__((unused)),
             const struct pqos_cpuinfo **topology __attribute__((unused)))
{
        function_called();

        return 0;
}

int
cpuinfo_fini(void)
{
        function_called();

        return 0;
}

int
machine_init(const unsigned max_core_id __attribute__((unused)))
{
        function_called();

        return 0;
}

int
machine_fini(void)
{
        function_called();

        return 0;
}

int
os_cap_init(const enum pqos_interface inter __attribute__((unused)))
{
        function_called();

        return 0;
}

int
_pqos_utils_init(int interface __attribute__((unused)))
{
        function_called();

        return PQOS_RETVAL_OK;
}

int
api_init(int interface __attribute__((unused)),
         enum pqos_vendor vendor __attribute__((unused)))
{
        function_called();

        return 0;
}

int
pqos_alloc_init(const struct pqos_cpuinfo *cpu __attribute__((unused)),
                const struct pqos_cap *cap __attribute__((unused)),
                const struct pqos_config *cfg __attribute__((unused)))
{
        function_called();

        return 0;
}

int
pqos_alloc_fini(void)
{
        function_called();

        return 0;
}

int
pqos_mon_init(const struct pqos_cpuinfo *cpu __attribute__((unused)),
              const struct pqos_cap *cap __attribute__((unused)),
              const struct pqos_config *cfg __attribute__((unused)))
{
        function_called();

        return 0;
}

int
pqos_mon_fini(void)
{
        function_called();

        return 0;
}

/* ======== tests ======== */

static void
test_interface_to_string(void **state __attribute__((unused)))
{
        assert_string_equal(interface_to_string(PQOS_INTER_MSR), "MSR");
        assert_string_equal(interface_to_string(PQOS_INTER_OS), "OS");
        assert_string_equal(interface_to_string(PQOS_INTER_OS_RESCTRL_MON),
                            "OS_RESCTRL_MON");
        assert_string_equal(interface_to_string(PQOS_INTER_AUTO), "AUTO");
        assert_string_equal(interface_to_string(-1), "Unknown");
}

static void
test_discover_interface_param(void **state __attribute__((unused)))
{
        enum pqos_interface interface;
        int i;

        enable_check_log_printf();
        for (i = -1; i <= PQOS_INTER_AUTO; i++) {
                interface = i;
                expect_function_call(log_printf);
                assert_int_equal(discover_interface(interface, NULL),
                                 PQOS_RETVAL_PARAM);
        };

#ifndef __linux
        expect_function_call(log_printf);
        assert_int_equal(discover_interface(PQOS_INTER_OS, &interface),
                         PQOS_RETVAL_PARAM);
        expect_function_call(log_printf);
        assert_int_equal(
            discover_interface(PQOS_INTER_OS_RESCTRL_MON, &interface),
            PQOS_RETVAL_PARAM);
#endif
        interface = -1;
        expect_function_call(log_printf);
        assert_int_equal(discover_interface(-1, &interface), PQOS_RETVAL_PARAM);

        interface = -1;
        expect_function_call(log_printf);
        assert_int_equal(discover_interface(PQOS_INTER_MSR - 1, &interface),
                         PQOS_RETVAL_PARAM);

        interface = -1;
        expect_function_call(log_printf);
        assert_int_equal(discover_interface(PQOS_INTER_AUTO + 1, &interface),
                         PQOS_RETVAL_PARAM);
}

static void
test_discover_interface_msr(void **state __attribute__((unused)))
{
        int ret;
        enum pqos_interface interface;

        disable_check_log_printf();

        interface = -1;
        expect_function_call(__wrap_getenv);
        will_return(__wrap_getenv, NULL);
        ret = discover_interface(PQOS_INTER_MSR, &interface);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(interface, PQOS_INTER_MSR);
}

static void
test_discover_interface_env_msr(void **state __attribute__((unused)))
{
        int ret;
        enum pqos_interface interface;
        enum pqos_interface bad = -1;

        disable_check_log_printf();

        interface = bad;
        expect_function_call(__wrap_getenv);
        will_return(__wrap_getenv, "MSR");
        ret = discover_interface(PQOS_INTER_MSR, &interface);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(interface, PQOS_INTER_MSR);

        interface = bad;
        expect_function_call(__wrap_getenv);
        will_return(__wrap_getenv, "MSR");
        ret = discover_interface(PQOS_INTER_AUTO, &interface);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(interface, PQOS_INTER_MSR);

        interface = bad;
        expect_function_call(__wrap_getenv);
        will_return(__wrap_getenv, "MSR");
        ret = discover_interface(PQOS_INTER_OS, &interface);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
        assert_int_equal(interface, bad);

        interface = bad;
        expect_function_call(__wrap_getenv);
        will_return(__wrap_getenv, "MSR");
        ret = discover_interface(PQOS_INTER_OS_RESCTRL_MON, &interface);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
        assert_int_equal(interface, bad);
}

static void
test_discover_interface_env_os(void **state __attribute__((unused)))
{
        int ret;
        enum pqos_interface interface;
        enum pqos_interface bad = -1;

        disable_check_log_printf();

        interface = bad;
        expect_function_call(__wrap_getenv);
        will_return(__wrap_getenv, "OS");
        ret = discover_interface(PQOS_INTER_OS, &interface);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(interface, PQOS_INTER_OS);

        interface = bad;
        expect_function_call(__wrap_getenv);
        will_return(__wrap_getenv, "OS");
        ret = discover_interface(PQOS_INTER_AUTO, &interface);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(interface, PQOS_INTER_OS);

        interface = bad;
        expect_function_call(__wrap_getenv);
        will_return(__wrap_getenv, "OS");
        ret = discover_interface(PQOS_INTER_MSR, &interface);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
        assert_int_equal(interface, bad);

        interface = bad;
        expect_function_call(__wrap_getenv);
        will_return(__wrap_getenv, "OS");
        ret = discover_interface(PQOS_INTER_OS_RESCTRL_MON, &interface);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
        assert_int_equal(interface, bad);
}

static void
test_discover_interface_env_unsupported(void **state __attribute__((unused)))
{
        int ret;
        enum pqos_interface interface;
        enum pqos_interface bad = -1;

        disable_check_log_printf();

        interface = bad;
        expect_function_call(__wrap_getenv);
        will_return(__wrap_getenv, "UNSUPPORTED");
        ret = discover_interface(PQOS_INTER_OS, &interface);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
        assert_int_equal(interface, bad);

        interface = bad;
        expect_function_call(__wrap_getenv);
        will_return(__wrap_getenv, "UNSUPPORTED");
        ret = discover_interface(PQOS_INTER_AUTO, &interface);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
        assert_int_equal(interface, bad);

        interface = bad;
        expect_function_call(__wrap_getenv);
        will_return(__wrap_getenv, "UNSUPPORTED");
        ret = discover_interface(PQOS_INTER_MSR, &interface);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
        assert_int_equal(interface, bad);

        interface = bad;
        expect_function_call(__wrap_getenv);
        will_return(__wrap_getenv, "UNSUPPORTED");
        ret = discover_interface(PQOS_INTER_OS_RESCTRL_MON, &interface);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
        assert_int_equal(interface, bad);
}

#ifdef __linux__

static void
test_discover_interface_os(void **state __attribute__((unused)))
{
        int ret;
        enum pqos_interface interface;

        disable_check_log_printf();

        interface = -1;
        expect_function_call(__wrap_getenv);
        will_return(__wrap_getenv, NULL);
        ret = discover_interface(PQOS_INTER_OS, &interface);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(interface, PQOS_INTER_OS);
}

static void
test_discover_interface_os_resctrl_mon(void **state __attribute__((unused)))
{
        int ret;
        enum pqos_interface interface;

        disable_check_log_printf();

        interface = -1;
        expect_function_call(__wrap_getenv);
        will_return(__wrap_getenv, NULL);
        ret = discover_interface(PQOS_INTER_OS_RESCTRL_MON, &interface);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(interface, PQOS_INTER_OS_RESCTRL_MON);
}

static void
test_discover_interface_auto_linux(void **state __attribute__((unused)))
{
        int ret;
        enum pqos_interface interface;

        disable_check_log_printf();

        interface = -1;
        expect_function_call(__wrap_getenv);
        will_return(__wrap_getenv, NULL);
        expect_function_call(resctrl_is_supported);
        will_return(resctrl_is_supported, PQOS_RETVAL_OK);
        ret = discover_interface(PQOS_INTER_AUTO, &interface);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(interface, PQOS_INTER_OS);

        interface = -1;
        expect_function_call(__wrap_getenv);
        will_return(__wrap_getenv, NULL);
        expect_function_call(resctrl_is_supported);
        will_return(resctrl_is_supported, PQOS_RETVAL_ERROR);
        ret = discover_interface(PQOS_INTER_AUTO, &interface);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(interface, PQOS_INTER_MSR);
}

#else

static void
test_discover_interface_auto(void **state __attribute__((unused)))
{
        int ret;
        enum pqos_interface interface;

        disable_check_log_printf();

        interface = -1;
        expect_function_call(__wrap_getenv);
        will_return(__wrap_getenv, NULL);
        ret = discover_interface(PQOS_INTER_AUTO, &interface);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(interface, PQOS_INTER_MSR);
}

#endif

static void
test_cap_l3ca_discover_param(void **state __attribute__((unused)))
{
        int i;
        enum pqos_interface interface;
        struct pqos_cap_l3ca *r_cap;
        const struct pqos_cpuinfo cpu;

        for (i = -1; i <= PQOS_INTER_AUTO; i++) {
                interface = i;
                assert_int_equal(cap_l3ca_discover(NULL, NULL, interface),
                                 PQOS_RETVAL_PARAM);
                assert_int_equal(cap_l3ca_discover(&r_cap, NULL, interface),
                                 PQOS_RETVAL_PARAM);
                assert_int_equal(cap_l3ca_discover(NULL, &cpu, interface),
                                 PQOS_RETVAL_PARAM);
        };
        assert_int_not_equal(cap_l3ca_discover(&r_cap, &cpu, -1),
                             PQOS_RETVAL_OK);
        assert_int_not_equal(cap_l3ca_discover(&r_cap, &cpu, PQOS_INTER_AUTO),
                             PQOS_RETVAL_OK);
}

static void
test_cap_l3ca_discover(void **state __attribute__((unused)))
{
        struct pqos_cap_l3ca *r_cap = NULL;
        const struct pqos_cpuinfo cpu;

        expect_function_call(__wrap_hw_cap_l3ca_discover);
        r_cap = NULL;
        will_return(__wrap_hw_cap_l3ca_discover, PQOS_RETVAL_OK);
        assert_int_equal(cap_l3ca_discover(&r_cap, &cpu, PQOS_INTER_MSR),
                         PQOS_RETVAL_OK);
        assert_non_null(r_cap);
        free(r_cap);
        expect_function_call(__wrap_hw_cap_l3ca_discover);
        r_cap = NULL;
        will_return(__wrap_hw_cap_l3ca_discover, PQOS_RETVAL_RESOURCE);
        assert_int_not_equal(cap_l3ca_discover(&r_cap, &cpu, PQOS_INTER_MSR),
                             PQOS_RETVAL_OK);
#ifdef __linux__
        expect_function_call(__wrap_os_cap_l3ca_discover);
        r_cap = NULL;
        will_return(__wrap_os_cap_l3ca_discover, PQOS_RETVAL_OK);
        assert_int_equal(cap_l3ca_discover(&r_cap, &cpu, PQOS_INTER_OS),
                         PQOS_RETVAL_OK);
        assert_non_null(r_cap);
        free(r_cap);
        expect_function_call(__wrap_os_cap_l3ca_discover);
        r_cap = NULL;
        will_return(__wrap_os_cap_l3ca_discover, PQOS_RETVAL_RESOURCE);
        assert_int_not_equal(cap_l3ca_discover(&r_cap, &cpu, PQOS_INTER_OS),
                             PQOS_RETVAL_OK);
        expect_function_call(__wrap_os_cap_l3ca_discover);
        r_cap = NULL;
        will_return(__wrap_os_cap_l3ca_discover, PQOS_RETVAL_OK);
        assert_int_equal(
            cap_l3ca_discover(&r_cap, &cpu, PQOS_INTER_OS_RESCTRL_MON),
            PQOS_RETVAL_OK);
        assert_non_null(r_cap);
        free(r_cap);
        expect_function_call(__wrap_os_cap_l3ca_discover);
        r_cap = NULL;
        will_return(__wrap_os_cap_l3ca_discover, PQOS_RETVAL_RESOURCE);
        assert_int_not_equal(
            cap_l3ca_discover(&r_cap, &cpu, PQOS_INTER_OS_RESCTRL_MON),
            PQOS_RETVAL_OK);
#endif
};

static void
test_cap_l2ca_discover_param(void **state __attribute__((unused)))
{
        int i;
        enum pqos_interface interface;
        struct pqos_cap_l2ca *r_cap;
        const struct pqos_cpuinfo cpu;

        for (i = -1; i <= PQOS_INTER_AUTO; i++) {
                interface = i;
                assert_int_equal(cap_l2ca_discover(NULL, NULL, interface),
                                 PQOS_RETVAL_PARAM);
                assert_int_equal(cap_l2ca_discover(&r_cap, NULL, interface),
                                 PQOS_RETVAL_PARAM);
                assert_int_equal(cap_l2ca_discover(NULL, &cpu, interface),
                                 PQOS_RETVAL_PARAM);
        };
        assert_int_not_equal(cap_l2ca_discover(&r_cap, &cpu, -1),
                             PQOS_RETVAL_OK);
        assert_int_not_equal(cap_l2ca_discover(&r_cap, &cpu, PQOS_INTER_AUTO),
                             PQOS_RETVAL_OK);
}

static void
test_cap_l2ca_discover(void **state __attribute__((unused)))
{
        struct pqos_cap_l2ca *r_cap;
        const struct pqos_cpuinfo cpu;

        expect_function_call(__wrap_hw_cap_l2ca_discover);
        r_cap = NULL;
        will_return(__wrap_hw_cap_l2ca_discover, PQOS_RETVAL_OK);
        assert_int_equal(cap_l2ca_discover(&r_cap, &cpu, PQOS_INTER_MSR),
                         PQOS_RETVAL_OK);
        assert_non_null(r_cap);
        free(r_cap);
        expect_function_call(__wrap_hw_cap_l2ca_discover);
        r_cap = NULL;
        will_return(__wrap_hw_cap_l2ca_discover, PQOS_RETVAL_RESOURCE);
        assert_int_not_equal(cap_l2ca_discover(&r_cap, &cpu, PQOS_INTER_MSR),
                             PQOS_RETVAL_OK);
#ifdef __linux__
        expect_function_call(__wrap_os_cap_l2ca_discover);
        r_cap = NULL;
        will_return(__wrap_os_cap_l2ca_discover, PQOS_RETVAL_OK);
        assert_int_equal(cap_l2ca_discover(&r_cap, &cpu, PQOS_INTER_OS),
                         PQOS_RETVAL_OK);
        assert_non_null(r_cap);
        free(r_cap);
        expect_function_call(__wrap_os_cap_l2ca_discover);
        r_cap = NULL;
        will_return(__wrap_os_cap_l2ca_discover, PQOS_RETVAL_RESOURCE);
        assert_int_not_equal(cap_l2ca_discover(&r_cap, &cpu, PQOS_INTER_OS),
                             PQOS_RETVAL_OK);
        expect_function_call(__wrap_os_cap_l2ca_discover);
        r_cap = NULL;
        will_return(__wrap_os_cap_l2ca_discover, PQOS_RETVAL_OK);
        assert_int_equal(
            cap_l2ca_discover(&r_cap, &cpu, PQOS_INTER_OS_RESCTRL_MON),
            PQOS_RETVAL_OK);
        assert_non_null(r_cap);
        free(r_cap);
        expect_function_call(__wrap_os_cap_l2ca_discover);
        r_cap = NULL;
        will_return(__wrap_os_cap_l2ca_discover, PQOS_RETVAL_RESOURCE);
        assert_int_not_equal(
            cap_l2ca_discover(&r_cap, &cpu, PQOS_INTER_OS_RESCTRL_MON),
            PQOS_RETVAL_OK);
#endif
}

static void
test_cap_mba_discover_param(void **state __attribute__((unused)))
{
        int i;
        enum pqos_interface interface;
        struct pqos_cap_mba *r_cap;
        const struct pqos_cpuinfo cpu;

        for (i = -1; i <= PQOS_INTER_AUTO; i++) {
                interface = i;
                assert_int_equal(cap_mba_discover(NULL, NULL, interface),
                                 PQOS_RETVAL_PARAM);
                assert_int_equal(cap_mba_discover(&r_cap, NULL, interface),
                                 PQOS_RETVAL_PARAM);
                assert_int_equal(cap_mba_discover(NULL, &cpu, interface),
                                 PQOS_RETVAL_PARAM);
        };
        assert_int_not_equal(cap_mba_discover(&r_cap, &cpu, -1),
                             PQOS_RETVAL_OK);
        assert_int_not_equal(cap_mba_discover(&r_cap, &cpu, PQOS_INTER_AUTO),
                             PQOS_RETVAL_OK);
}

static void
test_cap_mba_discover(void **state __attribute__((unused)))
{
        struct pqos_cap_mba *r_cap;
        struct pqos_cpuinfo cpu;

        cpu.vendor = PQOS_VENDOR_AMD;
        expect_function_call(__wrap_amd_cap_mba_discover);
        r_cap = NULL;
        will_return(__wrap_amd_cap_mba_discover, PQOS_RETVAL_OK);
        assert_int_equal(cap_mba_discover(&r_cap, &cpu, PQOS_INTER_MSR),
                         PQOS_RETVAL_OK);
        assert_non_null(r_cap);
        free(r_cap);
        expect_function_call(__wrap_amd_cap_mba_discover);
        r_cap = NULL;
        will_return(__wrap_amd_cap_mba_discover, PQOS_RETVAL_RESOURCE);
        assert_int_not_equal(cap_mba_discover(&r_cap, &cpu, PQOS_INTER_MSR),
                             PQOS_RETVAL_OK);
        cpu.vendor = PQOS_VENDOR_INTEL;
        expect_function_call(__wrap_hw_cap_mba_discover);
        r_cap = NULL;
        will_return(__wrap_hw_cap_mba_discover, PQOS_RETVAL_OK);
        assert_int_equal(cap_mba_discover(&r_cap, &cpu, PQOS_INTER_MSR),
                         PQOS_RETVAL_OK);
        assert_non_null(r_cap);
        free(r_cap);
        expect_function_call(__wrap_hw_cap_mba_discover);
        r_cap = NULL;
        will_return(__wrap_hw_cap_mba_discover, PQOS_RETVAL_RESOURCE);
        assert_int_not_equal(cap_mba_discover(&r_cap, &cpu, PQOS_INTER_MSR),
                             PQOS_RETVAL_OK);
#ifdef __linux__
        expect_function_call(__wrap_os_cap_mba_discover);
        r_cap = NULL;
        will_return(__wrap_os_cap_mba_discover, PQOS_RETVAL_OK);
        assert_int_equal(cap_mba_discover(&r_cap, &cpu, PQOS_INTER_OS),
                         PQOS_RETVAL_OK);
        assert_non_null(r_cap);
        free(r_cap);
        expect_function_call(__wrap_os_cap_mba_discover);
        r_cap = NULL;
        will_return(__wrap_os_cap_mba_discover, PQOS_RETVAL_RESOURCE);
        assert_int_not_equal(cap_mba_discover(&r_cap, &cpu, PQOS_INTER_OS),
                             PQOS_RETVAL_OK);
        expect_function_call(__wrap_os_cap_mba_discover);
        r_cap = NULL;
        will_return(__wrap_os_cap_mba_discover, PQOS_RETVAL_OK);
        assert_int_equal(
            cap_mba_discover(&r_cap, &cpu, PQOS_INTER_OS_RESCTRL_MON),
            PQOS_RETVAL_OK);
        assert_non_null(r_cap);
        free(r_cap);
        expect_function_call(__wrap_os_cap_mba_discover);
        r_cap = NULL;
        will_return(__wrap_os_cap_mba_discover, PQOS_RETVAL_RESOURCE);
        assert_int_not_equal(
            cap_mba_discover(&r_cap, &cpu, PQOS_INTER_OS_RESCTRL_MON),
            PQOS_RETVAL_OK);
#endif
}

static void
test_cap_xxx_discover_malloc_fail(void **state __attribute__((unused)))
{
        struct pqos_cap_l3ca *cap_l3;
        struct pqos_cap_l2ca *cap_l2;
        struct pqos_cap_mba *cap_mb;
        struct pqos_cpuinfo cpu;

        enable_malloc_force_fail();
        assert_int_equal(cap_l3ca_discover(&cap_l3, &cpu, PQOS_INTER_MSR),
                         PQOS_RETVAL_RESOURCE);
        assert_int_equal(cap_l2ca_discover(&cap_l2, &cpu, PQOS_INTER_MSR),
                         PQOS_RETVAL_RESOURCE);
        assert_int_equal(cap_mba_discover(&cap_mb, &cpu, PQOS_INTER_MSR),
                         PQOS_RETVAL_RESOURCE);
        disable_malloc_force_fail();
}

static void
test_discover_capabilities(void **state __attribute__((unused)))
{
        struct pqos_cap *p_cap;
        struct pqos_cpuinfo cpu;

        assert_int_equal(discover_capabilities(NULL, NULL, -1),
                         PQOS_RETVAL_PARAM);
        assert_int_equal(discover_capabilities(&p_cap, NULL, -1),
                         PQOS_RETVAL_PARAM);
        assert_int_equal(discover_capabilities(NULL, &cpu, -1),
                         PQOS_RETVAL_PARAM);

        assert_int_equal(discover_capabilities(&p_cap, &cpu, -1),
                         PQOS_RETVAL_ERROR);
        assert_int_equal(discover_capabilities(&p_cap, &cpu, PQOS_INTER_AUTO),
                         PQOS_RETVAL_ERROR);

        expect_function_call(__wrap_hw_cap_mon_discover);
        will_return(__wrap_hw_cap_mon_discover, PQOS_RETVAL_RESOURCE);
        expect_function_call(__wrap_hw_cap_l3ca_discover);
        will_return(__wrap_hw_cap_l3ca_discover, PQOS_RETVAL_RESOURCE);
        expect_function_call(__wrap_hw_cap_l2ca_discover);
        will_return(__wrap_hw_cap_l2ca_discover, PQOS_RETVAL_RESOURCE);
        expect_function_call(__wrap_hw_cap_mba_discover);
        will_return(__wrap_hw_cap_mba_discover, PQOS_RETVAL_RESOURCE);
        assert_int_equal(discover_capabilities(&p_cap, &cpu, PQOS_INTER_MSR),
                         PQOS_RETVAL_ERROR);

        expect_function_call(__wrap_hw_cap_mon_discover);
        will_return(__wrap_hw_cap_mon_discover, PQOS_RETVAL_ERROR);
        assert_int_equal(discover_capabilities(&p_cap, &cpu, PQOS_INTER_MSR),
                         PQOS_RETVAL_ERROR);

        expect_function_call(__wrap_hw_cap_mon_discover);
        will_return(__wrap_hw_cap_mon_discover, PQOS_RETVAL_OK);
        expect_function_call(__wrap_hw_cap_l3ca_discover);
        will_return(__wrap_hw_cap_l3ca_discover, PQOS_RETVAL_ERROR);
        assert_int_equal(discover_capabilities(&p_cap, &cpu, PQOS_INTER_MSR),
                         PQOS_RETVAL_ERROR);

        expect_function_call(__wrap_hw_cap_mon_discover);
        will_return(__wrap_hw_cap_mon_discover, PQOS_RETVAL_OK);
        expect_function_call(__wrap_hw_cap_l3ca_discover);
        will_return(__wrap_hw_cap_l3ca_discover, PQOS_RETVAL_OK);
        expect_function_call(__wrap_hw_cap_l2ca_discover);
        will_return(__wrap_hw_cap_l2ca_discover, PQOS_RETVAL_ERROR);
        assert_int_equal(discover_capabilities(&p_cap, &cpu, PQOS_INTER_MSR),
                         PQOS_RETVAL_ERROR);

        expect_function_call(__wrap_hw_cap_mon_discover);
        will_return(__wrap_hw_cap_mon_discover, PQOS_RETVAL_OK);
        expect_function_call(__wrap_hw_cap_l3ca_discover);
        will_return(__wrap_hw_cap_l3ca_discover, PQOS_RETVAL_OK);
        expect_function_call(__wrap_hw_cap_l2ca_discover);
        will_return(__wrap_hw_cap_l2ca_discover, PQOS_RETVAL_OK);
        expect_function_call(__wrap_hw_cap_mba_discover);
        will_return(__wrap_hw_cap_mba_discover, PQOS_RETVAL_ERROR);
        assert_int_equal(discover_capabilities(&p_cap, &cpu, PQOS_INTER_MSR),
                         PQOS_RETVAL_ERROR);

        expect_function_call(__wrap_hw_cap_mon_discover);
        will_return(__wrap_hw_cap_mon_discover, PQOS_RETVAL_RESOURCE);
        expect_function_call(__wrap_hw_cap_l3ca_discover);
        will_return(__wrap_hw_cap_l3ca_discover, PQOS_RETVAL_OK);
        expect_function_call(__wrap_hw_cap_l2ca_discover);
        will_return(__wrap_hw_cap_l2ca_discover, PQOS_RETVAL_OK);
        expect_function_call(__wrap_hw_cap_mba_discover);
        will_return(__wrap_hw_cap_mba_discover, PQOS_RETVAL_RESOURCE);
        assert_int_equal(discover_capabilities(&p_cap, &cpu, PQOS_INTER_MSR),
                         PQOS_RETVAL_OK);

        expect_function_call(__wrap_hw_cap_mon_discover);
        will_return(__wrap_hw_cap_mon_discover, PQOS_RETVAL_OK);
        expect_function_call(__wrap_hw_cap_l3ca_discover);
        will_return(__wrap_hw_cap_l3ca_discover, PQOS_RETVAL_RESOURCE);
        expect_function_call(__wrap_hw_cap_l2ca_discover);
        will_return(__wrap_hw_cap_l2ca_discover, PQOS_RETVAL_RESOURCE);
        expect_function_call(__wrap_hw_cap_mba_discover);
        will_return(__wrap_hw_cap_mba_discover, PQOS_RETVAL_OK);
        assert_int_equal(discover_capabilities(&p_cap, &cpu, PQOS_INTER_MSR),
                         PQOS_RETVAL_OK);

        expect_function_call(__wrap_hw_cap_mon_discover);
        will_return(__wrap_hw_cap_mon_discover, PQOS_RETVAL_OK);
        expect_function_call(__wrap_hw_cap_l3ca_discover);
        will_return(__wrap_hw_cap_l3ca_discover, PQOS_RETVAL_OK);
        expect_function_call(__wrap_hw_cap_l2ca_discover);
        will_return(__wrap_hw_cap_l2ca_discover, PQOS_RETVAL_OK);
        expect_function_call(__wrap_hw_cap_mba_discover);
        will_return(__wrap_hw_cap_mba_discover, PQOS_RETVAL_OK);
        assert_int_equal(discover_capabilities(&p_cap, &cpu, PQOS_INTER_MSR),
                         PQOS_RETVAL_OK);
#ifdef __linux__
        expect_function_call(__wrap_os_cap_mon_discover);
        will_return(__wrap_os_cap_mon_discover, PQOS_RETVAL_OK);
        expect_function_call(__wrap_os_cap_l3ca_discover);
        will_return(__wrap_os_cap_l3ca_discover, PQOS_RETVAL_OK);
        expect_function_call(__wrap_os_cap_l2ca_discover);
        will_return(__wrap_os_cap_l2ca_discover, PQOS_RETVAL_OK);
        expect_function_call(__wrap_os_cap_mba_discover);
        will_return(__wrap_os_cap_mba_discover, PQOS_RETVAL_OK);
        expect_function_call(__wrap_os_cap_get_mba_ctrl);
        will_return(__wrap_os_cap_get_mba_ctrl, PQOS_RETVAL_ERROR);
        assert_int_equal(discover_capabilities(&p_cap, &cpu, PQOS_INTER_OS),
                         PQOS_RETVAL_ERROR);
        expect_function_call(__wrap_os_cap_mon_discover);
        will_return(__wrap_os_cap_mon_discover, PQOS_RETVAL_OK);
        expect_function_call(__wrap_os_cap_l3ca_discover);
        will_return(__wrap_os_cap_l3ca_discover, PQOS_RETVAL_OK);
        expect_function_call(__wrap_os_cap_l2ca_discover);
        will_return(__wrap_os_cap_l2ca_discover, PQOS_RETVAL_OK);
        expect_function_call(__wrap_os_cap_mba_discover);
        will_return(__wrap_os_cap_mba_discover, PQOS_RETVAL_OK);
        expect_function_call(__wrap_os_cap_get_mba_ctrl);
        will_return(__wrap_os_cap_get_mba_ctrl, PQOS_RETVAL_ERROR);
        assert_int_equal(
            discover_capabilities(&p_cap, &cpu, PQOS_INTER_OS_RESCTRL_MON),
            PQOS_RETVAL_ERROR);
#endif
}

static void
test_discover_capabilities_malloc_fail(void **state __attribute__((unused)))
{
        struct pqos_cap *p_cap;
        struct pqos_cpuinfo cpu;

        expect_function_call(__wrap_hw_cap_mon_discover);
        will_return(__wrap_hw_cap_mon_discover, PQOS_RETVAL_OK);
        expect_function_call(__wrap_hw_cap_l3ca_discover);
        will_return(__wrap_hw_cap_l3ca_discover, PQOS_RETVAL_OK);
        expect_function_call(__wrap_hw_cap_l2ca_discover);
        will_return(__wrap_hw_cap_l2ca_discover, PQOS_RETVAL_OK);
        expect_function_call(__wrap_hw_cap_mba_discover);
        will_return(__wrap_hw_cap_mba_discover, PQOS_RETVAL_OK);
        enable_malloc_force_fail_n(5);
        assert_int_equal(discover_capabilities(&p_cap, &cpu, PQOS_INTER_MSR),
                         PQOS_RETVAL_ERROR);
        disable_malloc_force_fail();
}

static void
test__pqos_api_init_exit(void **state __attribute__((unused)))
{
        expect_function_call(__wrap_open);
        expect_string(__wrap_open, path, LOCKFILE);
        expect_value(__wrap_open, oflags, O_WRONLY | O_CREAT);
        expect_value(__wrap_open, mode, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        will_return(__wrap_open, -1);
        assert_int_equal(_pqos_api_init(), -1);

        expect_function_call(__wrap_open);
        expect_string(__wrap_open, path, LOCKFILE);
        expect_value(__wrap_open, oflags, O_WRONLY | O_CREAT);
        expect_value(__wrap_open, mode, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        will_return(__wrap_open, LOCKFILENO);
        expect_function_call(__wrap_pthread_mutex_init);
        will_return(__wrap_pthread_mutex_init, -1);
        expect_function_call(__wrap_close);
        expect_value(__wrap_close, fildes, LOCKFILENO);
        will_return(__wrap_close, 0);
        assert_int_equal(_pqos_api_init(), -1);

        expect_function_call(__wrap_open);
        expect_string(__wrap_open, path, LOCKFILE);
        expect_value(__wrap_open, oflags, O_WRONLY | O_CREAT);
        expect_value(__wrap_open, mode, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        will_return(__wrap_open, LOCKFILENO);
        expect_function_call(__wrap_pthread_mutex_init);
        will_return(__wrap_pthread_mutex_init, 0);
        assert_int_equal(_pqos_api_init(), 0);

        assert_int_equal(_pqos_api_init(), -1);

        expect_function_call(__wrap_close);
        expect_value(__wrap_close, fildes, LOCKFILENO);
        will_return(__wrap_close, 0);
        expect_function_call(__wrap_pthread_mutex_destroy);
        will_return(__wrap_pthread_mutex_destroy, 0);
        assert_int_equal(_pqos_api_exit(), 0);

        expect_function_call(__wrap_open);
        expect_string(__wrap_open, path, LOCKFILE);
        expect_value(__wrap_open, oflags, O_WRONLY | O_CREAT);
        expect_value(__wrap_open, mode, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        will_return(__wrap_open, LOCKFILENO);
        expect_function_call(__wrap_pthread_mutex_init);
        will_return(__wrap_pthread_mutex_init, 0);
        assert_int_equal(_pqos_api_init(), 0);

        expect_function_call(__wrap_close);
        expect_value(__wrap_close, fildes, LOCKFILENO);
        will_return(__wrap_close, 0);
        expect_function_call(__wrap_pthread_mutex_destroy);
        will_return(__wrap_pthread_mutex_destroy, -1);
        assert_int_equal(_pqos_api_exit(), -1);

        expect_function_call(__wrap_open);
        expect_string(__wrap_open, path, LOCKFILE);
        expect_value(__wrap_open, oflags, O_WRONLY | O_CREAT);
        expect_value(__wrap_open, mode, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        will_return(__wrap_open, LOCKFILENO);
        expect_function_call(__wrap_pthread_mutex_init);
        will_return(__wrap_pthread_mutex_init, 0);
        assert_int_equal(_pqos_api_init(), 0);

        expect_function_call(__wrap_close);
        expect_value(__wrap_close, fildes, LOCKFILENO);
        will_return(__wrap_close, -1);
        expect_function_call(__wrap_pthread_mutex_destroy);
        will_return(__wrap_pthread_mutex_destroy, 0);
        assert_int_equal(_pqos_api_exit(), -1);
}

/* ======== main ======== */
int
main(void)
{
        int result = 0;

        const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_interface_to_string),
            cmocka_unit_test(test_discover_interface_param),
            cmocka_unit_test(test_discover_interface_msr),
            cmocka_unit_test(test_discover_interface_env_msr),
            cmocka_unit_test(test_discover_interface_env_os),
            cmocka_unit_test(test_discover_interface_env_unsupported),
#ifdef __linux__
            cmocka_unit_test(test_discover_interface_os),
            cmocka_unit_test(test_discover_interface_os_resctrl_mon),
            cmocka_unit_test(test_discover_interface_auto_linux),
#else
            cmocka_unit_test(test_discover_interface_auto),
#endif
            cmocka_unit_test(test_cap_l3ca_discover_param),
            cmocka_unit_test(test_cap_l3ca_discover),
            cmocka_unit_test(test_cap_l2ca_discover_param),
            cmocka_unit_test(test_cap_l2ca_discover),
            cmocka_unit_test(test_cap_mba_discover_param),
            cmocka_unit_test(test_cap_mba_discover),
            cmocka_unit_test(test_cap_xxx_discover_malloc_fail),
            cmocka_unit_test(test_discover_capabilities),
            cmocka_unit_test(test_discover_capabilities_malloc_fail),
            cmocka_unit_test(test__pqos_api_init_exit),
        };

        result += cmocka_run_group_tests(tests, NULL, NULL);

        return result;
}
