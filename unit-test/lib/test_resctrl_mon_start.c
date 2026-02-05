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

char *
resctrl_mon_new_group(void)
{
        return strdup(mock_type(char *));
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

int
__wrap_scandir(const char *restrict dirp,
               struct dirent ***restrict namelist,
               int (*filter)(const struct dirent *) __attribute__((unused)),
               int (*compar)(const struct dirent **, const struct dirent **)
                   __attribute__((unused)))
{
        int ret = -1;

        *namelist = NULL;

        if (strcmp(dirp, RESCTRL_PATH "/mon_groups") == 0 ||
            strcmp(dirp, RESCTRL_PATH "/mon_groups/") == 0)
                return 0;

        assert_string_equal(dirp, "scandir");

        return ret;
}

FILE *
__wrap_pqos_fopen(const char *name, const char *mode)
{
        unsigned i;

        const struct {
                const char path[256];
                const char mode[5];
                const char text[256];
        } file[] = {
            {.path = RESCTRL_PATH_INFO_L3_MON "/max_threshold_occupancy",
             .mode = "r",
             .text = "16000"},
        };

        for (i = 0; i < DIM(file); i++) {
                if (strcmp(mode, file[i].mode) == 0 &&
                    strcmp(name, file[i].path) == 0) {
                        FILE *fd = tmpfile();

                        assert_non_null(fd);

                        if (strcmp(mode, "r") == 0)
                                fprintf(fd, "%s", file[i].text);

                        fseek(fd, 0, SEEK_SET);
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

        ret = resctrl_mon_stop(&group);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

int
main(void)
{
        int result = 0;

        const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_resctrl_mon_start_core),
            cmocka_unit_test(test_resctrl_mon_start_pid),
        };

        result += cmocka_run_group_tests(tests, test_init_mon, test_fini);

        return result;
}
