/*
 * BSD LICENSE
 *
 * Copyright(c) 2020 Intel Corporation. All rights reserved.
 * All rights reserved.
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

#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include "test.h"
#include "mock_common.h"

#include "os_cap.h"

/* ======== mock ======== */

int
os_cap_mon_resctrl_support(const enum pqos_mon_event event,
                           int *supported,
                           uint32_t *scale)
{
        int ret;

        check_expected(event);
        assert_non_null(supported);
        assert_non_null(scale);

        ret = mock_type(int);
        if (ret == PQOS_RETVAL_OK) {
                *supported = mock_type(int);
                if (*supported)
                        *scale = mock_type(int);
        }

        return ret;
}

int
os_cap_mon_perf_support(const enum pqos_mon_event event,
                        int *supported,
                        uint32_t *scale)
{
        int ret;

        check_expected(event);
        assert_non_null(supported);
        assert_non_null(scale);

        ret = mock_type(int);
        if (ret == PQOS_RETVAL_OK) {
                *supported = mock_type(int);
                if (*supported)
                        *scale = mock_type(int);
        }

        return ret;
}

/* ======== os_cap_l3ca_discover ======== */

static void
test_os_cap_l3ca_discover_unsupported(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        struct pqos_cap_l3ca cap;
        int ret;

        expect_string(__wrap_pqos_dir_exists, path, "/sys/fs/resctrl/info/L3");
        will_return(__wrap_pqos_dir_exists, 0);
        expect_string(__wrap_pqos_dir_exists, path,
                      "/sys/fs/resctrl/info/L3CODE");
        will_return(__wrap_pqos_dir_exists, 0);

        ret = os_cap_l3ca_discover(&cap, data->cpu);
        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);
}

static void
test_os_cap_l3ca_discover_cdp_off(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        struct pqos_cap_l3ca cap;
        unsigned num_grps = 5;
        int ret;

        expect_string(__wrap_pqos_dir_exists, path, "/sys/fs/resctrl/info/L3");
        will_return(__wrap_pqos_dir_exists, 1);

        /* detect cdp support */
        expect_string(__wrap_pqos_file_contains, fname, "/proc/cpuinfo");
        expect_string(__wrap_pqos_file_contains, str, "cdp_l3");
        will_return(__wrap_pqos_file_contains, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_file_contains, 1);

        /* read number of classes */
        will_return(__wrap_resctrl_alloc_get_num_closids, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_num_closids, num_grps);

        /* read number of ways */
        expect_string(__wrap_pqos_fread_uint64, fname,
                      "/sys/fs/resctrl/info/L3/cbm_mask");
        expect_value(__wrap_pqos_fread_uint64, base, 16);
        will_return(__wrap_pqos_fread_uint64, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_fread_uint64,
                    (1 << data->cap_l3ca.num_ways) - 1);

        /* read way contention mask */
        expect_string(__wrap_pqos_file_exists, path,
                      "/sys/fs/resctrl/info/L3/shareable_bits");
        will_return(__wrap_pqos_file_exists, 1);
        expect_string(__wrap_pqos_fread_uint64, fname,
                      "/sys/fs/resctrl/info/L3/shareable_bits");
        expect_value(__wrap_pqos_fread_uint64, base, 16);
        will_return(__wrap_pqos_fread_uint64, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_fread_uint64, data->cap_l3ca.way_contention);

        ret = os_cap_l3ca_discover(&cap, data->cpu);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(cap.cdp, 1);
        assert_int_equal(cap.cdp_on, 0);
        assert_int_equal(cap.way_size, data->cpu->l3.way_size);
        assert_int_equal(cap.num_classes, num_grps);
        assert_int_equal(cap.num_ways, data->cap_l3ca.num_ways);
        assert_int_equal(cap.way_contention, data->cap_l3ca.way_contention);
}

