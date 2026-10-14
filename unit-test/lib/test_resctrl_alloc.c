/*
 * BSD LICENSE
 *
 * Copyright(c) 2020-2026 Intel Corporation. All rights reserved.
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

#include "allocation.h"
#include "mock_cap.h"
#include "resctrl_alloc.h"
#include "test.h"

#include <dirent.h>
#include <errno.h>

void __real_free(void *ptr);

/* ======== mock ======== */

/*
 * Pointers allocated by __wrap_scandir and the number of times each of them
 * was passed to free(). Used to verify that resctrl_alloc_get_num_closids()
 * releases the scandir() namelist on every return path.
 */
static void *scandir_namelist_ptr;
static void *scandir_entry_ptr;
static int scandir_namelist_freed;
static int scandir_entry_freed;

void
__wrap_free(void *ptr)
{
        if (ptr != NULL && ptr == scandir_namelist_ptr)
                scandir_namelist_freed++;
        if (ptr != NULL && ptr == scandir_entry_ptr)
                scandir_entry_freed++;

        __real_free(ptr);
}

int
__wrap_scandir(const char *dirp __attribute__((unused)),
               struct dirent ***namelist,
               int (*filter)(const struct dirent *) __attribute__((unused)),
               int (*compar)(const struct dirent **, const struct dirent **)
                   __attribute__((unused)))
{
        struct dirent **list = malloc(sizeof(*list));
        struct dirent *entry = malloc(sizeof(*entry) + 16);

        assert_non_null(list);
        assert_non_null(entry);

        memset(entry, 0, sizeof(*entry));
        snprintf(entry->d_name, 16, "L3");
        list[0] = entry;
        *namelist = list;

        scandir_namelist_ptr = list;
        scandir_entry_ptr = entry;
        scandir_namelist_freed = 0;
        scandir_entry_freed = 0;

        return 1;
}

int
__wrap_pqos_file_exists(const char *path __attribute__((unused)))
{
        return mock_type(int);
}

int
__wrap_pqos_fread_uint64(const char *fname __attribute__((unused)),
                         unsigned base __attribute__((unused)),
                         uint64_t *value)
{
        *value = mock_type(uint64_t);

        return mock_type(int);
}

int
__wrap_setvbuf(FILE *stream, char *buf, int type, size_t size)
{
        assert_non_null(stream);
        assert_non_null(buf);
        assert_int_equal(type, _IOFBF);
        assert_int_not_equal(size, 0);

        return 0;
}

/* a negative value queued is the errno the call fails with */
int
__wrap_kill(pid_t pid, int sig)
{
        int ret;

        check_expected(pid);
        check_expected(sig);

        ret = mock_type(int);
        if (ret >= 0)
                return ret;
        errno = -ret;
        return -1;
}

FILE *
resctrl_alloc_fopen(const unsigned class_id, const char *name, const char *mode)
{
        check_expected(class_id);
        check_expected(name);
        check_expected(mode);

        return mock_ptr_type(FILE *);
}

int
resctrl_alloc_fclose(FILE *fd)
{
        assert_non_null(fd);

        return PQOS_RETVAL_OK;
}

/* ======== resctrl_alloc_get_grps_num ======== */

static void
test_resctrl_alloc_get_grps_num_l2(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        unsigned grps_num;

        ret = resctrl_alloc_get_grps_num(data->cap, &grps_num);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(grps_num, data->cap_l2ca.num_classes);
}

static void
test_resctrl_alloc_get_grps_num_l3(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        unsigned grps_num;

        ret = resctrl_alloc_get_grps_num(data->cap, &grps_num);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(grps_num, data->cap_l3ca.num_classes);
}

static void
test_resctrl_alloc_get_grps_num_mba(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        unsigned grps_num;

        ret = resctrl_alloc_get_grps_num(data->cap, &grps_num);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(grps_num, data->cap_mba.num_classes);
}

static void
test_resctrl_alloc_get_grps_num_all(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        unsigned grps_num;

        ret = resctrl_alloc_get_grps_num(data->cap, &grps_num);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(grps_num, data->cap_l2ca.num_classes);
}

