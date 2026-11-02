/*
 * BSD LICENSE
 *
 * Copyright(c) 2020-2026 Intel Corporation. All rights reserved.
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

#include "hw_cap.h"
#include "machine.h"
#include "test.h"

#define MAX_CPUID_LEAFS 20

#define NON_CONTIGUOUS_CBM_UNSUPPORTED 0
#define NON_CONTIGUOUS_CBM_SUPPORTED   1
#define NON_CONTIGUOUS_CBM_BIT         3

struct test_lcpuid {
        unsigned leaf;
        unsigned subleaf;
        struct cpuid_out out;
};

static unsigned _cpuid_count;
static struct test_lcpuid _cpuid[MAX_CPUID_LEAFS];

static void
_lcpuid_add(const unsigned leaf,
            const unsigned subleaf,
            uint32_t eax,
            uint32_t ebx,
            uint32_t ecx,
            uint32_t edx)
{
        assert_true(_cpuid_count < MAX_CPUID_LEAFS);

        struct test_lcpuid *cpuid = &(_cpuid[_cpuid_count]);

        cpuid->leaf = leaf;
        cpuid->subleaf = subleaf;
        cpuid->out.eax = eax;
        cpuid->out.ebx = ebx;
        cpuid->out.ecx = ecx;
        cpuid->out.edx = edx;

        _cpuid_count++;
}

static int
_init(void **state __attribute__((unused)))
{
        _cpuid_count = 0;

        /* cpuinfo_get_cpu_model()/family(), read by L3 CAT detection */
        _lcpuid_add(0x1, 0x0, 0x0, 0x0, 0x0, 0x0);

        return 0;
}

/* ======== mock ======== */

/* set to make the next MBA domain list fail, as a failed allocation does */
static int _mba_ids_fail;

unsigned *__real_pqos_cpu_get_mba_ids(const struct pqos_cpuinfo *cpu,
                                      unsigned *count);

unsigned *
__wrap_pqos_cpu_get_mba_ids(const struct pqos_cpuinfo *cpu, unsigned *count)
{
        if (_mba_ids_fail) {
                _mba_ids_fail = 0;
                return NULL;
        }

        return __real_pqos_cpu_get_mba_ids(cpu, count);
}

void
__wrap_lcpuid(const unsigned leaf,
              const unsigned subleaf,
              struct cpuid_out *out)
{
        unsigned i;

        for (i = 0; i < _cpuid_count; ++i) {
                const struct test_lcpuid *cpuid = &(_cpuid[i]);

                if (cpuid->leaf == leaf && cpuid->subleaf == subleaf) {
                        *out = cpuid->out;
                        return;
                }
        }

        fail_msg("Unexpected CPUID leaf=0x%x subleaf=0x%x", leaf, subleaf);
}

int
hw_cap_l3ca_cdp(const struct pqos_cpuinfo *cpu, int *enabled)
{
        int ret;

        assert_non_null(cpu);

        ret = mock_type(int);
        if (ret == PQOS_RETVAL_OK)
                *enabled = mock_type(int);

        return ret;
}

int
hw_cap_l3ca_iordt(const struct pqos_cpuinfo *cpu, int *enabled)
{
        int ret;

        assert_non_null(cpu);

        ret = mock_type(int);
        if (ret == PQOS_RETVAL_OK)
                *enabled = mock_type(int);

        return ret;
}

int
hw_cap_l2ca_cdp(const struct pqos_cpuinfo *cpu, int *enabled)
{
        int ret;

        assert_non_null(cpu);

        ret = mock_type(int);
        if (ret == PQOS_RETVAL_OK)
                *enabled = mock_type(int);

        return ret;
}

int
hw_cap_mon_snc_state(const struct pqos_cpuinfo *cpu,
                     unsigned *snc_num,
                     enum pqos_snc_mode *snc_mode)
{
        assert_non_null(cpu);
        *snc_num = 1;
        *snc_mode = PQOS_SNC_LOCAL;

        return PQOS_RETVAL_OK;
}

/* ======== hw_cap_mon_discover ======== */

static void
test_hw_cap_mon_discover_unsupported(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        struct pqos_cap_mon *cap_mon = NULL;
        enum pqos_interface iface = PQOS_INTER_MSR;
        int ret;

        _lcpuid_add(0x7, 0x0, 0x0, 0x0, 0x0, 0x0);

        ret = hw_cap_mon_discover(&cap_mon, data->cpu, iface);
        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);
        assert_null(cap_mon);
}

static void
test_hw_cap_mon_discover_unsupported2(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        struct pqos_cap_mon *cap_mon = NULL;
        enum pqos_interface iface = PQOS_INTER_MSR;
        int ret;

        _lcpuid_add(0x7, 0x0, 0x0, 0x1000, 0x0, 0x0);
        _lcpuid_add(0xf, 0x0, 0x0, 0x0, 0x0, 0x0);

        ret = hw_cap_mon_discover(&cap_mon, data->cpu, iface);
        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);
        assert_null(cap_mon);
}

