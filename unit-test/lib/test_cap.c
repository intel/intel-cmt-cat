/*
 * BSD LICENSE
 *
 * Copyright(c) 2022-2026 Intel Corporation. All rights reserved.
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

#include "test_cap.h"

#include "cap.h"
#include "cores_domains.h"
#include "erdt.h"
#include "log.h"
#include "mem_regions.h"
#include "mock_cap.h"
#include "mock_iordt.h"
#include "mrrm.h"
#include "test.h"

#include <fcntl.h>
#include <sys/stat.h>

/* ======== mock ========*/

int
__wrap_hybrid_cap_discover(struct pqos_hybrid_capabilities **cap,
                           const struct pqos_cpuinfo *cpu)
{
        assert_non_null(cap);
        assert_non_null(cpu);
        /* "the processors could not be read", which the library carries on
         * without. RESOURCE would mean an allocation failed, and that is fatal
         */
        return PQOS_RETVAL_UNAVAILABLE;
}
int
__wrap_cpuinfo_init(enum pqos_interface interface __attribute__((unused)),
                    int prepare_for_access __attribute__((unused)),
                    const struct pqos_cpuinfo **topology)
{
        assert_non_null(topology);
        *topology = mock_type(const struct pqos_cpuinfo *);

        return 0;
}

int
__wrap_cpuinfo_fini(void)
{
        return 0;
};

int
__wrap_hw_cap_mon_discover(struct pqos_cap_mon **r_cap,
                           const struct pqos_cpuinfo *cpu
                           __attribute__((unused)))
{
        function_called();
        assert_non_null(r_cap);
        *r_cap = mock_type(struct pqos_cap_mon *);

        return 0;
}

int
__wrap_os_cap_mon_discover(struct pqos_cap_mon **r_cap,
                           const struct pqos_cpuinfo *cpu
                           __attribute__((unused)))
{
        function_called();
        assert_non_null(r_cap);
        *r_cap = mock_type(struct pqos_cap_mon *);

        return 0;
}

int
__wrap_hw_cap_l3ca_discover(struct pqos_cap_l3ca *cap,
                            const struct pqos_cpuinfo *cpu
                            __attribute__((unused)))
{
        assert_non_null(cap);
        memcpy(cap, mock_type(struct pqos_cap_l3ca *), sizeof(*cap));

        return 0;
}

int
__wrap_os_cap_l3ca_discover(struct pqos_cap_l3ca *cap,
                            const struct pqos_cpuinfo *cpu
                            __attribute__((unused)))
{
        assert_non_null(cap);
        memcpy(cap, mock_type(struct pqos_cap_l3ca *), sizeof(*cap));

        return 0;
}

int
__wrap_hw_cap_l2ca_discover(struct pqos_cap_l2ca *cap,
                            const struct pqos_cpuinfo *cpu
                            __attribute__((unused)))
{
        assert_non_null(cap);
        memcpy(cap, mock_type(struct pqos_cap_l2ca *), sizeof(*cap));

        return 0;
}

int
__wrap_os_cap_l2ca_discover(struct pqos_cap_l2ca *cap,
                            const struct pqos_cpuinfo *cpu
                            __attribute__((unused)))
{
        assert_non_null(cap);
        memcpy(cap, mock_type(struct pqos_cap_l2ca *), sizeof(*cap));

        return 0;
}

int
__wrap_hw_cap_mba_discover(struct pqos_cap_mba *cap,
                           const struct pqos_cpuinfo *cpu
                           __attribute__((unused)))
{
        assert_non_null(cap);
        memcpy(cap, mock_type(struct pqos_cap_mba *), sizeof(*cap));

        return 0;
}

int
__wrap_os_cap_mba_discover(struct pqos_cap_mba *cap,
                           const struct pqos_cpuinfo *cpu
                           __attribute__((unused)))
{
        assert_non_null(cap);
        memcpy(cap, mock_type(struct pqos_cap_mba *), sizeof(*cap));

        return 0;
}

int
__wrap_os_cap_init(const enum pqos_interface inter)
{
        const LargestIntegralType valid_inters[] = {PQOS_INTER_OS,
                                                    PQOS_INTER_OS_RESCTRL_MON};

        function_called();
        assert_in_set(inter, valid_inters, DIM(valid_inters));
        return PQOS_RETVAL_OK;
}

/* the MMIO case's ERDT: where the case sets mmio_case, the wrapped
 * erdt_init() gives it and the core to domain map's init fails, unless the
 * case sets mmio_init_ok too, where the init goes through: the maps and the
 * MRRM it needs are given without a platform, the device table has no
 * channel, and the memory regions are not described. Elsewhere the originals
 * run, and find no table
 */
static int mmio_case;
static int mmio_init_ok;
static struct pqos_erdt_info mmio_erdt;
static struct pqos_cores_domains mmio_cores_domains;
static struct pqos_mrrm_info mmio_mrrm;
static struct pqos_devinfo mmio_dev;

