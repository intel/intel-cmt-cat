/*
 * BSD LICENSE
 *
 * Copyright(c) 2020-2023 Intel Corporation. All rights reserved.
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

/* ======== mock ======== */

int
__wrap_setvbuf(FILE *stream, char *buf, int type, size_t size)
{
        assert_non_null(stream);
        assert_non_null(buf);
        assert_int_equal(type, _IOFBF);
        assert_int_not_equal(size, 0);

        return 0;
}

int
__wrap_kill(pid_t pid, int sig)
{
        check_expected(pid);
        check_expected(sig);

        return mock_type(int);
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
        will_return(__wrap_kill, -1);

        ret = resctrl_alloc_task_validate(task);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
}

int
main(void)
{
        int result = 0;

        const struct CMUnitTest tests_l3ca[] = {
            cmocka_unit_test(test_resctrl_alloc_get_grps_num_l3),
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
            cmocka_unit_test(test_resctrl_alloc_task_validate_error)};

        result += cmocka_run_group_tests(tests_l3ca, test_init_l3ca, test_fini);
        result += cmocka_run_group_tests(tests_l2ca, test_init_l2ca, test_fini);
        result += cmocka_run_group_tests(tests_mba, test_init_mba, test_fini);
        result += cmocka_run_group_tests(tests_all, test_init_all, test_fini);
        result += cmocka_run_group_tests(tests_unsupported,
                                         test_init_unsupported, test_fini);

        return result;
}