static void
test_hw_cap_mon_discover_tmem(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        enum pqos_interface iface = PQOS_INTER_MSR;
        struct pqos_cap_mon *cap_mon = NULL;
        int ret;
        uint32_t max_rmid = 10;
        uint32_t scale_factor = 128;
        uint32_t counter_length = 24;

        _lcpuid_add(0x7, 0x0, 0x0, 0x1000, 0x0, 0x0);
        _lcpuid_add(0xf, 0x0, 0x0, max_rmid - 1, 0x0, 0x2);
        _lcpuid_add(0xf, 0x1, counter_length - 24, scale_factor, max_rmid - 1,
                    0x2);
        _lcpuid_add(0xa, 0x0, 0x0, 0x0, 0x0, 0x0);

        ret = hw_cap_mon_discover(&cap_mon, data->cpu, iface);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_non_null(cap_mon);
        assert_int_equal(cap_mon->num_events, 1);
        assert_int_equal(cap_mon->max_rmid, max_rmid);
        assert_int_equal(cap_mon->events[0].type, PQOS_MON_EVENT_TMEM_BW);
        assert_int_equal(cap_mon->events[0].max_rmid, max_rmid);
        assert_int_equal(cap_mon->events[0].scale_factor, scale_factor);
        assert_int_equal(cap_mon->events[0].counter_length, counter_length);

        free(cap_mon);
}

static void
test_hw_cap_mon_discover_lmem(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        enum pqos_interface iface = PQOS_INTER_MSR;
        struct pqos_cap_mon *cap_mon = NULL;
        int ret;
        uint32_t max_rmid = 10;
        uint32_t scale_factor = 128;
        uint32_t counter_length = 24;

        _lcpuid_add(0x7, 0x0, 0x0, 0x1000, 0x0, 0x0);
        _lcpuid_add(0xf, 0x0, 0x0, max_rmid - 1, 0x0, 0x2);
        _lcpuid_add(0xf, 0x1, counter_length - 24, scale_factor, max_rmid - 1,
                    0x4);
        _lcpuid_add(0xa, 0x0, 0x0, 0x0, 0x0, 0x0);

        ret = hw_cap_mon_discover(&cap_mon, data->cpu, iface);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_non_null(cap_mon);
        assert_int_equal(cap_mon->num_events, 1);
        assert_int_equal(cap_mon->max_rmid, max_rmid);
        assert_int_equal(cap_mon->events[0].type, PQOS_MON_EVENT_LMEM_BW);
        assert_int_equal(cap_mon->events[0].max_rmid, max_rmid);
        assert_int_equal(cap_mon->events[0].scale_factor, scale_factor);
        assert_int_equal(cap_mon->events[0].counter_length, counter_length);

        free(cap_mon);
}

static void
test_hw_cap_mon_discover_llc(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        enum pqos_interface iface = PQOS_INTER_MSR;
        struct pqos_cap_mon *cap_mon = NULL;
        int ret;
        uint32_t max_rmid = 10;
        uint32_t scale_factor = 128;
        uint32_t counter_length = 24;

        _lcpuid_add(0x7, 0x0, 0x0, 0x1000, 0x0, 0x0);
        _lcpuid_add(0xf, 0x0, 0x0, max_rmid - 1, 0x0, 0x2);
        _lcpuid_add(0xf, 0x1, counter_length - 24, scale_factor, max_rmid - 1,
                    0x1);
        _lcpuid_add(0xa, 0x0, 0x0, 0x0, 0x0, 0x0);

        ret = hw_cap_mon_discover(&cap_mon, data->cpu, iface);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_non_null(cap_mon);
        assert_int_equal(cap_mon->num_events, 1);
        assert_int_equal(cap_mon->max_rmid, max_rmid);
        assert_int_equal(cap_mon->events[0].type, PQOS_MON_EVENT_L3_OCCUP);
        assert_int_equal(cap_mon->events[0].max_rmid, max_rmid);
        assert_int_equal(cap_mon->events[0].scale_factor, scale_factor);
        assert_int_equal(cap_mon->events[0].counter_length, counter_length);

        free(cap_mon);
}

static void
test_hw_cap_mon_discover_rmem(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        enum pqos_interface iface = PQOS_INTER_MSR;
        struct pqos_cap_mon *cap_mon = NULL;
        int ret;
        unsigned i;
        uint32_t max_rmid = 10;
        uint32_t scale_factor = 128;
        uint32_t counter_length = 24;
        int found_lmem = 0;
        int found_tmem = 0;
        int found_rmem = 0;

        _lcpuid_add(0x7, 0x0, 0x0, 0x1000, 0x0, 0x0);
        _lcpuid_add(0xf, 0x0, 0x0, max_rmid - 1, 0x0, 0x2);
        _lcpuid_add(0xf, 0x1, counter_length - 24, scale_factor, max_rmid - 1,
                    0x6);
        _lcpuid_add(0xa, 0x0, 0x0, 0x0, 0x0, 0x0);

        ret = hw_cap_mon_discover(&cap_mon, data->cpu, iface);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_non_null(cap_mon);
        assert_int_equal(cap_mon->num_events, 3);
        assert_int_equal(cap_mon->max_rmid, max_rmid);

        for (i = 0; i < cap_mon->num_events; i++) {
                assert_int_equal(cap_mon->events[i].max_rmid, max_rmid);
                assert_int_equal(cap_mon->events[i].scale_factor, scale_factor);
                assert_int_equal(cap_mon->events[i].counter_length,
                                 counter_length);

                if (cap_mon->events[i].type == PQOS_MON_EVENT_LMEM_BW)
                        found_lmem = 1;
                else if (cap_mon->events[i].type == PQOS_MON_EVENT_TMEM_BW)
                        found_tmem = 1;
                else if (cap_mon->events[i].type == PQOS_MON_EVENT_RMEM_BW)
                        found_rmem = 1;
        }
        assert_true(found_lmem);
        assert_true(found_tmem);
        assert_true(found_rmem);

        free(cap_mon);
}

