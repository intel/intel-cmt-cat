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

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <string.h>

#include "mock_resctrl_schemata.h"

int
__wrap_resctrl_schemata_read(FILE *fd, struct resctrl_schemata *schemata)
{
        assert_non_null(fd);
        assert_non_null(schemata);

        return mock_type(int);
}

int
__wrap_resctrl_schemata_l3ca_write(FILE *fd,
                                   const struct resctrl_schemata *schemata)
{
        assert_non_null(fd);
        assert_non_null(schemata);

        return mock_type(int);
}

int
__wrap_resctrl_schemata_l2ca_write(FILE *fd,
                                   const struct resctrl_schemata *schemata)
{
        assert_non_null(fd);
        assert_non_null(schemata);

        return mock_type(int);
}

int
__wrap_resctrl_schemata_mba_write(FILE *fd,
                                  const struct resctrl_schemata *schemata)
{
        assert_non_null(fd);
        assert_non_null(schemata);

        return mock_type(int);
}

int
__wrap_resctrl_schemata_l2ca_set(struct resctrl_schemata *schemata,
                                 unsigned resource_id,
                                 const struct pqos_l2ca *ca)
{
        assert_non_null(schemata);
        assert_non_null(ca);
        check_expected(resource_id);

        return mock_type(int);
}

int
__wrap_resctrl_schemata_l2ca_get(const struct resctrl_schemata *schemata,
                                 unsigned resource_id,
                                 struct pqos_l2ca *ca)
{
        assert_non_null(schemata);
        assert_non_null(ca);
        check_expected(resource_id);

        return mock_type(int);
}

int
__wrap_resctrl_schemata_l3ca_set(struct resctrl_schemata *schemata,
                                 unsigned resource_id,
                                 const struct pqos_l3ca *ca)
{
        assert_non_null(schemata);
        assert_non_null(ca);
        check_expected(resource_id);

        return mock_type(int);
}

int
__wrap_resctrl_schemata_l3ca_get(const struct resctrl_schemata *schemata,
                                 unsigned resource_id,
                                 struct pqos_l3ca *ca)
{
        assert_non_null(schemata);
        assert_non_null(ca);
        check_expected(resource_id);

        return mock_type(int);
}

int
__wrap_resctrl_schemata_mba_set(struct resctrl_schemata *schemata,
                                unsigned resource_id,
                                const struct pqos_mba *ca)
{
        assert_non_null(schemata);
        assert_non_null(ca);
        check_expected(resource_id);

        return mock_type(int);
}

int
__wrap_resctrl_schemata_mba_get(const struct resctrl_schemata *schemata,
                                unsigned resource_id,
                                struct pqos_mba *ca)
{
        assert_non_null(schemata);
        assert_non_null(ca);
        check_expected(resource_id);

        return mock_type(int);
}