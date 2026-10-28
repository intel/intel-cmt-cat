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

#include "mock_cap.h"
#include "mock_perf_monitoring.h"
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

int
resctrl_mon_is_event_supported(const enum pqos_mon_event event)
{
        if (event == PQOS_MON_EVENT_L3_OCCUP ||
            event == PQOS_MON_EVENT_LMEM_BW ||
            event == PQOS_MON_EVENT_RMEM_BW || event == PQOS_MON_EVENT_TMEM_BW)
                return mock_type(int);

        return 0;
}

int
resctrl_mon_assoc_set(const unsigned lcore, const char *name)
{
        return __wrap_resctrl_mon_assoc_set(lcore, name);
}

int
resctrl_mon_assoc_set_pid(const pid_t task, const char *name)
{
        return __wrap_resctrl_mon_assoc_set_pid(task, name);
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

/* a name of NULL is a group that could not be named */
char *
resctrl_mon_new_group(void)
{
        const char *name = mock_type(char *);

        return name != NULL ? strdup(name) : NULL;
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

/* the groups the mon_groups of CLOS 0 and of COS1 list, each up to a NULL;
 * none for a class whose list is NULL. A case that sets one has
 * mon_groups_unlist() for its teardown, which cmocka runs after a failed
 * case too, so that no case after it is given its list, nor reads one left
 * in its stack frame
 */
static const char *const *mon_groups_listed[2];

/* set, the read of a class's mon_groups fails, as with EMFILE */
static int mon_groups_unreadable;

static int
mon_groups_unlist(void **state __attribute__((unused)))
{
        memset(mon_groups_listed, 0, sizeof(mon_groups_listed));
        mon_groups_unreadable = 0;

        /* and the core groups a case started and did not stop, which the
         * library lists until then
         */
        resctrl_mon_fini();

        return 0;
}

int
__wrap_scandir(const char *restrict dirp,
               struct dirent ***restrict namelist,
               int (*filter)(const struct dirent *) __attribute__((unused)),
               int (*compar)(const struct dirent **, const struct dirent **)
                   __attribute__((unused)))
{
        const char *const *listed;
        int num = 0;
        int i;

        *namelist = NULL;

        if (mon_groups_unreadable) {
                errno = EMFILE;
                return -1;
        }
        if (strcmp(dirp, RESCTRL_PATH "/mon_groups") == 0 ||
            strcmp(dirp, RESCTRL_PATH "/mon_groups/") == 0)
                listed = mon_groups_listed[0];
        else if (strcmp(dirp, RESCTRL_PATH "/COS1/mon_groups") == 0 ||
                 strcmp(dirp, RESCTRL_PATH "/COS1/mon_groups/") == 0)
                listed = mon_groups_listed[1];
        else {
                assert_string_equal(dirp, "scandir");
                return -1;
        }

        while (listed != NULL && listed[num] != NULL)
                num++;
        if (num == 0)
                return 0;

        *namelist = calloc(num, sizeof(**namelist));
        assert_non_null(*namelist);
        for (i = 0; i < num; i++) {
                const size_t len = strlen(listed[i]);

                /* room for the name past the structure too, whatever length
                 * the C library declares d_name with
                 */
                (*namelist)[i] = calloc(1, sizeof(struct dirent) + len + 1);
                assert_non_null((*namelist)[i]);
                memcpy((*namelist)[i]->d_name, listed[i], len + 1);
        }

        return num;
}

FILE *
__wrap_pqos_fopen(const char *name, const char *mode)
{
        unsigned i;

        /* a file the library closes with fclose() has plain_close set, so
         * that no pqos_fclose() is expected for it
         */
        const struct {
                const char path[256];
                const char mode[5];
                const char text[256];
                int plain_close;
        } file[] = {
            {.path = RESCTRL_PATH_INFO_L3_MON "/max_threshold_occupancy",
             .mode = "r",
             .text = "16000"},
            {.path = RESCTRL_PATH "/mon_groups/pqos-other/tasks",
             .mode = "r",
             .text = "",
             .plain_close = 1},
            {.path = RESCTRL_PATH "/mon_groups/pqos-busy/tasks",
             .mode = "r",
             .text = "1234\n",
             .plain_close = 1},
            {.path = RESCTRL_PATH "/COS1/mon_groups/pqos-busy/tasks",
             .mode = "r",
             .text = "1234\n",
             .plain_close = 1},
            {.path = RESCTRL_PATH "/mon_groups/pqos-a/cpus_list",
             .mode = "r",
             .text = "",
             .plain_close = 1},
            {.path = RESCTRL_PATH "/mon_groups/pqos-a/tasks",
             .mode = "r",
             .text = "",
             .plain_close = 1},
            {.path = RESCTRL_PATH "/COS1/mon_groups/pqos-a/cpus_list",
             .mode = "r",
             .text = "5\n",
             .plain_close = 1},
            {.path = RESCTRL_PATH "/mon_groups/pqos-split/tasks",
             .mode = "r",
             .text = "",
             .plain_close = 1},
            {.path = RESCTRL_PATH "/COS1/mon_groups/pqos-split/tasks",
             .mode = "r",
             .text = "1234\n",
             .plain_close = 1},
            {.path = RESCTRL_PATH
             "/mon_groups/pqos-other/mon_data/mon_L3_00/llc_occupancy",
             .mode = "r",
             .text = "100\n"},
            {.path = RESCTRL_PATH
             "/mon_groups/pqos-other/mon_data/mon_L3_40/llc_occupancy",
             .mode = "r",
             .text = "100\n"},
            {.path = RESCTRL_PATH
             "/mon_groups/pqos-other/mon_data/mon_L3_62/llc_occupancy",
             .mode = "r",
             .text = "100\n"},
            {.path = RESCTRL_PATH
             "/mon_groups/pqos-other/mon_data/mon_L3_63/llc_occupancy",
             .mode = "r",
             .text = "100\n"},
        };

        for (i = 0; i < DIM(file); i++) {
                if (strcmp(mode, file[i].mode) == 0 &&
                    strcmp(name, file[i].path) == 0) {
                        FILE *fd = tmpfile();

                        assert_non_null(fd);

                        if (strcmp(mode, "r") == 0)
                                fprintf(fd, "%s", file[i].text);

                        fseek(fd, 0, SEEK_SET);
                        if (!file[i].plain_close)
                                expect_function_call(__wrap_pqos_fclose);
                        return fd;
                }
        }

        assert_string_equal(name, "fopen");

        return NULL;
}

int
__wrap_pqos_fclose(FILE *fd)
{
        function_called();
        assert_non_null(fd);

        return fclose(fd);
}

/* ======== resctrl_mon_start ======== */

static void
test_resctrl_mon_start_core(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        struct pqos_mon_data group;
        struct pqos_mon_data_internal intl;
        unsigned cores[] = {1};
        int ret;
        unsigned i;

        memset(&group, 0, sizeof(struct pqos_mon_data));
        group.intl = &intl;
        group.num_cores = 1;
        group.cores = cores;
        group.event = PQOS_MON_EVENT_TMEM_BW;
        memset(&intl, 0, sizeof(struct pqos_mon_data_internal));

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);
        will_return_maybe(resctrl_mon_is_supported, 1);
        will_return_maybe(resctrl_mon_is_event_supported, 1);
        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 1);
        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 1);
        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 1);
        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 1);
        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 1);

        /* start monitoring */
        will_return(resctrl_mon_new_group, "test");
        expect_value(__wrap_resctrl_mon_assoc_set, lcore, cores[0]);
        expect_string(__wrap_resctrl_mon_assoc_set, name, "test");
        will_return(__wrap_resctrl_mon_assoc_set, PQOS_RETVAL_OK);

        ret = resctrl_mon_start(&group);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_string_equal(intl.resctrl.mon_group, "test");

        /* stop monitoring */
        expect_string(__wrap_pqos_dir_exists, path,
                      RESCTRL_PATH "/mon_groups/test");
        will_return(__wrap_pqos_dir_exists, 1);
        expect_value(__wrap_resctrl_mon_cpumask_read, class_id, 0);
        expect_string(__wrap_resctrl_mon_cpumask_read, resctrl_group, "test");
        will_return(__wrap_resctrl_mon_cpumask_read, PQOS_RETVAL_OK);
        expect_value(__wrap_resctrl_cpumask_unset, lcore, cores[0]);
        expect_value(__wrap_resctrl_mon_cpumask_write, class_id, 0);
        expect_string(__wrap_resctrl_mon_cpumask_write, resctrl_group, "test");
        will_return(__wrap_resctrl_mon_cpumask_write, PQOS_RETVAL_OK);
        expect_string(__wrap_pqos_dir_exists, path,
                      RESCTRL_PATH "/mon_groups/test");
        will_return(__wrap_pqos_dir_exists, 1);
        expect_value(__wrap_resctrl_mon_cpumask_read, class_id, 0);
        expect_string(__wrap_resctrl_mon_cpumask_read, resctrl_group, "test");
        will_return(__wrap_resctrl_mon_cpumask_read, PQOS_RETVAL_OK);
        for (i = 0; i < data->cpu->num_cores; ++i) {
                expect_value(__wrap_resctrl_cpumask_get, lcore,
                             data->cpu->cores[i].lcore);
                will_return(__wrap_resctrl_cpumask_get, 0);
        }
        expect_value(__wrap_resctrl_mon_rmdir, class_id, 0);
        expect_string(__wrap_resctrl_mon_rmdir, name, "test");
        will_return(__wrap_resctrl_mon_rmdir, 0);

        ret = resctrl_mon_stop(&group);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

