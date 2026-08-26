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
#include "common.h"
#include "dump.h"
#include "dump_rmids.h"
#include "output.h"
#include "pqos.h"

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* clang-format off */
#include <cmocka.h>
/* clang-format on */

/* Both dump entry points check the ERDT information first, so a system
 * configuration that carries one is enough to reach the option checks these
 * test cases are about. Nothing below them is reached, because every case here
 * is a command line that has to be rejected.
 */
static struct pqos_erdt_info test_erdt;
static struct pqos_sysconfig test_sys = {.erdt = &test_erdt};

/* ======== dump_mmio_regs ======== */

static void
test_dump_mmio_regs_requires_a_domain(void **state)
{
        UNUSED_ARG(state);

        run_void_function(dump_mmio_regs, &test_sys);

        assert_int_equal(output_exit_was_called(), 1);
        assert_int_equal(output_get_exit_status(), EXIT_FAILURE);
        assert_int_equal(output_has_text("Provide either --socket or "
                                         "--dump-domain-id"),
                         1);
}

static void
test_dump_mmio_regs_requires_a_space(void **state)
{
        UNUSED_ARG(state);

        /* a complete selection but for --space, which used to dump CMRC and
         * report success, because the space selector defaults to zero
         */
        selfn_dump_domain_id("0");

        run_void_function(dump_mmio_regs, &test_sys);

        assert_int_equal(output_exit_was_called(), 1);
        assert_int_equal(output_get_exit_status(), EXIT_FAILURE);
        assert_int_equal(output_has_text("Provide --space"), 1);
        assert_int_equal(output_has_text("MMIO space dump"), 0);
}

/* ======== dump_rmid_regs ======== */

static void
test_dump_rmid_regs_requires_a_domain(void **state)
{
        UNUSED_ARG(state);

        run_void_function(dump_rmid_regs, &test_sys);

        assert_int_equal(output_exit_was_called(), 1);
        assert_int_equal(output_get_exit_status(), EXIT_FAILURE);
        assert_int_equal(
            output_has_text("Missing --dump-rmid-domain-ids option"), 1);
}

static void
test_dump_rmid_regs_requires_rmids(void **state)
{
        UNUSED_ARG(state);

        selfn_dump_rmid_domain_ids("0");

        run_void_function(dump_rmid_regs, &test_sys);

        assert_int_equal(output_exit_was_called(), 1);
        assert_int_equal(output_get_exit_status(), EXIT_FAILURE);
        assert_int_equal(output_has_text("Missing --dump-rmids option"), 1);
}

static void
test_dump_rmid_regs_requires_a_type(void **state)
{
        UNUSED_ARG(state);

        /* a complete selection but for --dump-rmid-type, which used to dump the
         * MBM registers and report success, because the type selector defaults
         * to zero
         */
        selfn_dump_rmid_domain_ids("0");
        selfn_dump_rmids("0");

        run_void_function(dump_rmid_regs, &test_sys);

        assert_int_equal(output_exit_was_called(), 1);
        assert_int_equal(output_get_exit_status(), EXIT_FAILURE);
        assert_int_equal(output_has_text("Missing --dump-rmid-type option"), 1);
        assert_int_equal(output_has_text("RMID MBM DUMP"), 0);
}

int
main(void)
{
        /* the selections are static in the modules under test and are only
         * ever added to, so the cases run from the emptiest command line to
         * the most complete one
         */
        const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_dump_mmio_regs_requires_a_domain),
            cmocka_unit_test(test_dump_mmio_regs_requires_a_space),
            cmocka_unit_test(test_dump_rmid_regs_requires_a_domain),
            cmocka_unit_test(test_dump_rmid_regs_requires_rmids),
            cmocka_unit_test(test_dump_rmid_regs_requires_a_type)};

        return cmocka_run_group_tests(tests, NULL, NULL);
}
