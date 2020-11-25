/*
 * BSD LICENSE
 *
 * Copyright(c) 2020 Intel Corporation. All rights reserved.
 * All rights reserved.
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

#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include "test.h"
#include "mock_cap.h"
#include "mock_resctrl.h"
#include "mock_resctrl_schemata.h"

#include "allocation.h"
#include "os_allocation.h"
#include "resctrl_alloc.h"

/* ======== os_alloc_reset_cores ======== */

static void
test_os_alloc_reset_cores(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        unsigned i;

        will_return(__wrap__pqos_cap_get, data->cpu);

        expect_value(__wrap_resctrl_alloc_cpumask_read, class_id, 0);
        will_return(__wrap_resctrl_alloc_cpumask_read, PQOS_RETVAL_OK);

        for (i = 0; i < data->cpu->num_cores; ++i) {
                unsigned lcore = data->cpu->cores[i].lcore;

                expect_value(__wrap_resctrl_cpumask_set, lcore, lcore);
        }

        expect_value(__wrap_resctrl_alloc_cpumask_write, class_id, 0);
        will_return(__wrap_resctrl_alloc_cpumask_write, PQOS_RETVAL_OK);

        ret = os_alloc_reset_cores();
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

/* ======== os_alloc_reset_schematas ======== */

static void
test_os_alloc_reset_schematas(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        unsigned grps;
        unsigned i;

        resctrl_alloc_get_grps_num(data->cap, &grps);

        will_return(__wrap__pqos_cap_get, data->cap);
        will_return(__wrap__pqos_cap_get, data->cpu);

        will_return(__wrap_resctrl_lock_exclusive, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_lock_release, PQOS_RETVAL_OK);

        for (i = 0; i < grps; ++i) {
                expect_value(__wrap_resctrl_alloc_schemata_write, class_id, i);
                expect_value(__wrap_resctrl_alloc_schemata_write, technology,
                             (unsigned)PQOS_TECHNOLOGY_ALL);
                will_return(__wrap_resctrl_alloc_schemata_write,
                            PQOS_RETVAL_OK);
        }

        ret = os_alloc_reset_schematas(&data->cap_l3ca, &data->cap_l2ca,
                                       &data->cap_mba);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

int
main(void)
{
        int result = 0;

        const struct CMUnitTest tests_l3ca[] = {};

        const struct CMUnitTest tests_l2ca[] = {};

        const struct CMUnitTest tests_mba[] = {};

        const struct CMUnitTest tests_all[] = {
            cmocka_unit_test(test_os_alloc_reset_cores),
            cmocka_unit_test(test_os_alloc_reset_schematas)};

        const struct CMUnitTest tests_unsupported[] = {};

        result += cmocka_run_group_tests(tests_l3ca, test_init_l3ca, test_fini);
        result += cmocka_run_group_tests(tests_l2ca, test_init_l2ca, test_fini);
        result += cmocka_run_group_tests(tests_mba, test_init_mba, test_fini);
        result += cmocka_run_group_tests(tests_all, test_init_all, test_fini);
        result += cmocka_run_group_tests(tests_unsupported,
                                         test_init_unsupported, test_fini);

        return result;
}