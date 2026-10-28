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
#include "monitoring.h"
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

/* the events resctrl_mon_is_event_supported() takes as supported: none, as
 * for a library that is not initialised, unless a case says otherwise
 */
static int events_supported;

int
resctrl_mon_is_event_supported(const enum pqos_mon_event event)
{
        return (events_supported & event) == event;
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

/* files a case serves, each a path and then its text, up to a NULL path */
static const char *const *served;

FILE *
__wrap_pqos_fopen(const char *name, const char *mode)
{
        FILE *fd = tmpfile();
        unsigned i;

        assert_non_null(fd);

        for (i = 0; served != NULL && served[i] != NULL; i += 2)
                if (strcmp(name, served[i]) == 0) {
                        fprintf(fd, "%s", served[i + 1]);
                        fseek(fd, 0, SEEK_SET);
                        return fd;
                }

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
        /* no class but the default group, which holds the core */
        expect_value(__wrap_resctrl_alloc_cpumask_read, class_id, 0);
        will_return(__wrap_resctrl_alloc_cpumask_read, PQOS_RETVAL_OK);
        expect_value(__wrap_resctrl_cpumask_get, lcore, lcore);
        will_return(__wrap_resctrl_cpumask_get, 1);

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
        /* no class but the default group, which holds the core */
        expect_value(__wrap_resctrl_alloc_cpumask_read, class_id, 0);
        will_return(__wrap_resctrl_alloc_cpumask_read, PQOS_RETVAL_OK);
        expect_value(__wrap_resctrl_cpumask_get, lcore, lcore);
        will_return(__wrap_resctrl_cpumask_get, 1);
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
        /* no class but the default group, which holds the core */
        expect_value(__wrap_resctrl_alloc_cpumask_read, class_id, 0);
        will_return(__wrap_resctrl_alloc_cpumask_read, PQOS_RETVAL_OK);
        expect_value(__wrap_resctrl_cpumask_get, lcore, lcore);
        will_return(__wrap_resctrl_cpumask_get, 1);

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
        /* no class but the default group, which holds the core */
        expect_value(__wrap_resctrl_alloc_cpumask_read, class_id, 0);
        will_return(__wrap_resctrl_alloc_cpumask_read, PQOS_RETVAL_OK);
        expect_value(__wrap_resctrl_cpumask_get, lcore, lcore);
        will_return(__wrap_resctrl_cpumask_get, 1);

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
        /* no class but the default group, which holds the core */
        expect_value(__wrap_resctrl_alloc_cpumask_read, class_id, 0);
        will_return(__wrap_resctrl_alloc_cpumask_read, PQOS_RETVAL_OK);
        expect_value(__wrap_resctrl_cpumask_get, lcore, lcore);
        will_return(__wrap_resctrl_cpumask_get, 1);

        expect_value(__wrap_resctrl_mon_mkdir, class_id, 0);
        expect_string(__wrap_resctrl_mon_mkdir, name, name);
        will_return(__wrap_resctrl_mon_mkdir, PQOS_RETVAL_ERROR);

        ret = resctrl_mon_assoc_set(lcore, name);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);

        /* error reading cpumask */
        will_return(resctrl_mon_is_supported, 1);
        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 0);
        /* no class but the default group, which holds the core */
        expect_value(__wrap_resctrl_alloc_cpumask_read, class_id, 0);
        will_return(__wrap_resctrl_alloc_cpumask_read, PQOS_RETVAL_OK);
        expect_value(__wrap_resctrl_cpumask_get, lcore, lcore);
        will_return(__wrap_resctrl_cpumask_get, 1);

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
        /* no class but the default group, which holds the core */
        expect_value(__wrap_resctrl_alloc_cpumask_read, class_id, 0);
        will_return(__wrap_resctrl_alloc_cpumask_read, PQOS_RETVAL_OK);
        expect_value(__wrap_resctrl_cpumask_get, lcore, lcore);
        will_return(__wrap_resctrl_cpumask_get, 1);

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
        int purged = 0;
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

        ret = resctrl_mon_assoc_restore(lcore, "own", &purged);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_resctrl_mon_assoc_restore_core_in_another_group(void **state
                                                     __attribute__((unused)))
{
        int ret;
        unsigned lcore = 1;
        int purged = 0;
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

        ret = resctrl_mon_assoc_restore(lcore, "own", &purged);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_resctrl_mon_assoc_restore_own_group_unreadable(void **state
                                                    __attribute__((unused)))
{
        int ret;
        unsigned lcore = 1;
        int purged = 0;
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

        ret = resctrl_mon_assoc_restore(lcore, "own", &purged);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_resctrl_mon_assoc_restore_core_moved_out(void **state
                                              __attribute__((unused)))
{
        int ret;
        unsigned lcore = 1;
        int purged = 0;
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

        /* so it is put back, in the class it is in, where the group has its
         * directory
         */
        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 2);
        expect_value(__wrap_resctrl_alloc_assoc_get, lcore, lcore);
        will_return(__wrap_resctrl_alloc_assoc_get, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_assoc_get, 1);
        expect_string(__wrap_pqos_dir_exists, path,
                      "/sys/fs/resctrl/COS1/mon_groups/own");
        will_return(__wrap_pqos_dir_exists, 1);
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

        ret = resctrl_mon_assoc_restore(lcore, "own", &purged);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_resctrl_mon_assoc_restore_core_moved_to_another_class(
    void **state __attribute__((unused)))
{
        int ret;
        unsigned lcore = 1;
        int purged = 0;
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

        /* it is put back in the group's directory in class 2, not 1, which
         * is made there, as the group still has one in class 1
         */
        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 3);
        expect_value(__wrap_resctrl_alloc_assoc_get, lcore, lcore);
        will_return(__wrap_resctrl_alloc_assoc_get, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_assoc_get, 2);
        expect_string(__wrap_pqos_dir_exists, path,
                      "/sys/fs/resctrl/COS2/mon_groups/own");
        will_return(__wrap_pqos_dir_exists, 0);
        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 3);
        expect_string(__wrap_pqos_dir_exists, path,
                      "/sys/fs/resctrl/mon_groups/own");
        will_return(__wrap_pqos_dir_exists, 0);
        expect_string(__wrap_pqos_dir_exists, path,
                      "/sys/fs/resctrl/COS1/mon_groups/own");
        will_return(__wrap_pqos_dir_exists, 1);
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

        ret = resctrl_mon_assoc_restore(lcore, "own", &purged);
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
        int purged = 0;
        struct pqos_cap cap;

        will_return_maybe(__wrap__pqos_get_cap, &cap);

        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 2);
        expect_value(__wrap_resctrl_alloc_assoc_get, lcore, lcore);
        will_return(__wrap_resctrl_alloc_assoc_get, PQOS_RETVAL_RESOURCE);

        test_log_count_start();
        ret = resctrl_mon_assoc_restore(lcore, "own", &purged);
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
        int purged = 0;
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
        ret = resctrl_mon_assoc_restore(lcore, "own", &purged);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(test_log_count_stop(), 0);
}

