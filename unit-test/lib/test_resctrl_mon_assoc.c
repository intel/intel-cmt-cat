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

#include <dirent.h>
#include <errno.h>

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
                /* PID 1 and 2 assigned to CLOS 0 */
                fprintf(fd, "1\n");
                fprintf(fd, "2\n");

        } else if (strcmp(name, "/sys/fs/resctrl/COS1/tasks") == 0) {
                /* PID 3 assigned to CLOS 1 */
                fprintf(fd, "3\n");

        } else {
                fclose(fd);
                return __real_pqos_fopen(name, mode);
        }

        fseek(fd, 0, SEEK_SET);
        return fd;
}

/* What a close is to fail with, 0 for one that succeeds. A tasks file is
 * written when it is closed, so this is the kernel's answer to the write
 */
static int fclose_errno;

int
__wrap_pqos_fclose(FILE *fd)
{
        const int ret = fclose(fd);

        if (fclose_errno == 0)
                return ret;
        errno = fclose_errno;
        fclose_errno = 0;
        return EOF;
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
test_resctrl_mon_assoc_get_small_buffer(void **state __attribute__((unused)))
{
        int ret;
        unsigned lcore = 1;
        char name[4];
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
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
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

        /* error getting CLOS number */
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

/* a core in no class, as an offline one is, is in no group either: nothing
 * else is read, and nothing is said, as for a core in a class and no group
 */
static void
test_resctrl_mon_assoc_get_core_in_no_class(void **state
                                            __attribute__((unused)))
{
        int ret;
        unsigned lcore = 1;
        char name[128];
        struct pqos_cap cap;

        will_return_maybe(__wrap__pqos_get_cap, &cap);

        will_return(resctrl_mon_is_supported, 1);
        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 2);
        expect_value(__wrap_resctrl_alloc_assoc_get, lcore, lcore);
        will_return(__wrap_resctrl_alloc_assoc_get, PQOS_RETVAL_RESOURCE);

        test_log_count_start();
        ret = resctrl_mon_assoc_get(lcore, name, sizeof(name));
        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);
        assert_int_equal(test_log_count_stop(), 0);
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

        /* error getting CLOS number */
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

/* a core in no class, as an offline one is, cannot be put in a group, which
 * is said once: no group is made
 */
static void
test_resctrl_mon_assoc_set_core_in_no_class(void **state
                                            __attribute__((unused)))
{
        int ret;
        unsigned lcore = 1;
        struct pqos_cap cap;

        will_return_maybe(__wrap__pqos_get_cap, &cap);

        will_return(resctrl_mon_is_supported, 1);
        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 2);
        expect_value(__wrap_resctrl_alloc_assoc_get, lcore, lcore);
        will_return(__wrap_resctrl_alloc_assoc_get, PQOS_RETVAL_RESOURCE);

        test_log_count_start();
        ret = resctrl_mon_assoc_set(lcore, "test");
        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);
        assert_int_equal(test_log_count_stop(), 1);
}

/* ======== resctrl_mon_assoc_restore ======== */

