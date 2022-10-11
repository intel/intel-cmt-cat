/*
 * BSD LICENSE
 *
 * Copyright(c) 2020-2022 Intel Corporation. All rights reserved.
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

#include "mock_resctrl_alloc.h"

#include "mock_test.h"

int
__wrap_resctrl_alloc_init(const struct pqos_cpuinfo *cpu,
                          const struct pqos_cap *cap)
{
        assert_non_null(cpu);
        assert_non_null(cap);

        return mock_type(int);
}

int
__wrap_resctrl_alloc_fini(void)
{
        return mock_type(int);
}

int
__wrap_resctrl_alloc_assoc_set(const unsigned lcore, const unsigned class_id)
{
        check_expected(lcore);
        check_expected(class_id);

        return mock_type(int);
}

int
__wrap_resctrl_alloc_assoc_get(const unsigned lcore, unsigned *class_id)
{
        int ret;

        check_expected(lcore);
        assert_non_null(class_id);

        ret = mock_type(int);
        if (ret == PQOS_RETVAL_OK)
                *class_id = mock_type(int);

        return ret;
}

int
__wrap_resctrl_alloc_assoc_set_pid(const pid_t task, const unsigned class_id)
{
        check_expected(task);
        check_expected(class_id);

        return mock_type(int);
}

int
__wrap_resctrl_alloc_assoc_get_pid(const pid_t task, unsigned *class_id)
{
        int ret;

        check_expected(task);
        assert_non_null(class_id);

        ret = mock_type(int);
        if (ret == PQOS_RETVAL_OK)
                *class_id = mock_type(int);

        return ret;
}

int
__wrap_resctrl_alloc_get_unused_group(const unsigned grps_num,
                                      unsigned *group_id)
{
        int ret;

        check_expected(grps_num);
        assert_non_null(group_id);

        ret = mock_type(int);
        if (ret == PQOS_RETVAL_OK)
                *group_id = mock_type(int);

        return ret;
}

int
__wrap_resctrl_alloc_cpumask_write(const unsigned class_id,
                                   const struct resctrl_cpumask *mask)
{
        check_expected(class_id);
        assert_non_null(mask);

        return mock_type(int);
}

int
__wrap_resctrl_alloc_cpumask_read(const unsigned class_id,
                                  struct resctrl_cpumask *mask)
{
        check_expected(class_id);
        assert_non_null(mask);

        return mock_type(int);
}

int
__wrap_resctrl_alloc_schemata_read(const unsigned class_id,
                                   struct resctrl_schemata *schemata)
{
        check_expected(class_id);
        assert_non_null(schemata);

        return mock_type(int);
}

int
__wrap_resctrl_alloc_schemata_write(const unsigned class_id,
                                    const unsigned technology,
                                    const struct resctrl_schemata *schemata)
{
        check_expected(class_id);
        check_expected(technology);
        assert_non_null(schemata);

        return mock_type(int);
}

int
__wrap_resctrl_alloc_task_write(const unsigned class_id, const pid_t task)
{
        check_expected(class_id);
        check_expected(task);

        return mock_type(int);
}

int
__wrap_resctrl_alloc_get_num_closids(unsigned *num_closids)
{
        int ret;

        ret = mock_type(int);
        if (ret == PQOS_RETVAL_OK)
                *num_closids = mock_type(int);

        return ret;
}

int
__wrap_resctrl_alloc_get_grps_num(const struct pqos_cap *cap,
                                  unsigned *grps_num)
{
        int ret;

        assert_non_null(cap);
        assert_non_null(grps_num);

        ret = mock_type(int);
        if (ret == PQOS_RETVAL_OK)
                *grps_num = mock_type(int);

        return ret;
}

int
__wrap_resctrl_alloc_task_validate(const pid_t task)
{
        check_expected(task);

        return mock_type(int);
}