/**
 * @brief The restore of a core whose group has no directory in class 0
 *
 * The core's group directory there is gone, and the core is in no group of
 * the class, as after it came back online in the default class: the restore up
 * to where it decides whether to make the directory again
 *
 * @param [in] lcore the core
 */
static void
expect_restore_to_missing_directory(const unsigned lcore)
{
        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 2);
        expect_value(__wrap_resctrl_alloc_assoc_get, lcore, lcore);
        will_return(__wrap_resctrl_alloc_assoc_get, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_assoc_get, 0);

        expect_value(__wrap_resctrl_mon_cpumask_read, class_id, 0);
        expect_string(__wrap_resctrl_mon_cpumask_read, resctrl_group, "own");
        will_return(__wrap_resctrl_mon_cpumask_read, PQOS_RETVAL_ERROR);

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
}

/* the purge removed the group's last directory, as it does once a core gone
 * offline has left it empty, and the core is back, in the default class: the
 * directory is made again and the core put in it, without a word, and the
 * mark is cleared
 */
static void
test_resctrl_mon_assoc_restore_group_made_again(void **state
                                                __attribute__((unused)))
{
        int ret;
        unsigned lcore = 1;
        int purged = 1;
        struct pqos_cap cap;

        will_return_maybe(__wrap__pqos_get_cap, &cap);

        expect_restore_to_missing_directory(lcore);

        /* the directory is made in class 0, and the core put in it */
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
        ret = resctrl_mon_assoc_restore(lcore, "own", &purged);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(test_log_count_stop(), 0);
        assert_int_equal(purged, 0);
}

