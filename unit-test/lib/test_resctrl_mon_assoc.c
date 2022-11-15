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

#include "mock_resctrl_monitoring.h"
#include "resctrl_monitoring.h"
#include "test.h"

#include <dirent.h>

/* ======== mock ======== */

int
resctrl_mon_is_supported(void)
{
        return mock_type(int);
}

int
resctrl_mon_cpumask_read(const unsigned class_id,
                         const char *resctrl_group,
                         struct resctrl_cpumask *mask)
{
        return __wrap_resctrl_mon_cpumask_read(class_id, resctrl_group, mask);
}

int
resctrl_mon_cpumask_write(const unsigned class_id,
                          const char *resctrl_group,
                          const struct resctrl_cpumask *mask)
{
        return __wrap_resctrl_mon_cpumask_write(class_id, resctrl_group, mask);
}

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
        if (ret == 0)
                *namelist = NULL;
        else if (ret == 1) {
                *namelist = malloc(sizeof(struct dirent *));
                *namelist[0] = malloc(sizeof(struct dirent) + 5);
                strncpy((*namelist)[0]->d_name, "test", 5);
        }

        return ret;
}

int
resctrl_mon_mkdir(const unsigned class_id, const char *name)
{
        return __wrap_resctrl_mon_mkdir(class_id, name);
}

int
resctrl_mon_rmdir(const unsigned class_id, const char *name)
{
        return __wrap_resctrl_mon_rmdir(class_id, name);
}

FILE *__real_pqos_fopen(const char *name, const char *mode);

FILE *
__wrap_pqos_fopen(const char *name, const char *mode)
{
        FILE *fd = tmpfile();

        assert_non_null(fd);

        if (strcmp(name, "/sys/fs/resctrl/mon_groups/test/tasks") == 0) {
                /* PID 1 assigned to "test" monitoring group */
                if (strcmp(mode, "r") == 0)
                        fprintf(fd, "1\n");

        } else if (strcmp(name, "/sys/fs/resctrl/tasks") == 0) {
                /* PID 1 and 2 assigned to COS 0 */
                fprintf(fd, "1\n");
                fprintf(fd, "2\n");

        } else if (strcmp(name, "/sys/fs/resctrl/COS1/tasks") == 0) {
                /* PID 3 assigned to COS 1 */
                fprintf(fd, "3\n");

        } else {
                fclose(fd);
                return __real_pqos_fopen(name, mode);
        }

        fseek(fd, 0, SEEK_SET);
        return fd;
}

int
__wrap_pqos_fclose(FILE *fd)
{
        return fclose(fd);
}

/* ======== resctrl_mon_assoc_get ======== */

static void
test_resctrl_mon_assoc_get_unsupported(void **state __attribute__((unused)))
{
        int ret;
        unsigned lcore = 1;
        char name[128];

        will_return(resctrl_mon_is_supported, 0);

        ret = resctrl_mon_assoc_get(lcore, name, sizeof(name));
        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);
}

static void
test_resctrl_mon_assoc_get_alloc_unsupported(void **state
                                             __attribute__((unused)))
{
        int ret;
        unsigned lcore = 1;
        char name[128];
        struct pqos_cap cap;
        unsigned class_id = 0;

        will_return_maybe(__wrap__pqos_get_cap, &cap);

        will_return(resctrl_mon_is_supported, 1);
        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 0);

        expect_string(__wrap_scandir, dirp, "/sys/fs/resctrl/mon_groups/");
        will_return(__wrap_scandir, 1);

        expect_value(__wrap_resctrl_mon_cpumask_read, class_id, class_id);
        expect_string(__wrap_resctrl_mon_cpumask_read, resctrl_group, "test");
        will_return(__wrap_resctrl_mon_cpumask_read, PQOS_RETVAL_OK);

        expect_value(__wrap_resctrl_cpumask_get, lcore, lcore);
        will_return(__wrap_resctrl_cpumask_get, 1);

        ret = resctrl_mon_assoc_get(lcore, name, sizeof(name));
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_string_equal(name, "test");
}

