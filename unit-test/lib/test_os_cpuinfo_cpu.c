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

/* ======== mock ======== */

int
__wrap_scandir(const char *restrict dirp,
               struct dirent ***restrict namelist,
               int (*filter)(const struct dirent *) __attribute__((unused)),
               int (*compar)(const struct dirent **, const struct dirent **)
                   __attribute__((unused)))
{
        int ret;

        check_expected(dirp);

        ret = mock_type(int);
        if (ret < 0)
                return ret;
        else if (ret == 0)
                *namelist = NULL;
        else if (strcmp(dirp, SYSTEM_CPU "/cpu1") == 0) {
                *namelist = malloc(sizeof(struct dirent *));
                *namelist[0] = malloc(sizeof(struct dirent) + 6);
                strncpy((*namelist)[0]->d_name, "node5", 6);
        } else {
                *namelist = malloc(sizeof(struct dirent *));
                *namelist[0] = malloc(sizeof(struct dirent) + 5);
                strncpy((*namelist)[0]->d_name, "test", 5);
        }

        return ret;
}

/* ======== os_cpuinfo_cpu_online ======== */

static void
test_os_cpuinfo_cpu_online(void **state __attribute__((unused)))
{
        unsigned lcore = 1;
        unsigned online;
        const char *path = SYSTEM_CPU "/cpu1/online";

        /* online */
        expect_string(__wrap_pqos_fread_uint, path, path);
        will_return(__wrap_pqos_fread_uint, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_fread_uint, 1);

        online = os_cpuinfo_cpu_online(lcore);
        assert_int_equal(online, 1);

        /* off-line*/
        expect_string(__wrap_pqos_fread_uint, path, path);
        will_return(__wrap_pqos_fread_uint, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_fread_uint, 0);

        online = os_cpuinfo_cpu_online(lcore);
        assert_int_equal(online, 0);
}

static void
test_os_cpuinfo_cpu_online_error(void **state __attribute__((unused)))
{
        unsigned lcore = 1;
        unsigned online;
        const char *path = SYSTEM_CPU "/cpu1/online";

        /* online */
        expect_string(__wrap_pqos_fread_uint, path, path);
        will_return(__wrap_pqos_fread_uint, PQOS_RETVAL_ERROR);

        online = os_cpuinfo_cpu_online(lcore);
        assert_int_equal(online, 0);
}

static void
test_os_cpuinfo_cpu_online_resource(void **state __attribute__((unused)))
{
        unsigned lcore = 1;
        unsigned online;
        const char *path = SYSTEM_CPU "/cpu1/online";

        /* online */
        expect_string(__wrap_pqos_fread_uint, path, path);
        will_return(__wrap_pqos_fread_uint, PQOS_RETVAL_RESOURCE);

        online = os_cpuinfo_cpu_online(lcore);
        assert_int_equal(online, 1);
}

/* ======== os_cpuinfo_cpu_socket ======== */

static void
test_os_cpuinfo_cpu_socket(void **state __attribute__((unused)))
{
        unsigned lcore = 1;
        unsigned socket;
        int ret;
        const char *path = SYSTEM_CPU "/cpu1/topology/physical_package_id";

        expect_string(__wrap_pqos_fread_uint, path, path);
        will_return(__wrap_pqos_fread_uint, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_fread_uint, 1);

        ret = os_cpuinfo_cpu_socket(lcore, &socket);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(socket, 1);
}

static void
test_os_cpuinfo_cpu_socket_error(void **state __attribute__((unused)))
{
        unsigned lcore = 1;
        unsigned socket;
        int ret;
        const char *path = SYSTEM_CPU "/cpu1/topology/physical_package_id";

        expect_string(__wrap_pqos_fread_uint, path, path);
        will_return(__wrap_pqos_fread_uint, PQOS_RETVAL_ERROR);

        ret = os_cpuinfo_cpu_socket(lcore, &socket);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
}

/* ======== os_cpuinfo_cpu_cache ======== */

static void
test_os_cpuinfo_cpu_cache_level_1(void **state __attribute__((unused)))
{
        int ret;
        unsigned level = 1;
        unsigned id = 3;
        unsigned lcore = 1;
        unsigned l3 = 0;
        unsigned l2 = 0;

        expect_string(__wrap_scandir, dirp, SYSTEM_CPU "/cpu1/cache");
        will_return(__wrap_scandir, 1);

        expect_string(__wrap_pqos_fread_uint, path,
                      SYSTEM_CPU "/cpu1/cache/test/level");
        will_return(__wrap_pqos_fread_uint, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_fread_uint, level);

        expect_string(__wrap_pqos_fread_uint, path,
                      SYSTEM_CPU "/cpu1/cache/test/id");
        will_return(__wrap_pqos_fread_uint, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_fread_uint, id);

        ret = os_cpuinfo_cpu_cache(lcore, &l3, &l2);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(l2, 0);
        assert_int_equal(l3, 0);
}

