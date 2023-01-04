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

#include "hw_monitoring.h"
#include "perf_monitoring.h"
#include "test.h"

static void
test_hw_mon_start_perf(void **state __attribute__((unused)))
{
        unsigned num_cores = 1;
        unsigned cores[] = {1};
        struct pqos_mon_data group;
        struct pqos_mon_data_internal intl;
        enum pqos_mon_event event =
            PQOS_PERF_EVENT_CYCLES | PQOS_PERF_EVENT_IPC;
        int ret;

        memset(&intl, 0, sizeof(intl));
        memset(&group, 0, sizeof(struct pqos_mon_data));
        group.intl = &intl;
        group.num_cores = num_cores;
        group.cores = cores;

        expect_value(__wrap_perf_mon_is_event_supported, event,
                     PQOS_PERF_EVENT_CYCLES);
        will_return_always(__wrap_perf_mon_is_event_supported, 1);

        expect_value(__wrap_perf_mon_start, event, PQOS_PERF_EVENT_CYCLES);
        will_return(__wrap_perf_mon_start, PQOS_RETVAL_OK);

        ret = hw_mon_start_perf(&group, event);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(group.intl->perf.event, PQOS_PERF_EVENT_CYCLES);

        expect_value(__wrap_perf_mon_stop, event, PQOS_PERF_EVENT_CYCLES);
        will_return(__wrap_perf_mon_stop, PQOS_RETVAL_OK);

        ret = hw_mon_stop_perf(&group);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

int
main(void)
{
        int result = 0;

        const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_hw_mon_start_perf)};

        result += cmocka_run_group_tests(tests, NULL, NULL);

        return result;
}
