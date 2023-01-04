/*
 * BSD LICENSE
 *
 * Copyright(c) 2022-2023 Intel Corporation. All rights reserved.
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
#include "resctrl.h"
#include "resctrl_monitoring.h"
#include "test.h"

/* ======== resctrl_mon_cpumask_read ======== */

static void
test_resctrl_mon_cpumask_read(void **state __attribute__((unused)))
{
        unsigned class_id;
        int ret;
        struct resctrl_cpumask mask;
        const char *resctrl_group = "test";

        memset(&mask, 0, sizeof(mask));

        /* COS 0 */
        class_id = 0;
        expect_string(__wrap_pqos_fopen, name,
                      "/sys/fs/resctrl/mon_groups/test/cpus");
        expect_string(__wrap_pqos_fopen, mode, "r");
        will_return(__wrap_pqos_fopen, (FILE *)1);
        expect_function_call(__wrap_pqos_fclose);
        will_return(__wrap_pqos_fclose, 0);

        will_return(__wrap_resctrl_cpumask_read, PQOS_RETVAL_OK);

        ret = resctrl_mon_cpumask_read(class_id, resctrl_group, &mask);
        assert_int_equal(ret, PQOS_RETVAL_OK);

        /* COS 1 */
        class_id = 1;
        expect_string(__wrap_pqos_fopen, name,
                      "/sys/fs/resctrl/COS1/mon_groups/test/cpus");
        expect_string(__wrap_pqos_fopen, mode, "r");
        will_return(__wrap_pqos_fopen, (FILE *)1);
        expect_function_call(__wrap_pqos_fclose);
        will_return(__wrap_pqos_fclose, 0);

        will_return(__wrap_resctrl_cpumask_read, PQOS_RETVAL_OK);

        ret = resctrl_mon_cpumask_read(class_id, resctrl_group, &mask);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_resctrl_mon_cpumask_read_error(void **state __attribute__((unused)))
{
        unsigned class_id = 0;
        int ret;
        const char *resctrl_group = "test";
        struct resctrl_cpumask mask;

        memset(&mask, 0, sizeof(mask));

        class_id = 0;
        expect_string(__wrap_pqos_fopen, name,
                      "/sys/fs/resctrl/mon_groups/test/cpus");
        expect_string(__wrap_pqos_fopen, mode, "r");
        will_return(__wrap_pqos_fopen, NULL);

        ret = resctrl_mon_cpumask_read(class_id, resctrl_group, &mask);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
}

/* ======== resctrl_mon_cpumask_read ======== */

static void
test_resctrl_mon_cpumask_write(void **state __attribute__((unused)))
{
        unsigned class_id;
        int ret;
        struct resctrl_cpumask mask;
        const char *resctrl_group = "test";

        memset(&mask, 0, sizeof(mask));

        /* COS 0 */
        class_id = 0;
        expect_string(__wrap_pqos_fopen, name,
                      "/sys/fs/resctrl/mon_groups/test/cpus");
        expect_string(__wrap_pqos_fopen, mode, "w");
        will_return(__wrap_pqos_fopen, (FILE *)1);
        expect_function_call(__wrap_pqos_fclose);
        will_return(__wrap_pqos_fclose, 0);

        will_return(__wrap_resctrl_cpumask_write, PQOS_RETVAL_OK);

        ret = resctrl_mon_cpumask_write(class_id, resctrl_group, &mask);
        assert_int_equal(ret, PQOS_RETVAL_OK);

        /* COS 1 */
        class_id = 1;
        expect_string(__wrap_pqos_fopen, name,
                      "/sys/fs/resctrl/COS1/mon_groups/test/cpus");
        expect_string(__wrap_pqos_fopen, mode, "w");
        will_return(__wrap_pqos_fopen, (FILE *)1);
        expect_function_call(__wrap_pqos_fclose);
        will_return(__wrap_pqos_fclose, 0);

        will_return(__wrap_resctrl_cpumask_write, PQOS_RETVAL_OK);

        ret = resctrl_mon_cpumask_write(class_id, resctrl_group, &mask);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_resctrl_mon_cpumask_write_error(void **state __attribute__((unused)))
{
        unsigned class_id = 0;
        int ret;
        const char *resctrl_group = "test";
        struct resctrl_cpumask mask;

        memset(&mask, 0, sizeof(mask));

        class_id = 0;
        expect_string(__wrap_pqos_fopen, name,
                      "/sys/fs/resctrl/mon_groups/test/cpus");
        expect_string(__wrap_pqos_fopen, mode, "w");
        will_return(__wrap_pqos_fopen, NULL);

        ret = resctrl_mon_cpumask_write(class_id, resctrl_group, &mask);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
}

static void
test_resctrl_mon_cpumask_write_invalid(void **state __attribute__((unused)))
{
        unsigned class_id;
        int ret;
        struct resctrl_cpumask mask;
        const char *resctrl_group = "test";

        memset(&mask, 0, sizeof(mask));

        /* COS 0 */
        class_id = 0;
        expect_string(__wrap_pqos_fopen, name,
                      "/sys/fs/resctrl/mon_groups/test/cpus");
        expect_string(__wrap_pqos_fopen, mode, "w");
        will_return(__wrap_pqos_fopen, (FILE *)1);
        expect_function_call(__wrap_pqos_fclose);
        will_return(__wrap_pqos_fclose, -1);

        will_return(__wrap_resctrl_cpumask_write, PQOS_RETVAL_OK);

        ret = resctrl_mon_cpumask_write(class_id, resctrl_group, &mask);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
}

int
main(void)
{
        const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_resctrl_mon_cpumask_read),
            cmocka_unit_test(test_resctrl_mon_cpumask_read_error),
            cmocka_unit_test(test_resctrl_mon_cpumask_write),
            cmocka_unit_test(test_resctrl_mon_cpumask_write_error),
            cmocka_unit_test(test_resctrl_mon_cpumask_write_invalid),
        };

        cmocka_run_group_tests(tests, NULL, NULL);
}
