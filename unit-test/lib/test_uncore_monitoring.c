/*
 * BSD  LICENSE
 *
 * Copyright(c) 2023-2026 Intel Corporation. All rights reserved.
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
#include "uncore_monitoring.h"

#include <dirent.h> /**< scandir() */
#include <fnmatch.h>
#include <string.h>

/* ======== mock ======== */

void
__wrap_lcpuid(const unsigned leaf,
              const unsigned subleaf,
              struct cpuid_out *out)
{
        check_expected(leaf);
        check_expected(subleaf);

        if (out != NULL)
                out->eax = mock_type(uint32_t);
}

static int
_uncore_mon_init_skx(void **state)
{
        int ret;

        ret = test_init_all(state);
        if (ret != 0)
                return ret;

        expect_value(__wrap_lcpuid, leaf, 1);
        expect_value(__wrap_lcpuid, subleaf, 0);
        will_return(__wrap_lcpuid, 0x00050050);
        expect_string(__wrap_scandir, dirp, "/sys/devices");
        will_return(__wrap_scandir, 1);

        /* initialis of the system */
        ret = uncore_mon_init(NULL, NULL);

        return ret == PQOS_RETVAL_OK ? 0 : -1;
}

static int
_uncore_mon_init_neg(void **state __attribute__((unused)))
{
        int ret;

        expect_value(__wrap_lcpuid, leaf, 1);
        expect_value(__wrap_lcpuid, subleaf, 0);
        will_return(__wrap_lcpuid, 0x00000000);

        /* initialis of the system */
        ret = uncore_mon_init(NULL, NULL);

        test_fini(state);

        return ret == PQOS_RETVAL_RESOURCE ? 0 : -1;
}

static int
_uncore_mon_fini(void **state __attribute__((unused)))
{
        int ret = uncore_mon_fini();

        test_fini(state);

        return ret == PQOS_RETVAL_OK ? 0 : -1;
}

int
__wrap_scandir(const char *restrict dirp,
               struct dirent ***restrict namelist,
               int (*filter)(const struct dirent *) __attribute__((unused)),
               int (*compar)(const struct dirent **, const struct dirent **)
                   __attribute__((unused)))
{
        int ret;

        check_expected(dirp);

        ret = mock_type(int);
        if (ret == 0)
                *namelist = NULL;
        else if (ret == 1) {
                const char *name = "uncore_cha_0";

                *namelist = malloc(sizeof(struct dirent *));
                *namelist[0] = malloc(sizeof(struct dirent) + strlen(name) + 1);
                strcpy((*namelist)[0]->d_name, name);
        }

        return ret;
}

/* ======== uncore_mon_discover ======== */

static void
test_uncore_mon_discover_skx(void **state __attribute__((unused)))
{
        int ret;
        enum pqos_mon_event event_param;

        expect_value(__wrap_lcpuid, leaf, 1);
        expect_value(__wrap_lcpuid, subleaf, 0);
        will_return(__wrap_lcpuid, 0x00050050);

        ret = uncore_mon_discover(&event_param);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(event_param, (PQOS_PERF_EVENT_LLC_MISS_PCIE_READ |
                                       PQOS_PERF_EVENT_LLC_MISS_PCIE_WRITE |
                                       PQOS_PERF_EVENT_LLC_REF_PCIE_READ |
                                       PQOS_PERF_EVENT_LLC_REF_PCIE_WRITE));
}

static void
test_uncore_mon_discover_unsupported(void **state __attribute__((unused)))
{
        int ret;
        enum pqos_mon_event event_param;

        expect_value(__wrap_lcpuid, leaf, 1);
        expect_value(__wrap_lcpuid, subleaf, 0);
        will_return(__wrap_lcpuid, 0x00000000);

        ret = uncore_mon_discover(&event_param);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(event_param, 0);
}

/* ======== uncore_mon_is_event_supported ======== */

static void
test_uncore_mon_is_event_supported_skx(void **state __attribute__((unused)))
{
        int ret;

        ret = uncore_mon_is_event_supported(PQOS_PERF_EVENT_LLC_MISS_PCIE_READ);
        assert_int_equal(ret, 1);

        ret =
            uncore_mon_is_event_supported(PQOS_PERF_EVENT_LLC_MISS_PCIE_WRITE);
        assert_int_equal(ret, 1);

        ret = uncore_mon_is_event_supported(PQOS_PERF_EVENT_LLC_REF_PCIE_READ);
        assert_int_equal(ret, 1);

        ret = uncore_mon_is_event_supported(PQOS_PERF_EVENT_LLC_REF_PCIE_WRITE);
        assert_int_equal(ret, 1);

        ret = uncore_mon_is_event_supported(PQOS_MON_EVENT_L3_OCCUP);
        assert_int_equal(ret, 0);
}

