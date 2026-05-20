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
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
/* clang-format off */
#include <cmocka.h>
/* clang-format on */

#include "main.h"
#include "pqos.h"

/*
 * Tests for the pure interface-selection helpers iface_narrow() and
 * iface_resolve() used by pqos to auto-detect --iface from the supplied
 * command-line options.
 */

/* ---- iface_narrow() ---------------------------------------------------- */

static void
test_narrow_any_is_noop(void **state)
{
        (void)state;
        assert_int_equal(iface_narrow(IFACE_ANY, IFACE_ANY), IFACE_ANY);
        assert_int_equal(iface_narrow(IFACE_MMIO, IFACE_ANY), IFACE_MMIO);
        assert_int_equal(iface_narrow(IFACE_MSR | IFACE_OS, IFACE_ANY),
                         IFACE_MSR | IFACE_OS);
}

static void
test_narrow_zero_is_noop(void **state)
{
        (void)state;
        /* Adding mask 0 should not change the current mask. */
        assert_int_equal(iface_narrow(IFACE_ANY, 0), IFACE_ANY);
        assert_int_equal(iface_narrow(IFACE_OS, 0), IFACE_OS);
}

static void
test_narrow_to_mmio_only(void **state)
{
        (void)state;
        assert_int_equal(iface_narrow(IFACE_ANY, IFACE_MMIO), IFACE_MMIO);
        assert_int_equal(iface_narrow(IFACE_MSR | IFACE_MMIO, IFACE_MMIO),
                         IFACE_MMIO);
}

static void
test_narrow_to_msr_or_os(void **state)
{
        (void)state;
        assert_int_equal(iface_narrow(IFACE_ANY, IFACE_MSR | IFACE_OS),
                         IFACE_MSR | IFACE_OS);
}

static void
test_narrow_conflict_returns_zero(void **state)
{
        (void)state;
        /* MMIO-only narrowed by OS-only is impossible. */
        assert_int_equal(iface_narrow(IFACE_MMIO, IFACE_OS), 0);
        /* Two disjoint multi-bit masks. */
        assert_int_equal(iface_narrow(IFACE_MSR | IFACE_OS, IFACE_MMIO), 0);
}

static void
test_narrow_repeated(void **state)
{
        (void)state;
        unsigned m = IFACE_ANY;

        m = iface_narrow(m, IFACE_MSR | IFACE_MMIO);
        assert_int_equal(m, IFACE_MSR | IFACE_MMIO);
        m = iface_narrow(m, IFACE_MMIO);
        assert_int_equal(m, IFACE_MMIO);
        m = iface_narrow(m, IFACE_MMIO);
        assert_int_equal(m, IFACE_MMIO);
}

/* ---- iface_resolve() --------------------------------------------------- */

static void
test_resolve_any_keeps_auto(void **state)
{
        enum pqos_interface out = PQOS_INTER_MSR;

        (void)state;
        assert_int_equal(
            iface_resolve(IFACE_ANY, 0, PQOS_INTER_AUTO, &out), 0);
        assert_int_equal(out, PQOS_INTER_AUTO);
}

static void
test_resolve_single_bit(void **state)
{
        enum pqos_interface out = PQOS_INTER_AUTO;

        (void)state;
        assert_int_equal(
            iface_resolve(IFACE_MMIO, 0, PQOS_INTER_AUTO, &out), 0);
        assert_int_equal(out, PQOS_INTER_MMIO);

        assert_int_equal(
            iface_resolve(IFACE_OS, 0, PQOS_INTER_AUTO, &out), 0);
        assert_int_equal(out, PQOS_INTER_OS);

        assert_int_equal(
            iface_resolve(IFACE_MSR, 0, PQOS_INTER_AUTO, &out), 0);
        assert_int_equal(out, PQOS_INTER_MSR);
}

static void
test_resolve_prefers_mmio(void **state)
{
        enum pqos_interface out = PQOS_INTER_AUTO;

        (void)state;
        /* Preference order is MMIO > MSR > OS. */
        assert_int_equal(iface_resolve(IFACE_MSR | IFACE_MMIO, 0,
                                       PQOS_INTER_AUTO, &out),
                         0);
        assert_int_equal(out, PQOS_INTER_MMIO);

        assert_int_equal(iface_resolve(IFACE_MMIO | IFACE_OS, 0,
                                       PQOS_INTER_AUTO, &out),
                         0);
        assert_int_equal(out, PQOS_INTER_MMIO);
}

