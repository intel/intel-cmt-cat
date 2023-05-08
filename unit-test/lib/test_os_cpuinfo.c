/*
 * BSD LICENSE
 *
 * Copyright(c) 2023 Intel Corporation. All rights reserved.
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

#include "log.h"
#include "mock_os_cpuinfo.h"
#include "pqos.h"
#include "test.h"

#include <dirent.h>

#define SYSTEM_CPU "/sys/devices/system/cpu"

#define CORE_OFFLINE 3
#define CORE_COUNT   6
struct pqos_coreinfo cores[CORE_COUNT] = {
    {.lcore = 0,
     .socket = 0,
     .l3_id = 0,
     .l2_id = 0,
     .l3cat_id = 0,
     .mba_id = 0,
#if PQOS_VERSION >= 50000
     .numa = 0
#endif
    },
    {.lcore = 1,
     .socket = 0,
     .l3_id = 0,
     .l2_id = 0,
     .l3cat_id = 0,
     .mba_id = 0,
#if PQOS_VERSION >= 50000
     .numa = 0
#endif
    },
    {.lcore = 2,
     .socket = 0,
     .l3_id = 0,
     .l2_id = 1,
     .l3cat_id = 0,
     .mba_id = 0,
#if PQOS_VERSION >= 50000
     .numa = 1
#endif
    },
    {.lcore = 3,
     .socket = 0,
     .l3_id = 0,
     .l2_id = 1,
     .l3cat_id = 0,
     .mba_id = 0,
#if PQOS_VERSION >= 50000
     .numa = 1
#endif
    },
    {.lcore = 4,
     .socket = 1,
     .l3_id = 1,
     .l2_id = 2,
     .l3cat_id = 0,
     .mba_id = 1,
#if PQOS_VERSION >= 50000
     .numa = 2
#endif
    },
    {.lcore = 5,
     .socket = 1,
     .l3_id = 1,
     .l2_id = 2,
     .l3cat_id = 0,
     .mba_id = 1,
#if PQOS_VERSION >= 50000
     .numa = 2
#endif
    },
};

/* ======== mock ======== */

int
__wrap_scandir(const char *restrict dirp,
               struct dirent ***restrict namelist,
               int (*filter)(const struct dirent *) __attribute__((unused)),
               int (*compar)(const struct dirent **, const struct dirent **)
                   __attribute__((unused)))
{
        int ret;
        unsigned i;
        struct dirent **list;

        check_expected(dirp);

        ret = mock_type(int);
        if (ret < 0)
                return ret;
        else if (ret == 0)
                *namelist = NULL;

        list = calloc(CORE_COUNT, sizeof(struct dirent *));

        for (i = 0; i < CORE_COUNT; ++i) {
                list[i] = malloc(sizeof(struct dirent) + 16);
                snprintf(list[i]->d_name, 16, "cpu%u", cores[i].lcore);
        }

        *namelist = list;
        return CORE_COUNT;
}

int
os_cpuinfo_cpu_online(unsigned lcore)
{
        assert_in_range(lcore, 0, CORE_COUNT - 1);

        return lcore != CORE_OFFLINE;
}

int
os_cpuinfo_cpu_socket(unsigned lcore, unsigned *socket)
{
        assert_in_range(lcore, 0, CORE_COUNT - 1);

        *socket = cores[lcore].socket;

        return mock_type(int);
}

int
os_cpuinfo_cpu_cache(unsigned lcore, unsigned *l3, unsigned *l2)
{
        assert_in_range(lcore, 0, CORE_COUNT - 1);

        *l3 = cores[lcore].l3_id;
        *l2 = cores[lcore].l2_id;

        return mock_type(int);
}

#if PQOS_VERSION >= 50000
int
os_cpuinfo_cpu_node(unsigned lcore, unsigned *node)
{
        assert_in_range(lcore, 0, CORE_COUNT - 1);

        *node = cores[lcore].numa;

        return mock_type(int);
}
#endif

/* ======== os_cpuinfo_topology ======== */

static void
test_os_cpuinfo_topology(void **state __attribute__((unused)))
{
        struct pqos_cpuinfo *cpuinfo;
        unsigned i;

        expect_string(__wrap_scandir, dirp, SYSTEM_CPU);
        will_return(__wrap_scandir, CORE_COUNT);

        will_return_always(os_cpuinfo_cpu_socket, PQOS_RETVAL_OK);
#if PQOS_VERSION >= 50000
        will_return_always(os_cpuinfo_cpu_node, PQOS_RETVAL_OK);
#endif
        will_return_always(os_cpuinfo_cpu_cache, PQOS_RETVAL_OK);

        cpuinfo = os_cpuinfo_topology();
        assert_non_null(cpuinfo);
        assert_int_equal(cpuinfo->num_cores, CORE_COUNT - 1);
        for (i = 0; i < cpuinfo->num_cores; ++i) {
                unsigned lcore = cpuinfo->cores[i].lcore;

                assert_in_range(lcore, 0, CORE_COUNT - 1);
                assert_int_equal(cpuinfo->cores[i].socket, cores[lcore].socket);
                assert_int_equal(cpuinfo->cores[i].l3_id, cores[lcore].l3_id);
                assert_int_equal(cpuinfo->cores[i].l2_id, cores[lcore].l2_id);
#if PQOS_VERSION >= 50000
                assert_int_equal(cpuinfo->cores[i].numa, cores[lcore].numa);
#endif
        }

        free(cpuinfo);
}

static void
test_os_cpuinfo_topology_error(void **state __attribute__((unused)))
{
        struct pqos_cpuinfo *cpuinfo;

        expect_string(__wrap_scandir, dirp, SYSTEM_CPU);
        will_return(__wrap_scandir, -1);
        cpuinfo = os_cpuinfo_topology();
        assert_null(cpuinfo);

        expect_string(__wrap_scandir, dirp, SYSTEM_CPU);
        will_return(__wrap_scandir, CORE_COUNT);
        will_return(os_cpuinfo_cpu_socket, PQOS_RETVAL_ERROR);
        cpuinfo = os_cpuinfo_topology();
        assert_null(cpuinfo);

        will_return_always(os_cpuinfo_cpu_socket, PQOS_RETVAL_OK);
#if PQOS_VERSION >= 50000
        expect_string(__wrap_scandir, dirp, SYSTEM_CPU);
        will_return(__wrap_scandir, CORE_COUNT);
        will_return(os_cpuinfo_cpu_node, PQOS_RETVAL_ERROR);
        cpuinfo = os_cpuinfo_topology();
        assert_null(cpuinfo);
#endif

        expect_string(__wrap_scandir, dirp, SYSTEM_CPU);
        will_return(__wrap_scandir, CORE_COUNT);
#if PQOS_VERSION >= 50000
        will_return_always(os_cpuinfo_cpu_node, PQOS_RETVAL_OK);
#endif
        will_return(os_cpuinfo_cpu_cache, PQOS_RETVAL_ERROR);
        cpuinfo = os_cpuinfo_topology();
        assert_null(cpuinfo);
}

int
main(void)
{
        int result = 0;

        const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_os_cpuinfo_topology),
            cmocka_unit_test(test_os_cpuinfo_topology_error),
        };

        result += cmocka_run_group_tests(tests, NULL, NULL);

        return result;
}
