/*
 * BSD LICENSE
 *
 * Copyright(c) 2026 Intel Corporation. All rights reserved.
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

#include "hybrid.h"

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>
/* clang-format off */
#include <cmocka.h>
/* clang-format on */

static void
test_parse_core_list(void **state)
{
        static const char *const invalid[] = {"",    ",1", "1,", "1,,2", "x",
                                              "2-1", "1-", "-1", "1-2-3"};
        unsigned *cores = NULL;
        unsigned count = 0, i;

        assert_int_equal(
            hybrid_parse_core_list("8,0-3,2,10-12", &cores, &count), 0);
        assert_int_equal(count, 8);
        assert_int_equal(cores[0], 0);
        assert_int_equal(cores[4], 8);
        assert_int_equal(cores[7], 12);
        free(cores);

        for (i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) {
                cores = NULL;
                count = 0;
                assert_int_equal(
                    hybrid_parse_core_list(invalid[i], &cores, &count), -1);
                assert_null(cores);
        }
        (void)state;
}

static void
test_enum_cores_uses_sysconfig_capability(void **state)
{
        const size_t cap_size =
            sizeof(struct pqos_cap) + sizeof(struct pqos_capability);
        const size_t hybrid_size = sizeof(struct hybrid_capabilities) +
                                   sizeof(struct hybrid_core_capability);
        struct hybrid_capabilities *hybrid = calloc(1, hybrid_size);
        struct pqos_cap *cap = calloc(1, cap_size);
        struct pqos_sysconfig sys = {0};
        struct hybrid_core_capability *core;

        assert_non_null(hybrid);
        assert_non_null(cap);
        hybrid->mem_size = hybrid_size;
        hybrid->status = HYBRID_STATUS_YES;
        hybrid->num_cores = 1;
        core = &hybrid->cores[0];
        core->mem_size = sizeof(*core);
        core->lcore = 3;
        core->socket = 1;
        core->core_type_valid = 1;
        core->core_type = 0x40;
        core->native_model_id = 4;

        cap->mem_size = cap_size;
        cap->num_cap = 1;
        cap->capabilities[0].type = PQOS_CAP_TYPE_HYBRID;
        cap->capabilities[0].u.hybrid = hybrid;
        sys.cap = cap;

        assert_int_equal(hybrid_enum_cores(&sys, "3"), 0);
        assert_int_equal(hybrid_enum_cores(&sys, "4"), -1);

        free(cap);
        free(hybrid);
        (void)state;
}

int
main(void)
{
        const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_parse_core_list),
            cmocka_unit_test(test_enum_cores_uses_sysconfig_capability)};

        return cmocka_run_group_tests(tests, NULL, NULL);
}