/* the purge removed the group's last directory, and something else has made
 * it again and put the core in it: the core is where it belongs, and the mark
 * is cleared, so that a later removal of that directory is not undone
 */
static void
test_resctrl_mon_assoc_restore_group_made_elsewhere(void **state
                                                    __attribute__((unused)))
{
        int ret;
        unsigned lcore = 1;
        int purged = 1;
        struct pqos_cap cap;

        will_return_maybe(__wrap__pqos_get_cap, &cap);

        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 2);
        expect_value(__wrap_resctrl_alloc_assoc_get, lcore, lcore);
        will_return(__wrap_resctrl_alloc_assoc_get, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_assoc_get, 0);
        expect_value(__wrap_resctrl_mon_cpumask_read, class_id, 0);
        expect_string(__wrap_resctrl_mon_cpumask_read, resctrl_group, "own");
        will_return(__wrap_resctrl_mon_cpumask_read, PQOS_RETVAL_OK);
        expect_value(__wrap_resctrl_cpumask_get, lcore, lcore);
        will_return(__wrap_resctrl_cpumask_get, 1);

        ret = resctrl_mon_assoc_restore(lcore, "own", &purged);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(purged, 0);
}

/* the purge removed the group's last directory, and while the restore looked
 * something else made it again and put the core in it: the search finds the
 * core in the group's own directory, and the mark is cleared there too
 */
static void
test_resctrl_mon_assoc_restore_own_group_found_again(void **state
                                                     __attribute__((unused)))
{
        int ret;
        unsigned lcore = 1;
        int purged = 1;
        struct pqos_cap cap;

        will_return_maybe(__wrap__pqos_get_cap, &cap);

        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 2);
        expect_value(__wrap_resctrl_alloc_assoc_get, lcore, lcore);
        will_return(__wrap_resctrl_alloc_assoc_get, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_assoc_get, 1);

        /* the group's directory is not there at the first read */
        expect_value(__wrap_resctrl_mon_cpumask_read, class_id, 1);
        expect_string(__wrap_resctrl_mon_cpumask_read, resctrl_group, "test");
        will_return(__wrap_resctrl_mon_cpumask_read, PQOS_RETVAL_ERROR);

        /* and the search of the class's groups finds the core in it */
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

        ret = resctrl_mon_assoc_restore(lcore, "test", &purged);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(purged, 0);
}

/* the group's directories are gone, but not through the purge, as when
 * pqos_mon_reset() or a writer to resctrl removed them: the group is not made
 * again, and the restore fails, which it says once
 */
static void
test_resctrl_mon_assoc_restore_group_removed_elsewhere(void **state
                                                       __attribute__((unused)))
{
        int ret;
        unsigned lcore = 1;
        int purged = 0;
        struct pqos_cap cap;

        will_return_maybe(__wrap__pqos_get_cap, &cap);

        expect_restore_to_missing_directory(lcore);
        expect_string(__wrap_pqos_dir_exists, path,
                      "/sys/fs/resctrl/mon_groups/own");
        will_return(__wrap_pqos_dir_exists, 0);
        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 2);
        expect_string(__wrap_pqos_dir_exists, path,
                      "/sys/fs/resctrl/COS1/mon_groups/own");
        will_return(__wrap_pqos_dir_exists, 0);

        test_log_count_start();
        ret = resctrl_mon_assoc_restore(lcore, "own", &purged);
        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);
        assert_int_equal(test_log_count_stop(), 1);
}

/* the restore up to its add: core 1 has left its group's directory in class
 * 1, and is in no group of the class, which the group still has a directory
 * in
 */
