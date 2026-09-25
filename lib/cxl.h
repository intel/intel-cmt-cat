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
 *
 */

/**
 * @brief Internal header for the CXL devices the operating system enumerates
 */

#ifndef __PQOS_CXL_H__
#define __PQOS_CXL_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "pqos.h"
#include "types.h"

/**
 * @brief Reads the CXL memory devices the operating system has mapped
 *
 * ACPI enumerates address ranges and windows, never devices, so which device is
 * behind a memory range is a question only the operating system can answer.
 * Linux answers it in /sys/bus/cxl: a committed CXL region carries the address
 * range it maps and names the endpoint decoder behind it, and that decoder's
 * endpoint names the memory device and the PCI function it sits on.
 *
 * Only devices with a mapped region are reported, because those are the only
 * ones an address can be behind: a device the platform has not mapped covers no
 * address, so no memory range can name it. The devices reported carry the
 * address range of their region, which is what makes them comparable to the
 * ranges MRRM describes.
 *
 * Capacities the devices declare are deliberately not among the fields. On the
 * one platform available a device reports half the capacity of the region it is
 * the only target of, and until that is understood a report carrying both
 * numbers would invite a comparison nobody can explain.
 *
 * @param [out] available whether the operating system has a CXL bus to ask -
 *              set on every return, because "no devices" and "no bus to ask"
 *              are different answers and the report states which it is
 * @param [out] num_devices how many devices were read, zero where the bus
 *              carries none
 * @param [out] devices the devices, allocated here and released by the caller
 *              with cxl_devices_free(). NULL where there are none
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK the bus was read, or there was none to read
 * @retval PQOS_RETVAL_RESOURCE out of memory
 * @retval PQOS_RETVAL_PARAM a parameter was NULL
 */
PQOS_LOCAL int cxl_devices_read(int *available,
                                unsigned *num_devices,
                                struct pqos_cxl_device **devices);

/**
 * @brief Releases what cxl_devices_read() allocated
 *
 * @param [in] devices the devices to release, NULL being nothing to do
 */
PQOS_LOCAL void cxl_devices_free(struct pqos_cxl_device *devices);

#ifdef __cplusplus
}
#endif

#endif /* __PQOS_CXL_H__ */
