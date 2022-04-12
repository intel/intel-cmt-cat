/*
 * BSD LICENSE
 *
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
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

#include "uncore_monitoring.h"

#include "machine.h"

#define CPU_MODEL_SKX 0x55

/**
 * @brief Detect cpu model
 *
 * @return detected cpu model
 */
static uint32_t
_get_cpu_model(void)
{
        uint32_t model;
        struct cpuid_out out;

        lcpuid(1, 0, &out);

        /* Read CPU model */
        model = (out.eax & 0xf0) >> 4;
        /* Read CPU extended model */
        model |= (out.eax & 0xf0000) >> 12;

        return model;
}

/**
 * @brief Discover uncore monitoring events
 *
 * @param [out] event Listo of supported events
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK success
 */
int
uncore_mon_discover(enum pqos_mon_event *event)
{
        uint32_t model = _get_cpu_model();

        if (model == CPU_MODEL_SKX)
                *event =
                    (enum pqos_mon_event)(PQOS_PERF_EVENT_LLC_MISS_PCIE_READ |
                                          PQOS_PERF_EVENT_LLC_MISS_PCIE_WRITE |
                                          PQOS_PERF_EVENT_LLC_REF_PCIE_READ |
                                          PQOS_PERF_EVENT_LLC_REF_PCIE_WRITE);
        else
                *event = (enum pqos_mon_event)0;

        return PQOS_RETVAL_OK;
}