static void
test_os_cap_l3ca_discover_cdp_on(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        struct pqos_cap_l3ca cap;
        unsigned num_grps = 5;
        int ret;

        expect_string(__wrap_pqos_dir_exists, path, "/sys/fs/resctrl/info/L3");
        will_return(__wrap_pqos_dir_exists, 0);
        expect_string(__wrap_pqos_dir_exists, path,
                      "/sys/fs/resctrl/info/L3CODE");
        will_return(__wrap_pqos_dir_exists, 1);
        expect_string(__wrap_pqos_dir_exists, path,
                      "/sys/fs/resctrl/info/L3DATA");
        will_return(__wrap_pqos_dir_exists, 1);

        /* read number of classes */
        will_return(__wrap_resctrl_alloc_get_num_closids, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_num_closids, num_grps);

        /* read number of ways */
        expect_string(__wrap_pqos_fread_uint64, fname,
                      "/sys/fs/resctrl/info/L3CODE/cbm_mask");
        expect_value(__wrap_pqos_fread_uint64, base, 16);
        will_return(__wrap_pqos_fread_uint64, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_fread_uint64,
                    (1 << data->cap_l3ca.num_ways) - 1);

        /* read way contention mask */
        expect_string(__wrap_pqos_file_exists, path,
                      "/sys/fs/resctrl/info/L3CODE/shareable_bits");
        will_return(__wrap_pqos_file_exists, 0);

        ret = os_cap_l3ca_discover(&cap, data->cpu);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(cap.cdp, 1);
        assert_int_equal(cap.cdp_on, 1);
        assert_int_equal(cap.way_size, data->cpu->l3.way_size);
        assert_int_equal(cap.num_classes, num_grps);
        assert_int_equal(cap.num_ways, data->cap_l3ca.num_ways);
        assert_int_equal(cap.way_contention, 0x0);
}

static void
test_os_cap_l3ca_discover_cdp_unsupported(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        struct pqos_cap_l3ca cap;
        unsigned num_grps = 5;
        int ret;

        expect_string(__wrap_pqos_dir_exists, path, "/sys/fs/resctrl/info/L3");
        will_return(__wrap_pqos_dir_exists, 1);

        /* detect cdp support */
        expect_string(__wrap_pqos_file_contains, fname, "/proc/cpuinfo");
        expect_string(__wrap_pqos_file_contains, str, "cdp_l3");
        will_return(__wrap_pqos_file_contains, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_file_contains, 0);

        /* read number of classes */
        will_return(__wrap_resctrl_alloc_get_num_closids, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_num_closids, num_grps);

        /* read number of ways */
        expect_string(__wrap_pqos_fread_uint64, fname,
                      "/sys/fs/resctrl/info/L3/cbm_mask");
        expect_value(__wrap_pqos_fread_uint64, base, 16);
        will_return(__wrap_pqos_fread_uint64, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_fread_uint64,
                    (1 << data->cap_l3ca.num_ways) - 1);

        /* read way contention mask */
        expect_string(__wrap_pqos_file_exists, path,
                      "/sys/fs/resctrl/info/L3/shareable_bits");
        will_return(__wrap_pqos_file_exists, 1);
        expect_string(__wrap_pqos_fread_uint64, fname,
                      "/sys/fs/resctrl/info/L3/shareable_bits");
        expect_value(__wrap_pqos_fread_uint64, base, 16);
        will_return(__wrap_pqos_fread_uint64, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_fread_uint64, data->cap_l3ca.way_contention);

        ret = os_cap_l3ca_discover(&cap, data->cpu);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(cap.cdp, 0);
        assert_int_equal(cap.cdp_on, 0);
        assert_int_equal(cap.way_size, data->cpu->l3.way_size);
        assert_int_equal(cap.num_classes, num_grps);
        assert_int_equal(cap.num_ways, data->cap_l3ca.num_ways);
        assert_int_equal(cap.way_contention, data->cap_l3ca.way_contention);
}

/* ======== os_cap_l2ca_discover ======== */

static void
test_os_cap_l2ca_discover_unsupported(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        struct pqos_cap_l2ca cap;
        int ret;

        expect_string(__wrap_pqos_dir_exists, path, "/sys/fs/resctrl/info/L2");
        will_return(__wrap_pqos_dir_exists, 0);
        expect_string(__wrap_pqos_dir_exists, path,
                      "/sys/fs/resctrl/info/L2CODE");
        will_return(__wrap_pqos_dir_exists, 0);

        ret = os_cap_l2ca_discover(&cap, data->cpu);
        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);
}