static void
test_resolve_msr_or_os_picks_msr(void **state)
{
        enum pqos_interface out = PQOS_INTER_AUTO;

        (void)state;
        /* When MMIO is excluded but both MSR and OS remain, MSR wins. */
        assert_int_equal(iface_resolve(IFACE_MSR | IFACE_OS, 0,
                                       PQOS_INTER_AUTO, &out),
                         0);
        assert_int_equal(out, PQOS_INTER_MSR);
}

static void
test_resolve_explicit_user_compatible(void **state)
{
        enum pqos_interface out = PQOS_INTER_AUTO;

        (void)state;
        /* User asks for OS and option mask still contains OS -> use OS. */
        assert_int_equal(
            iface_resolve(IFACE_OS, 1, PQOS_INTER_OS, &out), 0);
        assert_int_equal(out, PQOS_INTER_OS);

        /* User asks for MMIO and constraint allows MMIO/MSR. */
        assert_int_equal(iface_resolve(IFACE_MMIO | IFACE_MSR, 1,
                                       PQOS_INTER_MMIO, &out),
                         0);
        assert_int_equal(out, PQOS_INTER_MMIO);
}

static void
test_resolve_explicit_user_conflict(void **state)
{
        enum pqos_interface out = PQOS_INTER_AUTO;

        (void)state;
        /* User asks for OS but options force MMIO -> conflict. */
        assert_int_equal(
            iface_resolve(IFACE_MMIO, 1, PQOS_INTER_OS, &out), -2);

        /* User asks for MMIO but options force OS (e.g. -p). */
        assert_int_equal(
            iface_resolve(IFACE_OS, 1, PQOS_INTER_MMIO, &out), -2);
}

static void
test_resolve_empty_mask(void **state)
{
        enum pqos_interface out = PQOS_INTER_AUTO;

        (void)state;
        assert_int_equal(iface_resolve(0, 0, PQOS_INTER_AUTO, &out), -1);
}

static void
test_resolve_user_auto_treated_as_no_override(void **state)
{
        enum pqos_interface out = PQOS_INTER_AUTO;

        (void)state;
        /* --iface=auto should NOT be treated as an explicit override:
         * fall through to mask-based selection (MMIO > MSR > OS). */
        assert_int_equal(iface_resolve(IFACE_MSR | IFACE_MMIO, 1,
                                       PQOS_INTER_AUTO, &out),
                         0);
        assert_int_equal(out, PQOS_INTER_MMIO);
}

/* ---- Combined "scenario" tests modelling realistic option sets -------- */

static void
test_scenario_print_mem_regions_alone(void **state)
{
        enum pqos_interface out = PQOS_INTER_AUTO;
        unsigned m = IFACE_ANY;

        (void)state;
        m = iface_narrow(m, IFACE_MMIO); /* --print-mem-regions */
        assert_int_equal(m, IFACE_MMIO);
        assert_int_equal(iface_resolve(m, 0, PQOS_INTER_AUTO, &out), 0);
        assert_int_equal(out, PQOS_INTER_MMIO);
}

static void
test_scenario_pid_only(void **state)
{
        enum pqos_interface out = PQOS_INTER_AUTO;
        unsigned m = IFACE_ANY;

        (void)state;
        m = iface_narrow(m, IFACE_OS); /* -p / --mon-pid */
        assert_int_equal(m, IFACE_OS);
        assert_int_equal(iface_resolve(m, 0, PQOS_INTER_AUTO, &out), 0);
        assert_int_equal(out, PQOS_INTER_OS);
}

static void
test_scenario_mon_channel_picks_mmio(void **state)
{
        enum pqos_interface out = PQOS_INTER_AUTO;
        unsigned m = IFACE_ANY;

        (void)state;
        m = iface_narrow(m, IFACE_MSR | IFACE_MMIO); /* --mon-channel */
        assert_int_equal(iface_resolve(m, 0, PQOS_INTER_AUTO, &out), 0);
        assert_int_equal(out, PQOS_INTER_MMIO); /* MMIO preferred */
}

