/*
 * BSD LICENSE
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

#include "allocation.h"
#include "allocation_common.h"
#include "test.h"

/* ======== pqos_alloc_init ======== */

#ifdef __linux__
static void
test_pqos_alloc_init_os(void **state __attribute__((unused)))
{
        const struct pqos_cpuinfo cpu;
        const struct pqos_cap cap;
        const struct pqos_config cfg;
        int ret;

        will_return(__wrap__pqos_get_inter, PQOS_INTER_OS);
        expect_value(__wrap_os_alloc_init, cpu, &cpu);
        expect_value(__wrap_os_alloc_init, cap, &cap);
        will_return(__wrap_os_alloc_init, PQOS_RETVAL_OK);

        ret = pqos_alloc_init(&cpu, &cap, &cfg);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}
#endif

static void
test_pqos_alloc_init_msr(void **state __attribute__((unused)))
{
        const struct pqos_cpuinfo cpu;
        const struct pqos_cap cap;
        const struct pqos_config cfg;
        int ret;

        will_return(__wrap__pqos_get_inter, PQOS_INTER_MSR);

        ret = pqos_alloc_init(&cpu, &cap, &cfg);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_bitmask_check_refuses_a_class_that_selects_nothing(void **state
                                                        __attribute__((unused)))
{
        struct pqos_l3ca l3ca[2];
        struct pqos_l2ca l2ca[2];

        memset(l3ca, 0, sizeof(l3ca));
        memset(l2ca, 0, sizeof(l2ca));

        /* a mask that selects something is accepted, for both resources */
        l3ca[0].class_id = 1;
        l3ca[0].u.ways_mask = 0xf;
        l2ca[0].class_id = 1;
        l2ca[0].u.ways_mask = 0x3;
        assert_int_equal(alloc_l3ca_check_bitmasks(l3ca, 1), PQOS_RETVAL_OK);
        assert_int_equal(alloc_l2ca_check_bitmasks(l2ca, 1), PQOS_RETVAL_OK);

        /* and a zero mask is refused - the class has no cache to allocate */
        l3ca[0].u.ways_mask = 0;
        l2ca[0].u.ways_mask = 0;
        assert_int_equal(alloc_l3ca_check_bitmasks(l3ca, 1), PQOS_RETVAL_PARAM);
        assert_int_equal(alloc_l2ca_check_bitmasks(l2ca, 1), PQOS_RETVAL_PARAM);

        /* a later class is checked as well as the first, which is what a loop
         * that stopped at index zero would get wrong
         */
        l3ca[0].u.ways_mask = 0xf;
        l3ca[1].class_id = 2;
        l3ca[1].u.ways_mask = 0;
        assert_int_equal(alloc_l3ca_check_bitmasks(l3ca, 2), PQOS_RETVAL_PARAM);

        assert_int_equal(alloc_l3ca_check_bitmasks(NULL, 1), PQOS_RETVAL_PARAM);
        assert_int_equal(alloc_l2ca_check_bitmasks(NULL, 1), PQOS_RETVAL_PARAM);
}

static void
test_bitmask_check_wants_both_masks_under_cdp(void **state
                                              __attribute__((unused)))
{
        struct pqos_l3ca l3ca;
        struct pqos_l2ca l2ca;

        memset(&l3ca, 0, sizeof(l3ca));
        memset(&l2ca, 0, sizeof(l2ca));
        l3ca.cdp = 1;
        l2ca.cdp = 1;

        /* with code and data separated a class needs both: one whose code
         * mask selects no way, or whose data mask selects none, is as unusable
         * as one where neither does
         */
        l3ca.u.s.data_mask = 0xf;
        l3ca.u.s.code_mask = 0xf;
        l2ca.u.s.data_mask = 0x3;
        l2ca.u.s.code_mask = 0x3;
        assert_int_equal(alloc_l3ca_check_bitmasks(&l3ca, 1), PQOS_RETVAL_OK);
        assert_int_equal(alloc_l2ca_check_bitmasks(&l2ca, 1), PQOS_RETVAL_OK);

        l3ca.u.s.code_mask = 0;
        l2ca.u.s.code_mask = 0;
        assert_int_equal(alloc_l3ca_check_bitmasks(&l3ca, 1),
                         PQOS_RETVAL_PARAM);
        assert_int_equal(alloc_l2ca_check_bitmasks(&l2ca, 1),
                         PQOS_RETVAL_PARAM);

        l3ca.u.s.code_mask = 0xf;
        l3ca.u.s.data_mask = 0;
        l2ca.u.s.code_mask = 0x3;
        l2ca.u.s.data_mask = 0;
        assert_int_equal(alloc_l3ca_check_bitmasks(&l3ca, 1),
                         PQOS_RETVAL_PARAM);
        assert_int_equal(alloc_l2ca_check_bitmasks(&l2ca, 1),
                         PQOS_RETVAL_PARAM);

        /* What the cdp flag decides is whether the code mask is read at all.
         * ways_mask and data_mask are the same eight bytes of the union, so a
         * class with two good masks and a zero ways_mask is not a state a
         * caller can present; the code mask is the other eight bytes, and this
         * is one a caller presents by simply not setting them. Without CDP they
         * are not consulted, so the class is accepted - which is also why the
         * check must not read them: a caller that filled in ways_mask alone
         * left them as they were, whatever that was.
         */
        memset(&l3ca, 0, sizeof(l3ca));
        memset(&l2ca, 0, sizeof(l2ca));
        l3ca.cdp = 0;
        l2ca.cdp = 0;
        l3ca.u.ways_mask = 0xf;
        l2ca.u.ways_mask = 0x3;
        l3ca.u.s.code_mask = 0;
        l2ca.u.s.code_mask = 0;
        assert_int_equal(alloc_l3ca_check_bitmasks(&l3ca, 1), PQOS_RETVAL_OK);
        assert_int_equal(alloc_l2ca_check_bitmasks(&l2ca, 1), PQOS_RETVAL_OK);
}

int
main(void)
{
        int result = 0;

        const struct CMUnitTest tests[] = {
#ifdef __linux__
            cmocka_unit_test(test_pqos_alloc_init_os),
#endif
            cmocka_unit_test(test_pqos_alloc_init_msr),
            cmocka_unit_test(
                test_bitmask_check_refuses_a_class_that_selects_nothing),
            cmocka_unit_test(test_bitmask_check_wants_both_masks_under_cdp),
        };

        result += cmocka_run_group_tests(tests, NULL, NULL);

        return result;
}