static void
test_os_cap_l2ca_discover_cdp_off(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        struct pqos_cap_l2ca cap;
        unsigned num_grps = 5;
        int ret;

        expect_string(__wrap_pqos_dir_exists, path, "/sys/fs/resctrl/info/L2");
        will_return(__wrap_pqos_dir_exists, 1);

        /* detect cdp support */
        expect_string(__wrap_pqos_file_contains, fname, "/proc/cpuinfo");
        expect_string(__wrap_pqos_file_contains, str, "cdp_l2");
        will_return(__wrap_pqos_file_contains, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_file_contains, 1);

        /* read number of classes */
        will_return(__wrap_resctrl_alloc_get_num_closids, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_num_closids, num_grps);

        /* read number of ways */
        expect_string(__wrap_pqos_fread_uint64, fname,
                      "/sys/fs/resctrl/info/L2/cbm_mask");
        expect_value(__wrap_pqos_fread_uint64, base, 16);
        will_return(__wrap_pqos_fread_uint64, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_fread_uint64,
                    (1 << data->cap_l2ca.num_ways) - 1);

        /* read way contention mask */
        expect_string(__wrap_pqos_file_exists, path,
                      "/sys/fs/resctrl/info/L2/shareable_bits");
        will_return(__wrap_pqos_file_exists, 1);
        expect_string(__wrap_pqos_fread_uint64, fname,
                      "/sys/fs/resctrl/info/L2/shareable_bits");
        expect_value(__wrap_pqos_fread_uint64, base, 16);
        will_return(__wrap_pqos_fread_uint64, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_fread_uint64, data->cap_l2ca.way_contention);

        ret = os_cap_l2ca_discover(&cap, data->cpu);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(cap.cdp, 1);
        assert_int_equal(cap.cdp_on, 0);
        assert_int_equal(cap.way_size, data->cpu->l2.way_size);
        assert_int_equal(cap.num_classes, num_grps);
        assert_int_equal(cap.num_ways, data->cap_l2ca.num_ways);
        assert_int_equal(cap.way_contention, data->cap_l2ca.way_contention);
}

static void
test_os_cap_l2ca_discover_cdp_on(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        struct pqos_cap_l2ca cap;
        unsigned num_grps = 5;
        int ret;

        expect_string(__wrap_pqos_dir_exists, path, "/sys/fs/resctrl/info/L2");
        will_return(__wrap_pqos_dir_exists, 0);
        expect_string(__wrap_pqos_dir_exists, path,
                      "/sys/fs/resctrl/info/L2CODE");
        will_return(__wrap_pqos_dir_exists, 1);
        expect_string(__wrap_pqos_dir_exists, path,
                      "/sys/fs/resctrl/info/L2DATA");
        will_return(__wrap_pqos_dir_exists, 1);

        /* read number of classes */
        will_return(__wrap_resctrl_alloc_get_num_closids, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_num_closids, num_grps);

        /* read number of ways */
        expect_string(__wrap_pqos_fread_uint64, fname,
                      "/sys/fs/resctrl/info/L2CODE/cbm_mask");
        expect_value(__wrap_pqos_fread_uint64, base, 16);
        will_return(__wrap_pqos_fread_uint64, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_fread_uint64,
                    (1 << data->cap_l2ca.num_ways) - 1);

        /* read way contention mask */
        expect_string(__wrap_pqos_file_exists, path,
                      "/sys/fs/resctrl/info/L2CODE/shareable_bits");
        will_return(__wrap_pqos_file_exists, 0);

        ret = os_cap_l2ca_discover(&cap, data->cpu);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(cap.cdp, 1);
        assert_int_equal(cap.cdp_on, 1);
        assert_int_equal(cap.way_size, data->cpu->l2.way_size);
        assert_int_equal(cap.num_classes, num_grps);
        assert_int_equal(cap.num_ways, data->cap_l2ca.num_ways);
        assert_int_equal(cap.way_contention, 0x0);
}

