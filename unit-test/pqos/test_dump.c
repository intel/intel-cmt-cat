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
#include "test_dump.h"

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
 *
 * They run after pqos_init(), so a refusal is a return: an exit would leave
 * the library initialised, with no pqos_fini() to remove its lock file. Each
 * case asserts the status and that the process was not ended.
 */
static struct pqos_erdt_info test_erdt;
static struct pqos_sysconfig test_sys = {.erdt = &test_erdt};

/* malloc() fails at its m_fail_malloc'th call, counting from 1, and 0 leaves
 * it alone. The dumps allocate their domain and RMID tables once the options
 * are checked, so a case with a complete selection can fail each of them.
 */
static int m_fail_malloc;

void *
__wrap_malloc(size_t size)
{
        if (m_fail_malloc > 0 && --m_fail_malloc == 0)
                return NULL;
        return __real_malloc(size);
}

/* ======== dump_mmio_regs ======== */

static void
test_dump_mmio_regs_requires_a_domain(void **state)
{
        int ret = PQOS_RETVAL_OK;

        UNUSED_ARG(state);

        run_function(dump_mmio_regs, ret, &test_sys);

        assert_int_equal(output_exit_was_called(), 0);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
        assert_int_equal(output_has_text("Provide either --socket or "
                                         "--dump-domain-id"),
                         1);
}

static void
test_dump_mmio_regs_requires_a_space(void **state)
{
        int ret = PQOS_RETVAL_OK;

        UNUSED_ARG(state);

        /* a complete selection but for --space, which used to dump CMRC and
         * report success, because the space selector defaults to zero. A
         * socket rather than a domain id, which would take its place in the
         * case after this one
         */
        selfn_dump_socket("1");

        run_function(dump_mmio_regs, ret, &test_sys);

        assert_int_equal(output_exit_was_called(), 0);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
        assert_int_equal(output_has_text("Provide --space"), 1);
        assert_int_equal(output_has_text("MMIO space dump"), 0);
}

static void
test_dump_mmio_regs_refuses_a_socket_without_agents(void **state)
{
        int ret = PQOS_RETVAL_OK;

        UNUSED_ARG(state);

        /* the socket the case above selected, and a space now: the ERDT here
         * lists no CPU agents, so no socket has the domains a CMRC dump reads
         */
        selfn_dump_space("cmrc");

        run_function(dump_mmio_regs, ret, &test_sys);

        assert_int_equal(output_exit_was_called(), 0);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
        assert_int_equal(
            output_has_text("Mismatch between socket and CPU Agents"), 1);
        assert_int_equal(output_has_text("Unable to get domain ids for socket"),
                         1);
}

static void
test_dump_mmio_regs_without_memory_for_socket_domains_returns(void **state)
{
        int ret = PQOS_RETVAL_OK;

        UNUSED_ARG(state);

        /* the socket and the space the cases above selected: the table of
         * the socket's domains is the allocation that fails, before the
         * ERDT is read for them
         */
        m_fail_malloc = 1;
        run_function(dump_mmio_regs, ret, &test_sys);
        m_fail_malloc = 0;

        assert_int_equal(output_exit_was_called(), 0);
        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);
        assert_int_equal(
            output_has_text("Error with memory allocation for domain_ids!"), 1);
        assert_int_equal(output_has_text("Unable to get domain ids for socket"),
                         0);
}

static void
test_dump_mmio_regs_without_memory_for_given_domains_returns(void **state)
{
        int ret = PQOS_RETVAL_OK;

        UNUSED_ARG(state);

        /* a domain id now, which takes the socket's place: the table of the
         * ids given is the allocation that fails
         */
        selfn_dump_domain_id("0");

        m_fail_malloc = 1;
        run_function(dump_mmio_regs, ret, &test_sys);
        m_fail_malloc = 0;

        assert_int_equal(output_exit_was_called(), 0);
        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);
        assert_int_equal(output_has_text("--socket is ignored"), 1);
        assert_int_equal(
            output_has_text("Error with memory allocation for domain_ids!"), 1);
}

static void
test_dump_mmio_regs_without_erdt_returns(void **state)
{
        int ret = PQOS_RETVAL_OK;
        struct pqos_sysconfig no_erdt = {.erdt = NULL};

        UNUSED_ARG(state);

        run_function(dump_mmio_regs, ret, &no_erdt);

        assert_int_equal(output_exit_was_called(), 0);
        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);
        assert_int_equal(output_has_text("ERDT info not available!"), 1);
}