static void
test_uncore_mon_is_event_supported_neg(void **state __attribute__((unused)))
{
        int ret;

        ret = uncore_mon_is_event_supported(PQOS_PERF_EVENT_LLC_MISS_PCIE_READ);
        assert_int_equal(ret, 0);

        ret =
            uncore_mon_is_event_supported(PQOS_PERF_EVENT_LLC_MISS_PCIE_WRITE);
        assert_int_equal(ret, 0);

        ret = uncore_mon_is_event_supported(PQOS_PERF_EVENT_LLC_REF_PCIE_READ);
        assert_int_equal(ret, 0);

        ret = uncore_mon_is_event_supported(PQOS_PERF_EVENT_LLC_REF_PCIE_WRITE);
        assert_int_equal(ret, 0);
}

/* ======== uncore_mon_init ======== */

static void
test_uncore_mon_init_skx(void **state __attribute__((unused)))
{
        int ret_value;
        struct pqos_cpuinfo cpu_param;
        struct pqos_cap cap_param;

        expect_value(__wrap_lcpuid, leaf, 1);
        expect_value(__wrap_lcpuid, subleaf, 0);
        will_return(__wrap_lcpuid, 0x00050050);
        expect_string(__wrap_scandir, dirp, "/sys/devices");
        will_return(__wrap_scandir, 1);

        ret_value = uncore_mon_init(&cpu_param, &cap_param);
        assert_int_equal(ret_value, PQOS_RETVAL_OK);
}

static void
test_uncore_mon_init_unsupported(void **state __attribute__((unused)))
{
        int ret_value;
        struct pqos_cpuinfo cpu_param;
        struct pqos_cap cap_param;

        expect_value(__wrap_lcpuid, leaf, 1);
        expect_value(__wrap_lcpuid, subleaf, 0);
        will_return(__wrap_lcpuid, 0x00000000);

        ret_value = uncore_mon_init(&cpu_param, &cap_param);
        assert_int_equal(ret_value, PQOS_RETVAL_RESOURCE);

        expect_value(__wrap_lcpuid, leaf, 1);
        expect_value(__wrap_lcpuid, subleaf, 0);
        will_return(__wrap_lcpuid, 0x00050050);
        expect_string(__wrap_scandir, dirp, "/sys/devices");
        will_return(__wrap_scandir, 0);

        ret_value = uncore_mon_init(&cpu_param, &cap_param);
        assert_int_equal(ret_value, PQOS_RETVAL_RESOURCE);
}

/* ======== uncore_mon_stop ======== */

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

        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);

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

/* ======== uncore_mon_start ======== */