static void
test_os_cap_l2ca_discover_cdp_unsupported(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        struct pqos_cap_l2ca cap;
        unsigned num_grps = 5;
        int ret;

        expect_string(__wrap_pqos_dir_exists, path, "/sys/fs/resctrl/info/L2");
        will_return(__wrap_pqos_dir_exists, 1);

        /* detect cdp support */
        expect_string(__wrap_pqos_file_contains, fname, "/proc/cpuinfo");
        expect_string(__wrap_pqos_file_contains, str, "cdp_l2");
        will_return(__wrap_pqos_file_contains, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_file_contains, 0);

        /* read number of classes */
        will_return(__wrap_resctrl_alloc_get_num_closids, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_num_closids, num_grps);

        /* read number of ways */
        expect_string(__wrap_pqos_fread_uint64, fname,
                      "/sys/fs/resctrl/info/L2/cbm_mask");
        expect_value(__wrap_pqos_fread_uint64, base, 16);
        will_return(__wrap_pqos_fread_uint64, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_fread_uint64,
                    (1 << data->cap_l2ca.num_ways) - 1);

        /* read way contention mask */
        expect_string(__wrap_pqos_file_exists, path,
                      "/sys/fs/resctrl/info/L2/shareable_bits");
        will_return(__wrap_pqos_file_exists, 1);
        expect_string(__wrap_pqos_fread_uint64, fname,
                      "/sys/fs/resctrl/info/L2/shareable_bits");
        expect_value(__wrap_pqos_fread_uint64, base, 16);
        will_return(__wrap_pqos_fread_uint64, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_fread_uint64, data->cap_l2ca.way_contention);

        ret = os_cap_l2ca_discover(&cap, data->cpu);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(cap.cdp, 0);
        assert_int_equal(cap.cdp_on, 0);
        assert_int_equal(cap.way_size, data->cpu->l2.way_size);
        assert_int_equal(cap.num_classes, num_grps);
        assert_int_equal(cap.num_ways, data->cap_l2ca.num_ways);
        assert_int_equal(cap.way_contention, data->cap_l2ca.way_contention);
}

/* ======== os_cap_mba_discover ======== */

static void
test_os_cap_mba_discover_unsupported(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        struct pqos_cap_mba cap;
        int ret;

        expect_string(__wrap_pqos_dir_exists, path, "/sys/fs/resctrl/info/MB");
        will_return(__wrap_pqos_dir_exists, 0);

        ret = os_cap_mba_discover(&cap, data->cpu);
        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);
}

static void
test_os_cap_mba_discover_default(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        struct pqos_cap_mba cap;
        unsigned num_grps = 5;
        int ret;

        expect_string(__wrap_pqos_dir_exists, path, "/sys/fs/resctrl/info/MB");
        will_return(__wrap_pqos_dir_exists, 1);

        /* read number of classes */
        will_return(__wrap_resctrl_alloc_get_num_closids, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_num_closids, num_grps);

        /* check if ctrl is enabled */
        expect_string(__wrap_pqos_file_contains, fname, "/proc/mounts");
        expect_string(__wrap_pqos_file_contains, str, "mba_MBps");
        will_return(__wrap_pqos_file_contains, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_file_contains, 0);

        /* read throttle_max */
        expect_string(__wrap_pqos_fread_uint64, fname,
                      "/sys/fs/resctrl/info/MB/min_bandwidth");
        expect_value(__wrap_pqos_fread_uint64, base, 10);
        will_return(__wrap_pqos_fread_uint64, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_fread_uint64, 100 - data->cap_mba.throttle_max);

        /* read throttle_step */
        expect_string(__wrap_pqos_fread_uint64, fname,
                      "/sys/fs/resctrl/info/MB/bandwidth_gran");
        expect_value(__wrap_pqos_fread_uint64, base, 10);
        will_return(__wrap_pqos_fread_uint64, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_fread_uint64, data->cap_mba.throttle_step);

        /* read delay_linear */
        expect_string(__wrap_pqos_fread_uint64, fname,
                      "/sys/fs/resctrl/info/MB/delay_linear");
        expect_value(__wrap_pqos_fread_uint64, base, 10);
        will_return(__wrap_pqos_fread_uint64, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_fread_uint64, 1);

        ret = os_cap_mba_discover(&cap, data->cpu);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(cap.num_classes, num_grps);
        assert_int_equal(cap.throttle_max, data->cap_mba.throttle_max);
        assert_int_equal(cap.throttle_step, data->cap_mba.throttle_step);
        assert_int_equal(cap.is_linear, 1);
}