static void
expect_restore_to_add(const unsigned lcore)
{
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
        will_return(__wrap_resctrl_alloc_assoc_get, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_assoc_get, 1);
        expect_string(__wrap_scandir, dirp, "/sys/fs/resctrl/COS1/mon_groups/");
        will_return(__wrap_scandir, 0);

        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 2);
        expect_value(__wrap_resctrl_alloc_assoc_get, lcore, lcore);
        will_return(__wrap_resctrl_alloc_assoc_get, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_assoc_get, 1);
        expect_string(__wrap_pqos_dir_exists, path,
                      "/sys/fs/resctrl/COS1/mon_groups/own");
        will_return(__wrap_pqos_dir_exists, 1);
        expect_value(__wrap_resctrl_mon_mkdir, class_id, 1);
        expect_string(__wrap_resctrl_mon_mkdir, name, "own");
        will_return(__wrap_resctrl_mon_mkdir, PQOS_RETVAL_OK);
        expect_value(__wrap_resctrl_mon_cpumask_read, class_id, 1);
        expect_string(__wrap_resctrl_mon_cpumask_read, resctrl_group, "own");
        will_return(__wrap_resctrl_mon_cpumask_read, PQOS_RETVAL_OK);
        expect_value(__wrap_resctrl_cpumask_set, lcore, lcore);
        expect_value(__wrap_resctrl_mon_cpumask_write, class_id, 1);
        expect_string(__wrap_resctrl_mon_cpumask_write, resctrl_group, "own");
}

/* the add the restore makes once more after a failed one, which reads the
 * directory's mask again, up to its write
 */
static void
expect_add_again(const unsigned lcore)
{
        expect_value(__wrap_resctrl_mon_mkdir, class_id, 1);
        expect_string(__wrap_resctrl_mon_mkdir, name, "own");
        will_return(__wrap_resctrl_mon_mkdir, PQOS_RETVAL_OK);
        expect_value(__wrap_resctrl_mon_cpumask_read, class_id, 1);
        expect_string(__wrap_resctrl_mon_cpumask_read, resctrl_group, "own");
        will_return(__wrap_resctrl_mon_cpumask_read, PQOS_RETVAL_OK);
        expect_value(__wrap_resctrl_cpumask_set, lcore, lcore);
        expect_value(__wrap_resctrl_mon_cpumask_write, class_id, 1);
        expect_string(__wrap_resctrl_mon_cpumask_write, resctrl_group, "own");
}

/* the core goes offline after the restore's last lookup: the kernel refuses
 * it in the cpus file, and the core is in no class when looked up again.
 * Nothing to restore, and nothing said
 */
static void
test_resctrl_mon_assoc_restore_core_gone_offline_at_the_write(
    void **state __attribute__((unused)))
{
        int ret;
        unsigned lcore = 1;
        int purged = 0;
        struct pqos_cap cap;

        will_return_maybe(__wrap__pqos_get_cap, &cap);

        expect_restore_to_add(lcore);
        will_return(__wrap_resctrl_mon_cpumask_write, PQOS_RETVAL_ERROR);
        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 2);
        expect_value(__wrap_resctrl_alloc_assoc_get, lcore, lcore);
        will_return(__wrap_resctrl_alloc_assoc_get, PQOS_RETVAL_RESOURCE);

        test_log_count_start();
        ret = resctrl_mon_assoc_restore(lcore, "own", &purged);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(test_log_count_stop(), 0);
}

/* the write fails with the core still in its class, as when another core of
 * the directory's mask went offline since the read: the add, made once more,
 * reads the mask again and succeeds, without a word
 */
static void
test_resctrl_mon_assoc_restore_add_retried(void **state __attribute__((unused)))
{
        int ret;
        unsigned lcore = 1;
        int purged = 0;
        struct pqos_cap cap;

        will_return_maybe(__wrap__pqos_get_cap, &cap);

        expect_restore_to_add(lcore);
        will_return(__wrap_resctrl_mon_cpumask_write, PQOS_RETVAL_ERROR);
        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 2);
        expect_value(__wrap_resctrl_alloc_assoc_get, lcore, lcore);
        will_return(__wrap_resctrl_alloc_assoc_get, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_assoc_get, 1);
        expect_add_again(lcore);
        will_return(__wrap_resctrl_mon_cpumask_write, PQOS_RETVAL_OK);

        test_log_count_start();
        ret = resctrl_mon_assoc_restore(lcore, "own", &purged);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(test_log_count_stop(), 0);
}

/* the write fails with the core still in its class, and fails again when the
 * add is made once more: the restore fails, and says so once
 */