static void
test_resctrl_mon_assoc_restore_core_in_its_group(void **state
                                                 __attribute__((unused)))
{
        int ret;
        unsigned lcore = 1;
        struct pqos_cap cap;

        will_return_maybe(__wrap__pqos_get_cap, &cap);

        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 2);
        expect_value(__wrap_resctrl_alloc_assoc_get, lcore, lcore);
        will_return(__wrap_resctrl_alloc_assoc_get, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_assoc_get, 1);

        /* the group's own directory has the core: no other group is read */
        expect_value(__wrap_resctrl_mon_cpumask_read, class_id, 1);
        expect_string(__wrap_resctrl_mon_cpumask_read, resctrl_group, "own");
        will_return(__wrap_resctrl_mon_cpumask_read, PQOS_RETVAL_OK);
        expect_value(__wrap_resctrl_cpumask_get, lcore, lcore);
        will_return(__wrap_resctrl_cpumask_get, 1);

        ret = resctrl_mon_assoc_restore(lcore, "own");
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_resctrl_mon_assoc_restore_core_in_another_group(void **state
                                                     __attribute__((unused)))
{
        int ret;
        unsigned lcore = 1;
        struct pqos_cap cap;

        will_return_maybe(__wrap__pqos_get_cap, &cap);

        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 2);
        expect_value(__wrap_resctrl_alloc_assoc_get, lcore, lcore);
        will_return(__wrap_resctrl_alloc_assoc_get, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_assoc_get, 1);

        expect_value(__wrap_resctrl_mon_cpumask_read, class_id, 1);
        expect_string(__wrap_resctrl_mon_cpumask_read, resctrl_group, "own");
        will_return(__wrap_resctrl_mon_cpumask_read, PQOS_RETVAL_OK);
        expect_value(__wrap_resctrl_cpumask_get, lcore, lcore);
        will_return(__wrap_resctrl_cpumask_get, 0);

        /* the search of the class's groups finds it in "test", which is
         * left as it is
         */
        will_return(resctrl_mon_is_supported, 1);
        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 2);
        expect_value(__wrap_resctrl_alloc_assoc_get, lcore, lcore);
        will_return(__wrap_resctrl_alloc_assoc_get, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_assoc_get, 1);
        expect_string(__wrap_scandir, dirp, "/sys/fs/resctrl/COS1/mon_groups/");
        will_return(__wrap_scandir, 1);
        expect_value(__wrap_resctrl_mon_cpumask_read, class_id, 1);
        expect_string(__wrap_resctrl_mon_cpumask_read, resctrl_group, "test");
        will_return(__wrap_resctrl_mon_cpumask_read, PQOS_RETVAL_OK);
        expect_value(__wrap_resctrl_cpumask_get, lcore, lcore);
        will_return(__wrap_resctrl_cpumask_get, 1);

        ret = resctrl_mon_assoc_restore(lcore, "own");
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_resctrl_mon_assoc_restore_own_group_unreadable(void **state
                                                    __attribute__((unused)))
{
        int ret;
        unsigned lcore = 1;
        struct pqos_cap cap;

        will_return_maybe(__wrap__pqos_get_cap, &cap);

        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 2);
        expect_value(__wrap_resctrl_alloc_assoc_get, lcore, lcore);
        will_return(__wrap_resctrl_alloc_assoc_get, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_assoc_get, 1);

        /* the group's own directory cannot be read, as when the group has
         * none in the core's class, which does not fail the restore
         */
        expect_value(__wrap_resctrl_mon_cpumask_read, class_id, 1);
        expect_string(__wrap_resctrl_mon_cpumask_read, resctrl_group, "own");
        will_return(__wrap_resctrl_mon_cpumask_read, PQOS_RETVAL_ERROR);

        /* the class's groups are searched instead, and the core, found in
         * "test", is left there: nothing is made or written
         */
        will_return(resctrl_mon_is_supported, 1);
        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 2);
        expect_value(__wrap_resctrl_alloc_assoc_get, lcore, lcore);
        will_return(__wrap_resctrl_alloc_assoc_get, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_assoc_get, 1);
        expect_string(__wrap_scandir, dirp, "/sys/fs/resctrl/COS1/mon_groups/");
        will_return(__wrap_scandir, 1);
        expect_value(__wrap_resctrl_mon_cpumask_read, class_id, 1);
        expect_string(__wrap_resctrl_mon_cpumask_read, resctrl_group, "test");
        will_return(__wrap_resctrl_mon_cpumask_read, PQOS_RETVAL_OK);
        expect_value(__wrap_resctrl_cpumask_get, lcore, lcore);
        will_return(__wrap_resctrl_cpumask_get, 1);

        ret = resctrl_mon_assoc_restore(lcore, "own");
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_resctrl_mon_assoc_restore_core_moved_out(void **state
                                              __attribute__((unused)))
{
        int ret;
        unsigned lcore = 1;
        struct pqos_cap cap;

        will_return_maybe(__wrap__pqos_get_cap, &cap);

        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 2);
        expect_value(__wrap_resctrl_alloc_assoc_get, lcore, lcore);
        will_return(__wrap_resctrl_alloc_assoc_get, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_assoc_get, 1);

        /* the group's directory has lost the core */
        expect_value(__wrap_resctrl_mon_cpumask_read, class_id, 1);
        expect_string(__wrap_resctrl_mon_cpumask_read, resctrl_group, "own");
        will_return(__wrap_resctrl_mon_cpumask_read, PQOS_RETVAL_OK);
        expect_value(__wrap_resctrl_cpumask_get, lcore, lcore);
        will_return(__wrap_resctrl_cpumask_get, 0);

        /* and no group of its class has it */
        will_return(resctrl_mon_is_supported, 1);
        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 2);
        expect_value(__wrap_resctrl_alloc_assoc_get, lcore, lcore);
        will_return(__wrap_resctrl_alloc_assoc_get, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_assoc_get, 1);
        expect_string(__wrap_scandir, dirp, "/sys/fs/resctrl/COS1/mon_groups/");
        will_return(__wrap_scandir, 0);

        /* so it is put back, in the class it is in */
        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 2);
        expect_value(__wrap_resctrl_alloc_assoc_get, lcore, lcore);
        will_return(__wrap_resctrl_alloc_assoc_get, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_assoc_get, 1);
        will_return(resctrl_mon_is_supported, 1);
        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 2);
        expect_value(__wrap_resctrl_alloc_assoc_get, lcore, lcore);
        will_return(__wrap_resctrl_alloc_assoc_get, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_assoc_get, 1);
        expect_value(__wrap_resctrl_mon_mkdir, class_id, 1);
        expect_string(__wrap_resctrl_mon_mkdir, name, "own");
        will_return(__wrap_resctrl_mon_mkdir, PQOS_RETVAL_OK);
        expect_value(__wrap_resctrl_mon_cpumask_read, class_id, 1);
        expect_string(__wrap_resctrl_mon_cpumask_read, resctrl_group, "own");
        will_return(__wrap_resctrl_mon_cpumask_read, PQOS_RETVAL_OK);
        expect_value(__wrap_resctrl_cpumask_set, lcore, lcore);
        expect_value(__wrap_resctrl_mon_cpumask_write, class_id, 1);
        expect_string(__wrap_resctrl_mon_cpumask_write, resctrl_group, "own");
        will_return(__wrap_resctrl_mon_cpumask_write, PQOS_RETVAL_OK);

        ret = resctrl_mon_assoc_restore(lcore, "own");
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_resctrl_mon_assoc_restore_core_moved_to_another_class(
    void **state __attribute__((unused)))
{
        int ret;
        unsigned lcore = 1;
        struct pqos_cap cap;

        will_return_maybe(__wrap__pqos_get_cap, &cap);

        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 3);
        expect_value(__wrap_resctrl_alloc_assoc_get, lcore, lcore);
        will_return(__wrap_resctrl_alloc_assoc_get, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_assoc_get, 1);

        /* the group's directory in class 1 has lost the core */
        expect_value(__wrap_resctrl_mon_cpumask_read, class_id, 1);
        expect_string(__wrap_resctrl_mon_cpumask_read, resctrl_group, "own");
        will_return(__wrap_resctrl_mon_cpumask_read, PQOS_RETVAL_OK);
        expect_value(__wrap_resctrl_cpumask_get, lcore, lcore);
        will_return(__wrap_resctrl_cpumask_get, 0);

        /* a writer to resctrl has moved it to class 2 since, where the
         * search finds it in no group
         */
        will_return(resctrl_mon_is_supported, 1);
        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 3);
        expect_value(__wrap_resctrl_alloc_assoc_get, lcore, lcore);
        will_return(__wrap_resctrl_alloc_assoc_get, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_assoc_get, 2);
        expect_string(__wrap_scandir, dirp, "/sys/fs/resctrl/COS2/mon_groups/");
        will_return(__wrap_scandir, 0);

        /* it is put back in the group's directory in class 2, not 1 */
        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 3);
        expect_value(__wrap_resctrl_alloc_assoc_get, lcore, lcore);
        will_return(__wrap_resctrl_alloc_assoc_get, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_assoc_get, 2);
        will_return(resctrl_mon_is_supported, 1);
        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 3);
        expect_value(__wrap_resctrl_alloc_assoc_get, lcore, lcore);
        will_return(__wrap_resctrl_alloc_assoc_get, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_assoc_get, 2);
        expect_value(__wrap_resctrl_mon_mkdir, class_id, 2);
        expect_string(__wrap_resctrl_mon_mkdir, name, "own");
        will_return(__wrap_resctrl_mon_mkdir, PQOS_RETVAL_OK);
        expect_value(__wrap_resctrl_mon_cpumask_read, class_id, 2);
        expect_string(__wrap_resctrl_mon_cpumask_read, resctrl_group, "own");
        will_return(__wrap_resctrl_mon_cpumask_read, PQOS_RETVAL_OK);
        expect_value(__wrap_resctrl_cpumask_set, lcore, lcore);
        expect_value(__wrap_resctrl_mon_cpumask_write, class_id, 2);
        expect_string(__wrap_resctrl_mon_cpumask_write, resctrl_group, "own");
        will_return(__wrap_resctrl_mon_cpumask_write, PQOS_RETVAL_OK);

        ret = resctrl_mon_assoc_restore(lcore, "own");
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

