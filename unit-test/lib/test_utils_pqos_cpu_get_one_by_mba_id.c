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

#include "cpuinfo.h"
#include "pqos.h"
#include "test.h"
#include "utils.h"

#include <stdlib.h>
#include <string.h>

static void
test_utils_pqos_cpu_get_one_by_mba_id_empty_list(void **state
                                                 __attribute__((unused)))
{
        unsigned lcore_param = 0;
        struct pqos_cpuinfo cpu_param;
        int return_value;
        unsigned mba_id_param = 2;

        cpu_param.num_cores = 0;

        return_value =
            pqos_cpu_get_one_by_mba_id(&cpu_param, mba_id_param, &lcore_param);

        assert_int_equal(return_value, PQOS_RETVAL_ERROR);
        assert_int_equal(lcore_param, 0);
}

static void
test_utils_pqos_cpu_get_one_by_mba_id_multiple_cores(void **state
                                                     __attribute__((unused)))
{
        const int num_cores = 4;
        struct pqos_cpuinfo *cpu_param =
            malloc(sizeof(struct pqos_cpuinfo) +
                   num_cores * sizeof(struct pqos_coreinfo));
        unsigned lcore_param = 4;
        int return_value;
        unsigned mba_id_param = 1;

        cpu_param->cores[0].mba_id = 1;
        cpu_param->cores[1].mba_id = 1;
        cpu_param->cores[2].mba_id = 1;
        cpu_param->cores[3].mba_id = 2;

        cpu_param->cores[0].lcore = 11;
        cpu_param->cores[1].lcore = 10;
        cpu_param->cores[2].lcore = 9;
        cpu_param->cores[3].lcore = 8;

        cpu_param->num_cores = num_cores;

        return_value =
            pqos_cpu_get_one_by_mba_id(cpu_param, mba_id_param, &lcore_param);

        assert_int_equal(return_value, PQOS_RETVAL_OK);
        assert_int_equal(lcore_param, 11);

        mba_id_param = 2;

        return_value =
            pqos_cpu_get_one_by_mba_id(cpu_param, mba_id_param, &lcore_param);

        assert_int_equal(return_value, PQOS_RETVAL_OK);
        assert_int_equal(lcore_param, 8);

        mba_id_param = 3;

        return_value =
            pqos_cpu_get_one_by_mba_id(cpu_param, mba_id_param, &lcore_param);

        assert_int_equal(return_value, PQOS_RETVAL_ERROR);
        assert_int_equal(lcore_param, 8);

        if (cpu_param != NULL)
                free(cpu_param);
}

static void
test_utils_pqos_cpu_get_one_by_mba_id_cpu_null(void **state
                                               __attribute__((unused)))
{
        unsigned lcore_param = 1;
        int return_value;
        unsigned mba_id_param = 1;

        return_value =
            pqos_cpu_get_one_by_mba_id(NULL, mba_id_param, &lcore_param);

        assert_int_equal(return_value, PQOS_RETVAL_PARAM);
        assert_int_equal(lcore_param, 1);
}

static void
test_utils_pqos_cpu_get_one_by_mba_id_mba_id_param_null(void **state
                                                        __attribute__((unused)))
{
        unsigned lcore_param = 1;
        struct pqos_cpuinfo cpu_param;
        int return_value;
        unsigned mba_id_param = 1;

        cpu_param.num_cores = 0;

        return_value =
            pqos_cpu_get_one_by_mba_id(&cpu_param, mba_id_param, NULL);

        assert_int_equal(return_value, PQOS_RETVAL_PARAM);
        assert_int_equal(lcore_param, 1);
}

int
main(void)
{
        int result = 0;

        const struct CMUnitTest tests_utils_pqos_cpu_get_one_by_mba_id[] = {
            cmocka_unit_test(test_utils_pqos_cpu_get_one_by_mba_id_empty_list),
            cmocka_unit_test(
                test_utils_pqos_cpu_get_one_by_mba_id_multiple_cores),
            cmocka_unit_test(test_utils_pqos_cpu_get_one_by_mba_id_cpu_null),
            cmocka_unit_test(
                test_utils_pqos_cpu_get_one_by_mba_id_mba_id_param_null)};

        result += cmocka_run_group_tests(tests_utils_pqos_cpu_get_one_by_mba_id,
                                         NULL, NULL);

        return result;
}