static void
test_resctrl_mon_assoc_restore_add_fails(void **state __attribute__((unused)))
{
        int ret;
        unsigned lcore = 1;
        int purged = 0;
        struct pqos_cap cap;

        will_return_maybe(__wrap__pqos_get_cap, &cap);

        expect_restore_to_add(lcore);
        will_return(__wrap_resctrl_mon_cpumask_write, PQOS_RETVAL_ERROR);
        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 2);
        expect_value(__wrap_resctrl_alloc_assoc_get, lcore, lcore);
        will_return(__wrap_resctrl_alloc_assoc_get, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_assoc_get, 1);
        expect_add_again(lcore);
        will_return(__wrap_resctrl_mon_cpumask_write, PQOS_RETVAL_ERROR);

        test_log_count_start();
        ret = resctrl_mon_assoc_restore(lcore, "own", &purged);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
        assert_int_equal(test_log_count_stop(), 1);
}

/* with no allocation class, a core the default group does not hold is
 * offline: nothing to restore, and nothing said
 */
static void
test_resctrl_mon_assoc_restore_core_offline_without_classes(
    void **state __attribute__((unused)))
{
        int ret;
        unsigned lcore = 1;
        int purged = 0;
        struct pqos_cap cap;

        will_return_maybe(__wrap__pqos_get_cap, &cap);

        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 0);
        expect_value(__wrap_resctrl_alloc_cpumask_read, class_id, 0);
        will_return(__wrap_resctrl_alloc_cpumask_read, PQOS_RETVAL_OK);
        expect_value(__wrap_resctrl_cpumask_get, lcore, lcore);
        will_return(__wrap_resctrl_cpumask_get, 0);

        test_log_count_start();
        ret = resctrl_mon_assoc_restore(lcore, "own", &purged);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(test_log_count_stop(), 0);
}

/* with no allocation class, a default group whose cpus file cannot be read
 * fails the restore, which says so once, and no core is looked for in what was
 * not read
 */
static void
test_resctrl_mon_assoc_restore_default_group_unreadable(void **state
                                                        __attribute__((unused)))
{
        int ret;
        unsigned lcore = 1;
        int purged = 0;
        struct pqos_cap cap;

        will_return_maybe(__wrap__pqos_get_cap, &cap);

        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 0);
        expect_value(__wrap_resctrl_alloc_cpumask_read, class_id, 0);
        will_return(__wrap_resctrl_alloc_cpumask_read, PQOS_RETVAL_ERROR);

        test_log_count_start();
        ret = resctrl_mon_assoc_restore(lcore, "own", &purged);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
        assert_int_equal(test_log_count_stop(), 1);
}

/* ======== resctrl_mon_poll ======== */

static int
poll_case_end(void **state __attribute__((unused)))
{
        served = NULL;
        events_supported = 0;

        return 0;
}

/* a lookup of \a lcore's class where there is no allocation class: in the
 * default group's cpus if \a online
 */
static void
expect_class_lookup(const unsigned lcore, const int online)
{
        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 0);
        expect_value(__wrap_resctrl_alloc_cpumask_read, class_id, 0);
        will_return(__wrap_resctrl_alloc_cpumask_read, PQOS_RETVAL_OK);
        expect_value(__wrap_resctrl_cpumask_get, lcore, lcore);
        will_return(__wrap_resctrl_cpumask_get, online);
}

/* the purge's check of whether another group is in pqos-g, whose cpus list
 * \a held alone, or no core where \a held is none of \a cpu's
 */
static void
expect_shared_check(const struct pqos_cpuinfo *cpu, const unsigned held)
{
        unsigned i;

        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 0);
        expect_string(__wrap_pqos_dir_exists, path,
                      "/sys/fs/resctrl/mon_groups/pqos-g");
        will_return(__wrap_pqos_dir_exists, 1);
        expect_value(__wrap_resctrl_mon_cpumask_read, class_id, 0);
        expect_string(__wrap_resctrl_mon_cpumask_read, resctrl_group, "pqos-g");
        will_return(__wrap_resctrl_mon_cpumask_read, PQOS_RETVAL_OK);
        for (i = 0; i < cpu->num_cores; i++) {
                expect_value(__wrap_resctrl_cpumask_get, lcore,
                             cpu->cores[i].lcore);
                will_return(__wrap_resctrl_cpumask_get,
                            cpu->cores[i].lcore == held);
        }
}