static void
test_os_cap_mba_discover_ctrl(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        struct pqos_cap_mba cap;
        unsigned num_grps = 5;
        int ret;

        expect_string(__wrap_pqos_dir_exists, path, "/sys/fs/resctrl/info/MB");
        will_return(__wrap_pqos_dir_exists, 1);

        /* read number of classes */
        will_return(__wrap_resctrl_alloc_get_num_closids, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_alloc_get_num_closids, num_grps);

        /* check if ctrl is enabled */
        expect_string(__wrap_pqos_file_contains, fname, "/proc/mounts");
        expect_string(__wrap_pqos_file_contains, str, "mba_MBps");
        will_return(__wrap_pqos_file_contains, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_file_contains, 1);

        /* read throttle_max */
        expect_string(__wrap_pqos_fread_uint64, fname,
                      "/sys/fs/resctrl/info/MB/min_bandwidth");
        expect_value(__wrap_pqos_fread_uint64, base, 10);
        will_return(__wrap_pqos_fread_uint64, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_fread_uint64, 100 - data->cap_mba.throttle_max);

        /* read throttle_step */
        expect_string(__wrap_pqos_fread_uint64, fname,
                      "/sys/fs/resctrl/info/MB/bandwidth_gran");
        expect_value(__wrap_pqos_fread_uint64, base, 10);
        will_return(__wrap_pqos_fread_uint64, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_fread_uint64, data->cap_mba.throttle_step);

        /* read delay_linear */
        expect_string(__wrap_pqos_fread_uint64, fname,
                      "/sys/fs/resctrl/info/MB/delay_linear");
        expect_value(__wrap_pqos_fread_uint64, base, 10);
        will_return(__wrap_pqos_fread_uint64, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_fread_uint64, 1);

        ret = os_cap_mba_discover(&cap, data->cpu);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(cap.num_classes, num_grps);
        assert_int_equal(cap.throttle_max, data->cap_mba.throttle_max);
        assert_int_equal(cap.throttle_step, data->cap_mba.throttle_step);
        assert_int_equal(cap.is_linear, 1);
        assert_int_equal(cap.ctrl, 1);
        assert_int_equal(cap.ctrl_on, 1);
}

/* ======== os_cap_mon_discover ======== */

static void
test_os_cap_mon_discover_unsupported(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        struct pqos_cap_mon *cap = NULL;

        expect_string(__wrap_pqos_file_contains, fname, "/proc/cpuinfo");
        expect_string(__wrap_pqos_file_contains, str, "cqm");
        will_return(__wrap_pqos_file_contains, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_file_contains, 0);

        ret = os_cap_mon_discover(&cap, data->cpu);
        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);
        assert_null(cap);
}

static void
detect_mon_support(const enum pqos_mon_event event, int resctrl, int perf)
{
        if (event == PQOS_MON_EVENT_RMEM_BW) {
                detect_mon_support(PQOS_MON_EVENT_LMEM_BW, resctrl, perf);
                detect_mon_support(PQOS_MON_EVENT_TMEM_BW, resctrl, perf);

                return;
        }

        expect_value(os_cap_mon_resctrl_support, event, event);
        will_return(os_cap_mon_resctrl_support, PQOS_RETVAL_OK);
        will_return(os_cap_mon_resctrl_support, resctrl);
        if (resctrl) {
                will_return(os_cap_mon_resctrl_support, 1);
                return;
        }

        expect_value(os_cap_mon_perf_support, event, event);
        will_return(os_cap_mon_perf_support, PQOS_RETVAL_OK);
        will_return(os_cap_mon_perf_support, perf);
        if (perf)
                will_return(os_cap_mon_perf_support, 1);
}