/* a core in no class, as an offline one is, has no group to be put back in:
 * the restore, which a poll makes for each core, succeeds without a word,
 * and reads or makes nothing
 */
static void
test_resctrl_mon_assoc_restore_core_in_no_class(void **state
                                                __attribute__((unused)))
{
        int ret;
        unsigned lcore = 1;
        struct pqos_cap cap;

        will_return_maybe(__wrap__pqos_get_cap, &cap);

        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 2);
        expect_value(__wrap_resctrl_alloc_assoc_get, lcore, lcore);
        will_return(__wrap_resctrl_alloc_assoc_get, PQOS_RETVAL_RESOURCE);

        test_log_count_start();
        ret = resctrl_mon_assoc_restore(lcore, "own");
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(test_log_count_stop(), 0);
}

/* the core goes offline during the restore, after its group's directory was
 * found without it: the search finds it in no class, nor does the second
 * lookup, and the restore succeeds without making the group
 */
static void
test_resctrl_mon_assoc_restore_core_gone_offline(void **state
                                                 __attribute__((unused)))
{
        int ret;
        unsigned lcore = 1;
        struct pqos_cap cap;

        will_return_maybe(__wrap__pqos_get_cap, &cap);

        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 2);
        expect_value(__wrap_resctrl_alloc_assoc_get, lcore, lcore);
        will_return(__wrap_resctrl_alloc_assoc_get, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_assoc_get, 1);

        expect_value(__wrap_resctrl_mon_cpumask_read, class_id, 1);
        expect_string(__wrap_resctrl_mon_cpumask_read, resctrl_group, "own");
        will_return(__wrap_resctrl_mon_cpumask_read, PQOS_RETVAL_OK);
        expect_value(__wrap_resctrl_cpumask_get, lcore, lcore);
        will_return(__wrap_resctrl_cpumask_get, 0);

        will_return(resctrl_mon_is_supported, 1);
        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 2);
        expect_value(__wrap_resctrl_alloc_assoc_get, lcore, lcore);
        will_return(__wrap_resctrl_alloc_assoc_get, PQOS_RETVAL_RESOURCE);

        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 2);
        expect_value(__wrap_resctrl_alloc_assoc_get, lcore, lcore);
        will_return(__wrap_resctrl_alloc_assoc_get, PQOS_RETVAL_RESOURCE);

        test_log_count_start();
        ret = resctrl_mon_assoc_restore(lcore, "own");
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(test_log_count_stop(), 0);
}