/* the directory pqos-g, looked for by the poll's read or by the purge */
static void
expect_group_directory(void)
{
        expect_string(__wrap_pqos_dir_exists, path,
                      "/sys/fs/resctrl/mon_groups/pqos-g");
        will_return(__wrap_pqos_dir_exists, 1);
}

/* a group of core 2 alone, with no allocation class. The poll that finds the
 * core offline reads the directory, which then lists no core, and the purge
 * saves its count, removes it and marks the group. The poll after it, with
 * the core back in the default class, makes the directory again from the
 * mark and puts the core in it, and the group's count goes on from the one
 * saved: its delta is the new directory's count, not a restart
 */
static void
test_resctrl_mon_poll_core_offline_and_back(void **state
                                            __attribute__((unused)))
{
        static const char *const offline[] = {
            "/sys/fs/resctrl/mon_groups/pqos-g/mon_data/mon_L3_00/"
            "mbm_total_bytes",
            "5000\n",
            "/sys/fs/resctrl/mon_groups/pqos-g/cpus_list",
            "\n",
            "/sys/fs/resctrl/mon_groups/pqos-g/tasks",
            "",
            NULL};
        static const char *const back[] = {
            "/sys/fs/resctrl/mon_groups/pqos-g/mon_data/mon_L3_00/"
            "mbm_total_bytes",
            "100\n", "/sys/fs/resctrl/mon_groups/pqos-g/cpus_list", "2\n",
            NULL};
        struct pqos_cap cap;
        struct pqos_cpuinfo *cpu;
        struct pqos_mon_data group;
        struct pqos_mon_data_internal intl;
        unsigned cores[] = {2};
        unsigned l3id[] = {0};
        unsigned i;

        cpu = calloc(1, sizeof(*cpu) + 4 * sizeof(cpu->cores[0]));
        assert_non_null(cpu);
        cpu->num_cores = 4;
        for (i = 0; i < cpu->num_cores; i++)
                cpu->cores[i].lcore = i;

        memset(&group, 0, sizeof(group));
        memset(&intl, 0, sizeof(intl));
        group.intl = &intl;
        group.num_cores = DIM(cores);
        group.cores = cores;
        group.event = PQOS_MON_EVENT_TMEM_BW;
        group.values.mbm_total = 4000;
        intl.resctrl.mon_group = strdup("pqos-g");
        assert_non_null(intl.resctrl.mon_group);
        intl.resctrl.l3id = l3id;
        intl.resctrl.num_l3id = DIM(l3id);
        intl.resctrl.valid_mbm_total = 1;

        will_return_maybe(__wrap__pqos_get_cap, &cap);
        will_return_maybe(__wrap__pqos_get_cpu, cpu);
        events_supported = PQOS_MON_EVENT_TMEM_BW;

        /* core 2 offline: nothing to restore, and the count read */
        served = offline;
        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 0);
        expect_class_lookup(2, 0);
        expect_group_directory();
        /* the purge: no group in the directory but this one, which no core
         * or task is in: the count saved, the directory removed
         */
        expect_shared_check(cpu, cpu->num_cores);
        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 0);
        expect_group_directory();
        expect_value(__wrap_resctrl_mon_rmdir, class_id, 0);
        expect_string(__wrap_resctrl_mon_rmdir, name, "pqos-g");
        will_return(__wrap_resctrl_mon_rmdir, PQOS_RETVAL_OK);

        assert_int_equal(resctrl_mon_poll(&group, PQOS_MON_EVENT_TMEM_BW),
                         PQOS_RETVAL_OK);
        assert_int_equal(group.values.mbm_total, 5000);
        assert_int_equal(group.values.mbm_total_delta, 1000);
        assert_int_equal(intl.resctrl.values_storage.mbm_total, 5000);
        assert_int_equal(intl.resctrl.purged, 1);

        /* core 2 back in the default class, its group's directory gone and
         * the core in no group: the directory is made again
         */
        served = back;
        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 0);
        expect_class_lookup(2, 1);
        expect_value(__wrap_resctrl_mon_cpumask_read, class_id, 0);
        expect_string(__wrap_resctrl_mon_cpumask_read, resctrl_group, "pqos-g");
        will_return(__wrap_resctrl_mon_cpumask_read, PQOS_RETVAL_ERROR);
        will_return(resctrl_mon_is_supported, 1);
        expect_class_lookup(2, 1);
        expect_string(__wrap_scandir, dirp, "/sys/fs/resctrl/mon_groups/");
        will_return(__wrap_scandir, 0);
        expect_class_lookup(2, 1);
        expect_value(__wrap_resctrl_mon_mkdir, class_id, 0);
        expect_string(__wrap_resctrl_mon_mkdir, name, "pqos-g");
        will_return(__wrap_resctrl_mon_mkdir, PQOS_RETVAL_OK);
        expect_value(__wrap_resctrl_mon_cpumask_read, class_id, 0);
        expect_string(__wrap_resctrl_mon_cpumask_read, resctrl_group, "pqos-g");
        will_return(__wrap_resctrl_mon_cpumask_read, PQOS_RETVAL_OK);
        expect_value(__wrap_resctrl_cpumask_set, lcore, 2);
        expect_value(__wrap_resctrl_mon_cpumask_write, class_id, 0);
        expect_string(__wrap_resctrl_mon_cpumask_write, resctrl_group,
                      "pqos-g");
        will_return(__wrap_resctrl_mon_cpumask_write, PQOS_RETVAL_OK);
        expect_group_directory();
        /* the purge: core 2 is in the directory, which is kept */
        expect_shared_check(cpu, 2);
        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 0);
        expect_group_directory();

        assert_int_equal(resctrl_mon_poll(&group, PQOS_MON_EVENT_TMEM_BW),
                         PQOS_RETVAL_OK);
        assert_int_equal(group.values.mbm_total, 5100);
        assert_int_equal(group.values.mbm_total_delta, 100);
        assert_int_equal(intl.resctrl.purged, 0);

        free(intl.resctrl.mon_group);
        free(cpu);
}