/* the cores of a group's directory in a class, as the parse reads them: lcore
 * alone
 */
static void
expect_group_cores(const struct pqos_cpuinfo *cpu,
                   const unsigned class_id,
                   const char *name,
                   const unsigned lcore)
{
        unsigned i;

        expect_value(__wrap_resctrl_mon_cpumask_read, class_id, class_id);
        expect_string(__wrap_resctrl_mon_cpumask_read, resctrl_group, name);
        will_return(__wrap_resctrl_mon_cpumask_read, PQOS_RETVAL_OK);
        for (i = 0; i < cpu->num_cores; i++) {
                expect_value(__wrap_resctrl_cpumask_get, lcore,
                             cpu->cores[i].lcore);
                will_return(__wrap_resctrl_cpumask_get,
                            cpu->cores[i].lcore == lcore);
        }
}

/**
 * @brief Starts core \a core where pqos-same, with core \a same on the same
 *        L3, and pqos-other, with core \a other on another L3, are listed
 *
 * pqos-same has a core of the L3 asked for, so its tasks are not listed (the
 * fopen mock fails a file it does not know): only those of pqos-other are,
 * which then has its occupancy read on the L3 of \a core, and is given the
 * core
 */
static void
start_shares_group_of_other_l3(void **state,
                               const unsigned core,
                               const unsigned same,
                               const unsigned other)
{
        struct test_data *data = (struct test_data *)*state;
        struct pqos_mon_data group;
        struct pqos_mon_data_internal intl;
        unsigned cores[] = {core};
        const char *const listed[] = {"pqos-same", "pqos-other", NULL};
        int ret;

        memset(&group, 0, sizeof(struct pqos_mon_data));
        group.intl = &intl;
        group.num_cores = 1;
        group.cores = cores;
        group.event = PQOS_MON_EVENT_TMEM_BW;
        memset(&intl, 0, sizeof(struct pqos_mon_data_internal));

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);
        will_return_maybe(resctrl_mon_is_supported, 1);
        will_return_maybe(resctrl_mon_is_event_supported, 1);
        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 1);
        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 1);

        mon_groups_listed[0] = listed;
        expect_group_cores(data->cpu, 0, "pqos-same", same);
        expect_group_cores(data->cpu, 0, "pqos-other", other);

        expect_string(__wrap_pqos_dir_exists, path,
                      RESCTRL_PATH "/mon_groups/pqos-other");
        will_return(__wrap_pqos_dir_exists, 1);
        expect_string(__wrap_pqos_dir_exists, path,
                      RESCTRL_PATH "/mon_groups/pqos-other");
        will_return(__wrap_pqos_dir_exists, 1);

        expect_value(__wrap_resctrl_mon_assoc_set, lcore, cores[0]);
        expect_string(__wrap_resctrl_mon_assoc_set, name, "pqos-other");
        will_return(__wrap_resctrl_mon_assoc_set, PQOS_RETVAL_OK);

        ret = resctrl_mon_start(&group);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_string_equal(intl.resctrl.mon_group, "pqos-other");

        free(intl.resctrl.mon_group);
        free(intl.resctrl.l3id);
}

static void
test_resctrl_mon_start_core_shares_group_of_other_l3(void **state)
{
        /* core 2 is on L3 0, core 1 too, and core 5 on L3 1 */
        start_shares_group_of_other_l3(state, 2, 1, 5);
}

/* how far the setups below move the test topology's L3 IDs up: cores 0 to 3
 * on L3 l3_shift, and cores 4 to 7 on L3 l3_shift + 1. l3ids_back(), their
 * teardown, which cmocka runs after a failed case too, moves them back
 */
static unsigned l3_shift;

static int
l3ids_shift(void **state, const unsigned by)
{
        struct test_data *data = (struct test_data *)*state;
        unsigned i;

        for (i = 0; i < data->cpu->num_cores; i++) {
                data->cpu->cores[i].l3_id += by;
                data->cpu->cores[i].l3cat_id += by;
        }
        l3_shift = by;

        return 0;
}

static int
l3ids_from_40(void **state)
{
        return l3ids_shift(state, 40);
}

static int
l3ids_from_62(void **state)
{
        return l3ids_shift(state, 62);
}

static int
l3ids_from_63(void **state)
{
        return l3ids_shift(state, 63);
}

static int
l3ids_back(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        unsigned i;

        for (i = 0; i < data->cpu->num_cores; i++) {
                data->cpu->cores[i].l3_id -= l3_shift;
                data->cpu->cores[i].l3cat_id -= l3_shift;
        }
        l3_shift = 0;

        return mon_groups_unlist(state);
}

static void
test_resctrl_mon_start_core_shares_group_of_other_l3_above_31(void **state)
{
        /* core 2 is on L3 40, core 1 too, and core 5 on L3 41: a mask of 32
         * bits lost both, and the start took pqos-same
         */
        start_shares_group_of_other_l3(state, 2, 1, 5);
}

static void
test_resctrl_mon_start_core_shares_group_on_l3_63(void **state)
{
        /* core 2 is on L3 62, core 1 too, and core 5 on L3 63: the parse
         * took a group with a core on L3 63 for one it cannot describe, and
         * the start made a new group
         */
        start_shares_group_of_other_l3(state, 2, 1, 5);
}

static void
test_resctrl_mon_start_core_on_l3_63_has_occupancy_read(void **state)
{
        /* core 6 is on L3 63, core 5 too, and core 1 on L3 62: the occupancy
         * of pqos-other was not read on L3 63, whose threshold went unchecked
         */
        start_shares_group_of_other_l3(state, 6, 5, 1);
}

/**
 * @brief A group of another L3 that monitors a task is not given the core
 *
 * @param [in] state test data
 * @param [in] max_clos number of classes the library is given: 0 as where
 *                      there is no allocation feature, which leaves the
 *                      default group's mon_groups, CLOS 0's, to look in
 */
static void
start_core_not_in_group_with_tasks(void **state, const unsigned max_clos)
{
        struct test_data *data = (struct test_data *)*state;
        struct pqos_mon_data group;
        struct pqos_mon_data_internal intl;
        unsigned cores[] = {2};
        const char *const listed[] = {"pqos-busy", NULL};
        int ret;

        memset(&group, 0, sizeof(struct pqos_mon_data));
        group.intl = &intl;
        group.num_cores = 1;
        group.cores = cores;
        group.event = PQOS_MON_EVENT_TMEM_BW;
        memset(&intl, 0, sizeof(struct pqos_mon_data_internal));

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);
        will_return_maybe(resctrl_mon_is_supported, 1);
        will_return_maybe(resctrl_mon_is_event_supported, 1);
        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, max_clos);
        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, max_clos);

        mon_groups_listed[0] = listed;
        expect_group_cores(data->cpu, 0, "pqos-busy", 5);
        expect_string(__wrap_pqos_dir_exists, path,
                      RESCTRL_PATH "/mon_groups/pqos-busy");
        will_return(__wrap_pqos_dir_exists, 1);

        will_return(resctrl_mon_new_group, "test");
        expect_value(__wrap_resctrl_mon_assoc_set, lcore, cores[0]);
        expect_string(__wrap_resctrl_mon_assoc_set, name, "test");
        will_return(__wrap_resctrl_mon_assoc_set, PQOS_RETVAL_OK);

        ret = resctrl_mon_start(&group);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_string_equal(intl.resctrl.mon_group, "test");

        free(intl.resctrl.mon_group);
        free(intl.resctrl.l3id);
}

static void
test_resctrl_mon_start_core_not_in_group_with_tasks(void **state)
{
        start_core_not_in_group_with_tasks(state, 1);
}

static void
test_resctrl_mon_start_core_not_in_group_with_tasks_no_alloc(void **state)
{
        start_core_not_in_group_with_tasks(state, 0);
}

static void
test_resctrl_mon_start_core_not_in_cos1_group_with_tasks(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        struct pqos_mon_data group;
        struct pqos_mon_data_internal intl;
        unsigned cores[] = {2};
        const char *const listed[] = {"pqos-busy", NULL};
        int ret;

        memset(&group, 0, sizeof(struct pqos_mon_data));
        group.intl = &intl;
        group.num_cores = 1;
        group.cores = cores;
        group.event = PQOS_MON_EVENT_TMEM_BW;
        memset(&intl, 0, sizeof(struct pqos_mon_data_internal));

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);
        will_return_maybe(resctrl_mon_is_supported, 1);
        will_return_maybe(resctrl_mon_is_event_supported, 1);
        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 2);
        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 2);

        /* a group of another L3 with no directory in CLOS 0, and one in COS1
         * that lists a task: a class without the group's directory is passed
         * over, and the classes after it are looked in
         */
        mon_groups_listed[1] = listed;
        expect_group_cores(data->cpu, 1, "pqos-busy", 5);
        expect_string(__wrap_pqos_dir_exists, path,
                      RESCTRL_PATH "/mon_groups/pqos-busy");
        will_return(__wrap_pqos_dir_exists, 0);
        expect_string(__wrap_pqos_dir_exists, path,
                      RESCTRL_PATH "/COS1/mon_groups/pqos-busy");
        will_return(__wrap_pqos_dir_exists, 1);

        will_return(resctrl_mon_new_group, "test");
        expect_value(__wrap_resctrl_mon_assoc_set, lcore, cores[0]);
        expect_string(__wrap_resctrl_mon_assoc_set, name, "test");
        will_return(__wrap_resctrl_mon_assoc_set, PQOS_RETVAL_OK);

        ret = resctrl_mon_start(&group);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_string_equal(intl.resctrl.mon_group, "test");

        free(intl.resctrl.mon_group);
        free(intl.resctrl.l3id);
}

