/*
 * BSD LICENSE
 *
 * Copyright(c) 2023-2026 Intel Corporation. All rights reserved.
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

#include "mock_iordt.h"

#include "mock_test.h"
#include "pqos.h"

int
__wrap_iordt_init(const struct pqos_cap *cap, struct pqos_devinfo **devinfo)
{
        function_called();
        assert_non_null(cap);
        assert_non_null(devinfo);

        return mock_type(int);
}

int
__wrap_iordt_fini(void)
{
        function_called();

        return mock_type(int);
}

int
__wrap_iordt_alloc_supported(const struct pqos_cap *cap)
{
        function_called();
        check_expected(cap);

        return mock_type(int);
}

int
__wrap_iordt_assoc_write(pqos_channel_t channel, unsigned class_id)
{
        function_called();
        check_expected(channel);
        check_expected(class_id);

        return mock_type(int);
}

int
__wrap_iordt_assoc_read(pqos_channel_t channel, unsigned *class_id)
{
        int ret;

        function_called();
        check_expected(channel);
        check_expected(class_id);
        assert_non_null(class_id);

        ret = mock_type(int);
        if (ret == PQOS_RETVAL_OK)
                *class_id = mock_type(unsigned);

        return ret;
}

int
__wrap_iordt_assoc_reset(const struct pqos_devinfo *dev)
{
        function_called();
        assert_non_null(dev);

        return mock_type(int);
}

int
__wrap_iordt_mon_assoc_write(const pqos_channel_t channel_id,
                             const pqos_rmid_t rmid)
{
        check_expected(channel_id);
        check_expected(rmid);

        return mock_type(int);
}

int
__wrap_iordt_mon_assoc_read(const pqos_channel_t channel_id, pqos_rmid_t *rmid)
{
        int ret;

        check_expected(channel_id);

        ret = mock_type(int);
        if (ret == PQOS_RETVAL_OK) {
                assert_non_null(rmid);
                *rmid = mock_type(unsigned);
        }

        return ret;
}

int
__wrap_iordt_mon_assoc_reset(const struct pqos_devinfo *dev)
{
        function_called();
        assert_non_null(dev);

        return mock_type(int);
}

int
__wrap_iordt_get_numa(const struct pqos_devinfo *devinfo,
                      pqos_channel_t channel_id,
                      unsigned *numa)
{
        int ret;

        assert_non_null(devinfo);
        check_expected(channel_id);

        ret = mock_type(int);
        if (ret == PQOS_RETVAL_OK) {
                assert_non_null(numa);
                *numa = mock_type(unsigned);
        }

        return ret;
}