static void
test_hw_cap_mon_discover_ipc(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        enum pqos_interface iface = PQOS_INTER_MSR;
        struct pqos_cap_mon *cap_mon = NULL;
        int ret;
        uint32_t max_rmid = 10;
        uint32_t scale_factor = 128;
        uint32_t counter_length = 24;

        _lcpuid_add(0x7, 0x0, 0x0, 0x1000, 0x0, 0x0);
        _lcpuid_add(0xf, 0x0, 0x0, max_rmid - 1, 0x0, 0x2);
        _lcpuid_add(0xf, 0x1, counter_length - 24, scale_factor, max_rmid - 1,
                    0x4);
        _lcpuid_add(0xa, 0x0, 0x0, 0x0, 0x0, 0x603);

        ret = hw_cap_mon_discover(&cap_mon, data->cpu, iface);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_non_null(cap_mon);
        assert_int_equal(cap_mon->num_events, 2);
        assert_int_equal(cap_mon->events[1].type, PQOS_PERF_EVENT_IPC);
        assert_int_equal(cap_mon->events[1].max_rmid, 0);
        assert_int_equal(cap_mon->events[1].scale_factor, 0);
        assert_int_equal(cap_mon->events[1].counter_length, 0);

        free(cap_mon);
}

static void
test_hw_cap_mon_discover_llc_miss(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        enum pqos_interface iface = PQOS_INTER_MSR;
        struct pqos_cap_mon *cap_mon = NULL;
        int ret;
        uint32_t max_rmid = 10;
        uint32_t scale_factor = 128;
        uint32_t counter_length = 24;

        _lcpuid_add(0x7, 0x0, 0x0, 0x1000, 0x0, 0x0);
        _lcpuid_add(0xf, 0x0, 0x0, max_rmid - 1, 0x0, 0x2);
        _lcpuid_add(0xf, 0x1, counter_length - 24, scale_factor, max_rmid - 1,
                    0x4);
        _lcpuid_add(0xa, 0x0, 0x7300803, 0x0, 0x0, 0x0);

        ret = hw_cap_mon_discover(&cap_mon, data->cpu, iface);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_non_null(cap_mon);
        assert_int_equal(cap_mon->num_events, 3);
        assert_int_equal(cap_mon->events[1].type, PQOS_PERF_EVENT_LLC_MISS);
        assert_int_equal(cap_mon->events[1].max_rmid, 0);
        assert_int_equal(cap_mon->events[1].scale_factor, 0);
        assert_int_equal(cap_mon->events[1].counter_length, 0);

        free(cap_mon);
}

static void
test_hw_cap_mon_discover_llc_ref(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        enum pqos_interface iface = PQOS_INTER_MSR;
        struct pqos_cap_mon *cap_mon = NULL;
        int ret;
        uint32_t max_rmid = 10;
        uint32_t scale_factor = 128;
        uint32_t counter_length = 24;

        _lcpuid_add(0x7, 0x0, 0x0, 0x1000, 0x0, 0x0);
        _lcpuid_add(0xf, 0x0, 0x0, max_rmid - 1, 0x0, 0x2);
        _lcpuid_add(0xf, 0x1, counter_length - 24, scale_factor, max_rmid - 1,
                    0x4);
        _lcpuid_add(0xa, 0x0, 0x7300803, 0x0, 0x0, 0x0);

        ret = hw_cap_mon_discover(&cap_mon, data->cpu, iface);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_non_null(cap_mon);
        assert_int_equal(cap_mon->num_events, 3);
        assert_int_equal(cap_mon->events[2].type, PQOS_PERF_EVENT_LLC_REF);
        assert_int_equal(cap_mon->events[2].max_rmid, 0);
        assert_int_equal(cap_mon->events[2].scale_factor, 0);
        assert_int_equal(cap_mon->events[2].counter_length, 0);

        free(cap_mon);
}

/* ======== test_hw_cap_l3ca_discover ======== */

static void
test_hw_cap_l3ca_discover_unsupported(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        struct pqos_cap_l3ca cap_l3ca;
        int ret;

        _lcpuid_add(0x07, 0x0, 0x0, 0x8000, 0x0, 0x0);
        _lcpuid_add(0x10, 0x0, 0x0, 0x0, 0x0, 0x0);

        /* brand string */
        _lcpuid_add(0x80000000, 0x0, 0x80000008, 0x0, 0x0, 0x0);
        _lcpuid_add(0x80000002, 0x0, 0x0, 0x0, 0x0, 0x0);
        _lcpuid_add(0x80000003, 0x0, 0x0, 0x0, 0x0, 0x0);
        _lcpuid_add(0x80000004, 0x0, 0x0, 0x0, 0x0, 0x0);

        ret = hw_cap_l3ca_discover(&cap_l3ca, data->cpu);
        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);
}

static void
test_hw_cap_l3ca_discover(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        struct pqos_cap_l3ca cap_l3ca;
        int ret;
        uint32_t num_classes = 16;
        uint32_t num_ways = 11;
        uint32_t way_contention = 0x600;

        _lcpuid_add(0x07, 0x0, 0x0, 0x8000, 0x0, 0x0);
        _lcpuid_add(0x10, 0x0, 0x0, 0x2, 0x0, 0x0);
        _lcpuid_add(0x10, 0x1, num_ways - 1, way_contention, 0x0,
                    num_classes - 1);

        ret = hw_cap_l3ca_discover(&cap_l3ca, data->cpu);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(cap_l3ca.num_classes, num_classes);
        assert_int_equal(cap_l3ca.num_ways, num_ways);
        assert_int_equal(cap_l3ca.cdp, 0);
        assert_int_equal(cap_l3ca.cdp_on, 0);
        assert_int_equal(cap_l3ca.way_contention, way_contention);
        assert_int_equal(cap_l3ca.non_contiguous_cbm,
                         NON_CONTIGUOUS_CBM_UNSUPPORTED);
}

