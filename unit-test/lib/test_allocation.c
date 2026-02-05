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

int
main(void)
{
        int result = 0;

        const struct CMUnitTest tests[] = {
#ifdef __linux__
            cmocka_unit_test(test_pqos_alloc_init_os),
#endif
            cmocka_unit_test(test_pqos_alloc_init_msr),
        };

        result += cmocka_run_group_tests(tests, NULL, NULL);

        return result;
}