static void
test_resctrl_mon_assoc_get_alloc_default(void **state __attribute__((unused)))
{
        int ret;
        unsigned lcore = 1;
        char name[128];
        struct pqos_cap cap;
        unsigned class_id = 0;

        will_return_maybe(__wrap__pqos_get_cap, &cap);

        will_return(resctrl_mon_is_supported, 1);
        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 2);
        expect_value(__wrap_resctrl_alloc_assoc_get, lcore, lcore);
        will_return(__wrap_resctrl_alloc_assoc_get, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_assoc_get, class_id);

        expect_string(__wrap_scandir, dirp, "/sys/fs/resctrl/mon_groups/");
        will_return(__wrap_scandir, 1);

        expect_value(__wrap_resctrl_mon_cpumask_read, class_id, class_id);
        expect_string(__wrap_resctrl_mon_cpumask_read, resctrl_group, "test");
        will_return(__wrap_resctrl_mon_cpumask_read, PQOS_RETVAL_OK);

        expect_value(__wrap_resctrl_cpumask_get, lcore, lcore);
        will_return(__wrap_resctrl_cpumask_get, 1);

        ret = resctrl_mon_assoc_get(lcore, name, sizeof(name));
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_string_equal(name, "test");
}

static void
test_resctrl_mon_assoc_get_alloc_nondefault(void **state
                                            __attribute__((unused)))
{
        int ret;
        unsigned lcore = 1;
        char name[128];
        struct pqos_cap cap;
        unsigned class_id = 1;

        will_return_maybe(__wrap__pqos_get_cap, &cap);

        will_return(resctrl_mon_is_supported, 1);
        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 2);
        expect_value(__wrap_resctrl_alloc_assoc_get, lcore, lcore);
        will_return(__wrap_resctrl_alloc_assoc_get, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_assoc_get, class_id);

        expect_string(__wrap_scandir, dirp, "/sys/fs/resctrl/COS1/mon_groups/");
        will_return(__wrap_scandir, 1);

        expect_value(__wrap_resctrl_mon_cpumask_read, class_id, class_id);
        expect_string(__wrap_resctrl_mon_cpumask_read, resctrl_group, "test");
        will_return(__wrap_resctrl_mon_cpumask_read, PQOS_RETVAL_OK);

        expect_value(__wrap_resctrl_cpumask_get, lcore, lcore);
        will_return(__wrap_resctrl_cpumask_get, 1);

        ret = resctrl_mon_assoc_get(lcore, name, sizeof(name));
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_string_equal(name, "test");
}

static void
test_resctrl_mon_assoc_get_unassigned(void **state __attribute__((unused)))
{
        int ret;
        unsigned lcore = 1;
        char name[128];
        struct pqos_cap cap;
        unsigned class_id = 0;

        will_return_maybe(__wrap__pqos_get_cap, &cap);

        /* No monitoring is started */
        will_return(resctrl_mon_is_supported, 1);
        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 2);
        expect_value(__wrap_resctrl_alloc_assoc_get, lcore, lcore);
        will_return(__wrap_resctrl_alloc_assoc_get, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_assoc_get, class_id);

        expect_string(__wrap_scandir, dirp, "/sys/fs/resctrl/mon_groups/");
        will_return(__wrap_scandir, 0);

        ret = resctrl_mon_assoc_get(lcore, name, sizeof(name));
        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);

        /* some monitoring in progress */
        will_return(resctrl_mon_is_supported, 1);
        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 2);
        expect_value(__wrap_resctrl_alloc_assoc_get, lcore, lcore);
        will_return(__wrap_resctrl_alloc_assoc_get, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_assoc_get, class_id);

        expect_string(__wrap_scandir, dirp, "/sys/fs/resctrl/mon_groups/");
        will_return(__wrap_scandir, 1);

        expect_value(__wrap_resctrl_mon_cpumask_read, class_id, class_id);
        expect_string(__wrap_resctrl_mon_cpumask_read, resctrl_group, "test");
        will_return(__wrap_resctrl_mon_cpumask_read, PQOS_RETVAL_OK);

        expect_value(__wrap_resctrl_cpumask_get, lcore, lcore);
        will_return(__wrap_resctrl_cpumask_get, 0);

        ret = resctrl_mon_assoc_get(lcore, name, sizeof(name));
        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);
}