static void
test_hw_cap_l3ca_discover_cdp(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        struct pqos_cap_l3ca cap_l3ca;
        int ret;
        uint32_t num_classes = 16;
        uint32_t num_ways = 11;
        uint32_t way_contention = 0x600;

        _lcpuid_add(0x07, 0x0, 0x0, 0x8000, 0x0, 0x0);
        _lcpuid_add(0x10, 0x0, 0x0, 0x2, 0x0, 0x0);
        _lcpuid_add(0x10, 0x1, num_ways - 1, way_contention, 0x4,
                    num_classes - 1);

        /* cdp enabled */
        will_return(hw_cap_l3ca_cdp, PQOS_RETVAL_OK);
        will_return(hw_cap_l3ca_cdp, 1);

        ret = hw_cap_l3ca_discover(&cap_l3ca, data->cpu);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(cap_l3ca.num_classes, num_classes / 2);
        assert_int_equal(cap_l3ca.num_ways, num_ways);
        assert_int_equal(cap_l3ca.cdp, 1);
        assert_int_equal(cap_l3ca.cdp_on, 1);
        assert_int_equal(cap_l3ca.way_contention, way_contention);

        /* cdp disabled */
        will_return(hw_cap_l3ca_cdp, PQOS_RETVAL_OK);
        will_return(hw_cap_l3ca_cdp, 0);

        ret = hw_cap_l3ca_discover(&cap_l3ca, data->cpu);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(cap_l3ca.num_classes, num_classes);
        assert_int_equal(cap_l3ca.num_ways, num_ways);
        assert_int_equal(cap_l3ca.cdp, 1);
        assert_int_equal(cap_l3ca.cdp_on, 0);
        assert_int_equal(cap_l3ca.way_contention, way_contention);

        /* cdp conflict */
        will_return(hw_cap_l3ca_cdp, PQOS_RETVAL_ERROR);

        ret = hw_cap_l3ca_discover(&cap_l3ca, data->cpu);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
}

static void
test_hw_cap_l3ca_discover_non_contiguous_cbm(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        struct pqos_cap_l3ca cap_l3ca;
        int ret;
        uint32_t num_classes = 16;
        uint32_t num_ways = 11;
        uint32_t way_contention = 0x600;
        uint32_t non_contiguous_cbm = NON_CONTIGUOUS_CBM_SUPPORTED
                                      << NON_CONTIGUOUS_CBM_BIT;

        _lcpuid_add(0x07, 0x0, 0x0, 0x8000, 0x0, 0x0);
        _lcpuid_add(0x10, 0x0, 0x0, 0x2, 0x0, 0x0);
        _lcpuid_add(0x10, 0x1, num_ways - 1, way_contention, non_contiguous_cbm,
                    num_classes - 1);

        ret = hw_cap_l3ca_discover(&cap_l3ca, data->cpu);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(cap_l3ca.non_contiguous_cbm,
                         NON_CONTIGUOUS_CBM_SUPPORTED);
}

static void
test_hw_cap_l3ca_discover_iordt(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        struct pqos_cap_l3ca cap_l3ca;
        int ret;
        uint32_t num_classes = 16;
        uint32_t num_ways = 11;
        uint32_t way_contention = 0x600;

        _lcpuid_add(0x07, 0x0, 0x0, 0x8000, 0x0, 0x0);
        _lcpuid_add(0x10, 0x0, 0x0, 0x2, 0x0, 0x0);
        _lcpuid_add(0x10, 0x1, num_ways - 1, way_contention, 0x2,
                    num_classes - 1);

        /* I/O RDT enabled */
        will_return(hw_cap_l3ca_iordt, PQOS_RETVAL_OK);
        will_return(hw_cap_l3ca_iordt, 1);

        ret = hw_cap_l3ca_discover(&cap_l3ca, data->cpu);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(cap_l3ca.num_classes, num_classes);
        assert_int_equal(cap_l3ca.num_ways, num_ways);
        assert_int_equal(cap_l3ca.cdp, 0);
        assert_int_equal(cap_l3ca.cdp_on, 0);
        assert_int_equal(cap_l3ca.iordt, 1);
        assert_int_equal(cap_l3ca.iordt_on, 1);
        assert_int_equal(cap_l3ca.way_contention, way_contention);

        /* I/O RDT disabled */
        will_return(hw_cap_l3ca_iordt, PQOS_RETVAL_OK);
        will_return(hw_cap_l3ca_iordt, 0);

        ret = hw_cap_l3ca_discover(&cap_l3ca, data->cpu);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(cap_l3ca.num_classes, num_classes);
        assert_int_equal(cap_l3ca.num_ways, num_ways);
        assert_int_equal(cap_l3ca.cdp, 0);
        assert_int_equal(cap_l3ca.cdp_on, 0);
        assert_int_equal(cap_l3ca.iordt, 1);
        assert_int_equal(cap_l3ca.iordt_on, 0);
        assert_int_equal(cap_l3ca.way_contention, way_contention);

        /* I/O RDT conflict */
        will_return(hw_cap_l3ca_iordt, PQOS_RETVAL_ERROR);

        ret = hw_cap_l3ca_discover(&cap_l3ca, data->cpu);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
}

/* ======== test_hw_cap_l2ca_discover ======== */

static void
test_hw_cap_l2ca_discover_alloc_unsupported(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        struct pqos_cap_l2ca cap_l2ca;
        int ret;

        _lcpuid_add(0x07, 0x0, 0x0, 0x0, 0x0, 0x0);

        ret = hw_cap_l2ca_discover(&cap_l2ca, data->cpu);
        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);
}