/* the core comes back online, in the default class, after the poll removed
 * the group's directory it had left empty: the directory is made again and
 * the core put in it, without a word
 */
static void
test_resctrl_mon_assoc_restore_group_made_again(void **state
                                                __attribute__((unused)))
{
        int ret;
        unsigned lcore = 1;
        struct pqos_cap cap;

        will_return_maybe(__wrap__pqos_get_cap, &cap);

        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 2);
        expect_value(__wrap_resctrl_alloc_assoc_get, lcore, lcore);
        will_return(__wrap_resctrl_alloc_assoc_get, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_assoc_get, 0);

        /* the group has no directory in class 0 any more */
        expect_value(__wrap_resctrl_mon_cpumask_read, class_id, 0);
        expect_string(__wrap_resctrl_mon_cpumask_read, resctrl_group, "own");
        will_return(__wrap_resctrl_mon_cpumask_read, PQOS_RETVAL_ERROR);

        /* nor is the core in any group of the class */
        will_return(resctrl_mon_is_supported, 1);
        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 2);
        expect_value(__wrap_resctrl_alloc_assoc_get, lcore, lcore);
        will_return(__wrap_resctrl_alloc_assoc_get, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_assoc_get, 0);
        expect_string(__wrap_scandir, dirp, "/sys/fs/resctrl/mon_groups/");
        will_return(__wrap_scandir, 0);

        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 2);
        expect_value(__wrap_resctrl_alloc_assoc_get, lcore, lcore);
        will_return(__wrap_resctrl_alloc_assoc_get, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_assoc_get, 0);

        /* the directory is made in class 0, and the core put in it */
        will_return(resctrl_mon_is_supported, 1);
        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 2);
        expect_value(__wrap_resctrl_alloc_assoc_get, lcore, lcore);
        will_return(__wrap_resctrl_alloc_assoc_get, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_assoc_get, 0);
        expect_value(__wrap_resctrl_mon_mkdir, class_id, 0);
        expect_string(__wrap_resctrl_mon_mkdir, name, "own");
        will_return(__wrap_resctrl_mon_mkdir, PQOS_RETVAL_OK);
        expect_value(__wrap_resctrl_mon_cpumask_read, class_id, 0);
        expect_string(__wrap_resctrl_mon_cpumask_read, resctrl_group, "own");
        will_return(__wrap_resctrl_mon_cpumask_read, PQOS_RETVAL_OK);
        expect_value(__wrap_resctrl_cpumask_set, lcore, lcore);
        expect_value(__wrap_resctrl_mon_cpumask_write, class_id, 0);
        expect_string(__wrap_resctrl_mon_cpumask_write, resctrl_group, "own");
        will_return(__wrap_resctrl_mon_cpumask_write, PQOS_RETVAL_OK);

        test_log_count_start();
        ret = resctrl_mon_assoc_restore(lcore, "own");
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(test_log_count_stop(), 0);
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