static void
test_resctrl_mon_assoc_get_error(void **state __attribute__((unused)))
{
        int ret;
        unsigned lcore = 1;
        char name[128];
        struct pqos_cap cap;

        will_return_maybe(__wrap__pqos_get_cap, &cap);

        /* error getting ctrl groups num */
        will_return(resctrl_mon_is_supported, 1);
        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_ERROR);

        ret = resctrl_mon_assoc_get(lcore, name, sizeof(name));
        assert_int_equal(ret, PQOS_RETVAL_ERROR);

        /* error getting list of monitoring groups */
        will_return(resctrl_mon_is_supported, 1);
        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 0);
        expect_string(__wrap_scandir, dirp, "/sys/fs/resctrl/mon_groups/");
        will_return(__wrap_scandir, -1);

        ret = resctrl_mon_assoc_get(lcore, name, sizeof(name));
        assert_int_equal(ret, PQOS_RETVAL_ERROR);

        /* error getting COS number */
        will_return(resctrl_mon_is_supported, 1);
        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 1);
        expect_value(__wrap_resctrl_alloc_assoc_get, lcore, lcore);
        will_return(__wrap_resctrl_alloc_assoc_get, PQOS_RETVAL_ERROR);

        ret = resctrl_mon_assoc_get(lcore, name, sizeof(name));
        assert_int_equal(ret, PQOS_RETVAL_ERROR);

        /* error reading cpumask */
        will_return(resctrl_mon_is_supported, 1);
        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 0);

        expect_string(__wrap_scandir, dirp, "/sys/fs/resctrl/mon_groups/");
        will_return(__wrap_scandir, 1);

        expect_value(__wrap_resctrl_mon_cpumask_read, class_id, 0);
        expect_string(__wrap_resctrl_mon_cpumask_read, resctrl_group, "test");
        will_return(__wrap_resctrl_mon_cpumask_read, PQOS_RETVAL_ERROR);

        ret = resctrl_mon_assoc_get(lcore, name, sizeof(name));
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
}

/* ======== resctrl_mon_assoc_set ======== */

static void
test_resctrl_mon_assoc_set_unsupported(void **state __attribute__((unused)))
{
        int ret;
        unsigned lcore = 1;
        char name[] = "test";

        will_return(resctrl_mon_is_supported, 0);

        ret = resctrl_mon_assoc_set(lcore, name);
        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);
}