static void
test_hw_cap_l2ca_discover_unsupported(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        struct pqos_cap_l2ca cap_l2ca;
        int ret;

        _lcpuid_add(0x07, 0x0, 0x0, 0x8000, 0x0, 0x0);
        _lcpuid_add(0x10, 0x0, 0x0, 0x0, 0x0, 0x0);

        ret = hw_cap_l2ca_discover(&cap_l2ca, data->cpu);
        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);
}

static void
test_hw_cap_l2ca_discover(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        struct pqos_cap_l2ca cap_l2ca;
        int ret;
        uint32_t num_classes = 16;
        uint32_t num_ways = 11;
        uint32_t way_contention = 0x600;

        _lcpuid_add(0x07, 0x0, 0x0, 0x8000, 0x0, 0x0);
        _lcpuid_add(0x10, 0x0, 0x0, 0x4, 0x0, 0x0);
        _lcpuid_add(0x10, 0x2, num_ways - 1, way_contention, 0x0,
                    num_classes - 1);

        ret = hw_cap_l2ca_discover(&cap_l2ca, data->cpu);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(cap_l2ca.num_classes, num_classes);
        assert_int_equal(cap_l2ca.num_ways, num_ways);
        assert_int_equal(cap_l2ca.cdp, 0);
        assert_int_equal(cap_l2ca.cdp_on, 0);
        assert_int_equal(cap_l2ca.way_contention, way_contention);
        assert_int_equal(cap_l2ca.non_contiguous_cbm,
                         NON_CONTIGUOUS_CBM_UNSUPPORTED);
}

static void
test_hw_cap_l2ca_discover_cdp(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        struct pqos_cap_l2ca cap_l2ca;
        int ret;
        uint32_t num_classes = 16;
        uint32_t num_ways = 11;
        uint32_t way_contention = 0x600;

        _lcpuid_add(0x07, 0x0, 0x0, 0x8000, 0x0, 0x0);
        _lcpuid_add(0x10, 0x0, 0x0, 0x4, 0x0, 0x0);
        _lcpuid_add(0x10, 0x2, num_ways - 1, way_contention, 0x4,
                    num_classes - 1);

        /* cdp enabled */
        will_return(hw_cap_l2ca_cdp, PQOS_RETVAL_OK);
        will_return(hw_cap_l2ca_cdp, 1);

        ret = hw_cap_l2ca_discover(&cap_l2ca, data->cpu);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(cap_l2ca.num_classes, num_classes / 2);
        assert_int_equal(cap_l2ca.num_ways, num_ways);
        assert_int_equal(cap_l2ca.cdp, 1);
        assert_int_equal(cap_l2ca.cdp_on, 1);
        assert_int_equal(cap_l2ca.way_contention, way_contention);

        /* cdp disabled */
        will_return(hw_cap_l2ca_cdp, PQOS_RETVAL_OK);
        will_return(hw_cap_l2ca_cdp, 0);

        ret = hw_cap_l2ca_discover(&cap_l2ca, data->cpu);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(cap_l2ca.num_classes, num_classes);
        assert_int_equal(cap_l2ca.num_ways, num_ways);
        assert_int_equal(cap_l2ca.cdp, 1);
        assert_int_equal(cap_l2ca.cdp_on, 0);
        assert_int_equal(cap_l2ca.way_contention, way_contention);

        /* cdp conflict */
        will_return(hw_cap_l2ca_cdp, PQOS_RETVAL_ERROR);

        ret = hw_cap_l2ca_discover(&cap_l2ca, data->cpu);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
}

static void
test_hw_cap_l2ca_discover_non_contiguous_cbm(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        struct pqos_cap_l2ca cap_l2ca;
        int ret;
        uint32_t num_classes = 16;
        uint32_t num_ways = 11;
        uint32_t way_contention = 0x600;
        uint32_t non_contiguous_cbm = NON_CONTIGUOUS_CBM_SUPPORTED
                                      << NON_CONTIGUOUS_CBM_BIT;

        _lcpuid_add(0x07, 0x0, 0x0, 0x8000, 0x0, 0x0);
        _lcpuid_add(0x10, 0x0, 0x0, 0x4, 0x0, 0x0);
        _lcpuid_add(0x10, 0x2, num_ways - 1, way_contention, non_contiguous_cbm,
                    num_classes - 1);

        ret = hw_cap_l2ca_discover(&cap_l2ca, data->cpu);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(cap_l2ca.non_contiguous_cbm,
                         NON_CONTIGUOUS_CBM_SUPPORTED);
}

/* ======== hw_cap_mba_discover ======== */

static void
test_hw_cap_mba_discover_alloc_unsupported(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        struct pqos_cap_mba cap_mba;
        int ret;

        _lcpuid_add(0x07, 0x0, 0x0, 0x0, 0x0, 0x0);

        ret = hw_cap_mba_discover(&cap_mba, data->cpu);
        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);
}

static void
test_hw_cap_mba_discover_unsupported(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        struct pqos_cap_mba cap_mba;
        int ret;

        _lcpuid_add(0x07, 0x0, 0x0, 0x8000, 0x0, 0x0);
        _lcpuid_add(0x10, 0x0, 0x0, 0x0, 0x0, 0x0);

        ret = hw_cap_mba_discover(&cap_mba, data->cpu);
        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);
}

static void
test_hw_cap_mba_discover_linear(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        struct pqos_cap_mba cap_mba;
        int ret;
        uint32_t num_classes = 8;
        uint32_t throttle_max = 90;
        uint32_t is_linear = 1;

        _lcpuid_add(0x07, 0x0, 0x0, 0x8000, 0x0, 0x0);
        _lcpuid_add(0x10, 0x0, 0x0, 0x8, 0x0, 0x2);
        _lcpuid_add(0x10, 0x3, throttle_max - 1, 0x0, is_linear << 2,
                    num_classes - 1);

        ret = hw_cap_mba_discover(&cap_mba, data->cpu);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(cap_mba.num_classes, num_classes);
        assert_int_equal(cap_mba.throttle_max, throttle_max);
        assert_int_equal(cap_mba.throttle_step, 100 - throttle_max);
        assert_int_equal(cap_mba.is_linear, is_linear);
}