static void
test_resctrl_mon_assoc_get_pid_small_buffer(void **state
                                            __attribute__((unused)))
{
        int ret;
        char name[4];
        struct pqos_cap cap;

        will_return_maybe(__wrap__pqos_get_cap, &cap);

        will_return(resctrl_mon_is_supported, 1);
        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 0);

        expect_string(__wrap_scandir, dirp, "/sys/fs/resctrl/mon_groups/");
        will_return(__wrap_scandir, 1);

        ret = resctrl_mon_assoc_get_pid(1, name, sizeof(name));
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
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

        /* error getting CLOS number */
        will_return(resctrl_mon_is_supported, 1);
        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_ERROR);

        ret = resctrl_mon_assoc_set_pid(pid, name);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
}

static void
test_resctrl_mon_assoc_set_pid_write(void **state __attribute__((unused)))
{
        pid_t pid = 1;
        char name[] = "test";
        struct pqos_cap cap;
        const struct {
                int err;
                int ret;
                unsigned logged;
        } answer[] = {
            /* the task ended after it was found: the caller skips it, and
             * nothing a user would see is said
             */
            {ESRCH, PQOS_RETVAL_PARAM, 0},
            /* and any other refusal is an error, which is */
            {EINVAL, PQOS_RETVAL_ERROR, 1},
        };
        unsigned i;

        will_return_maybe(__wrap__pqos_get_cap, &cap);

        for (i = 0; i < DIM(answer); i++) {
                will_return(resctrl_mon_is_supported, 1);
                will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
                will_return(__wrap_resctrl_alloc_get_grps_num, 0);
                expect_value(__wrap_resctrl_mon_mkdir, class_id, 0);
                expect_string(__wrap_resctrl_mon_mkdir, name, name);
                will_return(__wrap_resctrl_mon_mkdir, PQOS_RETVAL_OK);
                fclose_errno = answer[i].err;

                test_log_count_start();
                assert_int_equal(resctrl_mon_assoc_set_pid(pid, name),
                                 answer[i].ret);
                assert_int_equal(test_log_count_stop(), answer[i].logged);
        }
}