static void
test_resctrl_mon_assoc_set(void **state __attribute__((unused)))
{
        int ret;
        unsigned lcore = 1;
        char name[] = "test";
        struct pqos_cap cap;

        will_return_maybe(__wrap__pqos_get_cap, &cap);

        will_return(resctrl_mon_is_supported, 1);
        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 0);

        expect_value(__wrap_resctrl_mon_mkdir, class_id, 0);
        expect_string(__wrap_resctrl_mon_mkdir, name, name);
        will_return(__wrap_resctrl_mon_mkdir, PQOS_RETVAL_OK);

        expect_value(__wrap_resctrl_mon_cpumask_read, class_id, 0);
        expect_string(__wrap_resctrl_mon_cpumask_read, resctrl_group, name);
        will_return(__wrap_resctrl_mon_cpumask_read, PQOS_RETVAL_OK);

        expect_value(__wrap_resctrl_cpumask_set, lcore, lcore);

        expect_value(__wrap_resctrl_mon_cpumask_write, class_id, 0);
        expect_string(__wrap_resctrl_mon_cpumask_write, resctrl_group, name);
        will_return(__wrap_resctrl_mon_cpumask_write, PQOS_RETVAL_OK);

        ret = resctrl_mon_assoc_set(lcore, name);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_resctrl_mon_assoc_set_error(void **state __attribute__((unused)))
{
        int ret;
        unsigned lcore = 1;
        char name[] = "test";
        struct pqos_cap cap;

        will_return_maybe(__wrap__pqos_get_cap, &cap);

        /* error getting COS number */
        will_return(resctrl_mon_is_supported, 1);
        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_ERROR);

        ret = resctrl_mon_assoc_set(lcore, name);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);

        /* error creating group */
        will_return(resctrl_mon_is_supported, 1);
        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 0);

        expect_value(__wrap_resctrl_mon_mkdir, class_id, 0);
        expect_string(__wrap_resctrl_mon_mkdir, name, name);
        will_return(__wrap_resctrl_mon_mkdir, PQOS_RETVAL_ERROR);

        ret = resctrl_mon_assoc_set(lcore, name);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);

        /* error reading cpumask */
        will_return(resctrl_mon_is_supported, 1);
        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 0);

        expect_value(__wrap_resctrl_mon_mkdir, class_id, 0);
        expect_string(__wrap_resctrl_mon_mkdir, name, name);
        will_return(__wrap_resctrl_mon_mkdir, PQOS_RETVAL_OK);

        expect_value(__wrap_resctrl_mon_cpumask_read, class_id, 0);
        expect_string(__wrap_resctrl_mon_cpumask_read, resctrl_group, name);
        will_return(__wrap_resctrl_mon_cpumask_read, PQOS_RETVAL_ERROR);

        ret = resctrl_mon_assoc_set(lcore, name);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);

        /* error writing cpumask */
        will_return(resctrl_mon_is_supported, 1);
        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 0);

        expect_value(__wrap_resctrl_mon_mkdir, class_id, 0);
        expect_string(__wrap_resctrl_mon_mkdir, name, name);
        will_return(__wrap_resctrl_mon_mkdir, PQOS_RETVAL_OK);

        expect_value(__wrap_resctrl_mon_cpumask_read, class_id, 0);
        expect_string(__wrap_resctrl_mon_cpumask_read, resctrl_group, name);
        will_return(__wrap_resctrl_mon_cpumask_read, PQOS_RETVAL_OK);

        expect_value(__wrap_resctrl_cpumask_set, lcore, lcore);

        expect_value(__wrap_resctrl_mon_cpumask_write, class_id, 0);
        expect_string(__wrap_resctrl_mon_cpumask_write, resctrl_group, name);
        will_return(__wrap_resctrl_mon_cpumask_write, PQOS_RETVAL_ERROR);

        ret = resctrl_mon_assoc_set(lcore, name);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
}

/* ======== resctrl_mon_assoc_get_pid ======== */

static void
test_resctrl_mon_assoc_get_pid_unsupported(void **state __attribute__((unused)))
{
        int ret;
        char name[128];
        pid_t task = 1;

        will_return(resctrl_mon_is_supported, 0);

        ret = resctrl_mon_assoc_get_pid(task, name, sizeof(name));
        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);
}

static void
test_resctrl_mon_assoc_get_pid_no_alloc(void **state __attribute__((unused)))
{
        int ret;
        char name[128];
        struct pqos_cap cap;

        will_return_maybe(__wrap__pqos_get_cap, &cap);

        will_return(resctrl_mon_is_supported, 1);
        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 0);

        expect_string(__wrap_scandir, dirp, "/sys/fs/resctrl/mon_groups/");
        will_return(__wrap_scandir, 1);

        ret = resctrl_mon_assoc_get_pid(1, name, sizeof(name));
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_string_equal(name, "test");
}

static void
test_resctrl_mon_assoc_get_pid_unassigned(void **state __attribute__((unused)))
{
        int ret;
        char name[128];
        struct pqos_cap cap;

        will_return_maybe(__wrap__pqos_get_cap, &cap);

        /* monitoring not started */
        will_return(resctrl_mon_is_supported, 1);
        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 0);

        expect_string(__wrap_scandir, dirp, "/sys/fs/resctrl/mon_groups/");
        will_return(__wrap_scandir, 0);

        ret = resctrl_mon_assoc_get_pid(2, name, sizeof(name));
        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);

        /* some monitoring is in progress */
        will_return(resctrl_mon_is_supported, 1);
        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 0);

        expect_string(__wrap_scandir, dirp, "/sys/fs/resctrl/mon_groups/");
        will_return(__wrap_scandir, 1);

        ret = resctrl_mon_assoc_get_pid(2, name, sizeof(name));
        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);
}