static void
test_hw_cap_mba_discover_non_linear(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        struct pqos_cap_mba cap_mba;
        int ret;
        uint32_t is_linear = 0;

        _lcpuid_add(0x07, 0x0, 0x0, 0x8000, 0x0, 0x0);
        _lcpuid_add(0x10, 0x0, 0x0, 0x8, 0x0, 0x0);
        _lcpuid_add(0x10, 0x3, 0x0, 0x0, is_linear << 2, 0x0);

        ret = hw_cap_mba_discover(&cap_mba, data->cpu);
        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);
}

static void
_test_hw_cap_mba_discover_mba40_supported(const struct pqos_cpuinfo *cpu,
                                          unsigned enabled)
{
        struct pqos_cap_mba cap_mba;
        int ret;
        uint32_t num_classes = 8;
        uint32_t throttle_max = 90;
        uint32_t is_linear = 1;
        unsigned *mba_ids;
        unsigned mba_id_num;
        unsigned i;

        _lcpuid_add(0x07, 0x0, 0x0, 0x8000, 0x0, 1U << 30);
        _lcpuid_add(0x10, 0x0, 0x0, 0x8, 0x0, 0x0);
        _lcpuid_add(0x10, 0x3, throttle_max - 1, 0x0, is_linear << 2,
                    num_classes - 1);

        for (i = 0; i < cpu->num_cores; i++) {
                expect_value(__wrap_msr_read, lcore, cpu->cores[i].lcore);
                expect_value(__wrap_msr_read, reg, PQOS_MSR_CORE_CAPABILITIES);
                will_return(__wrap_msr_read, MACHINE_RETVAL_OK);
                will_return(__wrap_msr_read,
                            PQOS_MSR_CORE_CAPABILITIES_MBA40_EN);
        }

        mba_ids = pqos_cpu_get_mba_ids(cpu, &mba_id_num);
        assert_non_null(mba_ids);

        for (i = 0; i < mba_id_num; i++) {
                unsigned core = 0;

                ret = pqos_cpu_get_one_by_mba_id(cpu, mba_ids[i], &core);
                assert_int_equal(ret, PQOS_RETVAL_OK);

                expect_value(__wrap_msr_probe, lcore, core);
                expect_value(__wrap_msr_probe, reg, PQOS_MSR_MBA_CFG);
                will_return(__wrap_msr_probe, MACHINE_RETVAL_OK);
                will_return(__wrap_msr_probe, 0);
        }

        for (i = 0; i < mba_id_num; i++) {
                unsigned core = 0;

                ret = pqos_cpu_get_one_by_mba_id(cpu, mba_ids[i], &core);
                assert_int_equal(ret, PQOS_RETVAL_OK);

                expect_value(__wrap_msr_read, lcore, core);
                expect_value(__wrap_msr_read, reg, PQOS_MSR_MBA_CFG);
                will_return(__wrap_msr_read, MACHINE_RETVAL_OK);
                will_return(__wrap_msr_read,
                            enabled ? PQOS_MSR_MBA_CFG_MBA40_EN : 0);
        }
        free(mba_ids);

        ret = hw_cap_mba_discover(&cap_mba, cpu);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(cap_mba.mba40, 1);
        assert_int_equal(cap_mba.mba40_on, enabled);
}

static void
test_hw_cap_mba_discover_mba40_supported_enabled(void **state)
{
        struct test_data *data = (struct test_data *)*state;

        _test_hw_cap_mba_discover_mba40_supported(data->cpu, 1);
}

static void
test_hw_cap_mba_discover_mba40_supported_disabled(void **state)
{
        struct test_data *data = (struct test_data *)*state;

        _test_hw_cap_mba_discover_mba40_supported(data->cpu, 0);
}

/* one socket of two MBA domains, numbered 1 and 3: their ids are no sockets */
static void
test_hw_cap_mba_discover_mba40_domains_not_sockets(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        const unsigned num_cores = data->cpu->num_cores;
        struct pqos_cpuinfo *cpu;
        unsigned i;

        cpu = malloc(sizeof(*cpu) + num_cores * sizeof(cpu->cores[0]));
        assert_non_null(cpu);
        memcpy(cpu, data->cpu,
               sizeof(*cpu) + num_cores * sizeof(cpu->cores[0]));

        for (i = 0; i < num_cores; i++) {
                struct pqos_coreinfo *coreinfo = &cpu->cores[i];

                coreinfo->socket = 0;
                coreinfo->l3_id = i < num_cores / 2 ? 1 : 3;
                coreinfo->l3cat_id = coreinfo->l3_id;
                coreinfo->mba_id = coreinfo->l3_id;
        }

        _test_hw_cap_mba_discover_mba40_supported(cpu, 1);

        free(cpu);
}

/* what the library logs at the verbose level, kept for a case to read */
static char _log_text[2048];

static void
_log_keep(void *context, const size_t size, const char *message)
{
        (void)context;
        (void)size;
        strncat(_log_text, message, sizeof(_log_text) - strlen(_log_text) - 1);
}

static void
_log_keep_start(void)
{
        _log_text[0] = '\0';
        (void)log_init(-1, _log_keep, NULL, LOG_VER_VERBOSE);
}