static void
test_uncore_mon_start_rdt(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        struct pqos_mon_data grp;
        struct pqos_mon_data_internal intl;
        unsigned sockets[] = {0};

        memset(&grp, 0, sizeof(grp));
        memset(&intl, 0, sizeof(intl));
        grp.intl = &intl;
        grp.intl->uncore.sockets = (unsigned *)&sockets;
        grp.intl->uncore.num_sockets = DIM(sockets);

        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);

        /* RDT events only*/
        ret = uncore_mon_start(
            &grp, (enum pqos_mon_event)(
                      PQOS_MON_EVENT_LMEM_BW | PQOS_MON_EVENT_TMEM_BW |
                      PQOS_MON_EVENT_RMEM_BW | PQOS_MON_EVENT_L3_OCCUP));
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_uncore_mon_start_llc_miss_pcie_read(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        struct pqos_mon_data grp;
        struct pqos_mon_data_internal intl;
        unsigned sockets[] = {0};

        memset(&grp, 0, sizeof(grp));
        memset(&intl, 0, sizeof(intl));
        grp.intl = &intl;
        grp.intl->uncore.sockets = (unsigned *)&sockets;
        grp.intl->uncore.num_sockets = DIM(sockets);

        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);

        /* AT_MSR_C_UNIT_CTRL + 0x10 * UNCORE_EVENT_LLC_MISS_PCIE_READ */
        /* UNIT_CTRL_UNFREEZE_COUNTER */
        expect_value(__wrap_msr_write, lcore, 0);
        expect_value(__wrap_msr_write, reg, 3584);
        expect_value(__wrap_msr_write, value, 65536);
        will_return(__wrap_msr_write, PQOS_RETVAL_OK);
        /* AT_MSR_C_UNIT_CTRL + 0x10 * UNCORE_EVENT_LLC_MISS_PCIE_READ */
        /* UNIT_CTRL_FREEZE_COUNTER */
        expect_value(__wrap_msr_write, lcore, 0);
        expect_value(__wrap_msr_write, reg, 3584);
        expect_value(__wrap_msr_write, value, 65792);
        will_return(__wrap_msr_write, PQOS_RETVAL_OK);
        /* AT_MSR_C_UNIT_CTRL + 0x10 * UNCORE_EVENT_LLC_MISS_PCIE_READ +
           OFFSET_CTRL0 */
        expect_value(__wrap_msr_write, lcore, 0);
        expect_value(__wrap_msr_write, reg, 3585);
        expect_value(__wrap_msr_write, value, 4203573);
        will_return(__wrap_msr_write, PQOS_RETVAL_OK);
        /* AT_MSR_C_UNIT_CTRL + 0x10 * UNCORE_EVENT_LLC_MISS_PCIE_READ +
           OFFSET_FILTER1 */
        expect_value(__wrap_msr_write, lcore, 0);
        expect_value(__wrap_msr_write, reg, 3590);
        expect_value(__wrap_msr_write, value, 277555);
        will_return(__wrap_msr_write, PQOS_RETVAL_OK);
        /* AT_MSR_C_UNIT_CTRL + 0x10 * UNCORE_EVENT_LLC_MISS_PCIE_READ */
        /* UNIT_CTRL_RESET_COUNTER */
        expect_value(__wrap_msr_write, lcore, 0);
        expect_value(__wrap_msr_write, reg, 3584);
        expect_value(__wrap_msr_write, value, 65794);
        will_return(__wrap_msr_write, PQOS_RETVAL_OK);
        /* AT_MSR_C_UNIT_CTRL + 0x10 * UNCORE_EVENT_LLC_MISS_PCIE_READ */
        /* UNIT_CTRL_UNFREEZE_COUNTER */
        expect_value(__wrap_msr_write, lcore, 0);
        expect_value(__wrap_msr_write, reg, 3584);
        expect_value(__wrap_msr_write, value, 65536);
        will_return(__wrap_msr_write, PQOS_RETVAL_OK);

        ret = uncore_mon_start(&grp, PQOS_PERF_EVENT_LLC_MISS_PCIE_READ);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_uncore_mon_start_llc_miss_pcie_write(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        struct pqos_mon_data grp;
        struct pqos_mon_data_internal intl;
        unsigned sockets[] = {0};

        memset(&grp, 0, sizeof(grp));
        memset(&intl, 0, sizeof(intl));
        grp.intl = &intl;
        grp.intl->uncore.sockets = (unsigned *)&sockets;
        grp.intl->uncore.num_sockets = DIM(sockets);

        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);

        expect_value(__wrap_msr_write, lcore, 0);
        expect_value(__wrap_msr_write, lcore, 0);
        expect_value(__wrap_msr_write, lcore, 0);
        expect_value(__wrap_msr_write, lcore, 0);
        expect_value(__wrap_msr_write, lcore, 0);
        expect_value(__wrap_msr_write, lcore, 0);
        /* AT_MSR_C_UNIT_CTRL + 0x10 * UNCORE_EVENT_LLC_MISS_PCIE_WRITE */
        expect_value(__wrap_msr_write, reg, 3600);
        /* AT_MSR_C_UNIT_CTRL + 0x10 * UNCORE_EVENT_LLC_MISS_PCIE_WRITE */
        expect_value(__wrap_msr_write, reg, 3600);
        /* AT_MSR_C_UNIT_CTRL + 0x10 * UNCORE_EVENT_LLC_MISS_PCIE_WRITE +
           OFFSET_CTRL0 */
        expect_value(__wrap_msr_write, reg, 3601);
        /* AT_MSR_C_UNIT_CTRL + 0x10 * UNCORE_EVENT_LLC_MISS_PCIE_WRITE +
           OFFSET_FILTER1 */
        expect_value(__wrap_msr_write, reg, 3606);
        /* AT_MSR_C_UNIT_CTRL + 0x10 * UNCORE_EVENT_LLC_MISS_PCIE_WRITE */
        expect_value(__wrap_msr_write, reg, 3600);
        /* AT_MSR_C_UNIT_CTRL + 0x10 * UNCORE_EVENT_LLC_MISS_PCIE_WRITE */
        expect_value(__wrap_msr_write, reg, 3600);
        /* UNIT_CTRL_UNFREEZE_COUNTER */
        expect_value(__wrap_msr_write, value, 65536);
        /* UNIT_CTRL_FREEZE_COUNTER */
        expect_value(__wrap_msr_write, value, 65792);
        /* calculated value */
        expect_value(__wrap_msr_write, value, 4203573);
        /* calculated value */
        expect_value(__wrap_msr_write, value, 0x10049033);
        /* UNIT_CTRL_RESET_COUNTER */
        expect_value(__wrap_msr_write, value, 65794);
        /* UNIT_CTRL_UNFREEZE_COUNTER */
        expect_value(__wrap_msr_write, value, 65536);
        will_return(__wrap_msr_write, PQOS_RETVAL_OK);
        will_return(__wrap_msr_write, PQOS_RETVAL_OK);
        will_return(__wrap_msr_write, PQOS_RETVAL_OK);
        will_return(__wrap_msr_write, PQOS_RETVAL_OK);
        will_return(__wrap_msr_write, PQOS_RETVAL_OK);
        will_return(__wrap_msr_write, PQOS_RETVAL_OK);

        ret = uncore_mon_start(&grp, PQOS_PERF_EVENT_LLC_MISS_PCIE_WRITE);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_uncore_mon_start_llc_ref_pcie_read(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        struct pqos_mon_data grp;
        struct pqos_mon_data_internal intl;
        unsigned sockets[] = {0};

        memset(&grp, 0, sizeof(grp));
        memset(&intl, 0, sizeof(intl));
        grp.intl = &intl;
        grp.intl->uncore.sockets = (unsigned *)&sockets;
        grp.intl->uncore.num_sockets = DIM(sockets);

        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);

        expect_value(__wrap_msr_write, lcore, 0);
        expect_value(__wrap_msr_write, lcore, 0);
        expect_value(__wrap_msr_write, lcore, 0);
        expect_value(__wrap_msr_write, lcore, 0);
        expect_value(__wrap_msr_write, lcore, 0);
        expect_value(__wrap_msr_write, lcore, 0);
        /* AT_MSR_C_UNIT_CTRL + 0x10 * UNCORE_EVENT_LLC_REF_PCIE_READ */
        expect_value(__wrap_msr_write, reg, 3616);
        /* AT_MSR_C_UNIT_CTRL + 0x10 * UNCORE_EVENT_LLC_REF_PCIE_READ */
        expect_value(__wrap_msr_write, reg, 3616);
        /* AT_MSR_C_UNIT_CTRL + 0x10 * UNCORE_EVENT_LLC_REF_PCIE_READ +
           OFFSET_CTRL0 */
        expect_value(__wrap_msr_write, reg, 3617);
        /* AT_MSR_C_UNIT_CTRL + 0x10 * UNCORE_EVENT_LLC_REF_PCIE_READ +
           OFFSET_FILTER1 */
        expect_value(__wrap_msr_write, reg, 3622);
        /* AT_MSR_C_UNIT_CTRL + 0x10 * UNCORE_EVENT_LLC_REF_PCIE_READ */
        expect_value(__wrap_msr_write, reg, 3616);
        /* AT_MSR_C_UNIT_CTRL + 0x10 * UNCORE_EVENT_LLC_REF_PCIE_READ */
        expect_value(__wrap_msr_write, reg, 3616);
        /* UNIT_CTRL_UNFREEZE_COUNTER */
        expect_value(__wrap_msr_write, value, 65536);
        /* UNIT_CTRL_FREEZE_COUNTER */
        expect_value(__wrap_msr_write, value, 65792);
        /* calculated value */
        expect_value(__wrap_msr_write, value, 0x401435);
        /* calculated value */
        expect_value(__wrap_msr_write, value, 277555);
        /* UNIT_CTRL_RESET_COUNTER */
        expect_value(__wrap_msr_write, value, 65794);
        /* UNIT_CTRL_UNFREEZE_COUNTER */
        expect_value(__wrap_msr_write, value, 65536);
        will_return(__wrap_msr_write, PQOS_RETVAL_OK);
        will_return(__wrap_msr_write, PQOS_RETVAL_OK);
        will_return(__wrap_msr_write, PQOS_RETVAL_OK);
        will_return(__wrap_msr_write, PQOS_RETVAL_OK);
        will_return(__wrap_msr_write, PQOS_RETVAL_OK);
        will_return(__wrap_msr_write, PQOS_RETVAL_OK);

        ret = uncore_mon_start(&grp, PQOS_PERF_EVENT_LLC_REF_PCIE_READ);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_uncore_mon_start_llc_ref_pcie_write(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        struct pqos_mon_data grp;
        struct pqos_mon_data_internal intl;
        unsigned sockets[] = {0};

        memset(&grp, 0, sizeof(grp));
        memset(&intl, 0, sizeof(intl));
        grp.intl = &intl;
        grp.intl->uncore.sockets = (unsigned *)&sockets;
        grp.intl->uncore.num_sockets = DIM(sockets);

        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);

        expect_value(__wrap_msr_write, lcore, 0);
        expect_value(__wrap_msr_write, lcore, 0);
        expect_value(__wrap_msr_write, lcore, 0);
        expect_value(__wrap_msr_write, lcore, 0);
        expect_value(__wrap_msr_write, lcore, 0);
        expect_value(__wrap_msr_write, lcore, 0);
        /* AT_MSR_C_UNIT_CTRL + 0x10 * UNCORE_EVENT_LLC_REF_PCIE_WRITE */
        expect_value(__wrap_msr_write, reg, 3632);
        /* AT_MSR_C_UNIT_CTRL + 0x10 * UNCORE_EVENT_LLC_REF_PCIE_WRITE */
        expect_value(__wrap_msr_write, reg, 3632);
        /* AT_MSR_C_UNIT_CTRL + 0x10 * UNCORE_EVENT_LLC_REF_PCIE_WRITE +
           OFFSET_CTRL0 */
        expect_value(__wrap_msr_write, reg, 3633);
        /* AT_MSR_C_UNIT_CTRL + 0x10 * UNCORE_EVENT_LLC_REF_PCIE_WRITE +
           OFFSET_FILTER1 */
        expect_value(__wrap_msr_write, reg, 3638);
        /* AT_MSR_C_UNIT_CTRL + 0x10 * UNCORE_EVENT_LLC_REF_PCIE_WRITE */
        expect_value(__wrap_msr_write, reg, 3632);
        /* AT_MSR_C_UNIT_CTRL + 0x10 * UNCORE_EVENT_LLC_REF_PCIE_WRITE */
        expect_value(__wrap_msr_write, reg, 3632);
        /* UNIT_CTRL_UNFREEZE_COUNTER */
        expect_value(__wrap_msr_write, value, 0x10000);
        /* UNIT_CTRL_FREEZE_COUNTER */
        expect_value(__wrap_msr_write, value, 65792);
        /* calculated value */
        expect_value(__wrap_msr_write, value, 0x401435);
        /* calculated value */
        expect_value(__wrap_msr_write, value, 0x10049033);
        /* UNIT_CTRL_RESET_COUNTER */
        expect_value(__wrap_msr_write, value, 65794);
        /* UNIT_CTRL_UNFREEZE_COUNTER */
        expect_value(__wrap_msr_write, value, 65536);
        will_return(__wrap_msr_write, PQOS_RETVAL_OK);
        will_return(__wrap_msr_write, PQOS_RETVAL_OK);
        will_return(__wrap_msr_write, PQOS_RETVAL_OK);
        will_return(__wrap_msr_write, PQOS_RETVAL_OK);
        will_return(__wrap_msr_write, PQOS_RETVAL_OK);
        will_return(__wrap_msr_write, PQOS_RETVAL_OK);

        ret = uncore_mon_start(&grp, PQOS_PERF_EVENT_LLC_REF_PCIE_WRITE);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_uncore_mon_start_error(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        struct pqos_mon_data grp;
        struct pqos_mon_data_internal intl;
        unsigned sockets[] = {0};

        memset(&grp, 0, sizeof(grp));
        memset(&intl, 0, sizeof(intl));
        grp.intl = &intl;
        grp.intl->uncore.sockets = (unsigned *)&sockets;
        grp.intl->uncore.num_sockets = DIM(sockets);

        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);

        expect_value(__wrap_msr_write, lcore, 0);
        /* AT_MSR_C_UNIT_CTRL + 0x10 * UNCORE_EVENT_LLC_MISS_PCIE_READ */
        expect_value(__wrap_msr_write, reg, 3584);
        /* UNIT_CTRL_UNFREEZE_COUNTER */
        expect_value(__wrap_msr_write, value, 65536);
        will_return(__wrap_msr_write, PQOS_RETVAL_ERROR);

        ret = uncore_mon_start(&grp, PQOS_PERF_EVENT_LLC_MISS_PCIE_READ);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);

        expect_value(__wrap_msr_write, lcore, 0);
        expect_value(__wrap_msr_write, lcore, 0);
        expect_value(__wrap_msr_write, lcore, 0);
        /* AT_MSR_C_UNIT_CTRL + 0x10 * UNCORE_EVENT_LLC_MISS_PCIE_READ */
        expect_value(__wrap_msr_write, reg, 3584);
        /* AT_MSR_C_UNIT_CTRL + 0x10 * UNCORE_EVENT_LLC_MISS_PCIE_READ */
        expect_value(__wrap_msr_write, reg, 3584);
        /* AT_MSR_C_UNIT_CTRL + 0x10 * UNCORE_EVENT_LLC_MISS_PCIE_READ +
           OFFSET_CTRL0 */
        expect_value(__wrap_msr_write, reg, 3585);
        /* UNIT_CTRL_UNFREEZE_COUNTER */
        expect_value(__wrap_msr_write, value, 65536);
        /* UNIT_CTRL_FREEZE_COUNTER */
        expect_value(__wrap_msr_write, value, 65792);
        /* calculated value */
        expect_value(__wrap_msr_write, value, 4203573);
        will_return(__wrap_msr_write, PQOS_RETVAL_OK);
        will_return(__wrap_msr_write, PQOS_RETVAL_OK);
        will_return(__wrap_msr_write, PQOS_RETVAL_ERROR);

        ret = uncore_mon_start(&grp, PQOS_PERF_EVENT_LLC_MISS_PCIE_READ);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);

        expect_value(__wrap_msr_write, lcore, 0);
        expect_value(__wrap_msr_write, lcore, 0);
        expect_value(__wrap_msr_write, lcore, 0);
        expect_value(__wrap_msr_write, lcore, 0);
        expect_value(__wrap_msr_write, lcore, 0);
        /* AT_MSR_C_UNIT_CTRL + 0x10 * UNCORE_EVENT_LLC_MISS_PCIE_READ */
        expect_value(__wrap_msr_write, reg, 3584);
        /* AT_MSR_C_UNIT_CTRL + 0x10 * UNCORE_EVENT_LLC_MISS_PCIE_READ */
        expect_value(__wrap_msr_write, reg, 3584);
        /* AT_MSR_C_UNIT_CTRL + 0x10 * UNCORE_EVENT_LLC_MISS_PCIE_READ +
           OFFSET_CTRL0 */
        expect_value(__wrap_msr_write, reg, 3585);
        /* AT_MSR_C_UNIT_CTRL + 0x10 * UNCORE_EVENT_LLC_MISS_PCIE_READ +
           OFFSET_FILTER1 */
        expect_value(__wrap_msr_write, reg, 3590);
        /* AT_MSR_C_UNIT_CTRL + 0x10 * UNCORE_EVENT_LLC_MISS_PCIE_READ */
        expect_value(__wrap_msr_write, reg, 3584);
        /* UNIT_CTRL_UNFREEZE_COUNTER */
        expect_value(__wrap_msr_write, value, 65536);
        /* UNIT_CTRL_FREEZE_COUNTER */
        expect_value(__wrap_msr_write, value, 65792);
        /* calculated value */
        expect_value(__wrap_msr_write, value, 4203573);
        /* calculated value */
        expect_value(__wrap_msr_write, value, 277555);
        /* UNIT_CTRL_RESET_COUNTER */
        expect_value(__wrap_msr_write, value, 65794);
        will_return(__wrap_msr_write, PQOS_RETVAL_OK);
        will_return(__wrap_msr_write, PQOS_RETVAL_OK);
        will_return(__wrap_msr_write, PQOS_RETVAL_OK);
        will_return(__wrap_msr_write, PQOS_RETVAL_OK);
        will_return(__wrap_msr_write, PQOS_RETVAL_ERROR);

        ret = uncore_mon_start(&grp, PQOS_PERF_EVENT_LLC_MISS_PCIE_READ);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
}