static void
test_os_cap_mon_discover_resctrl(struct test_data *data,
                                 const enum pqos_mon_event event)
{
        int ret;
        struct pqos_cap_mon *cap = NULL;
        uint32_t max_rmid = 128;

        expect_string(__wrap_pqos_file_contains, fname, "/proc/cpuinfo");
        expect_string(__wrap_pqos_file_contains, str, "cqm");
        will_return(__wrap_pqos_file_contains, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_file_contains, 1);

        expect_string(__wrap_pqos_file_exists, path,
                      "/sys/fs/resctrl/info/L3_MON/num_rmids");
        will_return(__wrap_pqos_file_exists, 1);

        expect_string(__wrap_pqos_fread_uint64, fname,
                      "/sys/fs/resctrl/info/L3_MON/num_rmids");
        expect_value(__wrap_pqos_fread_uint64, base, 10);
        will_return(__wrap_pqos_fread_uint64, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_fread_uint64, max_rmid);

        detect_mon_support(PQOS_MON_EVENT_L3_OCCUP,
                           event == PQOS_MON_EVENT_L3_OCCUP, 0);
        detect_mon_support(PQOS_MON_EVENT_LMEM_BW,
                           event == PQOS_MON_EVENT_LMEM_BW, 0);
        detect_mon_support(PQOS_MON_EVENT_TMEM_BW,
                           event == PQOS_MON_EVENT_TMEM_BW, 0);
        detect_mon_support(PQOS_MON_EVENT_RMEM_BW,
                           event == PQOS_MON_EVENT_RMEM_BW, 0);
        detect_mon_support(PQOS_PERF_EVENT_LLC_MISS, 0, 0);
        detect_mon_support(PQOS_PERF_EVENT_IPC, 0, 0);

        ret = os_cap_mon_discover(&cap, data->cpu);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_non_null(cap);
        assert_int_equal(cap->l3_size, data->cpu->l3.total_size);
        assert_int_equal(cap->num_events, 1);
        assert_int_equal(cap->max_rmid, max_rmid);
        assert_int_equal(cap->events[0].type, event);
        assert_int_equal(cap->events[0].scale_factor, 1);
        assert_int_equal(cap->events[0].max_rmid, max_rmid);

        free(cap);
}

static void
test_os_cap_mon_discover_perf(struct test_data *data,
                              const enum pqos_mon_event event)
{
        int ret;
        struct pqos_cap_mon *cap = NULL;

        expect_string(__wrap_pqos_file_contains, fname, "/proc/cpuinfo");
        expect_string(__wrap_pqos_file_contains, str, "cqm");
        will_return(__wrap_pqos_file_contains, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_file_contains, 1);

        expect_string(__wrap_pqos_file_exists, path,
                      "/sys/fs/resctrl/info/L3_MON/num_rmids");
        will_return(__wrap_pqos_file_exists, 0);

        detect_mon_support(PQOS_MON_EVENT_L3_OCCUP, 0,
                           event == PQOS_MON_EVENT_L3_OCCUP);
        detect_mon_support(PQOS_MON_EVENT_LMEM_BW, 0,
                           event == PQOS_MON_EVENT_LMEM_BW);
        detect_mon_support(PQOS_MON_EVENT_TMEM_BW, 0,
                           event == PQOS_MON_EVENT_TMEM_BW);
        detect_mon_support(PQOS_MON_EVENT_RMEM_BW, 0,
                           event == PQOS_MON_EVENT_RMEM_BW);
        detect_mon_support(PQOS_PERF_EVENT_LLC_MISS, 0,
                           event == PQOS_PERF_EVENT_LLC_MISS);
        detect_mon_support(PQOS_PERF_EVENT_IPC, 0,
                           event == PQOS_PERF_EVENT_IPC);

        ret = os_cap_mon_discover(&cap, data->cpu);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_non_null(cap);
        assert_int_equal(cap->l3_size, data->cpu->l3.total_size);
        assert_int_equal(cap->num_events, 1);
        assert_int_equal(cap->max_rmid, 0);
        assert_int_equal(cap->events[0].type, event);
        assert_int_equal(cap->events[0].scale_factor, 1);
        assert_int_equal(cap->events[0].max_rmid, 0);

        free(cap);
}