int
__wrap_erdt_init(const struct pqos_cap *cap,
                 struct pqos_cpuinfo *cpu,
                 struct pqos_erdt_info **erdt_info)
{
        if (!mmio_case)
                return __real_erdt_init(cap, cpu, erdt_info);

        *erdt_info = &mmio_erdt;
        return PQOS_RETVAL_OK;
}

void
__wrap_erdt_fini(void)
{
        if (mmio_case)
                function_called();
        else
                __real_erdt_fini();
}

int
__wrap_cores_domains_init(unsigned int num_cores,
                          const struct pqos_erdt_info *erdt,
                          struct pqos_cores_domains **cores_domains)
{
        if (!mmio_case)
                return __real_cores_domains_init(num_cores, erdt,
                                                 cores_domains);

        function_called();
        assert_ptr_equal(erdt, &mmio_erdt);
        if (!mmio_init_ok)
                return PQOS_RETVAL_ERROR;

        mmio_cores_domains.num_cores = num_cores;
        *cores_domains = &mmio_cores_domains;
        return PQOS_RETVAL_OK;
}

void
__wrap_cores_domains_fini(void)
{
        if (mmio_case)
                function_called();
        else
                __real_cores_domains_fini();
}

int
__wrap_channels_domains_init(unsigned int num_channels,
                             const struct pqos_erdt_info *erdt,
                             const struct pqos_devinfo *devinfo,
                             struct pqos_channels_domains **channels_domains)
{
        if (!mmio_case)
                return __real_channels_domains_init(num_channels, erdt, devinfo,
                                                    channels_domains);

        assert_ptr_equal(erdt, &mmio_erdt);
        assert_ptr_equal(devinfo, &mmio_dev);
        *channels_domains = NULL;
        return PQOS_RETVAL_OK;
}

int
__wrap_mrrm_init(const struct pqos_cap *cap, struct pqos_mrrm_info **mrrm)
{
        if (!mmio_case)
                return __real_mrrm_init(cap, mrrm);

        *mrrm = &mmio_mrrm;
        return PQOS_RETVAL_OK;
}

int
__wrap_mem_regions_init(const struct pqos_mrrm_info *mrrm,
                        struct pqos_mem_regions **regions)
{
        if (!mmio_case)
                return __real_mem_regions_init(mrrm, regions);

        /* the regions are not described; the init goes on without them */
        return PQOS_RETVAL_RESOURCE;
}

/* ======== setup ========*/
static int
setup_cap_init_msr(void **state __attribute__((unused)))
{
        int ret;
        struct test_data *data;
        unsigned technology = 0;

        technology |= 1 << PQOS_CAP_TYPE_MBA;
        technology |= 1 << PQOS_CAP_TYPE_L3CA;
        technology |= 1 << PQOS_CAP_TYPE_L2CA;

        ret = test_init(state, technology);
        if (ret != 0)
                return ret;

        data = (struct test_data *)*state;
        data->interface = PQOS_INTER_MSR;

        return 0;
}

static int
setup_cap_init_os(void **state __attribute__((unused)))
{
        int ret;
        struct test_data *data;
        unsigned technology = 0;

        technology |= 1 << PQOS_CAP_TYPE_MBA;
        technology |= 1 << PQOS_CAP_TYPE_L3CA;
        technology |= 1 << PQOS_CAP_TYPE_L2CA;

        ret = test_init(state, technology);
        if (ret != 0)
                return ret;

        data = (struct test_data *)*state;
        data->interface = PQOS_INTER_OS;

        return 0;
}

static int
setup_cap_init_os_resctrl_mon(void **state __attribute__((unused)))
{
        int ret;
        struct test_data *data;
        unsigned technology = 0;

        technology |= 1 << PQOS_CAP_TYPE_MBA;
        technology |= 1 << PQOS_CAP_TYPE_L3CA;
        technology |= 1 << PQOS_CAP_TYPE_L2CA;

        ret = test_init(state, technology);
        if (ret != 0)
                return ret;

        data = (struct test_data *)*state;
        data->interface = PQOS_INTER_OS_RESCTRL_MON;

        return 0;
}

/* ======== _pqos_check_init  ======== */

static void
test__pqos_check_init_before_init(void **state __attribute__((unused)))
{
        int ret;

        ret = _pqos_check_init(0);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        ret = _pqos_check_init(1);
        assert_int_not_equal(ret, PQOS_RETVAL_OK);
}

