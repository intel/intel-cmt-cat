/*
 *  BSD  LICENSE
 *
 * Copyright(c) 2023 Intel Corporation. All rights reserved.
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

#include "test.h"
#include "utils.h"

/* ======== _pqos_cap_get_type ======== */

static void
test__pqos_cap_get_type_param(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        const struct pqos_capability *p_cap_item;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);

        p_cap_item = _pqos_cap_get_type(-1);
        assert_null(p_cap_item);
}

static void
test__pqos_cap_get_type(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        const struct pqos_capability *p_cap_item;
        unsigned i;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);

        for (i = 0; i < PQOS_CAP_TYPE_NUMOF; i++) {
                p_cap_item = _pqos_cap_get_type(i);
                assert_non_null(p_cap_item);
                assert_int_equal(p_cap_item->type, i);
        }
}

/* ======== pqos_cap_get_type ======== */

static void
test_pqos_cap_get_type_param(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        const struct pqos_capability *p_cap_item;
        int ret;

        /* Invalid event */
        ret = pqos_cap_get_type(data->cap, 0xfffffff, &p_cap_item);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);

        /* NULL pointers */
        ret = pqos_cap_get_type(NULL, PQOS_CAP_TYPE_MON, &p_cap_item);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
        ret = pqos_cap_get_type(data->cap, PQOS_CAP_TYPE_MON, NULL);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
}

static void
test_pqos_cap_get_type_resource(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        const struct pqos_capability *p_cap_item;
        int ret;

        ret = pqos_cap_get_type(data->cap, PQOS_CAP_TYPE_MON, &p_cap_item);
        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);
}

static void
test_pqos_cap_get_type(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        const struct pqos_capability *p_cap_item;
        int ret;
        unsigned i;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);

        for (i = 0; i < PQOS_CAP_TYPE_NUMOF; i++) {
                ret = pqos_cap_get_type(data->cap, i, &p_cap_item);
                assert_int_equal(ret, PQOS_RETVAL_OK);
                assert_int_equal(p_cap_item->type, i);
        }
}

/* ======== pqos_cpu_get_numa ======== */
#if PQOS_VERSION >= 50000
static void
test_utils_pqos_cpu_get_numa_empty(void **state __attribute__((unused)))
{
        unsigned count = 0;
        struct pqos_cpuinfo cpu;
        unsigned *numa;

        cpu.num_cores = 0;

        numa = pqos_cpu_get_numa(&cpu, &count);
        assert_non_null(numa);
        assert_int_equal(count, 0);

        if (numa != NULL)
                free(numa);
}

static void
test_utils_pqos_cpu_get_numa(void **state __attribute__((unused)))
{
        const int num_cores = 4;
        struct pqos_cpuinfo *cpu =
            malloc(sizeof(struct pqos_cpuinfo) +
                   num_cores * sizeof(struct pqos_coreinfo));
        unsigned count;
        unsigned *numa;

        cpu->cores[0].numa = 1;
        cpu->cores[1].numa = 2;
        cpu->cores[2].numa = 2;
        cpu->cores[3].numa = 3;
        cpu->num_cores = num_cores;

        numa = pqos_cpu_get_numa(cpu, &count);

        assert_non_null(numa);
        assert_int_equal(count, 3);
        assert_int_equal(numa[0], 1);
        assert_int_equal(numa[1], 2);
        assert_int_equal(numa[2], 3);

        if (numa != NULL)
                free(numa);
        free(cpu);
}

static void
test_utils_pqos_cpu_get_numa_param(void **state __attribute__((unused)))
{
        unsigned count = 1;
        unsigned *numa;
        struct pqos_cpuinfo cpu;

        cpu.num_cores = 0;

        numa = pqos_cpu_get_numa(NULL, &count);
        assert_null(numa);
        if (numa != NULL)
                free(numa);

        numa = pqos_cpu_get_numa(&cpu, NULL);
        assert_null(numa);
        if (numa != NULL)
                free(numa);
}
#endif

/* ======== pqos_cpu_get_numaid ======== */

#if PQOS_VERSION >= 50000
static void
test_utils_pqos_cpu_get_numaid_empty(void **state __attribute__((unused)))
{
        unsigned lcore = 0;
        struct pqos_cpuinfo cpu;
        int return_value;
        unsigned numaid;

        cpu.num_cores = 0;

        return_value = pqos_cpu_get_numaid(&cpu, lcore, &numaid);
        assert_int_equal(return_value, PQOS_RETVAL_ERROR);
}