/* the id of the i-th MBA domain of the fixture */
static unsigned
_mba_id(const struct pqos_cpuinfo *cpu, const unsigned i)
{
        unsigned mba_id_num = 0;
        unsigned *mba_ids = pqos_cpu_get_mba_ids(cpu, &mba_id_num);
        unsigned id;

        assert_non_null(mba_ids);
        assert_true(i < mba_id_num);
        id = mba_ids[i];
        free(mba_ids);
        return id;
}

/*
 * The cores of a part that reports MBA 4.0, and its MBA configuration register
 * read on the first core of each domain, which answers as \a probe says:
 * \a probe[i] is what the read in the i-th domain returns, and -1 ends the
 * reads expected
 */
static void
_mba40_probe_queue(const struct pqos_cpuinfo *cpu, const int *probe)
{
        unsigned *mba_ids;
        unsigned mba_id_num;
        unsigned i;

        _lcpuid_add(0x07, 0x0, 0x0, 0x8000, 0x0, 1U << 30);
        _lcpuid_add(0x10, 0x0, 0x0, 0x8, 0x0, 0x0);
        _lcpuid_add(0x10, 0x3, 90 - 1, 0x0, (1 << 2) | 1, 16 - 1);

        for (i = 0; i < cpu->num_cores; i++) {
                expect_value(__wrap_msr_read, lcore, cpu->cores[i].lcore);
                expect_value(__wrap_msr_read, reg, PQOS_MSR_CORE_CAPABILITIES);
                will_return(__wrap_msr_read, MACHINE_RETVAL_OK);
                will_return(__wrap_msr_read,
                            PQOS_MSR_CORE_CAPABILITIES_MBA40_EN);
        }

        mba_ids = pqos_cpu_get_mba_ids(cpu, &mba_id_num);
        assert_non_null(mba_ids);
        for (i = 0; i < mba_id_num && probe[i] != -1; i++) {
                unsigned core = 0;

                assert_int_equal(
                    pqos_cpu_get_one_by_mba_id(cpu, mba_ids[i], &core),
                    PQOS_RETVAL_OK);
                expect_value(__wrap_msr_probe, lcore, core);
                expect_value(__wrap_msr_probe, reg, PQOS_MSR_MBA_CFG);
                will_return(__wrap_msr_probe, probe[i]);
                if (probe[i] == MACHINE_RETVAL_OK)
                        will_return(__wrap_msr_probe, 0);
        }
        free(mba_ids);
}

/*
 * the cores report MBA 4.0, but the part has no MBA configuration register:
 * every domain refuses it
 */
static void
test_hw_cap_mba_discover_mba40_without_mba_cfg(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        struct pqos_cap_mba cap_mba;
        const int probe[] = {MACHINE_RETVAL_FAULT, MACHINE_RETVAL_FAULT, -1};
        int ret;

        _mba40_probe_queue(data->cpu, probe);

        _log_keep_start();
        ret = hw_cap_mba_discover(&cap_mba, data->cpu);
        test_log_init_silent();
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(cap_mba.mba40, 0);
        assert_int_equal(cap_mba.mba40_on, 0);
        assert_int_equal(cap_mba.num_classes, 16);
        /* the reason names no domain of its own: none has the register */
        assert_non_null(strstr(_log_text, "MBA_CFG MSR (0xC84) cannot be read "
                                          "in any of the 2 MBA domains\n"));
}

/* the MBA domain list cannot be made: discovery fails, MBA is not dropped */
static void
test_hw_cap_mba_discover_mba40_mba_ids_fail(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        struct pqos_cap_mba cap_mba;
        const int probe[] = {-1};
        int ret;

        _mba40_probe_queue(data->cpu, probe);

        _mba_ids_fail = 1;
        ret = hw_cap_mba_discover(&cap_mba, data->cpu);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
        assert_int_equal(_mba_ids_fail, 0);
}

/*
 * one domain has the register and the other does not, either way round: a
 * request to turn MBA 4.0 off could not reach the first, and discovery fails
 */
static void
test_hw_cap_mba_discover_mba40_later_domain_without_mba_cfg(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        struct pqos_cap_mba cap_mba;
        const int probe[] = {MACHINE_RETVAL_OK, MACHINE_RETVAL_FAULT, -1};
        char expected[128];
        int ret;

        _mba40_probe_queue(data->cpu, probe);

        _log_keep_start();
        ret = hw_cap_mba_discover(&cap_mba, data->cpu);
        test_log_init_silent();
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
        /* the error names the domain without the register */
        snprintf(expected, sizeof(expected),
                 "can be read in 1 of 2 MBA domains, not in 1 of them (the "
                 "first, MBA domain %u)\n",
                 _mba_id(data->cpu, 1));
        assert_non_null(strstr(_log_text, expected));
}

static void
test_hw_cap_mba_discover_mba40_first_domain_without_mba_cfg(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        struct pqos_cap_mba cap_mba;
        const int probe[] = {MACHINE_RETVAL_FAULT, MACHINE_RETVAL_OK, -1};
        int ret;

        _mba40_probe_queue(data->cpu, probe);

        ret = hw_cap_mba_discover(&cap_mba, data->cpu);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
}

/*
 * CORE_CAPABILITIES is there but reports no MBA 4.0 on any core: MBA_CFG is
 * not probed at all, and discovery succeeds without MBA 4.0
 */
