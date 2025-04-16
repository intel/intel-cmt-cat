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

#include "mock_os_monitoring.h"

#include "mock_test.h"

int
__wrap_os_mon_reset(const struct pqos_mon_config *cfg)
{
        check_expected_ptr(cfg);
        return mock_type(int);
}

int
__wrap_os_mon_start_cores(const unsigned num_cores,
                          const unsigned *cores,
                          const enum pqos_mon_event event,
                          void *context,
                          struct pqos_mon_mem_region *mem_regions,
                          struct pqos_mon_data *group,
                          const struct pqos_mon_options *opt)
{
        check_expected(num_cores);
        check_expected_ptr(cores);
        check_expected(event);
        check_expected_ptr(context);
        check_expected_ptr(mem_regions);
        assert_non_null(group);
        assert_non_null(opt);

        return mock_type(int);
}

int
__wrap_os_mon_stop(struct pqos_mon_data *group)
{
        check_expected_ptr(group);

        return mock_type(int);
}

int
__wrap_os_mon_poll(struct pqos_mon_data *group, const enum pqos_mon_event event)
{
        check_expected_ptr(group);
        check_expected(event);

        return mock_type(int);
}

int
__wrap_os_mon_start_pids(const unsigned num_pids,
                         const pid_t *pids,
                         const enum pqos_mon_event event,
                         void *context,
                         struct pqos_mon_data *group)
{
        check_expected(num_pids);
        assert_non_null(pids);
        check_expected(event);
        check_expected_ptr(context);
        assert_non_null(group);

        return mock_type(int);
}

int
__wrap_os_mon_add_pids(const unsigned num_pids,
                       const pid_t *pids,
                       struct pqos_mon_data *group)
{
        check_expected(num_pids);
        check_expected_ptr(pids);
        check_expected_ptr(group);

        return mock_type(int);
}

int
__wrap_os_mon_remove_pids(const unsigned num_pids,
                          const pid_t *pids,
                          struct pqos_mon_data *group)
{
        check_expected(num_pids);
        check_expected_ptr(pids);
        check_expected_ptr(group);

        return mock_type(int);
}