static void
test_resctrl_mon_start_core_not_in_group_with_tasks_in_cos1(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        struct pqos_mon_data group;
        struct pqos_mon_data_internal intl;
        unsigned cores[] = {2};
        const char *const listed[] = {"pqos-split", NULL};
        int ret;

        memset(&group, 0, sizeof(struct pqos_mon_data));
        group.intl = &intl;
        group.num_cores = 1;
        group.cores = cores;
        group.event = PQOS_MON_EVENT_TMEM_BW;
        memset(&intl, 0, sizeof(struct pqos_mon_data_internal));

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);
        will_return_maybe(resctrl_mon_is_supported, 1);
        will_return_maybe(resctrl_mon_is_event_supported, 1);
        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 2);
        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 2);

        /* a group of another L3 with a directory in both classes, of which
         * CLOS 0's lists no task and COS1's one: a class whose directory
         * lists none is not the last looked in either
         */
        mon_groups_listed[0] = listed;
        mon_groups_listed[1] = listed;
        expect_group_cores(data->cpu, 0, "pqos-split", 5);
        expect_group_cores(data->cpu, 1, "pqos-split", 6);
        expect_string(__wrap_pqos_dir_exists, path,
                      RESCTRL_PATH "/mon_groups/pqos-split");
        will_return(__wrap_pqos_dir_exists, 1);
        expect_string(__wrap_pqos_dir_exists, path,
                      RESCTRL_PATH "/COS1/mon_groups/pqos-split");
        will_return(__wrap_pqos_dir_exists, 1);

        will_return(resctrl_mon_new_group, "test");
        expect_value(__wrap_resctrl_mon_assoc_set, lcore, cores[0]);
        expect_string(__wrap_resctrl_mon_assoc_set, name, "test");
        will_return(__wrap_resctrl_mon_assoc_set, PQOS_RETVAL_OK);

        ret = resctrl_mon_start(&group);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_string_equal(intl.resctrl.mon_group, "test");

        free(intl.resctrl.mon_group);
        free(intl.resctrl.l3id);
}

static void
test_resctrl_mon_start_pid(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        struct pqos_mon_data group;
        struct pqos_mon_data_internal intl;
        pid_t pids[] = {1};
        int ret;

        memset(&group, 0, sizeof(struct pqos_mon_data));
        group.intl = &intl;
        group.num_pids = 1;
        group.pids = pids;
        group.tid_map = pids;
        group.tid_nr = 1;
        group.event = PQOS_MON_EVENT_TMEM_BW;
        memset(&intl, 0, sizeof(struct pqos_mon_data_internal));

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);
        will_return_maybe(resctrl_mon_is_supported, 1);
        will_return_maybe(resctrl_mon_is_event_supported, 1);

        will_return(resctrl_mon_new_group, "test");
        expect_value(__wrap_resctrl_mon_assoc_set_pid, task, pids[0]);
        expect_string(__wrap_resctrl_mon_assoc_set_pid, name, "test");
        will_return(__wrap_resctrl_mon_assoc_set_pid, PQOS_RETVAL_OK);

        ret = resctrl_mon_start(&group);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_string_equal(intl.resctrl.mon_group, "test");

        expect_value(__wrap_resctrl_alloc_task_validate, task, pids[0]);
        will_return(__wrap_resctrl_alloc_task_validate, PQOS_RETVAL_OK);
        expect_value(__wrap_resctrl_mon_assoc_set_pid, task, pids[0]);
        expect_any(__wrap_resctrl_mon_assoc_set_pid, name);
        will_return(__wrap_resctrl_mon_assoc_set_pid, PQOS_RETVAL_OK);
        expect_value(__wrap_resctrl_mon_rmdir, class_id, 0);
        expect_string(__wrap_resctrl_mon_rmdir, name, "test");
        will_return(__wrap_resctrl_mon_rmdir, 0);
        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 1);
        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 1);

        ret = resctrl_mon_stop(&group);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_resctrl_mon_start_pid_cleanup_on_error(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        struct pqos_mon_data group;
        struct pqos_mon_data_internal intl;
        pid_t pids[] = {1};
        int ret;

        memset(&group, 0, sizeof(struct pqos_mon_data));
        group.intl = &intl;
        group.num_pids = 1;
        group.pids = pids;
        group.tid_map = pids;
        group.tid_nr = 1;
        group.event = PQOS_MON_EVENT_TMEM_BW;
        memset(&intl, 0, sizeof(struct pqos_mon_data_internal));

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);
        will_return_maybe(resctrl_mon_is_supported, 1);
        will_return_maybe(resctrl_mon_is_event_supported, 1);

        will_return(resctrl_mon_new_group, "test");
        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 1);
        expect_value(__wrap_resctrl_mon_assoc_set_pid, task, pids[0]);
        expect_string(__wrap_resctrl_mon_assoc_set_pid, name, "test");
        will_return(__wrap_resctrl_mon_assoc_set_pid, PQOS_RETVAL_BUSY);
        /* the thread is still there, so the failure stands */
        expect_value(__wrap_resctrl_alloc_task_validate, task, pids[0]);
        will_return(__wrap_resctrl_alloc_task_validate, PQOS_RETVAL_OK);
        expect_value(__wrap_resctrl_mon_rmdir, class_id, 0);
        expect_string(__wrap_resctrl_mon_rmdir, name, "test");
        will_return(__wrap_resctrl_mon_rmdir, 0);

        ret = resctrl_mon_start(&group);
        assert_int_equal(ret, PQOS_RETVAL_BUSY);
        assert_null(intl.resctrl.mon_group);
}

/*
 * Sets up a group of one process and its threads, as os_mon_start_pids()
 * passes it on
 */
static void
pid_group(struct pqos_mon_data *group,
          struct pqos_mon_data_internal *intl,
          pid_t *pid,
          pid_t *tids,
          const unsigned num_tids)
{
        memset(group, 0, sizeof(*group));
        memset(intl, 0, sizeof(*intl));
        group->intl = intl;
        group->num_pids = 1;
        group->pids = pid;
        group->tid_map = tids;
        group->tid_nr = num_tids;
        group->event = PQOS_MON_EVENT_TMEM_BW;
}

static void
test_resctrl_mon_start_pid_ended_thread(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        struct pqos_mon_data group;
        struct pqos_mon_data_internal intl;
        pid_t pid = 1;
        pid_t tids[] = {1, 2, 3, 4};
        struct pqos_mon_perf_ctx ctx[DIM(tids)];
        int ret;

        pid_group(&group, &intl, &pid, tids, DIM(tids));
        memset(ctx, 0, sizeof(ctx));
        intl.perf.ctx = ctx;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);
        will_return_maybe(resctrl_mon_is_supported, 1);
        will_return_maybe(resctrl_mon_is_event_supported, 1);

        will_return(resctrl_mon_new_group, "test");
        expect_value(__wrap_resctrl_mon_assoc_set_pid, task, tids[0]);
        expect_string(__wrap_resctrl_mon_assoc_set_pid, name, "test");
        will_return(__wrap_resctrl_mon_assoc_set_pid, PQOS_RETVAL_OK);
        /* the second thread has ended by the time it is added, which the add
         * says, and is skipped as it is
         */
        expect_value(__wrap_resctrl_mon_assoc_set_pid, task, tids[1]);
        expect_string(__wrap_resctrl_mon_assoc_set_pid, name, "test");
        will_return(__wrap_resctrl_mon_assoc_set_pid, PQOS_RETVAL_PARAM);
        /* the add of the third fails otherwise, and it is gone as well */
        expect_value(__wrap_resctrl_mon_assoc_set_pid, task, tids[2]);
        expect_string(__wrap_resctrl_mon_assoc_set_pid, name, "test");
        will_return(__wrap_resctrl_mon_assoc_set_pid, PQOS_RETVAL_ERROR);
        expect_value(__wrap_resctrl_alloc_task_validate, task, tids[2]);
        will_return(__wrap_resctrl_alloc_task_validate, PQOS_RETVAL_ERROR);
        expect_value(__wrap_resctrl_mon_assoc_set_pid, task, tids[3]);
        expect_string(__wrap_resctrl_mon_assoc_set_pid, name, "test");
        will_return(__wrap_resctrl_mon_assoc_set_pid, PQOS_RETVAL_OK);

        test_log_count_start();
        ret = resctrl_mon_start(&group);
        /* a race the start takes in its stride says nothing a user sees */
        assert_int_equal(test_log_count_stop(), 0);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_string_equal(intl.resctrl.mon_group, "test");
        /* and the two skipped are marked, for the stop to pass over */
        assert_int_equal(ctx[0].untracked, 0);
        assert_int_equal(ctx[1].untracked, 1);
        assert_int_equal(ctx[2].untracked, 1);
        assert_int_equal(ctx[3].untracked, 0);

        free(intl.resctrl.mon_group);
}