static void
test_hw_cap_mba_discover_mba40_no_core_reports_it(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        struct pqos_cap_mba cap_mba;
        unsigned i;
        int ret;

        _lcpuid_add(0x07, 0x0, 0x0, 0x8000, 0x0, 1U << 30);
        _lcpuid_add(0x10, 0x0, 0x0, 0x8, 0x0, 0x0);
        _lcpuid_add(0x10, 0x3, 90 - 1, 0x0, (1 << 2) | 1, 16 - 1);

        for (i = 0; i < data->cpu->num_cores; i++) {
                expect_value(__wrap_msr_read, lcore, data->cpu->cores[i].lcore);
                expect_value(__wrap_msr_read, reg, PQOS_MSR_CORE_CAPABILITIES);
                will_return(__wrap_msr_read, MACHINE_RETVAL_OK);
                will_return(__wrap_msr_read, 0);
        }

        ret = hw_cap_mba_discover(&cap_mba, data->cpu);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(cap_mba.mba40, 0);
        assert_int_equal(cap_mba.mba40_on, 0);
}

/*
 * MBA_CFG cannot be read for another reason than the core refusing it, the
 * core gone offline say, which tells nothing about the register: discovery
 * fails, MBA 4.0 is not dropped in silence
 */
static void
test_hw_cap_mba_discover_mba40_core_unreadable(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        struct pqos_cap_mba cap_mba;
        const int probe[] = {MACHINE_RETVAL_ERROR, -1};
        int ret;

        _mba40_probe_queue(data->cpu, probe);

        ret = hw_cap_mba_discover(&cap_mba, data->cpu);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
}

static void
test_hw_cap_mba_discover_mba40_unsupported(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        struct pqos_cap_mba cap_mba;
        int ret;
        uint32_t num_classes = 8;
        uint32_t throttle_max = 90;
        uint32_t is_linear = 1;

        _lcpuid_add(0x07, 0x0, 0x0, 0x8000, 0x0, 0x0);
        _lcpuid_add(0x10, 0x0, 0x0, 0x8, 0x0, 0x0);
        _lcpuid_add(0x10, 0x3, throttle_max - 1, 0x0, is_linear << 2,
                    num_classes - 1);

        ret = hw_cap_mba_discover(&cap_mba, data->cpu);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(cap_mba.mba40, 0);
        assert_int_equal(cap_mba.mba40_on, 0);
}

int
main(void)
{
        int result = 0;

        const struct CMUnitTest tests[] = {
            cmocka_unit_test_setup(test_hw_cap_mon_discover_unsupported, _init),
            cmocka_unit_test_setup(test_hw_cap_mon_discover_unsupported2,
                                   _init),
            cmocka_unit_test_setup(test_hw_cap_mon_discover_tmem, _init),
            cmocka_unit_test_setup(test_hw_cap_mon_discover_lmem, _init),
            cmocka_unit_test_setup(test_hw_cap_mon_discover_llc, _init),
            cmocka_unit_test_setup(test_hw_cap_mon_discover_rmem, _init),
            cmocka_unit_test_setup(test_hw_cap_mon_discover_ipc, _init),
            cmocka_unit_test_setup(test_hw_cap_mon_discover_llc_miss, _init),
            cmocka_unit_test_setup(test_hw_cap_mon_discover_llc_ref, _init),
            cmocka_unit_test_setup(test_hw_cap_l3ca_discover_unsupported,
                                   _init),
            cmocka_unit_test_setup(test_hw_cap_l3ca_discover, _init),
            cmocka_unit_test_setup(test_hw_cap_l3ca_discover_cdp, _init),
            cmocka_unit_test_setup(test_hw_cap_l3ca_discover_non_contiguous_cbm,
                                   _init),
            cmocka_unit_test_setup(test_hw_cap_l3ca_discover_iordt, _init),
            cmocka_unit_test_setup(test_hw_cap_l2ca_discover_alloc_unsupported,
                                   _init),
            cmocka_unit_test_setup(test_hw_cap_l2ca_discover_unsupported,
                                   _init),
            cmocka_unit_test_setup(test_hw_cap_l2ca_discover, _init),
            cmocka_unit_test_setup(test_hw_cap_l2ca_discover_cdp, _init),
            cmocka_unit_test_setup(test_hw_cap_l2ca_discover_non_contiguous_cbm,
                                   _init),
            cmocka_unit_test_setup(test_hw_cap_mba_discover_alloc_unsupported,
                                   _init),
            cmocka_unit_test_setup(test_hw_cap_mba_discover_unsupported, _init),
            cmocka_unit_test_setup(test_hw_cap_mba_discover_linear, _init),
            cmocka_unit_test_setup(test_hw_cap_mba_discover_non_linear, _init),
            cmocka_unit_test_setup(
                test_hw_cap_mba_discover_mba40_supported_enabled, _init),
            cmocka_unit_test_setup(
                test_hw_cap_mba_discover_mba40_supported_disabled, _init),
            cmocka_unit_test_setup(
                test_hw_cap_mba_discover_mba40_domains_not_sockets, _init),
            cmocka_unit_test_setup(
                test_hw_cap_mba_discover_mba40_without_mba_cfg, _init),
            cmocka_unit_test_setup(test_hw_cap_mba_discover_mba40_mba_ids_fail,
                                   _init),
            cmocka_unit_test_setup(
                test_hw_cap_mba_discover_mba40_later_domain_without_mba_cfg,
                _init),
            cmocka_unit_test_setup(
                test_hw_cap_mba_discover_mba40_first_domain_without_mba_cfg,
                _init),
            cmocka_unit_test_setup(
                test_hw_cap_mba_discover_mba40_no_core_reports_it, _init),
            cmocka_unit_test_setup(
                test_hw_cap_mba_discover_mba40_core_unreadable, _init),
            cmocka_unit_test_setup(test_hw_cap_mba_discover_mba40_unsupported,
                                   _init),
        };

        result +=
            cmocka_run_group_tests(tests, test_init_unsupported, test_fini);

        return result;
}