static void
test_resctrl_mon_assoc_get_pid_alloc_default(void **state
                                             __attribute__((unused)))
{
        int ret;
        char name[128];
        struct pqos_cap cap;

        will_return_maybe(__wrap__pqos_get_cap, &cap);

        will_return(resctrl_mon_is_supported, 1);
        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 1);

        expect_value(__wrap_resctrl_alloc_assoc_get_pid, task, 1);
        will_return(__wrap_resctrl_alloc_assoc_get_pid, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_assoc_get_pid, 0);

        expect_string(__wrap_scandir, dirp, "/sys/fs/resctrl/mon_groups/");
        will_return(__wrap_scandir, 1);

        ret = resctrl_mon_assoc_get_pid(1, name, sizeof(name));
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_string_equal(name, "test");
}

/* ======== resctrl_mon_assoc_set_pid ======== */

static void
test_resctrl_mon_assoc_set_pid_unsupported(void **state __attribute__((unused)))
{
        int ret;
        pid_t pid = 1;
        char name[] = "test";

        will_return(resctrl_mon_is_supported, 0);

        ret = resctrl_mon_assoc_set_pid(pid, name);
        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);
}

static void
test_resctrl_mon_assoc_set_pid(void **state __attribute__((unused)))
{
        int ret;
        pid_t pid = 1;
        char name[] = "test";
        struct pqos_cap cap;

        will_return_maybe(__wrap__pqos_get_cap, &cap);

        will_return(resctrl_mon_is_supported, 1);
        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 0);

        expect_value(__wrap_resctrl_mon_mkdir, class_id, 0);
        expect_string(__wrap_resctrl_mon_mkdir, name, name);
        will_return(__wrap_resctrl_mon_mkdir, PQOS_RETVAL_OK);

        ret = resctrl_mon_assoc_set_pid(pid, name);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_resctrl_mon_assoc_set_pid_error(void **state __attribute__((unused)))
{
        int ret;
        pid_t pid = 1;
        char name[] = "test";
        struct pqos_cap cap;

        will_return_maybe(__wrap__pqos_get_cap, &cap);

        /* error getting COS number */
        will_return(resctrl_mon_is_supported, 1);
        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_ERROR);

        ret = resctrl_mon_assoc_set_pid(pid, name);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
}

int
main(void)
{
        const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_resctrl_mon_assoc_get_unsupported),
            cmocka_unit_test(test_resctrl_mon_assoc_get_alloc_unsupported),
            cmocka_unit_test(test_resctrl_mon_assoc_get_alloc_default),
            cmocka_unit_test(test_resctrl_mon_assoc_get_alloc_nondefault),
            cmocka_unit_test(test_resctrl_mon_assoc_get_unassigned),
            cmocka_unit_test(test_resctrl_mon_assoc_get_error),
            cmocka_unit_test(test_resctrl_mon_assoc_set_unsupported),
            cmocka_unit_test(test_resctrl_mon_assoc_set),
            cmocka_unit_test(test_resctrl_mon_assoc_set_error),
            cmocka_unit_test(test_resctrl_mon_assoc_get_pid_unsupported),
            cmocka_unit_test(test_resctrl_mon_assoc_get_pid_no_alloc),
            cmocka_unit_test(test_resctrl_mon_assoc_get_pid_unassigned),
            cmocka_unit_test(test_resctrl_mon_assoc_get_pid_alloc_default),
            cmocka_unit_test(test_resctrl_mon_assoc_set_pid_unsupported),
            cmocka_unit_test(test_resctrl_mon_assoc_set_pid),
            cmocka_unit_test(test_resctrl_mon_assoc_set_pid_error),
        };

        cmocka_run_group_tests(tests, NULL, NULL);
}