static void
test_resctrl_mon_start_pid_thread_perf_found_ended(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        struct pqos_mon_data group;
        struct pqos_mon_data_internal intl;
        pid_t pid = 1;
        pid_t tids[] = {1, 2, 3};
        struct pqos_mon_perf_ctx ctx[DIM(tids)];
        int ret;

        pid_group(&group, &intl, &pid, tids, DIM(tids));
        memset(ctx, 0, sizeof(ctx));
        intl.perf.ctx = ctx;
        /* a perf start found the second thread ended: its ID may be another
         * task's by now, and that task is not to be moved into this group
         */
        ctx[1].untracked = 1;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);
        will_return_maybe(resctrl_mon_is_supported, 1);
        will_return_maybe(resctrl_mon_is_event_supported, 1);

        will_return(resctrl_mon_new_group, "test");
        expect_value(__wrap_resctrl_mon_assoc_set_pid, task, tids[0]);
        expect_string(__wrap_resctrl_mon_assoc_set_pid, name, "test");
        will_return(__wrap_resctrl_mon_assoc_set_pid, PQOS_RETVAL_OK);
        expect_value(__wrap_resctrl_mon_assoc_set_pid, task, tids[2]);
        expect_string(__wrap_resctrl_mon_assoc_set_pid, name, "test");
        will_return(__wrap_resctrl_mon_assoc_set_pid, PQOS_RETVAL_OK);

        ret = resctrl_mon_start(&group);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_string_equal(intl.resctrl.mon_group, "test");

        free(intl.resctrl.mon_group);
}

static void
test_resctrl_mon_start_pid_every_thread_ended(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        struct pqos_mon_data group;
        struct pqos_mon_data_internal intl;
        pid_t pid = 1;
        pid_t tids[] = {1, 2};
        unsigned i;
        int ret;

        pid_group(&group, &intl, &pid, tids, DIM(tids));

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);
        will_return_maybe(resctrl_mon_is_supported, 1);
        will_return_maybe(resctrl_mon_is_event_supported, 1);

        will_return(resctrl_mon_new_group, "test");
        for (i = 0; i < DIM(tids); i++) {
                expect_value(__wrap_resctrl_mon_assoc_set_pid, task, tids[i]);
                expect_string(__wrap_resctrl_mon_assoc_set_pid, name, "test");
                will_return(__wrap_resctrl_mon_assoc_set_pid,
                            PQOS_RETVAL_PARAM);
        }
        /* nothing was added, and the group is removed again */
        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 1);
        expect_value(__wrap_resctrl_mon_rmdir, class_id, 0);
        expect_string(__wrap_resctrl_mon_rmdir, name, "test");
        will_return(__wrap_resctrl_mon_rmdir, 0);

        ret = resctrl_mon_start(&group);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
        assert_null(intl.resctrl.mon_group);
}

static void
test_resctrl_mon_start_pid_exited_leader(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        struct pqos_mon_data group;
        struct pqos_mon_data_internal intl;
        pid_t pid = 5;
        pid_t tids[] = {5, 6};
        int ret;

        pid_group(&group, &intl, &pid, tids, DIM(tids));

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);
        will_return_maybe(resctrl_mon_is_supported, 1);
        will_return_maybe(resctrl_mon_is_event_supported, 1);

        will_return(resctrl_mon_new_group, "test");
        /* the leader has exited and the kernel refused its write, while kill()
         * still finds the process by its other thread: no check is made, which
         * would find it and fail the start
         */
        expect_value(__wrap_resctrl_mon_assoc_set_pid, task, tids[0]);
        expect_string(__wrap_resctrl_mon_assoc_set_pid, name, "test");
        will_return(__wrap_resctrl_mon_assoc_set_pid, PQOS_RETVAL_PARAM);
        expect_value(__wrap_resctrl_mon_assoc_set_pid, task, tids[1]);
        expect_string(__wrap_resctrl_mon_assoc_set_pid, name, "test");
        will_return(__wrap_resctrl_mon_assoc_set_pid, PQOS_RETVAL_OK);

        ret = resctrl_mon_start(&group);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_string_equal(intl.resctrl.mon_group, "test");

        free(intl.resctrl.mon_group);
}

/*
 * The group os_mon_add_pids() passes on: the name of the group the process's
 * first threads are already monitored in, so its directories are not this
 * start's to remove
 */
static void
pid_group_added_to(struct pqos_mon_data *group,
                   struct pqos_mon_data_internal *intl,
                   pid_t *pid,
                   pid_t *tids,
                   const unsigned num_tids)
{
        pid_group(group, intl, pid, tids, num_tids);
        intl->resctrl.mon_group = strdup("existing");
        assert_non_null(intl->resctrl.mon_group);
}

static void
test_resctrl_mon_start_pid_add_every_thread_ended(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        struct pqos_mon_data group;
        struct pqos_mon_data_internal intl;
        pid_t pid = 5;
        pid_t tids[] = {5, 6};
        unsigned i;
        int ret;

        pid_group_added_to(&group, &intl, &pid, tids, DIM(tids));

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);
        will_return_maybe(resctrl_mon_is_supported, 1);
        will_return_maybe(resctrl_mon_is_event_supported, 1);

        /* the group being added to has its directory in class 0, and the
         * threads added are in class 1, which gets one made for them
         */
        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 2);
        expect_string(__wrap_pqos_dir_exists, path,
                      RESCTRL_PATH "/mon_groups/existing");
        will_return(__wrap_pqos_dir_exists, 1);
        expect_string(__wrap_pqos_dir_exists, path,
                      RESCTRL_PATH "/COS1/mon_groups/existing");
        will_return(__wrap_pqos_dir_exists, 0);
        for (i = 0; i < DIM(tids); i++) {
                expect_value(__wrap_resctrl_mon_assoc_set_pid, task, tids[i]);
                expect_string(__wrap_resctrl_mon_assoc_set_pid, name,
                              "existing");
                will_return(__wrap_resctrl_mon_assoc_set_pid,
                            PQOS_RETVAL_PARAM);
        }
        /* nothing was added: the directory made for it goes, and the
         * group's own stays
         */
        expect_value(__wrap_resctrl_mon_rmdir, class_id, 1);
        expect_string(__wrap_resctrl_mon_rmdir, name, "existing");
        will_return(__wrap_resctrl_mon_rmdir, 0);

        ret = resctrl_mon_start(&group);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
        assert_null(intl.resctrl.mon_group);
}

static void
test_resctrl_mon_start_pid_add_failure(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        struct pqos_mon_data group;
        struct pqos_mon_data_internal intl;
        pid_t pid = 5;
        pid_t tids[] = {5};
        int ret;

        pid_group_added_to(&group, &intl, &pid, tids, DIM(tids));

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);
        will_return_maybe(resctrl_mon_is_supported, 1);
        will_return_maybe(resctrl_mon_is_event_supported, 1);

        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 1);
        expect_string(__wrap_pqos_dir_exists, path,
                      RESCTRL_PATH "/mon_groups/existing");
        will_return(__wrap_pqos_dir_exists, 1);
        expect_value(__wrap_resctrl_mon_assoc_set_pid, task, tids[0]);
        expect_string(__wrap_resctrl_mon_assoc_set_pid, name, "existing");
        will_return(__wrap_resctrl_mon_assoc_set_pid, PQOS_RETVAL_BUSY);
        expect_value(__wrap_resctrl_alloc_task_validate, task, tids[0]);
        will_return(__wrap_resctrl_alloc_task_validate, PQOS_RETVAL_OK);
        /* a failure of any kind leaves the group being added to standing */

        ret = resctrl_mon_start(&group);
        assert_int_equal(ret, PQOS_RETVAL_BUSY);
        assert_null(intl.resctrl.mon_group);
}

static void
test_resctrl_mon_stop_pid_thread_ended_at_the_write(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        struct pqos_mon_data group;
        struct pqos_mon_data_internal intl;
        pid_t pid = 1;
        pid_t tids[] = {1, 2};
        int ret;

        pid_group(&group, &intl, &pid, tids, DIM(tids));
        intl.resctrl.mon_group = strdup("test");
        assert_non_null(intl.resctrl.mon_group);

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);
        will_return_maybe(resctrl_mon_is_supported, 1);

        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 1);
        /* the first thread is there when checked and gone when written */
        expect_value(__wrap_resctrl_alloc_task_validate, task, tids[0]);
        will_return(__wrap_resctrl_alloc_task_validate, PQOS_RETVAL_OK);
        expect_value(__wrap_resctrl_mon_assoc_set_pid, task, tids[0]);
        expect_any(__wrap_resctrl_mon_assoc_set_pid, name);
        will_return(__wrap_resctrl_mon_assoc_set_pid, PQOS_RETVAL_PARAM);
        /* so the stop goes on to the next, and removes the group */
        expect_value(__wrap_resctrl_alloc_task_validate, task, tids[1]);
        will_return(__wrap_resctrl_alloc_task_validate, PQOS_RETVAL_OK);
        expect_value(__wrap_resctrl_mon_assoc_set_pid, task, tids[1]);
        expect_any(__wrap_resctrl_mon_assoc_set_pid, name);
        will_return(__wrap_resctrl_mon_assoc_set_pid, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 1);
        expect_value(__wrap_resctrl_mon_rmdir, class_id, 0);
        expect_string(__wrap_resctrl_mon_rmdir, name, "test");
        will_return(__wrap_resctrl_mon_rmdir, 0);

        ret = resctrl_mon_stop(&group);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_null(intl.resctrl.mon_group);
}