static void
test_os_cpuinfo_cpu_cache_level_2(void **state __attribute__((unused)))
{
        int ret;
        unsigned level = 2;
        unsigned id = 3;
        unsigned lcore = 1;
        unsigned l3 = 0;
        unsigned l2 = 0;

        expect_string(__wrap_scandir, dirp, SYSTEM_CPU "/cpu1/cache");
        will_return(__wrap_scandir, 1);

        expect_string(__wrap_pqos_fread_uint, path,
                      SYSTEM_CPU "/cpu1/cache/test/level");
        will_return(__wrap_pqos_fread_uint, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_fread_uint, level);

        expect_string(__wrap_pqos_fread_uint, path,
                      SYSTEM_CPU "/cpu1/cache/test/id");
        will_return(__wrap_pqos_fread_uint, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_fread_uint, id);

        ret = os_cpuinfo_cpu_cache(lcore, &l3, &l2);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(l2, id);
        assert_int_equal(l3, 0);
}

static void
test_os_cpuinfo_cpu_cache_level_3(void **state __attribute__((unused)))
{
        int ret;
        unsigned level = 3;
        unsigned id = 3;
        unsigned lcore = 1;
        unsigned l3 = 0;
        unsigned l2 = 0;

        expect_string(__wrap_scandir, dirp, SYSTEM_CPU "/cpu1/cache");
        will_return(__wrap_scandir, 1);

        expect_string(__wrap_pqos_fread_uint, path,
                      SYSTEM_CPU "/cpu1/cache/test/level");
        will_return(__wrap_pqos_fread_uint, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_fread_uint, level);

        expect_string(__wrap_pqos_fread_uint, path,
                      SYSTEM_CPU "/cpu1/cache/test/id");
        will_return(__wrap_pqos_fread_uint, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_fread_uint, id);

        ret = os_cpuinfo_cpu_cache(lcore, &l3, &l2);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(l2, 0);
        assert_int_equal(l3, id);
}

static void
test_os_cpuinfo_cpu_cache_error(void **state __attribute__((unused)))
{
        int ret;
        unsigned level = 3;
        unsigned lcore = 1;
        unsigned l3 = 0;
        unsigned l2 = 0;

        /* invalid level */
        {
                expect_string(__wrap_scandir, dirp, SYSTEM_CPU "/cpu1/cache");
                will_return(__wrap_scandir, 1);

                expect_string(__wrap_pqos_fread_uint, path,
                              SYSTEM_CPU "/cpu1/cache/test/level");
                will_return(__wrap_pqos_fread_uint, PQOS_RETVAL_ERROR);

                ret = os_cpuinfo_cpu_cache(lcore, &l3, &l2);
                assert_int_equal(ret, PQOS_RETVAL_ERROR);
        }

        /* invalid id */
        {
                expect_string(__wrap_scandir, dirp, SYSTEM_CPU "/cpu1/cache");
                will_return(__wrap_scandir, 1);

                expect_string(__wrap_pqos_fread_uint, path,
                              SYSTEM_CPU "/cpu1/cache/test/level");
                will_return(__wrap_pqos_fread_uint, PQOS_RETVAL_OK);
                will_return(__wrap_pqos_fread_uint, level);

                expect_string(__wrap_pqos_fread_uint, path,
                              SYSTEM_CPU "/cpu1/cache/test/id");
                will_return(__wrap_pqos_fread_uint, PQOS_RETVAL_ERROR);

                ret = os_cpuinfo_cpu_cache(lcore, &l3, &l2);
                assert_int_equal(ret, PQOS_RETVAL_ERROR);
        }
}

#if PQOS_VERSION >= 50000
/* ======== os_cpuinfo_cpu_node ======== */

static void
test_os_cpuinfo_cpu_node(void **state __attribute__((unused)))
{
        unsigned lcore = 1;
        unsigned node;
        int ret;

        expect_string(__wrap_scandir, dirp, SYSTEM_CPU "/cpu1");
        will_return(__wrap_scandir, 1);

        ret = os_cpuinfo_cpu_node(lcore, &node);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(node, 5);
}
#endif

int
main(void)
{
        int result = 0;

        const struct CMUnitTest tests[] = {
                cmocka_unit_test(test_os_cpuinfo_cpu_online),
                cmocka_unit_test(test_os_cpuinfo_cpu_online_error),
                cmocka_unit_test(test_os_cpuinfo_cpu_online_resource),
                cmocka_unit_test(test_os_cpuinfo_cpu_socket),
                cmocka_unit_test(test_os_cpuinfo_cpu_socket_error),
                cmocka_unit_test(test_os_cpuinfo_cpu_cache_level_1),
                cmocka_unit_test(test_os_cpuinfo_cpu_cache_level_2),
                cmocka_unit_test(test_os_cpuinfo_cpu_cache_level_3),
                cmocka_unit_test(test_os_cpuinfo_cpu_cache_error),
#if PQOS_VERSION >= 50000
                cmocka_unit_test(test_os_cpuinfo_cpu_node),
#endif
        };

        result += cmocka_run_group_tests(tests, NULL, NULL);

        return result;
}
