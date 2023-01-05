/*
 * BSD LICENSE
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

#include "cap.h"
#include "cpu_registers.h"
#include "log.h"
#include "machine.h"
#include "monitoring.h"
#include "test.h"
#include "types.h"
#include "uncore_monitoring.h"

#include <dirent.h> /**< scandir() */
#include <fnmatch.h>
#include <string.h>

void
__wrap_lcpuid(const unsigned leaf,
              const unsigned subleaf,
              struct cpuid_out *out)
{
        assert_int_equal(leaf, 1);
        assert_int_equal(subleaf, 0);
        out->eax = 0x00050050;
}

static void
test_uncore_monitoring_stop(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret_value;
        struct pqos_mon_data grp;
        struct pqos_mon_data_internal intl;
        unsigned sockets[] = {0, 1};

        memset(&grp, 0, sizeof(grp));
        memset(&intl, 0, sizeof(intl));
        grp.intl = &intl;
        grp.intl->uncore.sockets = (unsigned *)&sockets;
        grp.intl->uncore.num_sockets = DIM(sockets);

        fprintf(stderr, "===> data %p\n", data);
        fprintf(stderr, "===> cpu %p\n", data->cpu);

        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);

        /* initialise the system */
        (void)uncore_mon_init(NULL, NULL);

        /* Only RDT events are started */
        grp.intl->hw.event = PQOS_MON_EVENT_L3_OCCUP | PQOS_MON_EVENT_LMEM_BW |
                             PQOS_MON_EVENT_TMEM_BW | PQOS_MON_EVENT_RMEM_BW;
        ret_value = uncore_mon_stop(&grp);
        assert_int_equal(ret_value, PQOS_RETVAL_OK);

        grp.intl->hw.event = PQOS_PERF_EVENT_LLC_MISS_PCIE_READ;
        expect_value(__wrap_msr_write, lcore, 0);
        expect_value(__wrap_msr_write, lcore, 4);
        /* UNCORE_EVENT_LLC_MISS_PCIE_READ */
        expect_value(__wrap_msr_write, reg, 3584);
        /* UNCORE_EVENT_LLC_MISS_PCIE_READ */
        expect_value(__wrap_msr_write, reg, 3584);
        /* UNIT_CTRL_RESET_CONTROL */
        expect_value(__wrap_msr_write, value, 65793);
        /* UNIT_CTRL_RESET_CONTROL */
        expect_value(__wrap_msr_write, value, 65793);
        will_return(__wrap_msr_write, PQOS_RETVAL_OK);
        will_return(__wrap_msr_write, PQOS_RETVAL_OK);

        ret_value = uncore_mon_stop(&grp);
        assert_int_equal(ret_value, PQOS_RETVAL_OK);

        grp.intl->hw.event = PQOS_PERF_EVENT_LLC_MISS_PCIE_WRITE;
        expect_value(__wrap_msr_write, lcore, 0);
        /* UNCORE_EVENT_LLC_MISS_PCIE_WRITE */
        expect_value(__wrap_msr_write, reg, 3600);
        /* UNIT_CTRL_RESET_CONTROL */
        expect_value(__wrap_msr_write, value, 65793);
        expect_value(__wrap_msr_write, lcore, 4);
        /* UNCORE_EVENT_LLC_MISS_PCIE_WRITE */
        expect_value(__wrap_msr_write, reg, 3600);
        /* UNIT_CTRL_RESET_CONTROL */
        expect_value(__wrap_msr_write, value, 65793);
        will_return(__wrap_msr_write, PQOS_RETVAL_OK);
        will_return(__wrap_msr_write, PQOS_RETVAL_OK);

        ret_value = uncore_mon_stop(&grp);
        assert_int_equal(ret_value, PQOS_RETVAL_OK);

        grp.intl->hw.event = PQOS_PERF_EVENT_LLC_REF_PCIE_READ;
        expect_value(__wrap_msr_write, lcore, 0);
        expect_value(__wrap_msr_write, lcore, 4);
        /* UNCORE_EVENT_LLC_REF_PCIE_READ */
        expect_value(__wrap_msr_write, reg, 3616);
        /* UNCORE_EVENT_LLC_REF_PCIE_READ */
        expect_value(__wrap_msr_write, reg, 3616);
        /* UNIT_CTRL_RESET_CONTROL */
        expect_value(__wrap_msr_write, value, 65793);
        /* UNIT_CTRL_RESET_CONTROL */
        expect_value(__wrap_msr_write, value, 65793);
        will_return(__wrap_msr_write, PQOS_RETVAL_OK);
        will_return(__wrap_msr_write, PQOS_RETVAL_OK);

        ret_value = uncore_mon_stop(&grp);
        assert_int_equal(ret_value, PQOS_RETVAL_OK);

        grp.intl->hw.event = PQOS_PERF_EVENT_LLC_REF_PCIE_WRITE;
        expect_value(__wrap_msr_write, lcore, 0);
        expect_value(__wrap_msr_write, lcore, 4);
        /* UNCORE_EVENT_LLC_REF_PCIE_WRITE */
        expect_value(__wrap_msr_write, reg, 3632);
        /* UNCORE_EVENT_LLC_REF_PCIE_WRITE */
        expect_value(__wrap_msr_write, reg, 3632);
        /* UNIT_CTRL_RESET_CONTROL */
        expect_value(__wrap_msr_write, value, 65793);
        /* UNIT_CTRL_RESET_CONTROL */
        expect_value(__wrap_msr_write, value, 65793);
        will_return(__wrap_msr_write, PQOS_RETVAL_OK);
        will_return(__wrap_msr_write, PQOS_RETVAL_OK);

        ret_value = uncore_mon_stop(&grp);
        assert_int_equal(ret_value, PQOS_RETVAL_OK);

        grp.intl->hw.event = PQOS_PERF_EVENT_LLC_MISS_PCIE_READ;
        expect_value(__wrap_msr_write, lcore, 0);
        /* UNCORE_EVENT_LLC_MISS_PCIE_READ */
        expect_value(__wrap_msr_write, reg, 3584);
        /* UNIT_CTRL_RESET_CONTROL */
        expect_value(__wrap_msr_write, value, 65793);
        will_return(__wrap_msr_write, PQOS_RETVAL_ERROR);

        ret_value = uncore_mon_stop(&grp);
        assert_int_equal(ret_value, PQOS_RETVAL_ERROR);
}

static void
test_uncore_monitoring_stop_invalid_socket(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret_value;
        struct pqos_mon_data grp;
        struct pqos_mon_data_internal intl;
        unsigned sockets[] = {3};

        memset(&grp, 0, sizeof(grp));
        memset(&intl, 0, sizeof(intl));
        grp.intl = &intl;
        grp.intl->uncore.sockets = (unsigned *)&sockets;
        grp.intl->uncore.num_sockets = DIM(sockets);

        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);

        ret_value = uncore_mon_stop(&grp);
        assert_int_equal(ret_value, PQOS_RETVAL_ERROR);
}

int
main(void)
{
        int result = 0;

        const struct CMUnitTest tests_uncore_monitoring[] = {
            cmocka_unit_test(test_uncore_monitoring_stop),
            cmocka_unit_test(test_uncore_monitoring_stop_invalid_socket),
        };

        result += cmocka_run_group_tests(tests_uncore_monitoring, test_init_all,
                                         test_fini);

        return result;
}
