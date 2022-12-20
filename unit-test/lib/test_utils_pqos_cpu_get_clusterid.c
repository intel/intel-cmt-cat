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
test_utils_pqos_cpu_get_clusterid_empty_list(void **state
                                             __attribute__((unused)))
{
        unsigned lcore_param = 0;
        struct pqos_cpuinfo cpu_param;
        int return_value;
        unsigned cluster = 2;

        cpu_param.num_cores = 0;

        return_value =
            pqos_cpu_get_clusterid(&cpu_param, lcore_param, &cluster);

        assert_int_equal(return_value, PQOS_RETVAL_ERROR);
        assert_int_equal(cluster, 2);

        return;
}

static void
test_utils_pqos_cpu_get_clusterid_multiple_cores_on_the_list(
    void **state __attribute__((unused)))
{
        const int num_cores = 4;
        struct pqos_cpuinfo *cpu_param =
            malloc(sizeof(struct pqos_cpuinfo) +
                   num_cores * sizeof(struct pqos_coreinfo));
        unsigned lcore_param = 1;
        int return_value;
        unsigned cluster = 4;

        cpu_param->cores[0].lcore = 1;
        cpu_param->cores[1].lcore = 1;
        cpu_param->cores[2].lcore = 1;
        cpu_param->cores[3].lcore = 2;

        cpu_param->cores[0].l3_id = 11;
        cpu_param->cores[1].l3_id = 10;
        cpu_param->cores[2].l3_id = 9;
        cpu_param->cores[3].l3_id = 8;

        cpu_param->num_cores = num_cores;

        return_value = pqos_cpu_get_clusterid(cpu_param, lcore_param, &cluster);

        assert_int_equal(return_value, PQOS_RETVAL_OK);
        assert_int_equal(cluster, 11);

        lcore_param = 2;

        return_value = pqos_cpu_get_clusterid(cpu_param, lcore_param, &cluster);

        assert_int_equal(return_value, PQOS_RETVAL_OK);
        assert_int_equal(cluster, 8);

        lcore_param = 3;

        return_value = pqos_cpu_get_clusterid(cpu_param, lcore_param, &cluster);

        assert_int_equal(return_value, PQOS_RETVAL_ERROR);
        assert_int_equal(cluster, 8);

        if (cpu_param != NULL)
                free(cpu_param);
}

static void
test_utils_pqos_cpu_get_clusterid_cpu_null(void **state __attribute__((unused)))
{
        unsigned lcore_param = 1;
        int return_value;
        unsigned cluster = 1;

        return_value = pqos_cpu_get_clusterid(NULL, lcore_param, &cluster);

        assert_int_equal(return_value, PQOS_RETVAL_PARAM);
        assert_int_equal(cluster, 1);
}

static void
test_utils_pqos_cpu_get_clusterid_cluster_null(void **state
                                               __attribute__((unused)))
{
        unsigned lcore_param = 1;
        struct pqos_cpuinfo cpu_param;
        int return_value;
        unsigned cluster = 1;

        cpu_param.num_cores = 0;

        return_value = pqos_cpu_get_clusterid(&cpu_param, lcore_param, NULL);

        assert_int_equal(return_value, PQOS_RETVAL_PARAM);
        assert_int_equal(cluster, 1);
}

int
main(void)
{
        int result = 0;

        const struct CMUnitTest tests_utils_pqos_cpu_get_clusterid[] = {
            cmocka_unit_test(test_utils_pqos_cpu_get_clusterid_empty_list),
            cmocka_unit_test(
                test_utils_pqos_cpu_get_clusterid_multiple_cores_on_the_list),
            cmocka_unit_test(test_utils_pqos_cpu_get_clusterid_cpu_null),
            cmocka_unit_test(test_utils_pqos_cpu_get_clusterid_cluster_null)};

        result += cmocka_run_group_tests(tests_utils_pqos_cpu_get_clusterid,
                                         NULL, NULL);

        return result;
}