static void
test_utils_pqos_cpu_get_numaid(void **state __attribute__((unused)))
{
        const int num_cores = 4;
        struct pqos_cpuinfo *cpu =
            malloc(sizeof(struct pqos_cpuinfo) +
                   num_cores * sizeof(struct pqos_coreinfo));
        int ret;
        unsigned numaid;
        int i;

        cpu->cores[0].lcore = 0;
        cpu->cores[0].numa = 1;
        cpu->cores[1].lcore = 1;
        cpu->cores[1].numa = 1;
        cpu->cores[2].lcore = 2;
        cpu->cores[2].numa = 2;
        cpu->cores[3].lcore = 3;
        cpu->cores[3].numa = 3;
        cpu->num_cores = num_cores;

        for (i = 0; i < num_cores; ++i) {
                ret = pqos_cpu_get_numaid(cpu, i, &numaid);
                assert_int_equal(ret, PQOS_RETVAL_OK);
                assert_int_equal(numaid, cpu->cores[i].numa);
        }

        ret = pqos_cpu_get_numaid(cpu, num_cores, &numaid);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);

        free(cpu);
}

static void
test_utils_pqos_cpu_get_numaid_param(void **state __attribute__((unused)))
{
        int ret;
        struct pqos_cpuinfo cpu;
        unsigned numaid;

        cpu.num_cores = 0;

        ret = pqos_cpu_get_numaid(NULL, 1, &numaid);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);

        ret = pqos_cpu_get_numaid(&cpu, 1, NULL);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
}
#endif

/* ======== pqos_cpu_get_one_by_numaid ======== */

#if PQOS_VERSION >= 50000
static void
test_pqos_cpu_get_one_by_numaid_empty(void **state __attribute__((unused)))
{
        struct pqos_cpuinfo cpu;
        int ret;
        unsigned numaid = 1;
        unsigned lcore;

        cpu.num_cores = 0;

        ret = pqos_cpu_get_one_by_numaid(&cpu, numaid, &lcore);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
}

static void
test_pqos_cpu_get_one_by_numaid(void **state __attribute__((unused)))
{
        const int num_cores = 4;
        struct pqos_cpuinfo *cpu =
            malloc(sizeof(struct pqos_cpuinfo) +
                   num_cores * sizeof(struct pqos_coreinfo));
        int ret;
        unsigned lcore;

        cpu->cores[0].lcore = 0;
        cpu->cores[0].numa = 1;
        cpu->cores[1].lcore = 1;
        cpu->cores[1].numa = 1;
        cpu->cores[2].lcore = 2;
        cpu->cores[2].numa = 2;
        cpu->cores[3].lcore = 3;
        cpu->cores[3].numa = 3;
        cpu->num_cores = num_cores;

        ret = pqos_cpu_get_one_by_numaid(cpu, 1, &lcore);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(lcore, 0);

        ret = pqos_cpu_get_one_by_numaid(cpu, 2, &lcore);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(lcore, 2);

        /* Test for invalid numaid */
        ret = pqos_cpu_get_one_by_numaid(cpu, 4, &lcore);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);

        if (cpu != NULL)
                free(cpu);
}

static void
test_pqos_cpu_get_one_by_numaid_param(void **state __attribute__((unused)))
{
        struct pqos_cpuinfo cpu;
        int ret;
        unsigned numaid = 1;
        unsigned lcore;

        cpu.num_cores = 0;

        ret = pqos_cpu_get_one_by_numaid(&cpu, numaid, NULL);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);

        ret = pqos_cpu_get_one_by_numaid(NULL, numaid, &lcore);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
}
#endif

int
main(void)
{
        int result = 0;

#if PQOS_VERSION >= 50000
        const struct CMUnitTest tests_snc[] = {
            cmocka_unit_test(test_utils_pqos_cpu_get_numa_empty),
            cmocka_unit_test(test_utils_pqos_cpu_get_numa),
            cmocka_unit_test(test_utils_pqos_cpu_get_numa_param),
            cmocka_unit_test(test_utils_pqos_cpu_get_numaid_empty),
            cmocka_unit_test(test_utils_pqos_cpu_get_numaid),
            cmocka_unit_test(test_utils_pqos_cpu_get_numaid_param),
            cmocka_unit_test(test_pqos_cpu_get_one_by_numaid_empty),
            cmocka_unit_test(test_pqos_cpu_get_one_by_numaid),
            cmocka_unit_test(test_pqos_cpu_get_one_by_numaid_param),
        };
#endif
        const struct CMUnitTest tests_all[] = {
            cmocka_unit_test(test__pqos_cap_get_type_param),
            cmocka_unit_test(test__pqos_cap_get_type),
            cmocka_unit_test(test_pqos_cap_get_type_param),
            cmocka_unit_test(test_pqos_cap_get_type),
        };

        const struct CMUnitTest tests_l3ca[] = {
            cmocka_unit_test(test_pqos_cap_get_type_resource),
        };

#if PQOS_VERSION >= 50000
        result += cmocka_run_group_tests(tests_snc, NULL, NULL);
#endif
        result += cmocka_run_group_tests(tests_all, test_init_all, test_fini);
        result += cmocka_run_group_tests(tests_l3ca, test_init_l3ca, test_fini);

        return result;
}