/* ======== dump_rmid_regs ======== */

static void
test_dump_rmid_regs_requires_a_domain(void **state)
{
        int ret = PQOS_RETVAL_OK;

        UNUSED_ARG(state);

        run_function(dump_rmid_regs, ret, &test_sys);

        assert_int_equal(output_exit_was_called(), 0);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
        assert_int_equal(
            output_has_text("Missing --dump-rmid-domain-ids option"), 1);
}

static void
test_dump_rmid_regs_requires_rmids(void **state)
{
        int ret = PQOS_RETVAL_OK;

        UNUSED_ARG(state);

        selfn_dump_rmid_domain_ids("0");

        run_function(dump_rmid_regs, ret, &test_sys);

        assert_int_equal(output_exit_was_called(), 0);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
        assert_int_equal(output_has_text("Missing --dump-rmids option"), 1);
}

static void
test_dump_rmid_regs_requires_a_type(void **state)
{
        int ret = PQOS_RETVAL_OK;

        UNUSED_ARG(state);

        /* a complete selection but for --dump-rmid-type, which used to dump the
         * MBM registers and report success, because the type selector defaults
         * to zero
         */
        selfn_dump_rmid_domain_ids("0");
        selfn_dump_rmids("0");

        run_function(dump_rmid_regs, ret, &test_sys);

        assert_int_equal(output_exit_was_called(), 0);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
        assert_int_equal(output_has_text("Missing --dump-rmid-type option"), 1);
        assert_int_equal(output_has_text("RMID MBM DUMP"), 0);
}

static void
test_dump_rmid_regs_without_memory_returns(void **state)
{
        static const struct {
                int fail;
                const char *said;
        } tables[] = {{1, "Error with memory allocation for domain_ids!"},
                      {2, "Error with memory allocation for rmids!"}};
        unsigned i;

        UNUSED_ARG(state);

        /* a complete selection now, of a type that reads no memory regions:
         * the first allocation the dump makes is the domain table, and the
         * second the RMID table
         */
        selfn_dump_rmid_type("l3");

        for (i = 0; i < DIM(tables); i++) {
                int ret = PQOS_RETVAL_OK;

                m_fail_malloc = tables[i].fail;
                run_function(dump_rmid_regs, ret, &test_sys);
                m_fail_malloc = 0;

                assert_int_equal(output_exit_was_called(), 0);
                assert_int_equal(ret, PQOS_RETVAL_RESOURCE);
                assert_int_equal(output_has_text(tables[i].said), 1);
        }
}

static void
test_dump_rmid_regs_without_erdt_returns(void **state)
{
        int ret = PQOS_RETVAL_OK;
        struct pqos_sysconfig no_erdt = {.erdt = NULL};

        UNUSED_ARG(state);

        run_function(dump_rmid_regs, ret, &no_erdt);

        assert_int_equal(output_exit_was_called(), 0);
        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);
        assert_int_equal(output_has_text("ERDT info not available!"), 1);
}

int
main(void)
{
        /* the selections are static in the modules under test and are only
         * ever added to, so the cases run from the emptiest command line to
         * the most complete one
         */
        const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_dump_mmio_regs_without_erdt_returns),
            cmocka_unit_test(test_dump_mmio_regs_requires_a_domain),
            cmocka_unit_test(test_dump_mmio_regs_requires_a_space),
            cmocka_unit_test(
                test_dump_mmio_regs_refuses_a_socket_without_agents),
            cmocka_unit_test(
                test_dump_mmio_regs_without_memory_for_socket_domains_returns),
            cmocka_unit_test(
                test_dump_mmio_regs_without_memory_for_given_domains_returns),
            cmocka_unit_test(test_dump_rmid_regs_without_erdt_returns),
            cmocka_unit_test(test_dump_rmid_regs_requires_a_domain),
            cmocka_unit_test(test_dump_rmid_regs_requires_rmids),
            cmocka_unit_test(test_dump_rmid_regs_requires_a_type),
            cmocka_unit_test(test_dump_rmid_regs_without_memory_returns)};

        return cmocka_run_group_tests(tests, NULL, NULL);
}