static void
test_uncore_mon_start_param(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        struct pqos_mon_data grp;
        struct pqos_mon_data_internal intl;
        unsigned sockets[] = {3};

        memset(&grp, 0, sizeof(grp));
        memset(&intl, 0, sizeof(intl));
        grp.intl = &intl;
        grp.intl->uncore.sockets = (unsigned *)&sockets;
        grp.intl->uncore.num_sockets = DIM(sockets);

        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);

        ret = uncore_mon_start(&grp, PQOS_PERF_EVENT_LLC_MISS_PCIE_READ);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
}

/* ======== uncore_mon_start ======== */

static void
test_uncore_mon_poll_llc_miss_pcie_read(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        struct pqos_mon_data grp;
        struct pqos_mon_data_internal intl;
        unsigned sockets[] = {0};
        uint64_t value = 0xDEAD;

        memset(&grp, 0, sizeof(grp));
        memset(&intl, 0, sizeof(intl));
        grp.intl = &intl;
        grp.intl->uncore.sockets = (unsigned *)&sockets;
        grp.intl->uncore.num_sockets = DIM(sockets);

        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);

        expect_value(__wrap_msr_read, lcore, 0);
        /* reg_unit_ctrl = IAT_MSR_C_UNIT_CTRL + 0x10 *
           UNCORE_EVENT_LLC_MISS_PCIE_READ + OFFSET_CTR0 */
        expect_value(__wrap_msr_read, reg, 3592);
        will_return(__wrap_msr_read, MACHINE_RETVAL_OK);
        will_return(__wrap_msr_read, value);

        ret = uncore_mon_poll(&grp, PQOS_PERF_EVENT_LLC_MISS_PCIE_READ);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(grp.intl->values.pcie.llc_misses.read, value);
}