static void
test__pqos_check_init_after_init(void **state __attribute__((unused)))
{
        int ret;

        ret = _pqos_check_init(0);
        assert_int_not_equal(ret, PQOS_RETVAL_OK);
        ret = _pqos_check_init(1);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

/* ======== _pqos_get_cap  ======== */

static void
test__pqos_get_cap_before_init(void **state __attribute__((unused)))
{
        SKIP_IF_ASSERTS_ARE_LIVE();

        const struct pqos_cap *ret;

        ret = __real__pqos_get_cap();
        assert_null(ret);
}

static void
test__pqos_get_cap_after_init(void **state __attribute__((unused)))
{
        const struct pqos_cap *ret;

        ret = __real__pqos_get_cap();
        assert_non_null(ret);
}

/* ======== _pqos_get_cpu  ======== */

static void
test__pqos_get_cpu_before_init(void **state __attribute__((unused)))
{
        SKIP_IF_ASSERTS_ARE_LIVE();

        const struct pqos_cpuinfo *ret;

        ret = __real__pqos_get_cpu();
        assert_null(ret);
}

static void
test__pqos_get_cpu_after_init(void **state __attribute__((unused)))
{
        const struct pqos_cpuinfo *ret;

        ret = __real__pqos_get_cpu();
        assert_non_null(ret);
}

/* ======== pqos_init ======== */

static void
test_pqos_init_param(void **state __attribute__((unused)))
{
        int ret;

        ret = pqos_init(NULL);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
}

static void
test_pqos_init(void **state __attribute__((unused)))
{
        int ret;
        struct pqos_config cfg;
        struct test_data *data;

        data = (struct test_data *)*state;

        assert_non_null(data);

        memset(&cfg, 0, sizeof(cfg));
        cfg.verbose = LOG_VER_SILENT;
        cfg.fd_log = -1;
        cfg.interface = data->interface;

        expect_function_call(__wrap_lock_init);
        will_return(__wrap_lock_init, 0);
        expect_function_call(__wrap_lock_get);

        will_return(__wrap_cpuinfo_init, data->cpu);
        will_return(__wrap_machine_init, PQOS_RETVAL_OK);
        expect_function_call(__wrap_machine_init);
        if (data->interface == PQOS_INTER_MSR) {
                will_return(__wrap_hw_cap_mon_discover, data->cap_mon);
                will_return(__wrap_hw_cap_l3ca_discover, &(data->cap_l3ca));
                will_return(__wrap_hw_cap_l2ca_discover, &(data->cap_l2ca));
                will_return(__wrap_hw_cap_mba_discover, &(data->cap_mba));
        }
#ifdef __linux__
        else if (data->interface == PQOS_INTER_OS ||
                 data->interface == PQOS_INTER_OS_RESCTRL_MON) {
                will_return(__wrap_os_cap_mon_discover, data->cap_mon);
                will_return(__wrap_os_cap_l3ca_discover, &(data->cap_l3ca));
                will_return(__wrap_os_cap_l2ca_discover, &(data->cap_l2ca));
                will_return(__wrap_os_cap_mba_discover, &(data->cap_mba));
        };
#endif
#ifdef __linux__
        if (data->interface != PQOS_INTER_MSR)
                expect_function_call(__wrap_os_cap_init);
#endif
        if (data->interface == PQOS_INTER_MSR)
                expect_function_call(__wrap_hw_cap_mon_discover);
#ifdef __linux__
        else if (data->interface == PQOS_INTER_OS ||
                 data->interface == PQOS_INTER_OS_RESCTRL_MON)
                expect_function_call(__wrap_os_cap_mon_discover);
#endif
        expect_function_call(__wrap_pqos_alloc_init);
        will_return(__wrap_pqos_alloc_init, PQOS_RETVAL_OK);
        expect_function_call(__wrap_pqos_mon_init);
        will_return(__wrap_pqos_mon_init, PQOS_RETVAL_OK);
        expect_function_call(__wrap_iordt_init);
        will_return(__wrap_iordt_init, PQOS_RETVAL_OK);
        expect_function_call(__wrap_lock_release);

        ret = pqos_init(&cfg);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

/* an init over MMIO whose core to domain map fails, fails, and frees the
 * ERDT read before it: no pqos_fini() follows a failed init to free it
 */
static void
test_pqos_init_mmio_map_failure_frees_the_erdt(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        struct pqos_config cfg;
        int ret;

        memset(&cfg, 0, sizeof(cfg));
        cfg.verbose = LOG_VER_SILENT;
        cfg.fd_log = -1;
        cfg.interface = PQOS_INTER_MMIO;

        expect_function_call(__wrap_lock_init);
        will_return(__wrap_lock_init, 0);
        expect_function_call(__wrap_lock_get);
        will_return(__wrap_cpuinfo_init, data->cpu);
        will_return(__wrap_machine_init, PQOS_RETVAL_OK);
        expect_function_call(__wrap_machine_init);
        will_return(__wrap_hw_cap_mon_discover, data->cap_mon);
        will_return(__wrap_hw_cap_l3ca_discover, &(data->cap_l3ca));
        will_return(__wrap_hw_cap_mba_discover, &(data->cap_mba));
        expect_function_call(__wrap_hw_cap_mon_discover);
        expect_function_call(__wrap_cores_domains_init);
        expect_function_call(__wrap_erdt_fini);
        expect_function_call(__wrap_machine_fini);
        will_return(__wrap_machine_fini, PQOS_RETVAL_OK);
        expect_function_call(__wrap_lock_release);
        expect_function_call(__wrap_lock_fini);
        will_return(__wrap_lock_fini, 0);

        mmio_case = 1;
        mmio_erdt.max_clos = 0;
        ret = pqos_init(&cfg);
        mmio_case = 0;

        assert_int_equal(ret, PQOS_RETVAL_ERROR);
}

/* an init over MMIO on a table whose classes do not fit its register blocks
 * fails before the core to domain map is made, and frees the table
 */
static void
test_pqos_init_mmio_refuses_classes_that_do_not_fit(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        struct pqos_config cfg;
        int ret;

        memset(&cfg, 0, sizeof(cfg));
        cfg.verbose = LOG_VER_SILENT;
        cfg.fd_log = -1;
        cfg.interface = PQOS_INTER_MMIO;

        expect_function_call(__wrap_lock_init);
        will_return(__wrap_lock_init, 0);
        expect_function_call(__wrap_lock_get);
        will_return(__wrap_cpuinfo_init, data->cpu);
        will_return(__wrap_machine_init, PQOS_RETVAL_OK);
        expect_function_call(__wrap_machine_init);
        will_return(__wrap_hw_cap_mon_discover, data->cap_mon);
        will_return(__wrap_hw_cap_l3ca_discover, &(data->cap_l3ca));
        will_return(__wrap_hw_cap_mba_discover, &(data->cap_mba));
        expect_function_call(__wrap_hw_cap_mon_discover);
        /* no cores_domains_init(): the table is refused before it */
        expect_function_call(__wrap_erdt_fini);
        expect_function_call(__wrap_machine_fini);
        will_return(__wrap_machine_fini, PQOS_RETVAL_OK);
        expect_function_call(__wrap_lock_release);
        expect_function_call(__wrap_lock_fini);
        will_return(__wrap_lock_fini, 0);

        /* a Max CLOS that counts no classes */
        mmio_case = 1;
        mmio_erdt.max_clos = UINT32_MAX;
        ret = pqos_init(&cfg);
        mmio_case = 0;
        mmio_erdt.max_clos = 0;

        assert_int_equal(ret, PQOS_RETVAL_ERROR);
}

/* an init over MMIO on a table of one class, Max CLOS 0, where CPUID has
 * more: the L3 CAT and MBA capabilities keep CPUID's counts, since a core's
 * association and the mask range alloc_reset() writes are the CPU's, and a
 * CDP change keeps the count it discovers, halved by CDP, not one capped at
 * the table's. The table's count is the I/O L3 CAT and MBA registers', which
 * the MMIO paths count themselves
 */
static void
test_pqos_init_mmio_keeps_the_cpuid_counts(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        struct pqos_config cfg;
        const struct pqos_cap *p_cap = NULL;
        const struct pqos_cap_l3ca *l3ca = NULL;
        const struct pqos_cap_mba *mba = NULL;
        struct pqos_cap_l3ca discovered;
        unsigned i;
        int ret;

        memset(&cfg, 0, sizeof(cfg));
        cfg.verbose = LOG_VER_SILENT;
        cfg.fd_log = -1;
        cfg.interface = PQOS_INTER_MMIO;

        /* the table's one class is fewer than CPUID's, with CDP on as well */
        assert_true(data->cap_l3ca.num_classes / 2 > 1);
        assert_true(data->cap_mba.num_classes > 1);

        expect_function_call(__wrap_lock_init);
        will_return(__wrap_lock_init, 0);
        expect_function_call(__wrap_lock_get);
        will_return(__wrap_cpuinfo_init, data->cpu);
        will_return(__wrap_machine_init, PQOS_RETVAL_OK);
        expect_function_call(__wrap_machine_init);
        will_return(__wrap_hw_cap_mon_discover, data->cap_mon);
        will_return(__wrap_hw_cap_l3ca_discover, &(data->cap_l3ca));
        will_return(__wrap_hw_cap_mba_discover, &(data->cap_mba));
        expect_function_call(__wrap_hw_cap_mon_discover);
        expect_function_call(__wrap_cores_domains_init);
        expect_function_call(__wrap_pqos_alloc_init);
        will_return(__wrap_pqos_alloc_init, PQOS_RETVAL_OK);
        expect_function_call(__wrap_pqos_mon_init);
        will_return(__wrap_pqos_mon_init, PQOS_RETVAL_OK);
        expect_function_call(__wrap_iordt_init);
        will_return(__wrap_iordt_init, PQOS_RETVAL_OK);
        expect_function_call(__wrap_lock_release);

        mmio_case = 1;
        mmio_init_ok = 1;
        mmio_erdt.max_clos = 0;
        mock_iordt_init_devinfo = &mmio_dev;
        ret = pqos_init(&cfg);
        mock_iordt_init_devinfo = NULL;
        assert_int_equal(ret, PQOS_RETVAL_OK);

        /* the library's own copy of the capabilities */
        expect_function_call(__wrap_lock_get);
        expect_function_call(__wrap_lock_release);
        ret = pqos_cap_get(&p_cap, NULL);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        for (i = 0; i < p_cap->num_cap; i++) {
                if (p_cap->capabilities[i].type == PQOS_CAP_TYPE_L3CA)
                        l3ca = p_cap->capabilities[i].u.l3ca;
                if (p_cap->capabilities[i].type == PQOS_CAP_TYPE_MBA)
                        mba = p_cap->capabilities[i].u.mba;
        }
        assert_non_null(l3ca);
        assert_non_null(mba);
        assert_int_equal(l3ca->num_classes, data->cap_l3ca.num_classes);
        assert_int_equal(mba->num_classes, data->cap_mba.num_classes);

        /* CDP halves CPUID's count; the change keeps what it discovers, the
         * count alloc_reset() doubles back into the CPU's mask registers
         */
        discovered = data->cap_l3ca;
        discovered.num_classes = data->cap_l3ca.num_classes / 2;
        discovered.cdp_on = 1;
        will_return(__wrap_hw_cap_l3ca_discover, &discovered);
        _pqos_cap_l3cdp_change(PQOS_REQUIRE_CDP_ON);
        assert_int_equal(l3ca->num_classes, discovered.num_classes);

        expect_function_call(__wrap_lock_get);
        expect_function_call(__wrap_pqos_mon_fini);
        will_return(__wrap_pqos_mon_fini, PQOS_RETVAL_OK);
        expect_function_call(__wrap_pqos_alloc_fini);
        will_return(__wrap_pqos_alloc_fini, PQOS_RETVAL_OK);
        expect_function_call(__wrap_cores_domains_fini);
        expect_function_call(__wrap_iordt_fini);
        will_return(__wrap_iordt_fini, PQOS_RETVAL_OK);
        expect_function_call(__wrap_erdt_fini);
        expect_function_call(__wrap_machine_fini);
        will_return(__wrap_machine_fini, PQOS_RETVAL_OK);
        expect_function_call(__wrap_lock_release);
        expect_function_call(__wrap_lock_fini);
        will_return(__wrap_lock_fini, 0);
        ret = pqos_fini();
        assert_int_equal(ret, PQOS_RETVAL_OK);

        mmio_init_ok = 0;
        mmio_case = 0;
}

/* ======== pqos_fini ======== */

static void
test_pqos_fini(void **state __attribute__((unused)))
{
        int ret;

        expect_function_call(__wrap_lock_get);
        expect_function_call(__wrap_pqos_mon_fini);
        will_return(__wrap_pqos_mon_fini, PQOS_RETVAL_OK);
        expect_function_call(__wrap_pqos_alloc_fini);
        will_return(__wrap_pqos_alloc_fini, PQOS_RETVAL_OK);
        expect_function_call(__wrap_iordt_fini);
        will_return(__wrap_iordt_fini, PQOS_RETVAL_OK);
        expect_function_call(__wrap_machine_fini);
        will_return(__wrap_machine_fini, PQOS_RETVAL_OK);
        expect_function_call(__wrap_lock_release);
        expect_function_call(__wrap_lock_fini);
        will_return(__wrap_lock_fini, 0);
        ret = pqos_fini();
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

/* ======== pqos_get_num_mem_regions ======== */

static void
test_pqos_get_num_mem_regions_before_init(void **state __attribute__((unused)))
{
        unsigned num_mem_regions = 0;
        int ret;

        ret = pqos_get_num_mem_regions(NULL);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);

        expect_function_call(__wrap_lock_get);
        expect_function_call(__wrap_lock_release);
        ret = pqos_get_num_mem_regions(&num_mem_regions);
        assert_int_equal(ret, PQOS_RETVAL_INIT);
}

/* ======== pqos_cap_get ======== */

static void
test_pqos_cap_get_before_init(void **state __attribute__((unused)))
{
        int ret;
        const struct pqos_cap *p_cap;
        const struct pqos_cpuinfo *p_cpu;

        ret = pqos_cap_get(NULL, NULL);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);

        expect_function_call(__wrap_lock_get);
        expect_function_call(__wrap_lock_release);
        ret = pqos_cap_get(&p_cap, &p_cpu);
        assert_int_equal(ret, PQOS_RETVAL_INIT);

        expect_function_call(__wrap_lock_get);
        expect_function_call(__wrap_lock_release);
        ret = pqos_cap_get(&p_cap, NULL);
        assert_int_equal(ret, PQOS_RETVAL_INIT);

        expect_function_call(__wrap_lock_get);
        expect_function_call(__wrap_lock_release);
        ret = pqos_cap_get(NULL, &p_cpu);
        assert_int_equal(ret, PQOS_RETVAL_INIT);
}

static void
test_pqos_cap_get_after_init(void **state __attribute__((unused)))
{
        int ret;
        const struct pqos_cap *p_cap;
        const struct pqos_cpuinfo *p_cpu;

        ret = pqos_cap_get(NULL, NULL);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
        expect_function_call(__wrap_lock_get);
        expect_function_call(__wrap_lock_release);
        ret = pqos_cap_get(&p_cap, &p_cpu);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        expect_function_call(__wrap_lock_get);
        expect_function_call(__wrap_lock_release);
        ret = pqos_cap_get(&p_cap, NULL);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        expect_function_call(__wrap_lock_get);
        expect_function_call(__wrap_lock_release);
        ret = pqos_cap_get(NULL, &p_cpu);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

/* ======== _pqos_cap_l3cdp_change ======== */

static void
test__pqos_cap_l3cdp_change_msr(void **state __attribute__((unused)))
{
        struct test_data *data;

        data = (struct test_data *)*state;

        if (data->interface == PQOS_INTER_MSR) {
                will_return(__wrap_hw_cap_l3ca_discover, &(data->cap_l3ca));
                _pqos_cap_l3cdp_change(PQOS_REQUIRE_CDP_OFF);
                will_return(__wrap_hw_cap_l3ca_discover, &(data->cap_l3ca));
                _pqos_cap_l3cdp_change(PQOS_REQUIRE_CDP_ON);
                will_return(__wrap_hw_cap_l3ca_discover, &(data->cap_l3ca));
                _pqos_cap_l3cdp_change(PQOS_REQUIRE_CDP_ANY);
                /* the three changes above are valid and run in either
                 * build; what a debug build cannot do is the call below,
                 * which the library asserts against. The guard sits here
                 * rather than at the top of the case so that the valid
                 * coverage is not lost, and before the will_return so that
                 * no expectation is left armed.
                 */
                SKIP_IF_ASSERTS_ARE_LIVE();

                will_return(__wrap_hw_cap_l3ca_discover, &(data->cap_l3ca));
                _pqos_cap_l3cdp_change(-1);
        }
}

#ifdef __linux__
static void
test__pqos_cap_l3cdp_change_os(void **state __attribute__((unused)))
{
        struct test_data *data;

        data = (struct test_data *)*state;

        if (data->interface == PQOS_INTER_OS) {
                will_return(__wrap_os_cap_l3ca_discover, &(data->cap_l3ca));
                _pqos_cap_l3cdp_change(PQOS_REQUIRE_CDP_OFF);
                will_return(__wrap_os_cap_l3ca_discover, &(data->cap_l3ca));
                _pqos_cap_l3cdp_change(PQOS_REQUIRE_CDP_ON);
                will_return(__wrap_os_cap_l3ca_discover, &(data->cap_l3ca));
                _pqos_cap_l3cdp_change(PQOS_REQUIRE_CDP_ANY);
                /* the changes above are valid and run in either build; the
                 * call below is the one a debug build asserts against, so the
                 * guard sits here rather than at the top of the case, and
                 * before the will_return so no expectation is left armed
                 */
                SKIP_IF_ASSERTS_ARE_LIVE();

                will_return(__wrap_os_cap_l3ca_discover, &(data->cap_l3ca));
                _pqos_cap_l3cdp_change(-1);
        }
}

static void
test__pqos_cap_l3cdp_change_os_resctrl_mon(void **state __attribute__((unused)))
{
        struct test_data *data;

        data = (struct test_data *)*state;

        if (data->interface == PQOS_INTER_OS_RESCTRL_MON) {
                will_return(__wrap_os_cap_l3ca_discover, &(data->cap_l3ca));
                _pqos_cap_l3cdp_change(PQOS_REQUIRE_CDP_OFF);
                will_return(__wrap_os_cap_l3ca_discover, &(data->cap_l3ca));
                _pqos_cap_l3cdp_change(PQOS_REQUIRE_CDP_ON);
                will_return(__wrap_os_cap_l3ca_discover, &(data->cap_l3ca));
                _pqos_cap_l3cdp_change(PQOS_REQUIRE_CDP_ANY);
                /* the changes above are valid and run in either build; the
                 * call below is the one a debug build asserts against, so the
                 * guard sits here rather than at the top of the case, and
                 * before the will_return so no expectation is left armed
                 */
                SKIP_IF_ASSERTS_ARE_LIVE();

                will_return(__wrap_os_cap_l3ca_discover, &(data->cap_l3ca));
                _pqos_cap_l3cdp_change(-1);
        }
}

#endif
/* ======== _pqos_cap_l2cdp_change ======== */

static void
test__pqos_cap_l2cdp_change_msr(void **state __attribute__((unused)))
{
        struct test_data *data;

        data = (struct test_data *)*state;

        if (data->interface == PQOS_INTER_MSR) {
                will_return(__wrap_hw_cap_l2ca_discover, &(data->cap_l2ca));
                _pqos_cap_l2cdp_change(PQOS_REQUIRE_CDP_OFF);
                will_return(__wrap_hw_cap_l2ca_discover, &(data->cap_l2ca));
                _pqos_cap_l2cdp_change(PQOS_REQUIRE_CDP_ON);
                will_return(__wrap_hw_cap_l2ca_discover, &(data->cap_l2ca));
                _pqos_cap_l2cdp_change(PQOS_REQUIRE_CDP_ANY);
                /* the changes above are valid and run in either build; the
                 * call below is the one a debug build asserts against, so the
                 * guard sits here rather than at the top of the case, and
                 * before the will_return so no expectation is left armed
                 */
                SKIP_IF_ASSERTS_ARE_LIVE();

                will_return(__wrap_hw_cap_l2ca_discover, &(data->cap_l2ca));
                _pqos_cap_l2cdp_change(-1);
        }
}

#ifdef __linux__
static void
test__pqos_cap_l2cdp_change_os(void **state __attribute__((unused)))
{
        struct test_data *data;

        data = (struct test_data *)*state;

        if (data->interface == PQOS_INTER_OS) {
                will_return(__wrap_os_cap_l2ca_discover, &(data->cap_l2ca));
                _pqos_cap_l2cdp_change(PQOS_REQUIRE_CDP_OFF);
                will_return(__wrap_os_cap_l2ca_discover, &(data->cap_l2ca));
                _pqos_cap_l2cdp_change(PQOS_REQUIRE_CDP_ON);
                will_return(__wrap_os_cap_l2ca_discover, &(data->cap_l2ca));
                _pqos_cap_l2cdp_change(PQOS_REQUIRE_CDP_ANY);
                /* the changes above are valid and run in either build; the
                 * call below is the one a debug build asserts against, so the
                 * guard sits here rather than at the top of the case, and
                 * before the will_return so no expectation is left armed
                 */
                SKIP_IF_ASSERTS_ARE_LIVE();

                will_return(__wrap_os_cap_l2ca_discover, &(data->cap_l2ca));
                _pqos_cap_l2cdp_change(-1);
        }
}

static void
test__pqos_cap_l2cdp_change_os_resctrl_mon(void **state __attribute__((unused)))
{
        struct test_data *data;

        data = (struct test_data *)*state;

        if (data->interface == PQOS_INTER_OS_RESCTRL_MON) {
                will_return(__wrap_os_cap_l2ca_discover, &(data->cap_l2ca));
                _pqos_cap_l2cdp_change(PQOS_REQUIRE_CDP_OFF);
                will_return(__wrap_os_cap_l2ca_discover, &(data->cap_l2ca));
                _pqos_cap_l2cdp_change(PQOS_REQUIRE_CDP_ON);
                will_return(__wrap_os_cap_l2ca_discover, &(data->cap_l2ca));
                _pqos_cap_l2cdp_change(PQOS_REQUIRE_CDP_ANY);
                /* the changes above are valid and run in either build; the
                 * call below is the one a debug build asserts against, so the
                 * guard sits here rather than at the top of the case, and
                 * before the will_return so no expectation is left armed
                 */
                SKIP_IF_ASSERTS_ARE_LIVE();

                will_return(__wrap_os_cap_l2ca_discover, &(data->cap_l2ca));
                _pqos_cap_l2cdp_change(-1);
        }
}
#endif

/* ======== _pqos_cap_mba_change ======== */

static void
test__pqos_cap_mba_change(void **state __attribute__((unused)))
{
        SKIP_IF_ASSERTS_ARE_LIVE();

        _pqos_cap_mba_change(-1);
        _pqos_cap_mba_change(PQOS_MBA_ANY);
        _pqos_cap_mba_change(PQOS_MBA_DEFAULT);
        _pqos_cap_mba_change(PQOS_MBA_CTRL);
}

/* ======== _pqos_cap_mba40_change ======== */

/* a reset wrote every MBA domain as asked: the state reads the same in all */
static void
test__pqos_cap_mba40_change(void **state __attribute__((unused)))
{
        const struct pqos_capability *item = NULL;
        struct pqos_cap_mba *mba;

        assert_int_equal(
            pqos_cap_get_type(__real__pqos_get_cap(), PQOS_CAP_TYPE_MBA, &item),
            PQOS_RETVAL_OK);
        mba = item->u.mba;

        mba->mba40_on = -1;
        _pqos_cap_mba40_change(PQOS_FEATURE_ON);
        assert_int_equal(mba->mba40_on, 1);

        mba->mba40_on = -1;
        _pqos_cap_mba40_change(PQOS_FEATURE_OFF);
        assert_int_equal(mba->mba40_on, 0);

        mba->mba40_on = -1;
        _pqos_cap_mba40_change(PQOS_FEATURE_ANY);
        assert_int_equal(mba->mba40_on, -1);

        mba->mba40_on = 0;
}

/* ======== _pqos_cap_mba40_refresh ======== */

int
__wrap_hw_cap_mba40_on(const struct pqos_cpuinfo *cpu __attribute__((unused)),
                       int *enabled)
{
        int ret = mock_type(int);

        if (ret == PQOS_RETVAL_OK)
                *enabled = mock_type(int);
        return ret;
}

/* a reset that failed half way: the state is read back from the domains */
static void
test__pqos_cap_mba40_refresh(void **state __attribute__((unused)))
{
        const struct pqos_capability *item = NULL;
        struct pqos_cap_mba *mba;

        assert_int_equal(
            pqos_cap_get_type(__real__pqos_get_cap(), PQOS_CAP_TYPE_MBA, &item),
            PQOS_RETVAL_OK);
        mba = item->u.mba;
        mba->mba40 = 1;

        mba->mba40_on = 1;
        will_return(__wrap_hw_cap_mba40_on, PQOS_RETVAL_OK);
        will_return(__wrap_hw_cap_mba40_on, -1);
        _pqos_cap_mba40_refresh();
        assert_int_equal(mba->mba40_on, -1);

        /* where it cannot be read, it is left and a warning says so */
        will_return(__wrap_hw_cap_mba40_on, PQOS_RETVAL_ERROR);
        test_log_count_start();
        _pqos_cap_mba40_refresh();
        assert_int_equal(test_log_count_stop(), 1);
        assert_int_equal(mba->mba40_on, -1);

        /* without MBA 4.0 there is nothing to read */
        mba->mba40 = 0;
        _pqos_cap_mba40_refresh();
        assert_int_equal(mba->mba40_on, -1);

        mba->mba40_on = 0;
}

int
main(void)
{
        int result = 0;

        const struct CMUnitTest tests_cap_param[] = {
            cmocka_unit_test(test_pqos_init_param),
        };

        const struct CMUnitTest tests_cap_init[] = {
            cmocka_unit_test(test__pqos_check_init_before_init),
            cmocka_unit_test(test__pqos_get_cap_before_init),
            cmocka_unit_test(test__pqos_get_cpu_before_init),
            cmocka_unit_test(test_pqos_cap_get_before_init),
            cmocka_unit_test(test_pqos_get_num_mem_regions_before_init),
            cmocka_unit_test(test_pqos_init),
            cmocka_unit_test(test_pqos_cap_get_after_init),
            cmocka_unit_test(test__pqos_get_cap_after_init),
            cmocka_unit_test(test__pqos_get_cpu_after_init),
            cmocka_unit_test(test__pqos_check_init_after_init),
            cmocka_unit_test(test__pqos_cap_l3cdp_change_msr),
            cmocka_unit_test(test__pqos_cap_l2cdp_change_msr),
#ifdef __linux__
            cmocka_unit_test(test__pqos_cap_l3cdp_change_os),
            cmocka_unit_test(test__pqos_cap_l2cdp_change_os),
            cmocka_unit_test(test__pqos_cap_l3cdp_change_os_resctrl_mon),
            cmocka_unit_test(test__pqos_cap_l2cdp_change_os_resctrl_mon),
#endif
            cmocka_unit_test(test__pqos_cap_mba_change),
            cmocka_unit_test(test__pqos_cap_mba40_change),
            cmocka_unit_test(test__pqos_cap_mba40_refresh),
            cmocka_unit_test(test_pqos_fini),
        };

        const struct CMUnitTest tests_cap_init_mmio[] = {
            cmocka_unit_test(test_pqos_init_mmio_map_failure_frees_the_erdt),
            cmocka_unit_test(
                test_pqos_init_mmio_refuses_classes_that_do_not_fit),
            cmocka_unit_test(test_pqos_init_mmio_keeps_the_cpuid_counts),
        };

        result += cmocka_run_group_tests(tests_cap_param, NULL, NULL);
        result += cmocka_run_group_tests(tests_cap_init, setup_cap_init_msr,
                                         test_fini);
#ifdef __linux__
        result += cmocka_run_group_tests(tests_cap_init, setup_cap_init_os,
                                         test_fini);
        result += cmocka_run_group_tests(
            tests_cap_init, setup_cap_init_os_resctrl_mon, test_fini);
#endif
        result += cmocka_run_group_tests(tests_cap_init_mmio,
                                         setup_cap_init_msr, test_fini);
        return result;
}
