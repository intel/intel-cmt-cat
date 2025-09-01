/*
 * BSD LICENSE
 *
 * Copyright(c) 2014-2025 Intel Corporation. All rights reserved.
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
 *
 */

#include "mmio_common.h"

#include "cap.h"
#include "erdt.h"
#include "log.h"
#include "mmio.h"

int
mmio_set_mbm_mba_mode(enum pqos_mbm_mba_modes mode,
                      const struct pqos_erdt_info *erdt)
{
        int ret = PQOS_RETVAL_OK;

        ASSERT(erdt != NULL);

        for (unsigned domain = 0; domain < erdt->num_cpu_agents; domain++) {
                ret = set_mbm_mba_mode_v1(
                    (const struct pqos_erdt_rmdd *)&erdt->cpu_agents[domain]
                        .rmdd,
                    mode);
                if (ret != PQOS_RETVAL_OK) {
                        LOG_ERROR("Unable to set MBM/MBA mode for "
                                  "Domain ID %d!\n",
                                  erdt->cpu_agents[domain].rmdd.domain_id);
                        break;
                }
        }

        return ret;
}