int
main(void)
{
        const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_resctrl_mon_assoc_get_unsupported),
            cmocka_unit_test(test_resctrl_mon_assoc_get_alloc_unsupported),
            cmocka_unit_test(test_resctrl_mon_assoc_get_alloc_default),
            cmocka_unit_test(test_resctrl_mon_assoc_get_alloc_nondefault),
            cmocka_unit_test(test_resctrl_mon_assoc_get_small_buffer),
            cmocka_unit_test(test_resctrl_mon_assoc_get_unassigned),
            cmocka_unit_test(test_resctrl_mon_assoc_get_error),
            cmocka_unit_test(test_resctrl_mon_assoc_get_core_in_no_class),
            cmocka_unit_test(test_resctrl_mon_assoc_set_unsupported),
            cmocka_unit_test(test_resctrl_mon_assoc_set),
            cmocka_unit_test(test_resctrl_mon_assoc_set_error),
            cmocka_unit_test(test_resctrl_mon_assoc_set_core_in_no_class),
            cmocka_unit_test(test_resctrl_mon_assoc_restore_core_in_its_group),
            cmocka_unit_test(
                test_resctrl_mon_assoc_restore_core_in_another_group),
            cmocka_unit_test(
                test_resctrl_mon_assoc_restore_own_group_unreadable),
            cmocka_unit_test(test_resctrl_mon_assoc_restore_core_moved_out),
            cmocka_unit_test(
                test_resctrl_mon_assoc_restore_core_moved_to_another_class),
            cmocka_unit_test(test_resctrl_mon_assoc_restore_core_in_no_class),
            cmocka_unit_test(test_resctrl_mon_assoc_restore_core_gone_offline),
            cmocka_unit_test(test_resctrl_mon_assoc_restore_group_made_again),
            cmocka_unit_test(test_resctrl_mon_assoc_get_pid_unsupported),
            cmocka_unit_test(test_resctrl_mon_assoc_get_pid_no_alloc),
            cmocka_unit_test(test_resctrl_mon_assoc_get_pid_unassigned),
            cmocka_unit_test(test_resctrl_mon_assoc_get_pid_alloc_default),
            cmocka_unit_test(test_resctrl_mon_assoc_get_pid_small_buffer),
            cmocka_unit_test(test_resctrl_mon_assoc_set_pid_unsupported),
            cmocka_unit_test(test_resctrl_mon_assoc_set_pid),
            cmocka_unit_test(test_resctrl_mon_assoc_set_pid_error),
            cmocka_unit_test(test_resctrl_mon_assoc_set_pid_write),
        };

        return cmocka_run_group_tests(tests, NULL, NULL);
}