static void
test_resctrl_mon_stop_pid_thread_never_added(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        struct pqos_mon_data group;
        struct pqos_mon_data_internal intl;
        pid_t pid = 1;
        pid_t tids[] = {1, 2};
        struct pqos_mon_perf_ctx ctx[DIM(tids)];
        int ret;

        pid_group(&group, &intl, &pid, tids, DIM(tids));
        memset(ctx, 0, sizeof(ctx));
        intl.perf.ctx = ctx;
        intl.resctrl.mon_group = strdup("test");
        assert_non_null(intl.resctrl.mon_group);
        /* the start found the first thread ended and did not add it: a task
         * with its ID now is somebody else's, and stays in its own group
         */
        ctx[0].untracked = 1;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);
        will_return_maybe(resctrl_mon_is_supported, 1);

        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 1);
        expect_value(__wrap_resctrl_alloc_task_validate, task, tids[1]);
        will_return(__wrap_resctrl_alloc_task_validate, PQOS_RETVAL_OK);
        expect_value(__wrap_resctrl_mon_assoc_set_pid, task, tids[1]);
        expect_any(__wrap_resctrl_mon_assoc_set_pid, name);
        will_return(__wrap_resctrl_mon_assoc_set_pid, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 1);
        expect_value(__wrap_resctrl_mon_rmdir, class_id, 0);
        expect_string(__wrap_resctrl_mon_rmdir, name, "test");
        will_return(__wrap_resctrl_mon_rmdir, 0);

        ret = resctrl_mon_stop(&group);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_null(intl.resctrl.mon_group);
}

static void
test_resctrl_mon_start_pid_add_fails_before_any_thread(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        struct pqos_mon_data group;
        struct pqos_mon_data_internal intl;
        pid_t pid = 5;
        pid_t tids[] = {5, 6};
        struct pqos_mon_perf_ctx ctx[DIM(tids)];
        int ret;

        pid_group_added_to(&group, &intl, &pid, tids, DIM(tids));
        memset(ctx, 0, sizeof(ctx));
        intl.perf.ctx = ctx;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);
        will_return_maybe(resctrl_mon_is_supported, 1);
        will_return_maybe(resctrl_mon_is_event_supported, 1);

        /* the add cannot see which classes the group has directories in, and
         * fails before it moves a thread
         */
        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_ERROR);

        ret = resctrl_mon_start(&group);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
        assert_null(intl.resctrl.mon_group);
        /* so neither thread is this group's to move out */
        assert_int_equal(ctx[0].untracked, 1);
        assert_int_equal(ctx[1].untracked, 1);

        /* and the stop that rolls the add back moves neither: wherever they
         * are monitored, they stay
         */
        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 1);

        ret = resctrl_mon_stop(&group);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_resctrl_mon_start_pid_unnamed(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        struct pqos_mon_data group;
        struct pqos_mon_data_internal intl;
        pid_t pid = 1;
        pid_t tids[] = {1, 2};
        struct pqos_mon_perf_ctx ctx[DIM(tids)];
        int ret;

        pid_group(&group, &intl, &pid, tids, DIM(tids));
        memset(ctx, 0, sizeof(ctx));
        intl.perf.ctx = ctx;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);
        will_return_maybe(resctrl_mon_is_supported, 1);
        will_return_maybe(resctrl_mon_is_event_supported, 1);

        /* no name for the group, so no thread is moved anywhere */
        will_return(resctrl_mon_new_group, NULL);

        ret = resctrl_mon_start(&group);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
        assert_null(intl.resctrl.mon_group);
        assert_int_equal(ctx[0].untracked, 1);
        assert_int_equal(ctx[1].untracked, 1);
}

static void
test_resctrl_mon_start_pid_fails_midway(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        struct pqos_mon_data group;
        struct pqos_mon_data_internal intl;
        pid_t pid = 1;
        pid_t tids[] = {1, 2, 3};
        struct pqos_mon_perf_ctx ctx[DIM(tids)];
        int ret;

        pid_group(&group, &intl, &pid, tids, DIM(tids));
        memset(ctx, 0, sizeof(ctx));
        intl.perf.ctx = ctx;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);
        will_return_maybe(resctrl_mon_is_supported, 1);
        will_return_maybe(resctrl_mon_is_event_supported, 1);

        will_return(resctrl_mon_new_group, "test");
        expect_value(__wrap_resctrl_mon_assoc_set_pid, task, tids[0]);
        expect_string(__wrap_resctrl_mon_assoc_set_pid, name, "test");
        will_return(__wrap_resctrl_mon_assoc_set_pid, PQOS_RETVAL_OK);
        /* the second thread is there and cannot be moved in */
        expect_value(__wrap_resctrl_mon_assoc_set_pid, task, tids[1]);
        expect_string(__wrap_resctrl_mon_assoc_set_pid, name, "test");
        will_return(__wrap_resctrl_mon_assoc_set_pid, PQOS_RETVAL_BUSY);
        expect_value(__wrap_resctrl_alloc_task_validate, task, tids[1]);
        will_return(__wrap_resctrl_alloc_task_validate, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 1);
        expect_value(__wrap_resctrl_mon_rmdir, class_id, 0);
        expect_string(__wrap_resctrl_mon_rmdir, name, "test");
        will_return(__wrap_resctrl_mon_rmdir, 0);

        ret = resctrl_mon_start(&group);
        assert_int_equal(ret, PQOS_RETVAL_BUSY);
        /* the first was moved in and is for the stop to move out; the one
         * that failed, and the one never reached, are not
         */
        assert_int_equal(ctx[0].untracked, 0);
        assert_int_equal(ctx[1].untracked, 1);
        assert_int_equal(ctx[2].untracked, 1);
}

static void
test_resctrl_mon_start_core_failure(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        struct pqos_mon_data group;
        struct pqos_mon_data_internal intl;
        unsigned cores[] = {1};
        int ret;

        memset(&group, 0, sizeof(struct pqos_mon_data));
        memset(&intl, 0, sizeof(struct pqos_mon_data_internal));
        group.intl = &intl;
        group.num_cores = DIM(cores);
        group.cores = cores;
        group.event = PQOS_MON_EVENT_TMEM_BW;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);
        will_return_maybe(resctrl_mon_is_supported, 1);
        will_return_maybe(resctrl_mon_is_event_supported, 1);
        /* the class count, for the assign and for the core groups it parses */
        will_return_count(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK, 1);
        will_return_count(__wrap_resctrl_alloc_get_grps_num, 1, 1);
        will_return_count(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK, 1);
        will_return_count(__wrap_resctrl_alloc_get_grps_num, 1, 1);

        will_return(resctrl_mon_new_group, "test");
        expect_value(__wrap_resctrl_mon_assoc_set, lcore, cores[0]);
        expect_string(__wrap_resctrl_mon_assoc_set, name, "test");
        will_return(__wrap_resctrl_mon_assoc_set, PQOS_RETVAL_ERROR);

        ret = resctrl_mon_start(&group);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
        /* the list of L3 IDs is gone, and cleared, so that the stop that rolls
         * the start back does not free it a second time
         */
        assert_null(intl.resctrl.l3id);
        assert_int_equal(intl.resctrl.num_l3id, 0);

        free(intl.resctrl.mon_group);
}

/* ======== core groups started in one directory ======== */

static void
core_group(struct pqos_mon_data *group,
           struct pqos_mon_data_internal *intl,
           unsigned *cores,
           const unsigned num_cores)
{
        memset(group, 0, sizeof(*group));
        memset(intl, 0, sizeof(*intl));
        group->intl = intl;
        group->num_cores = num_cores;
        group->cores = cores;
        group->event = PQOS_MON_EVENT_TMEM_BW;
}

/**
 * @brief Starts core group \a group, where no directory is listed, in a new
 *        one named \a name
 *
 * The case gives the cap, the topology and resctrl_mon_is_supported(), and no
 * event, so that no occupancy is read
 */
static void
start_core_group(struct pqos_mon_data *group,
                 struct pqos_mon_data_internal *intl,
                 unsigned *cores,
                 const unsigned num_cores,
                 const char *name)
{
        unsigned i;
        int ret;

        core_group(group, intl, cores, num_cores);

        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 1);
        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 1);
        will_return(resctrl_mon_new_group, name);
        for (i = 0; i < num_cores; i++) {
                expect_value(__wrap_resctrl_mon_assoc_set, lcore, cores[i]);
                expect_string(__wrap_resctrl_mon_assoc_set, name, name);
                will_return(__wrap_resctrl_mon_assoc_set, PQOS_RETVAL_OK);
        }

        ret = resctrl_mon_start(group);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_string_equal(intl->resctrl.mon_group, name);
}

static void
core_group_free(struct pqos_mon_data_internal *intl)
{
        free(intl->resctrl.mon_group);
        free(intl->resctrl.l3id);
}

/* pqos-a has cores 2 and 5, on L3s 0 and 1. Core 2 has gone offline, which
 * leaves core 5 alone in the directory: a group started on core 1, of L3 0, is
 * not given pqos-a, which is still read on L3 0, but a directory of its own
 */