static void
test_uncore_mon_poll_llc_miss_pcie_write(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        struct pqos_mon_data grp;
        struct pqos_mon_data_internal intl;
        unsigned sockets[] = {0};
        uint64_t value = 0xDEAD;

        memset(&grp, 0, sizeof(grp));
        memset(&intl, 0, sizeof(intl));
        grp.intl = &intl;
        grp.intl->uncore.sockets = (unsigned *)&sockets;
        grp.intl->uncore.num_sockets = DIM(sockets);

        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);

        expect_value(__wrap_msr_read, lcore, 0);
        /* reg_unit_ctrl = IAT_MSR_C_UNIT_CTRL + 0x10 *
           UNCORE_EVENT_LLC_MISS_PCIE_WRITE + OFFSET_CTR0 */
        expect_value(__wrap_msr_read, reg, 3608);
        will_return(__wrap_msr_read, MACHINE_RETVAL_OK);
        will_return(__wrap_msr_read, value);

        ret = uncore_mon_poll(&grp, PQOS_PERF_EVENT_LLC_MISS_PCIE_WRITE);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(grp.intl->values.pcie.llc_misses.write, value);
}

static void
test_uncore_mon_poll_llc_ref_pcie_read(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        struct pqos_mon_data grp;
        struct pqos_mon_data_internal intl;
        unsigned sockets[] = {0};
        uint64_t value = 0xDEAD;

        memset(&grp, 0, sizeof(grp));
        memset(&intl, 0, sizeof(intl));
        grp.intl = &intl;
        grp.intl->uncore.sockets = (unsigned *)&sockets;
        grp.intl->uncore.num_sockets = DIM(sockets);

        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);

        expect_value(__wrap_msr_read, lcore, 0);
        expect_value(__wrap_msr_read, reg, 3624);
        /* reg_unit_ctrl = IAT_MSR_C_UNIT_CTRL + 0x10 *
           UNCORE_EVENT_LLC_REF_PCIE_READ + OFFSET_CTR0 */
        will_return(__wrap_msr_read, MACHINE_RETVAL_OK);
        will_return(__wrap_msr_read, value);

        ret = uncore_mon_poll(&grp, PQOS_PERF_EVENT_LLC_REF_PCIE_READ);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(grp.intl->values.pcie.llc_references.read, value);
}