static void
test_os_cap_mon_discover_resctrl_llc(void **state)
{
        struct test_data *data = (struct test_data *)*state;

        test_os_cap_mon_discover_resctrl(data, PQOS_MON_EVENT_L3_OCCUP);
}

static void
test_os_cap_mon_discover_perf_llc(void **state)
{
        struct test_data *data = (struct test_data *)*state;

        test_os_cap_mon_discover_perf(data, PQOS_MON_EVENT_L3_OCCUP);
}

static void
test_os_cap_mon_discover_resctrl_lmem(void **state)
{
        struct test_data *data = (struct test_data *)*state;

        test_os_cap_mon_discover_resctrl(data, PQOS_MON_EVENT_LMEM_BW);
}

static void
test_os_cap_mon_discover_perf_lmem(void **state)
{
        struct test_data *data = (struct test_data *)*state;

        test_os_cap_mon_discover_perf(data, PQOS_MON_EVENT_LMEM_BW);
}

static void
test_os_cap_mon_discover_resctrl_tmem(void **state)
{
        struct test_data *data = (struct test_data *)*state;

        test_os_cap_mon_discover_resctrl(data, PQOS_MON_EVENT_TMEM_BW);
}

static void
test_os_cap_mon_discover_perf_tmem(void **state)
{
        struct test_data *data = (struct test_data *)*state;

        test_os_cap_mon_discover_perf(data, PQOS_MON_EVENT_TMEM_BW);
}

static void
test_os_cap_mon_discover_resctrl_rmem(void **state)
{
        struct test_data *data = (struct test_data *)*state;

        test_os_cap_mon_discover_resctrl(data, PQOS_MON_EVENT_RMEM_BW);
}

static void
test_os_cap_mon_discover_perf_rmem(void **state)
{
        struct test_data *data = (struct test_data *)*state;

        test_os_cap_mon_discover_perf(data, PQOS_MON_EVENT_RMEM_BW);
}

/* ======== os_cap_init ======== */

static void
test_os_cap_init_unsupported(void **state __attribute__((unused)))
{
        int ret;

        expect_string(__wrap_pqos_file_contains, fname, "/proc/filesystems");
        expect_string(__wrap_pqos_file_contains, str, "resctrl");
        will_return(__wrap_pqos_file_contains, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_file_contains, 0);

        ret = os_cap_init(PQOS_INTER_OS);
        assert_int_equal(ret, PQOS_RETVAL_INTER);
}