static void
test_resctrl_mon_start_core_not_in_group_of_offline_core(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        struct pqos_mon_data a, b;
        struct pqos_mon_data_internal intl_a, intl_b;
        unsigned cores_a[] = {2, 5};
        unsigned cores_b[] = {1};
        const char *const listed[] = {"pqos-a", NULL};
        int ret;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);
        will_return_maybe(resctrl_mon_is_supported, 1);
        will_return_maybe(resctrl_mon_is_event_supported, 0);

        start_core_group(&a, &intl_a, cores_a, DIM(cores_a), "pqos-a");

        core_group(&b, &intl_b, cores_b, DIM(cores_b));
        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 1);
        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 1);
        mon_groups_listed[0] = listed;
        expect_group_cores(data->cpu, 0, "pqos-a", 5);
        will_return(resctrl_mon_new_group, "pqos-b");
        expect_value(__wrap_resctrl_mon_assoc_set, lcore, cores_b[0]);
        expect_string(__wrap_resctrl_mon_assoc_set, name, "pqos-b");
        will_return(__wrap_resctrl_mon_assoc_set, PQOS_RETVAL_OK);

        ret = resctrl_mon_start(&b);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_string_equal(intl_b.resctrl.mon_group, "pqos-b");

        core_group_free(&intl_a);
        core_group_free(&intl_b);
}

/* groups on cores 2 and 5, of L3s 0 and 1, are in one directory, as two
 * groups of different L3s can be given one, and core 2 is offline: the stop
 * of core 5's group leaves the directory, which core 2's group is still in,
 * and that group's stop removes it
 */
static void
test_resctrl_mon_stop_leaves_directory_of_group_offline(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        struct pqos_mon_data a, b;
        struct pqos_mon_data_internal intl_a, intl_b;
        unsigned cores_a[] = {2};
        unsigned cores_b[] = {5};
        unsigned i;
        int ret;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);
        will_return_maybe(resctrl_mon_is_supported, 1);
        will_return_maybe(resctrl_mon_is_event_supported, 0);

        start_core_group(&a, &intl_a, cores_a, DIM(cores_a), "pqos-ab");
        start_core_group(&b, &intl_b, cores_b, DIM(cores_b), "pqos-ab");

        /* core 5 is taken out of the directory, which is left */
        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 1);
        expect_string(__wrap_pqos_dir_exists, path,
                      RESCTRL_PATH "/mon_groups/pqos-ab");
        will_return(__wrap_pqos_dir_exists, 1);
        expect_value(__wrap_resctrl_mon_cpumask_read, class_id, 0);
        expect_string(__wrap_resctrl_mon_cpumask_read, resctrl_group,
                      "pqos-ab");
        will_return(__wrap_resctrl_mon_cpumask_read, PQOS_RETVAL_OK);
        expect_value(__wrap_resctrl_cpumask_unset, lcore, cores_b[0]);
        expect_value(__wrap_resctrl_mon_cpumask_write, class_id, 0);
        expect_string(__wrap_resctrl_mon_cpumask_write, resctrl_group,
                      "pqos-ab");
        will_return(__wrap_resctrl_mon_cpumask_write, PQOS_RETVAL_OK);

        ret = resctrl_mon_stop(&b);
        assert_int_equal(ret, PQOS_RETVAL_OK);

        /* then core 2's group, with no core in the directory and no other
         * group listed: the directory is removed
         */
        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 1);
        expect_string(__wrap_pqos_dir_exists, path,
                      RESCTRL_PATH "/mon_groups/pqos-ab");
        will_return(__wrap_pqos_dir_exists, 1);
        expect_value(__wrap_resctrl_mon_cpumask_read, class_id, 0);
        expect_string(__wrap_resctrl_mon_cpumask_read, resctrl_group,
                      "pqos-ab");
        will_return(__wrap_resctrl_mon_cpumask_read, PQOS_RETVAL_OK);
        expect_value(__wrap_resctrl_cpumask_unset, lcore, cores_a[0]);
        expect_value(__wrap_resctrl_mon_cpumask_write, class_id, 0);
        expect_string(__wrap_resctrl_mon_cpumask_write, resctrl_group,
                      "pqos-ab");
        will_return(__wrap_resctrl_mon_cpumask_write, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 1);
        expect_string(__wrap_pqos_dir_exists, path,
                      RESCTRL_PATH "/mon_groups/pqos-ab");
        will_return(__wrap_pqos_dir_exists, 1);
        expect_value(__wrap_resctrl_mon_cpumask_read, class_id, 0);
        expect_string(__wrap_resctrl_mon_cpumask_read, resctrl_group,
                      "pqos-ab");
        will_return(__wrap_resctrl_mon_cpumask_read, PQOS_RETVAL_OK);
        for (i = 0; i < data->cpu->num_cores; ++i) {
                expect_value(__wrap_resctrl_cpumask_get, lcore,
                             data->cpu->cores[i].lcore);
                will_return(__wrap_resctrl_cpumask_get, 0);
        }
        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 1);
        expect_value(__wrap_resctrl_mon_rmdir, class_id, 0);
        expect_string(__wrap_resctrl_mon_rmdir, name, "pqos-ab");
        will_return(__wrap_resctrl_mon_rmdir, 0);

        ret = resctrl_mon_stop(&a);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

/* a core group the start could not name, as one whose start failed before it
 * was given a directory, is stopped with no file read or written: with no
 * name, the paths would be the class's own, whose cores the stop would take
 * out of it
 */
static void
test_resctrl_mon_stop_unnamed_core_group(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        struct pqos_mon_data group;
        struct pqos_mon_data_internal intl;
        unsigned cores[] = {1};
        int ret;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);
        core_group(&group, &intl, cores, DIM(cores));
        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 2);

        ret = resctrl_mon_stop(&group);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

/* ======== resctrl_mon_purge ======== */

/* the purge removes the empty directory of a group that no other group is
 * in, and marks the group, whose last directory that was
 */
static void
test_resctrl_mon_purge_marks_group_of_last_directory(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        struct pqos_mon_data a;
        struct pqos_mon_data_internal intl_a;
        unsigned cores_a[] = {2};
        unsigned i;
        int ret;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);
        will_return_maybe(resctrl_mon_is_supported, 1);
        will_return_maybe(resctrl_mon_is_event_supported, 0);

        start_core_group(&a, &intl_a, cores_a, DIM(cores_a), "pqos-a");

        /* no other group is in the directory, which has no core */
        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 1);
        expect_string(__wrap_pqos_dir_exists, path,
                      RESCTRL_PATH "/mon_groups/pqos-a");
        will_return(__wrap_pqos_dir_exists, 1);
        expect_value(__wrap_resctrl_mon_cpumask_read, class_id, 0);
        expect_string(__wrap_resctrl_mon_cpumask_read, resctrl_group, "pqos-a");
        will_return(__wrap_resctrl_mon_cpumask_read, PQOS_RETVAL_OK);
        for (i = 0; i < data->cpu->num_cores; ++i) {
                expect_value(__wrap_resctrl_cpumask_get, lcore,
                             data->cpu->cores[i].lcore);
                will_return(__wrap_resctrl_cpumask_get, 0);
        }

        /* nor a task: it is removed */
        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 1);
        expect_string(__wrap_pqos_dir_exists, path,
                      RESCTRL_PATH "/mon_groups/pqos-a");
        will_return(__wrap_pqos_dir_exists, 1);
        expect_value(__wrap_resctrl_mon_rmdir, class_id, 0);
        expect_string(__wrap_resctrl_mon_rmdir, name, "pqos-a");
        will_return(__wrap_resctrl_mon_rmdir, 0);

        ret = resctrl_mon_purge(&a);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(intl_a.resctrl.purged, 1);

        core_group_free(&intl_a);
}

/* a group with directories in classes 0 and 1: the purge removes the empty
 * one, in class 0, and keeps the one core 5 is in, so that the directory it
 * removed was not the group's last, and the group is not marked
 */
static void
test_resctrl_mon_purge_marks_no_group_with_a_directory_left(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        struct pqos_mon_data a;
        struct pqos_mon_data_internal intl_a;
        unsigned cores_a[] = {2, 5};
        unsigned i;
        int ret;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);
        will_return_maybe(resctrl_mon_is_supported, 1);
        will_return_maybe(resctrl_mon_is_event_supported, 0);

        start_core_group(&a, &intl_a, cores_a, DIM(cores_a), "pqos-a");

        /* no other group in either directory: class 0's has no core, and
         * class 1's has the group's core 5
         */
        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 2);
        expect_string(__wrap_pqos_dir_exists, path,
                      RESCTRL_PATH "/mon_groups/pqos-a");
        will_return(__wrap_pqos_dir_exists, 1);
        expect_value(__wrap_resctrl_mon_cpumask_read, class_id, 0);
        expect_string(__wrap_resctrl_mon_cpumask_read, resctrl_group, "pqos-a");
        will_return(__wrap_resctrl_mon_cpumask_read, PQOS_RETVAL_OK);
        for (i = 0; i < data->cpu->num_cores; ++i) {
                expect_value(__wrap_resctrl_cpumask_get, lcore,
                             data->cpu->cores[i].lcore);
                will_return(__wrap_resctrl_cpumask_get, 0);
        }
        expect_string(__wrap_pqos_dir_exists, path,
                      RESCTRL_PATH "/COS1/mon_groups/pqos-a");
        will_return(__wrap_pqos_dir_exists, 1);
        expect_group_cores(data->cpu, 1, "pqos-a", 5);

        /* class 0's is removed, class 1's kept */
        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 2);
        expect_string(__wrap_pqos_dir_exists, path,
                      RESCTRL_PATH "/mon_groups/pqos-a");
        will_return(__wrap_pqos_dir_exists, 1);
        expect_value(__wrap_resctrl_mon_rmdir, class_id, 0);
        expect_string(__wrap_resctrl_mon_rmdir, name, "pqos-a");
        will_return(__wrap_resctrl_mon_rmdir, 0);
        expect_string(__wrap_pqos_dir_exists, path,
                      RESCTRL_PATH "/COS1/mon_groups/pqos-a");
        will_return(__wrap_pqos_dir_exists, 1);

        ret = resctrl_mon_purge(&a);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(intl_a.resctrl.purged, 0);

        core_group_free(&intl_a);
}