static void
test_scenario_cdp_reset_picks_msr(void **state)
{
        enum pqos_interface out = PQOS_INTER_AUTO;
        unsigned m = IFACE_ANY;

        (void)state;
        m = iface_narrow(m, IFACE_MSR | IFACE_OS); /* -R cdp-on */
        assert_int_equal(iface_resolve(m, 0, PQOS_INTER_AUTO, &out), 0);
        /* MMIO not in mask, so MSR wins over OS. */
        assert_int_equal(out, PQOS_INTER_MSR);
}

static void
test_scenario_print_io_devs_picks_msr(void **state)
{
        enum pqos_interface out = PQOS_INTER_AUTO;
        unsigned m = IFACE_ANY;

        (void)state;
        m = iface_narrow(m, IFACE_MSR); /* --print-io-devs */
        assert_int_equal(iface_resolve(m, 0, PQOS_INTER_AUTO, &out), 0);
        assert_int_equal(out, PQOS_INTER_MSR);
}

static void
test_scenario_pid_plus_print_mem_regions_conflict(void **state)
{
        unsigned m = IFACE_ANY;

        (void)state;
        m = iface_narrow(m, IFACE_OS); /* -p */
        assert_int_equal(m, IFACE_OS);
        m = iface_narrow(m, IFACE_MMIO); /* --print-mem-regions */
        assert_int_equal(m, 0); /* irreconcilable */
}

static void
test_scenario_uncore_aet_plus_explicit_os_conflict(void **state)
{
        enum pqos_interface out = PQOS_INTER_AUTO;
        unsigned m = IFACE_ANY;

        (void)state;
        m = iface_narrow(m, IFACE_MSR | IFACE_MMIO); /* aet event */
        assert_int_equal(
            iface_resolve(m, 1, PQOS_INTER_OS, &out), -2);
}

static void
test_scenario_no_constraint_keeps_auto(void **state)
{
        enum pqos_interface out = PQOS_INTER_MSR;
        unsigned m = IFACE_ANY;

        (void)state;
        /* No options narrowed the mask: keep AUTO so the library
         * performs its own auto-detection (legacy behaviour). */
        assert_int_equal(iface_resolve(m, 0, PQOS_INTER_AUTO, &out), 0);
        assert_int_equal(out, PQOS_INTER_AUTO);
}

int
main(void)
{
        const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_narrow_any_is_noop),
            cmocka_unit_test(test_narrow_zero_is_noop),
            cmocka_unit_test(test_narrow_to_mmio_only),
            cmocka_unit_test(test_narrow_to_msr_or_os),
            cmocka_unit_test(test_narrow_conflict_returns_zero),
            cmocka_unit_test(test_narrow_repeated),
            cmocka_unit_test(test_resolve_any_keeps_auto),
            cmocka_unit_test(test_resolve_single_bit),
            cmocka_unit_test(test_resolve_prefers_mmio),
            cmocka_unit_test(test_resolve_msr_or_os_picks_msr),
            cmocka_unit_test(test_resolve_explicit_user_compatible),
            cmocka_unit_test(test_resolve_explicit_user_conflict),
            cmocka_unit_test(test_resolve_empty_mask),
            cmocka_unit_test(test_resolve_user_auto_treated_as_no_override),
            cmocka_unit_test(test_scenario_print_mem_regions_alone),
            cmocka_unit_test(test_scenario_pid_only),
            cmocka_unit_test(test_scenario_mon_channel_picks_mmio),
            cmocka_unit_test(test_scenario_cdp_reset_picks_msr),
            cmocka_unit_test(test_scenario_print_io_devs_picks_msr),
            cmocka_unit_test(test_scenario_pid_plus_print_mem_regions_conflict),
            cmocka_unit_test(
                test_scenario_uncore_aet_plus_explicit_os_conflict),
            cmocka_unit_test(test_scenario_no_constraint_keeps_auto),
        };

        return cmocka_run_group_tests(tests, NULL, NULL);
}