/* ======== resctrl_alloc_cpumask_write ======== */

static void
test_resctrl_alloc_cpumask_write(void **state __attribute__((unused)))
{
        unsigned class_id = 1;
        int ret;
        struct resctrl_cpumask mask;

        memset(&mask, 0, sizeof(mask));

        expect_value(resctrl_alloc_fopen, class_id, class_id);
        expect_string(resctrl_alloc_fopen, name, "cpus");
        expect_string(resctrl_alloc_fopen, mode, "w");
        will_return(resctrl_alloc_fopen, (FILE *)1);

        will_return(__wrap_resctrl_cpumask_write, PQOS_RETVAL_OK);

        ret = resctrl_alloc_cpumask_write(class_id, &mask);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_resctrl_alloc_cpumask_write_fopen(void **state __attribute__((unused)))
{
        unsigned class_id = 1;
        int ret;
        struct resctrl_cpumask mask;

        memset(&mask, 0, sizeof(mask));

        expect_value(resctrl_alloc_fopen, class_id, class_id);
        expect_string(resctrl_alloc_fopen, name, "cpus");
        expect_string(resctrl_alloc_fopen, mode, "w");
        will_return(resctrl_alloc_fopen, NULL);

        ret = resctrl_alloc_cpumask_write(class_id, &mask);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
}

/* ======== resctrl_alloc_cpumask_read ======== */

static void
test_resctrl_alloc_cpumask_read(void **state __attribute__((unused)))
{
        unsigned class_id = 1;
        int ret;
        struct resctrl_cpumask mask;

        memset(&mask, 0, sizeof(mask));

        expect_value(resctrl_alloc_fopen, class_id, class_id);
        expect_string(resctrl_alloc_fopen, name, "cpus");
        expect_string(resctrl_alloc_fopen, mode, "r");
        will_return(resctrl_alloc_fopen, (FILE *)1);

        will_return(__wrap_resctrl_cpumask_read, PQOS_RETVAL_OK);

        ret = resctrl_alloc_cpumask_read(class_id, &mask);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_resctrl_alloc_cpumask_read_fopen(void **state __attribute__((unused)))
{
        unsigned class_id = 1;
        int ret;
        struct resctrl_cpumask mask;

        memset(&mask, 0, sizeof(mask));

        expect_value(resctrl_alloc_fopen, class_id, class_id);
        expect_string(resctrl_alloc_fopen, name, "cpus");
        expect_string(resctrl_alloc_fopen, mode, "r");
        will_return(resctrl_alloc_fopen, NULL);

        ret = resctrl_alloc_cpumask_read(class_id, &mask);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
}

/* ========= resctrl_alloc_schemata_read ======== */

static void
test_resctrl_alloc_schemata_read(void **state __attribute__((unused)))
{
        unsigned class_id = 1;
        int ret;
        struct resctrl_schemata *schmt = (struct resctrl_schemata *)1;

        expect_value(resctrl_alloc_fopen, class_id, class_id);
        expect_string(resctrl_alloc_fopen, name, "schemata");
        expect_string(resctrl_alloc_fopen, mode, "r");
        will_return(resctrl_alloc_fopen, (FILE *)1);

        will_return(__wrap_resctrl_schemata_read, PQOS_RETVAL_OK);

        ret = resctrl_alloc_schemata_read(class_id, schmt);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_resctrl_alloc_schemata_read_fopen(void **state __attribute__((unused)))
{
        unsigned class_id = 1;
        int ret;
        struct resctrl_schemata *schmt = (struct resctrl_schemata *)1;

        expect_value(resctrl_alloc_fopen, class_id, class_id);
        expect_string(resctrl_alloc_fopen, name, "schemata");
        expect_string(resctrl_alloc_fopen, mode, "r");
        will_return(resctrl_alloc_fopen, NULL);

        ret = resctrl_alloc_schemata_read(class_id, schmt);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
}

/* ======== resctrl_alloc_schemata_write ======== */

static void
test_resctrl_alloc_schemata_write_l3ca(void **state __attribute__((unused)))
{
        unsigned class_id = 1;
        unsigned technology = PQOS_TECHNOLOGY_L3CA;
        int ret;
        struct resctrl_schemata *schmt = (struct resctrl_schemata *)1;

        expect_value(resctrl_alloc_fopen, class_id, class_id);
        expect_string(resctrl_alloc_fopen, name, "schemata");
        expect_string(resctrl_alloc_fopen, mode, "w");
        will_return(resctrl_alloc_fopen, (FILE *)1);

        will_return(__wrap_resctrl_schemata_l3ca_write, PQOS_RETVAL_OK);

        ret = resctrl_alloc_schemata_write(class_id, technology, schmt);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_resctrl_alloc_schemata_write_l2ca(void **state __attribute__((unused)))
{
        unsigned class_id = 1;
        unsigned technology = PQOS_TECHNOLOGY_L2CA;
        int ret;
        struct resctrl_schemata *schmt = (struct resctrl_schemata *)1;

        expect_value(resctrl_alloc_fopen, class_id, class_id);
        expect_string(resctrl_alloc_fopen, name, "schemata");
        expect_string(resctrl_alloc_fopen, mode, "w");
        will_return(resctrl_alloc_fopen, (FILE *)1);

        will_return(__wrap_resctrl_schemata_l2ca_write, PQOS_RETVAL_OK);

        ret = resctrl_alloc_schemata_write(class_id, technology, schmt);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_resctrl_alloc_schemata_write_mba(void **state __attribute__((unused)))
{
        unsigned class_id = 1;
        unsigned technology = PQOS_TECHNOLOGY_MBA;
        int ret;
        struct resctrl_schemata *schmt = (struct resctrl_schemata *)1;

        expect_value(resctrl_alloc_fopen, class_id, class_id);
        expect_string(resctrl_alloc_fopen, name, "schemata");
        expect_string(resctrl_alloc_fopen, mode, "w");
        will_return(resctrl_alloc_fopen, (FILE *)1);

        will_return(__wrap_resctrl_schemata_mba_write, PQOS_RETVAL_OK);

        ret = resctrl_alloc_schemata_write(class_id, technology, schmt);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_resctrl_alloc_schemata_write_fopen(void **state __attribute__((unused)))
{
        unsigned class_id = 1;
        unsigned technology = 0;
        int ret;
        struct resctrl_schemata *schmt = (struct resctrl_schemata *)1;

        expect_value(resctrl_alloc_fopen, class_id, class_id);
        expect_string(resctrl_alloc_fopen, name, "schemata");
        expect_string(resctrl_alloc_fopen, mode, "w");
        will_return(resctrl_alloc_fopen, NULL);

        ret = resctrl_alloc_schemata_write(class_id, technology, schmt);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
}

/* ======== resctrl_alloc_task_validate ======== */

static void
test_resctrl_alloc_task_validate_ok(void **state __attribute__((unused)))
{
        int ret;
        pid_t task = 1;

        expect_value(__wrap_kill, pid, task);
        expect_value(__wrap_kill, sig, 0);
        will_return(__wrap_kill, 0);

        ret = resctrl_alloc_task_validate(task);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_resctrl_alloc_task_validate_error(void **state __attribute__((unused)))
{
        int ret;
        pid_t task = 1;

        expect_value(__wrap_kill, pid, task);
        expect_value(__wrap_kill, sig, 0);
        will_return(__wrap_kill, -ESRCH);

        ret = resctrl_alloc_task_validate(task);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
}

static void
test_resctrl_alloc_task_validate_no_permission(void **state
                                               __attribute__((unused)))
{
        int ret;
        pid_t task = 1;

        /* a task this process may not signal is still there */
        expect_value(__wrap_kill, pid, task);
        expect_value(__wrap_kill, sig, 0);
        will_return(__wrap_kill, -EPERM);

        ret = resctrl_alloc_task_validate(task);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_resctrl_alloc_task_search_no_groups(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        unsigned class_id = 0;
        pid_t task = 1;
        int ret;

        data->cap_l3ca.num_classes = 0;

        expect_value(__wrap_kill, pid, task);
        expect_value(__wrap_kill, sig, 0);
        will_return(__wrap_kill, 0);

        ret = resctrl_alloc_task_search(&class_id, data->cap, task);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
}

/* what the last search_tasks_file() logged at the default verbosity */
static unsigned search_logged;

/*
 * Searches a single CLOS tasks file holding content for task 1234, where the
 * task is there or not when the search starts (alive_before) and when it has
 * read every file without finding it (alive_after)
 */
static int
search_tasks_file(struct test_data *data,
                  const char *content,
                  const int alive_before,
                  const int alive_after,
                  unsigned *class_id)
{
        const unsigned num_classes = data->cap_l3ca.num_classes;
        const pid_t task = 1234;
        FILE *fd = tmpfile();
        int ret;

        assert_non_null(fd);
        fprintf(fd, "%s", content);
        rewind(fd);
        data->cap_l3ca.num_classes = 1;

        expect_value(__wrap_kill, pid, task);
        expect_value(__wrap_kill, sig, 0);
        will_return(__wrap_kill, alive_before ? 0 : -ESRCH);
        if (alive_before) {
                expect_value(resctrl_alloc_fopen, class_id, 0);
                expect_string(resctrl_alloc_fopen, name, "tasks");
                expect_string(resctrl_alloc_fopen, mode, "r");
                will_return(resctrl_alloc_fopen, fd);
        }
        if (alive_before && strstr(content, "1234") == NULL) {
                expect_value(__wrap_kill, pid, task);
                expect_value(__wrap_kill, sig, 0);
                will_return(__wrap_kill, alive_after ? 0 : -ESRCH);
        }

        test_log_count_start();
        ret = resctrl_alloc_task_search(class_id, data->cap, task);
        search_logged = test_log_count_stop();

        data->cap_l3ca.num_classes = num_classes;
        fclose(fd);

        return ret;
}

static void
test_resctrl_alloc_task_search_found(void **state)
{
        unsigned class_id = 7;
        int ret;

        ret = search_tasks_file((struct test_data *)*state, "1\n1234\n2\n", 1,
                                1, &class_id);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(class_id, 0);
}

static void
test_resctrl_alloc_task_search_ended_before(void **state)
{
        unsigned class_id = 7;
        int ret;

        ret = search_tasks_file((struct test_data *)*state, "1\n2\n", 0, 0,
                                &class_id);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
        assert_int_equal(search_logged, 0);
        assert_int_equal(class_id, 7);
}

static void
test_resctrl_alloc_task_search_ended_during(void **state)
{
        unsigned class_id = 7;
        int ret;

        /* a task that ended while the files were read is in none of them */
        ret = search_tasks_file((struct test_data *)*state, "1\n2\n", 1, 0,
                                &class_id);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
        assert_int_equal(search_logged, 0);
        assert_int_equal(class_id, 7);
}

static void
test_resctrl_alloc_task_search_not_found(void **state)
{
        unsigned class_id = 7;
        int ret;

        /* one that is still running and in no file is an error */
        ret = search_tasks_file((struct test_data *)*state, "1\n2\n", 1, 1,
                                &class_id);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
        assert_int_equal(search_logged, 1);
        assert_int_equal(class_id, 7);
}

static void
test_resctrl_alloc_get_unused_group_no_groups(void **state
                                              __attribute__((unused)))
{
        unsigned group_id = 0;
        int ret;

        ret = resctrl_alloc_get_unused_group(0, &group_id);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
}

/* ======== resctrl_alloc_get_num_closids ======== */

static void
test_resctrl_alloc_get_num_closids(void **state __attribute__((unused)))
{
        unsigned num_closids = 0;
        int ret;

        will_return(__wrap_pqos_file_exists, 1);
        will_return(__wrap_pqos_fread_uint64, 16);
        will_return(__wrap_pqos_fread_uint64, PQOS_RETVAL_OK);

        ret = resctrl_alloc_get_num_closids(&num_closids);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(num_closids, 16);

        /* namelist allocated by scandir() must be released */
        assert_int_equal(scandir_entry_freed, 1);
        assert_int_equal(scandir_namelist_freed, 1);
}

static void
test_resctrl_alloc_get_num_closids_read_error(void **state
                                              __attribute__((unused)))
{
        unsigned num_closids = 0;
        int ret;

        will_return(__wrap_pqos_file_exists, 1);
        will_return(__wrap_pqos_fread_uint64, 0);
        will_return(__wrap_pqos_fread_uint64, PQOS_RETVAL_ERROR);

        ret = resctrl_alloc_get_num_closids(&num_closids);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);

        /*
         * Even on the read error path the namelist allocated by scandir()
         * must be released (Coverity resource leak regression check).
         */
        assert_int_equal(scandir_entry_freed, 1);
        assert_int_equal(scandir_namelist_freed, 1);
}

int
main(void)
{
        int result = 0;

        const struct CMUnitTest tests_l3ca[] = {
            cmocka_unit_test(test_resctrl_alloc_get_grps_num_l3),
            cmocka_unit_test(test_resctrl_alloc_task_search_found),
            cmocka_unit_test(test_resctrl_alloc_task_search_ended_before),
            cmocka_unit_test(test_resctrl_alloc_task_search_ended_during),
            cmocka_unit_test(test_resctrl_alloc_task_search_not_found),
            cmocka_unit_test(test_resctrl_alloc_task_search_no_groups),
            cmocka_unit_test(test_resctrl_alloc_schemata_write_l3ca)};

        const struct CMUnitTest tests_l2ca[] = {
            cmocka_unit_test(test_resctrl_alloc_get_grps_num_l2),
            cmocka_unit_test(test_resctrl_alloc_schemata_write_l2ca)};

        const struct CMUnitTest tests_mba[] = {
            cmocka_unit_test(test_resctrl_alloc_get_grps_num_mba),
            cmocka_unit_test(test_resctrl_alloc_schemata_write_mba)};

        const struct CMUnitTest tests_all[] = {
            cmocka_unit_test(test_resctrl_alloc_get_grps_num_all)};

        const struct CMUnitTest tests_unsupported[] = {
            cmocka_unit_test(test_resctrl_alloc_cpumask_write),
            cmocka_unit_test(test_resctrl_alloc_cpumask_write_fopen),
            cmocka_unit_test(test_resctrl_alloc_cpumask_read),
            cmocka_unit_test(test_resctrl_alloc_cpumask_read_fopen),
            cmocka_unit_test(test_resctrl_alloc_schemata_read),
            cmocka_unit_test(test_resctrl_alloc_schemata_read_fopen),
            cmocka_unit_test(test_resctrl_alloc_schemata_write_fopen),
            cmocka_unit_test(test_resctrl_alloc_task_validate_ok),
            cmocka_unit_test(test_resctrl_alloc_task_validate_error),
            cmocka_unit_test(test_resctrl_alloc_task_validate_no_permission),
            cmocka_unit_test(test_resctrl_alloc_get_unused_group_no_groups)};

        const struct CMUnitTest tests_num_closids[] = {
            cmocka_unit_test(test_resctrl_alloc_get_num_closids),
            cmocka_unit_test(test_resctrl_alloc_get_num_closids_read_error)};

        result += cmocka_run_group_tests(tests_l3ca, test_init_l3ca, test_fini);
        result += cmocka_run_group_tests(tests_l2ca, test_init_l2ca, test_fini);
        result += cmocka_run_group_tests(tests_mba, test_init_mba, test_fini);
        result += cmocka_run_group_tests(tests_all, test_init_all, test_fini);
        result += cmocka_run_group_tests(tests_unsupported,
                                         test_init_unsupported, test_fini);
        result += cmocka_run_group_tests(tests_num_closids, NULL, NULL);

        return result;
}
