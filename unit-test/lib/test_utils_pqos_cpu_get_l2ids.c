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

#include "cpuinfo.h"
#include "pqos.h"
#include "test.h"
#include "utils.h"

#include <stdlib.h>
#include <string.h>

static void
test_utils_pqos_cpu_get_l2ids_empty_list(void **state __attribute__((unused)))
{
        unsigned count_param = 0;
        struct pqos_cpuinfo cpu_param;
        unsigned *return_value;

        cpu_param.num_cores = 0;

        return_value = pqos_cpu_get_l2ids(&cpu_param, &count_param);

        assert_non_null(return_value);
        assert_int_equal(count_param, 0);

        if (return_value != NULL)
                free(return_value);
}

static void
test_utils_pqos_cpu_get_l2ids_multiple_cores_on_the_list(
    void **state __attribute__((unused)))
{
        const int num_cores = 4;
        struct pqos_cpuinfo *cpu_param =
            malloc(sizeof(struct pqos_cpuinfo) +
                   num_cores * sizeof(struct pqos_coreinfo));
        unsigned count_param = 4;
        unsigned *return_value;

        cpu_param->cores[0].l2_id = 1;
        cpu_param->cores[1].l2_id = 2;
        cpu_param->cores[2].l2_id = 2;
        cpu_param->cores[3].l2_id = 3;

        cpu_param->num_cores = num_cores;

        return_value = pqos_cpu_get_l2ids(cpu_param, &count_param);

        assert_non_null(return_value);
        assert_int_equal(count_param, 3);
        assert_int_equal(return_value[0], 1);
        assert_int_equal(return_value[1], 2);
        assert_int_equal(return_value[2], 3);

        if (return_value != NULL)
                free(return_value);

        if (cpu_param != NULL)
                free(cpu_param);
}

static void
test_utils_pqos_cpu_get_l2ids_cpu_null(void **state __attribute__((unused)))
{
        unsigned count_param = 1;
        unsigned *return_value;

        return_value = pqos_cpu_get_l2ids(NULL, &count_param);

        assert_null(return_value);
        assert_int_equal(count_param, 1);

        if (return_value != NULL)
                free(return_value);
}

static void
test_utils_pqos_cpu_get_l2ids_count_null(void **state __attribute__((unused)))
{
        unsigned count_param = 1;
        struct pqos_cpuinfo cpu_param;
        unsigned *return_value;

        cpu_param.num_cores = 0;

        return_value = pqos_cpu_get_l2ids(&cpu_param, NULL);

        assert_null(return_value);
        assert_int_equal(count_param, 1);

        if (return_value != NULL)
                free(return_value);
}

int
main(void)
{
        int result = 0;

        const struct CMUnitTest tests_utils_pqos_cpu_get_l2ids[] = {
            cmocka_unit_test(test_utils_pqos_cpu_get_l2ids_empty_list),
            cmocka_unit_test(
                test_utils_pqos_cpu_get_l2ids_multiple_cores_on_the_list),
            cmocka_unit_test(test_utils_pqos_cpu_get_l2ids_cpu_null),
            cmocka_unit_test(test_utils_pqos_cpu_get_l2ids_count_null)};

        result +=
            cmocka_run_group_tests(tests_utils_pqos_cpu_get_l2ids, NULL, NULL);

        return result;
}