/* the purge of a group whose directory, pqos-a in CLOS 0 alone, no other
 * group, core or task is in, and whose removal gives \a removal
 */
static void
expect_pqos_a_purge(const struct pqos_cpuinfo *cpu, const int removal)
{
        unsigned i;

        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 1);
        expect_string(__wrap_pqos_dir_exists, path,
                      RESCTRL_PATH "/mon_groups/pqos-a");
        will_return(__wrap_pqos_dir_exists, 1);
        expect_value(__wrap_resctrl_mon_cpumask_read, class_id, 0);
        expect_string(__wrap_resctrl_mon_cpumask_read, resctrl_group, "pqos-a");
        will_return(__wrap_resctrl_mon_cpumask_read, PQOS_RETVAL_OK);
        for (i = 0; i < cpu->num_cores; ++i) {
                expect_value(__wrap_resctrl_cpumask_get, lcore,
                             cpu->cores[i].lcore);
                will_return(__wrap_resctrl_cpumask_get, 0);
        }
        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 1);
        expect_string(__wrap_pqos_dir_exists, path,
                      RESCTRL_PATH "/mon_groups/pqos-a");
        will_return(__wrap_pqos_dir_exists, 1);
        expect_value(__wrap_resctrl_mon_rmdir, class_id, 0);
        expect_string(__wrap_resctrl_mon_rmdir, name, "pqos-a");
        will_return(__wrap_resctrl_mon_rmdir, removal);
}

/* a group that its caller moved after the start, as one kept by value in an
 * array that grew, is still the group the list has: the purge of the copy
 * finds no other group in its directory and removes it, and the copy's
 * unlisting takes the group off the list, so that another group with a
 * directory of that name is alone in it
 */
static void
test_resctrl_mon_purge_of_a_group_moved_since_its_start(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        struct pqos_mon_data a, moved, b;
        struct pqos_mon_data_internal intl_a, intl_b;
        unsigned cores_a[] = {2};
        unsigned cores_b[] = {3};
        int ret;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);
        will_return_maybe(resctrl_mon_is_supported, 1);
        will_return_maybe(resctrl_mon_is_event_supported, 0);

        start_core_group(&a, &intl_a, cores_a, DIM(cores_a), "pqos-a");
        moved = a;
        memset(&a, 0, sizeof(a));

        expect_pqos_a_purge(data->cpu, PQOS_RETVAL_OK);
        ret = resctrl_mon_purge(&moved);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(intl_a.resctrl.purged, 1);

        resctrl_mon_unlist(&moved);
        core_group(&b, &intl_b, cores_b, DIM(cores_b));
        intl_b.resctrl.mon_group = strdup("pqos-a");
        assert_non_null(intl_b.resctrl.mon_group);
        expect_pqos_a_purge(data->cpu, PQOS_RETVAL_OK);
        ret = resctrl_mon_purge(&b);
        assert_int_equal(ret, PQOS_RETVAL_OK);

        core_group_free(&intl_a);
        core_group_free(&intl_b);
}

/* pqos-a was started for core 5, on L3 64, past the L3s a mask holds, and
 * core 5 has gone offline, so that only the list of started groups says the
 * directory is in use, and that list cannot say on which L3: a group started
 * on core 1, of L3 63, is not given pqos-a, whose cores are not read, but a
 * directory of its own
 */
static void
test_resctrl_mon_start_core_not_in_group_started_past_l3_63(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        struct pqos_mon_data a, b;
        struct pqos_mon_data_internal intl_a, intl_b;
        unsigned cores_a[] = {5};
        unsigned cores_b[] = {1};
        const char *const listed[] = {"pqos-a", NULL};
        int ret;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);
        will_return_maybe(resctrl_mon_is_supported, 1);
        will_return_maybe(resctrl_mon_is_event_supported, 0);

        /* past L3 63, the start makes a directory without a look at those
         * there are
         */
        core_group(&a, &intl_a, cores_a, DIM(cores_a));
        will_return(resctrl_mon_new_group, "pqos-a");
        expect_value(__wrap_resctrl_mon_assoc_set, lcore, cores_a[0]);
        expect_string(__wrap_resctrl_mon_assoc_set, name, "pqos-a");
        will_return(__wrap_resctrl_mon_assoc_set, PQOS_RETVAL_OK);
        ret = resctrl_mon_start(&a);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_string_equal(intl_a.resctrl.mon_group, "pqos-a");

        core_group(&b, &intl_b, cores_b, DIM(cores_b));
        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 1);
        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 1);
        mon_groups_listed[0] = listed;
        will_return(resctrl_mon_new_group, "pqos-b");
        expect_value(__wrap_resctrl_mon_assoc_set, lcore, cores_b[0]);
        expect_string(__wrap_resctrl_mon_assoc_set, name, "pqos-b");
        will_return(__wrap_resctrl_mon_assoc_set, PQOS_RETVAL_OK);

        ret = resctrl_mon_start(&b);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_string_equal(intl_b.resctrl.mon_group, "pqos-b");

        core_group_free(&intl_a);
        core_group_free(&intl_b);
}

/* the group's last directory goes between the purge's look at it and its
 * removal, as another writer may remove it: the removal finds it gone, which
 * is not the purge's removal, and the group is not marked
 */
static void
test_resctrl_mon_purge_of_a_directory_already_gone(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        struct pqos_mon_data a;
        struct pqos_mon_data_internal intl_a;
        unsigned cores_a[] = {2};
        int ret;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);
        will_return_maybe(resctrl_mon_is_supported, 1);
        will_return_maybe(resctrl_mon_is_event_supported, 0);

        start_core_group(&a, &intl_a, cores_a, DIM(cores_a), "pqos-a");

        expect_pqos_a_purge(data->cpu, PQOS_RETVAL_RESOURCE);
        ret = resctrl_mon_purge(&a);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(intl_a.resctrl.purged, 0);

        core_group_free(&intl_a);
}

/* a reset that fails before it removes anything, at the read of mon_groups,
 * leaves the marks as they were: a purge after it marks the group with the
 * count of no reset
 */
static void
test_resctrl_mon_reset_that_fails_first_leaves_the_marks(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        struct pqos_mon_data a;
        struct pqos_mon_data_internal intl_a;
        unsigned cores_a[] = {2};
        int ret;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);
        will_return_maybe(resctrl_mon_is_supported, 1);
        will_return_maybe(resctrl_mon_is_event_supported, 0);

        mon_groups_unreadable = 1;
        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 1);
        assert_int_equal(resctrl_mon_reset(), PQOS_RETVAL_ERROR);
        mon_groups_unreadable = 0;

        start_core_group(&a, &intl_a, cores_a, DIM(cores_a), "pqos-a");

        expect_pqos_a_purge(data->cpu, PQOS_RETVAL_OK);
        ret = resctrl_mon_purge(&a);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(intl_a.resctrl.purged, 1);

        core_group_free(&intl_a);
}

/* a reset that finds a directory gone at its removal, as another writer may
 * remove it, goes on, and counts: a purge after it marks the group with the
 * count of one reset
 */
static void
test_resctrl_mon_reset_of_a_directory_already_gone(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        static const char *const listed[] = {"pqos-x", NULL};
        struct pqos_mon_data a;
        struct pqos_mon_data_internal intl_a;
        unsigned cores_a[] = {2};
        int ret;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);
        will_return_maybe(resctrl_mon_is_supported, 1);
        will_return_maybe(resctrl_mon_is_event_supported, 0);

        mon_groups_listed[0] = listed;
        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 1);
        expect_value(__wrap_resctrl_mon_rmdir, class_id, 0);
        expect_string(__wrap_resctrl_mon_rmdir, name, "pqos-x");
        will_return(__wrap_resctrl_mon_rmdir, PQOS_RETVAL_RESOURCE);
        assert_int_equal(resctrl_mon_reset(), PQOS_RETVAL_OK);
        mon_groups_listed[0] = NULL;

        start_core_group(&a, &intl_a, cores_a, DIM(cores_a), "pqos-a");

        expect_pqos_a_purge(data->cpu, PQOS_RETVAL_OK);
        ret = resctrl_mon_purge(&a);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(intl_a.resctrl.purged, 2);

        core_group_free(&intl_a);
}

/* a stop that finds its group's directory gone at the removal, as another
 * writer may remove it, has nothing left to do, and succeeds
 */