static void
test_uncore_mon_poll_llc_ref_pcie_write(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        struct pqos_mon_data grp;
        struct pqos_mon_data_internal intl;
        unsigned sockets[] = {0};
        uint64_t value = 0xDEAD;

        memset(&grp, 0, sizeof(grp));
        memset(&intl, 0, sizeof(intl));
        grp.intl = &intl;
        grp.intl->uncore.sockets = (unsigned *)&sockets;
        grp.intl->uncore.num_sockets = DIM(sockets);

        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);

        expect_value(__wrap_msr_read, lcore, 0);
        /* reg_unit_ctrl = IAT_MSR_C_UNIT_CTRL + 0x10 *
           UNCORE_EVENT_LLC_REF_PCIE_WRITE + OFFSET_CTR0 */
        expect_value(__wrap_msr_read, reg, 3640);
        will_return(__wrap_msr_read, MACHINE_RETVAL_OK);
        will_return(__wrap_msr_read, value);

        ret = uncore_mon_poll(&grp, PQOS_PERF_EVENT_LLC_REF_PCIE_WRITE);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(grp.intl->values.pcie.llc_references.write, value);
}

static void
test_uncore_mon_poll_param(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        struct pqos_mon_data grp;
        struct pqos_mon_data_internal intl;
        unsigned sockets[] = {0};

        memset(&grp, 0, sizeof(grp));
        memset(&intl, 0, sizeof(intl));
        grp.intl = &intl;
        grp.intl->uncore.sockets = (unsigned *)&sockets;
        grp.intl->uncore.num_sockets = DIM(sockets);

        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);

        ret = uncore_mon_poll(&grp, PQOS_MON_EVENT_L3_OCCUP);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);

        sockets[0] = 3;

        ret = uncore_mon_poll(&grp, PQOS_PERF_EVENT_LLC_MISS_PCIE_READ);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
}