/* ======== resctrl_mon_count_delta ======== */

/* a first reading is the baseline, and growth is the delta */
static void
test_resctrl_mon_count_delta_grows(void **state __attribute__((unused)))
{
        int valid = 0;

        assert_int_equal(resctrl_mon_count_delta(0, 1000, &valid), 0);
        assert_int_equal(valid, 1);
        assert_int_equal(resctrl_mon_count_delta(1000, 1500, &valid), 500);
        assert_int_equal(resctrl_mon_count_delta(1500, 1500, &valid), 0);
        assert_int_equal(valid, 1);
}

/* a count that falls has started again, as a directory made again does: 0,
 * and the reading starts again from it, not a delta near 2^64
 */
static void
test_resctrl_mon_count_delta_fall_starts_again(void **state
                                               __attribute__((unused)))
{
        int valid = 1;

        assert_int_equal(resctrl_mon_count_delta(5000000, 100, &valid), 0);
        assert_int_equal(valid, 1);
        assert_int_equal(resctrl_mon_count_delta(100, 400, &valid), 300);
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
            cmocka_unit_test(
                test_resctrl_mon_assoc_restore_group_made_elsewhere),
            cmocka_unit_test(
                test_resctrl_mon_assoc_restore_group_removed_elsewhere),
            cmocka_unit_test(
                test_resctrl_mon_assoc_restore_own_group_found_again),
            cmocka_unit_test_teardown(
                test_resctrl_mon_poll_core_offline_and_back, poll_case_end),
            cmocka_unit_test(
                test_resctrl_mon_assoc_restore_core_gone_offline_at_the_write),
            cmocka_unit_test(test_resctrl_mon_assoc_restore_add_retried),
            cmocka_unit_test(test_resctrl_mon_assoc_restore_add_fails),
            cmocka_unit_test(
                test_resctrl_mon_assoc_restore_core_offline_without_classes),
            cmocka_unit_test(
                test_resctrl_mon_assoc_restore_default_group_unreadable),
            cmocka_unit_test(test_resctrl_mon_count_delta_grows),
            cmocka_unit_test(test_resctrl_mon_count_delta_fall_starts_again),
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