static void
test_resctrl_mon_stop_of_a_directory_already_gone(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        struct pqos_mon_data a;
        struct pqos_mon_data_internal intl_a;
        unsigned cores_a[] = {2};
        unsigned i;
        int ret;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);
        will_return_maybe(resctrl_mon_is_supported, 1);
        will_return_maybe(resctrl_mon_is_event_supported, 0);

        start_core_group(&a, &intl_a, cores_a, DIM(cores_a), "pqos-a");

        for (i = 0; i < 3; i++) {
                will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
                will_return(__wrap_resctrl_alloc_get_grps_num, 1);
        }
        expect_string(__wrap_pqos_dir_exists, path,
                      RESCTRL_PATH "/mon_groups/pqos-a");
        will_return(__wrap_pqos_dir_exists, 1);
        expect_value(__wrap_resctrl_mon_cpumask_read, class_id, 0);
        expect_string(__wrap_resctrl_mon_cpumask_read, resctrl_group, "pqos-a");
        will_return(__wrap_resctrl_mon_cpumask_read, PQOS_RETVAL_OK);
        expect_value(__wrap_resctrl_cpumask_unset, lcore, 2);
        expect_value(__wrap_resctrl_mon_cpumask_write, class_id, 0);
        expect_string(__wrap_resctrl_mon_cpumask_write, resctrl_group,
                      "pqos-a");
        will_return(__wrap_resctrl_mon_cpumask_write, PQOS_RETVAL_OK);
        expect_string(__wrap_pqos_dir_exists, path,
                      RESCTRL_PATH "/mon_groups/pqos-a");
        will_return(__wrap_pqos_dir_exists, 1);
        expect_value(__wrap_resctrl_mon_cpumask_read, class_id, 0);
        expect_string(__wrap_resctrl_mon_cpumask_read, resctrl_group, "pqos-a");
        will_return(__wrap_resctrl_mon_cpumask_read, PQOS_RETVAL_OK);
        for (i = 0; i < data->cpu->num_cores; ++i) {
                expect_value(__wrap_resctrl_cpumask_get, lcore,
                             data->cpu->cores[i].lcore);
                will_return(__wrap_resctrl_cpumask_get, 0);
        }
        expect_value(__wrap_resctrl_mon_rmdir, class_id, 0);
        expect_string(__wrap_resctrl_mon_rmdir, name, "pqos-a");
        will_return(__wrap_resctrl_mon_rmdir, PQOS_RETVAL_RESOURCE);

        ret = resctrl_mon_stop(&a);
        assert_int_equal(ret, PQOS_RETVAL_OK);

        core_group_free(&intl_a);
}

/* after a pqos_mon_reset(), the purge marks the group with the count of
 * resets the restore then compares, so that the directory it removes is made
 * again: the mark is 2 after one reset
 */
static void
test_resctrl_mon_purge_after_a_reset_marks_its_count(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        struct pqos_mon_data a;
        struct pqos_mon_data_internal intl_a;
        unsigned cores_a[] = {2};
        unsigned i;
        int ret;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);
        will_return_maybe(resctrl_mon_is_supported, 1);
        will_return_maybe(resctrl_mon_is_event_supported, 0);

        /* the reset, which finds no group to remove */
        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 1);
        assert_int_equal(resctrl_mon_reset(), PQOS_RETVAL_OK);

        start_core_group(&a, &intl_a, cores_a, DIM(cores_a), "pqos-a");

        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 1);
        expect_string(__wrap_pqos_dir_exists, path,
                      RESCTRL_PATH "/mon_groups/pqos-a");
        will_return(__wrap_pqos_dir_exists, 1);
        expect_value(__wrap_resctrl_mon_cpumask_read, class_id, 0);
        expect_string(__wrap_resctrl_mon_cpumask_read, resctrl_group, "pqos-a");
        will_return(__wrap_resctrl_mon_cpumask_read, PQOS_RETVAL_OK);
        for (i = 0; i < data->cpu->num_cores; ++i) {
                expect_value(__wrap_resctrl_cpumask_get, lcore,
                             data->cpu->cores[i].lcore);
                will_return(__wrap_resctrl_cpumask_get, 0);
        }
        will_return(__wrap_resctrl_alloc_get_grps_num, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_grps_num, 1);
        expect_string(__wrap_pqos_dir_exists, path,
                      RESCTRL_PATH "/mon_groups/pqos-a");
        will_return(__wrap_pqos_dir_exists, 1);
        expect_value(__wrap_resctrl_mon_rmdir, class_id, 0);
        expect_string(__wrap_resctrl_mon_rmdir, name, "pqos-a");
        will_return(__wrap_resctrl_mon_rmdir, 0);

        ret = resctrl_mon_purge(&a);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(intl_a.resctrl.purged, 2);

        core_group_free(&intl_a);
}

/* groups on cores 2 and 5 are in one directory, with no core in it, as when
 * both cores are offline: the purge of either leaves the directory, which
 * the other is in, and reads nothing
 */
static void
test_resctrl_mon_purge_leaves_directory_another_group_is_in(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        struct pqos_mon_data a, b;
        struct pqos_mon_data_internal intl_a, intl_b;
        unsigned cores_a[] = {2};
        unsigned cores_b[] = {5};
        int ret;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);
        will_return_maybe(resctrl_mon_is_supported, 1);
        will_return_maybe(resctrl_mon_is_event_supported, 0);

        start_core_group(&a, &intl_a, cores_a, DIM(cores_a), "pqos-ab");
        start_core_group(&b, &intl_b, cores_b, DIM(cores_b), "pqos-ab");

        ret = resctrl_mon_purge(&a);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        ret = resctrl_mon_purge(&b);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(intl_a.resctrl.purged, 0);
        assert_int_equal(intl_b.resctrl.purged, 0);

        core_group_free(&intl_a);
        core_group_free(&intl_b);
}

int
main(void)
{
        int result = 0;

        const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_resctrl_mon_start_core),
            cmocka_unit_test_teardown(
                test_resctrl_mon_start_core_shares_group_of_other_l3,
                mon_groups_unlist),
            cmocka_unit_test_setup_teardown(
                test_resctrl_mon_start_core_shares_group_of_other_l3_above_31,
                l3ids_from_40, l3ids_back),
            cmocka_unit_test_setup_teardown(
                test_resctrl_mon_start_core_shares_group_on_l3_63,
                l3ids_from_62, l3ids_back),
            cmocka_unit_test_setup_teardown(
                test_resctrl_mon_start_core_on_l3_63_has_occupancy_read,
                l3ids_from_62, l3ids_back),
            cmocka_unit_test_setup_teardown(
                test_resctrl_mon_start_core_not_in_group_started_past_l3_63,
                l3ids_from_63, l3ids_back),
            cmocka_unit_test_teardown(
                test_resctrl_mon_start_core_not_in_group_with_tasks,
                mon_groups_unlist),
            cmocka_unit_test_teardown(
                test_resctrl_mon_start_core_not_in_group_with_tasks_no_alloc,
                mon_groups_unlist),
            cmocka_unit_test_teardown(
                test_resctrl_mon_start_core_not_in_cos1_group_with_tasks,
                mon_groups_unlist),
            cmocka_unit_test_teardown(
                test_resctrl_mon_start_core_not_in_group_with_tasks_in_cos1,
                mon_groups_unlist),
            cmocka_unit_test(test_resctrl_mon_start_pid),
            cmocka_unit_test(test_resctrl_mon_start_pid_cleanup_on_error),
            cmocka_unit_test(test_resctrl_mon_start_pid_ended_thread),
            cmocka_unit_test(
                test_resctrl_mon_start_pid_thread_perf_found_ended),
            cmocka_unit_test(test_resctrl_mon_start_pid_every_thread_ended),
            cmocka_unit_test(test_resctrl_mon_start_pid_exited_leader),
            cmocka_unit_test(test_resctrl_mon_start_pid_add_every_thread_ended),
            cmocka_unit_test(test_resctrl_mon_start_pid_add_failure),
            cmocka_unit_test(
                test_resctrl_mon_stop_pid_thread_ended_at_the_write),
            cmocka_unit_test(test_resctrl_mon_stop_pid_thread_never_added),
            cmocka_unit_test(
                test_resctrl_mon_start_pid_add_fails_before_any_thread),
            cmocka_unit_test(test_resctrl_mon_start_pid_unnamed),
            cmocka_unit_test(test_resctrl_mon_start_pid_fails_midway),
            cmocka_unit_test(test_resctrl_mon_start_core_failure),
            cmocka_unit_test_teardown(
                test_resctrl_mon_start_core_not_in_group_of_offline_core,
                mon_groups_unlist),
            cmocka_unit_test_teardown(
                test_resctrl_mon_stop_leaves_directory_of_group_offline,
                mon_groups_unlist),
            cmocka_unit_test(test_resctrl_mon_stop_unnamed_core_group),
            cmocka_unit_test_teardown(
                test_resctrl_mon_purge_marks_group_of_last_directory,
                mon_groups_unlist),
            cmocka_unit_test_teardown(
                test_resctrl_mon_purge_leaves_directory_another_group_is_in,
                mon_groups_unlist),
            cmocka_unit_test_teardown(
                test_resctrl_mon_purge_marks_no_group_with_a_directory_left,
                mon_groups_unlist),
            cmocka_unit_test_teardown(
                test_resctrl_mon_purge_after_a_reset_marks_its_count,
                mon_groups_unlist),
            cmocka_unit_test_teardown(
                test_resctrl_mon_purge_of_a_group_moved_since_its_start,
                mon_groups_unlist),
            cmocka_unit_test_teardown(
                test_resctrl_mon_purge_of_a_directory_already_gone,
                mon_groups_unlist),
            cmocka_unit_test_teardown(
                test_resctrl_mon_reset_that_fails_first_leaves_the_marks,
                mon_groups_unlist),
            cmocka_unit_test_teardown(
                test_resctrl_mon_reset_of_a_directory_already_gone,
                mon_groups_unlist),
            cmocka_unit_test_teardown(
                test_resctrl_mon_stop_of_a_directory_already_gone,
                mon_groups_unlist),
        };

        result += cmocka_run_group_tests(tests, test_init_mon, test_fini);

        return result;
}