static void
test_uncore_mon_poll_error(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        struct pqos_mon_data grp;
        struct pqos_mon_data_internal intl;
        unsigned sockets[] = {0};

        memset(&grp, 0, sizeof(grp));
        memset(&intl, 0, sizeof(intl));
        grp.intl = &intl;
        grp.intl->uncore.sockets = (unsigned *)&sockets;
        grp.intl->uncore.num_sockets = DIM(sockets);

        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);

        expect_value(__wrap_msr_read, lcore, 0);
        /* reg_unit_ctrl = IAT_MSR_C_UNIT_CTRL + 0x10 *
           UNCORE_EVENT_LLC_MISS_PCIE_READ + OFFSET_CTR0 */
        expect_value(__wrap_msr_read, reg, 3592);
        will_return(__wrap_msr_read, MACHINE_RETVAL_ERROR);

        ret = uncore_mon_poll(&grp, PQOS_PERF_EVENT_LLC_MISS_PCIE_READ);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
}

int
main(void)
{
        int result = 0;

        const struct CMUnitTest tests_init[] = {
            cmocka_unit_test(test_uncore_mon_init_skx),
            cmocka_unit_test(test_uncore_mon_init_unsupported),
            cmocka_unit_test(test_uncore_mon_discover_skx),
            cmocka_unit_test(test_uncore_mon_discover_unsupported),
        };

        const struct CMUnitTest tests_skx[] = {
            cmocka_unit_test(test_uncore_mon_is_event_supported_skx),
            cmocka_unit_test(test_uncore_monitoring_stop),
            cmocka_unit_test(test_uncore_monitoring_stop_invalid_socket),
            cmocka_unit_test(test_uncore_mon_start_rdt),
            cmocka_unit_test(test_uncore_mon_start_llc_miss_pcie_read),
            cmocka_unit_test(test_uncore_mon_start_llc_miss_pcie_write),
            cmocka_unit_test(test_uncore_mon_start_llc_ref_pcie_read),
            cmocka_unit_test(test_uncore_mon_start_llc_ref_pcie_write),
            cmocka_unit_test(test_uncore_mon_start_error),
            cmocka_unit_test(test_uncore_mon_start_param),
            cmocka_unit_test(test_uncore_mon_poll_llc_miss_pcie_read),
            cmocka_unit_test(test_uncore_mon_poll_llc_miss_pcie_write),
            cmocka_unit_test(test_uncore_mon_poll_llc_ref_pcie_read),
            cmocka_unit_test(test_uncore_mon_poll_llc_ref_pcie_write),
            cmocka_unit_test(test_uncore_mon_poll_param),
            cmocka_unit_test(test_uncore_mon_poll_error),
        };

        const struct CMUnitTest tests_neg[] = {
            cmocka_unit_test(test_uncore_mon_is_event_supported_neg),
        };

        result += cmocka_run_group_tests(tests_init, NULL, _uncore_mon_fini);
        result += cmocka_run_group_tests(tests_skx, _uncore_mon_init_skx,
                                         _uncore_mon_fini);
        result += cmocka_run_group_tests(tests_neg, _uncore_mon_init_neg,
                                         _uncore_mon_fini);

        return result;
}