static void
test_os_cap_init_mounted(void **state __attribute__((unused)))
{
        int ret;

        expect_string(__wrap_pqos_file_contains, fname, "/proc/filesystems");
        expect_string(__wrap_pqos_file_contains, str, "resctrl");
        will_return(__wrap_pqos_file_contains, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_file_contains, 1);

        expect_string(__wrap_pqos_file_exists, path, "/sys/fs/resctrl/cpus");
        will_return(__wrap_pqos_file_exists, 1);

        ret = os_cap_init(PQOS_INTER_OS);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_os_cap_init_unmounted(void **state __attribute__((unused)))
{
        int ret;

        expect_string(__wrap_pqos_file_contains, fname, "/proc/filesystems");
        expect_string(__wrap_pqos_file_contains, str, "resctrl");
        will_return(__wrap_pqos_file_contains, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_file_contains, 1);

        expect_string(__wrap_pqos_file_exists, path, "/sys/fs/resctrl/cpus");
        will_return(__wrap_pqos_file_exists, 0);

        /* probe for mba ctrl */
        expect_value(__wrap_resctrl_mount, l3_cdp_cfg, PQOS_REQUIRE_CDP_OFF);
        expect_value(__wrap_resctrl_mount, l2_cdp_cfg, PQOS_REQUIRE_CDP_OFF);
        expect_value(__wrap_resctrl_mount, mba_cfg, PQOS_MBA_CTRL);
        will_return(__wrap_resctrl_mount, PQOS_RETVAL_ERROR);

        expect_value(__wrap_resctrl_mount, l3_cdp_cfg, PQOS_REQUIRE_CDP_OFF);
        expect_value(__wrap_resctrl_mount, l2_cdp_cfg, PQOS_REQUIRE_CDP_OFF);
        expect_value(__wrap_resctrl_mount, mba_cfg, PQOS_MBA_DEFAULT);
        will_return(__wrap_resctrl_mount, PQOS_RETVAL_OK);

        ret = os_cap_init(PQOS_INTER_OS);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_os_cap_init_resctrl_mon(void **state __attribute__((unused)))
{
        int ret;

        expect_string(__wrap_pqos_file_contains, fname, "/proc/filesystems");
        expect_string(__wrap_pqos_file_contains, str, "resctrl");
        will_return(__wrap_pqos_file_contains, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_file_contains, 1);

        expect_string(__wrap_pqos_file_exists, path, "/sys/fs/resctrl/cpus");
        will_return(__wrap_pqos_file_exists, 1);

        expect_string(__wrap_pqos_dir_exists, path,
                      "/sys/fs/resctrl/info/L3_MON");
        will_return(__wrap_pqos_dir_exists, 0);

        ret = os_cap_init(PQOS_INTER_OS_RESCTRL_MON);
        assert_int_equal(ret, PQOS_RETVAL_INTER);
}

int
main(void)
{
        int result = 0;

        const struct CMUnitTest tests_l3ca[] = {
            cmocka_unit_test(test_os_cap_l3ca_discover_cdp_off),
            cmocka_unit_test(test_os_cap_l3ca_discover_cdp_on),
            cmocka_unit_test(test_os_cap_l3ca_discover_cdp_unsupported)};

        const struct CMUnitTest tests_l2ca[] = {
            cmocka_unit_test(test_os_cap_l2ca_discover_cdp_off),
            cmocka_unit_test(test_os_cap_l2ca_discover_cdp_on),
            cmocka_unit_test(test_os_cap_l2ca_discover_cdp_unsupported)};

        const struct CMUnitTest tests_mba[] = {
            cmocka_unit_test(test_os_cap_mba_discover_default),
            cmocka_unit_test(test_os_cap_mba_discover_ctrl)};

        const struct CMUnitTest tests_mon[] = {
            cmocka_unit_test(test_os_cap_mon_discover_resctrl_llc),
            cmocka_unit_test(test_os_cap_mon_discover_perf_llc),
            cmocka_unit_test(test_os_cap_mon_discover_resctrl_lmem),
            cmocka_unit_test(test_os_cap_mon_discover_perf_lmem),
            cmocka_unit_test(test_os_cap_mon_discover_resctrl_tmem),
            cmocka_unit_test(test_os_cap_mon_discover_perf_tmem),
            cmocka_unit_test(test_os_cap_mon_discover_resctrl_rmem),
            cmocka_unit_test(test_os_cap_mon_discover_perf_rmem)};

        const struct CMUnitTest tests_all[] = {
            cmocka_unit_test(test_os_cap_init_mounted),
            cmocka_unit_test(test_os_cap_init_unmounted),
            cmocka_unit_test(test_os_cap_init_resctrl_mon)};

        const struct CMUnitTest tests_unsupported[] = {
            cmocka_unit_test(test_os_cap_l3ca_discover_unsupported),
            cmocka_unit_test(test_os_cap_l2ca_discover_unsupported),
            cmocka_unit_test(test_os_cap_mba_discover_unsupported),
            cmocka_unit_test(test_os_cap_mon_discover_unsupported),
            cmocka_unit_test(test_os_cap_init_unsupported)};

        result += cmocka_run_group_tests(tests_l3ca, test_init_l3ca, test_fini);
        result += cmocka_run_group_tests(tests_l2ca, test_init_l2ca, test_fini);
        result += cmocka_run_group_tests(tests_mba, test_init_mba, test_fini);
        result += cmocka_run_group_tests(tests_mon, test_init_all, test_fini);
        result += cmocka_run_group_tests(tests_all, test_init_all, test_fini);
        result += cmocka_run_group_tests(tests_unsupported,
                                         test_init_unsupported, test_fini);

        return result;
}
