/*
 * BSD LICENSE
 *
 * Copyright(c) 2014-2015 Intel Corporation. All rights reserved.
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
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.O
 *
 */

/**
 * Allocation profiles API
 */

#include <stdint.h>
#include <stdio.h>
#include "pqos.h"

#ifndef __PROFILES_H__
#define __PROFILES_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Prints list of supported L3CA profiles to STDOUT
 *
 * @param [in] fp file stream to write profile descriptions to
 */
void profile_l3ca_list(FILE *fp);

/**
 * @brief Retrieves selected L3CA profile by its \a id
 *
 * @param [in] id profile identity (string)
 * @param [in] l3ca L3CA capability structure
 * @param [out] p_num number of L3CA classes of service retrieved for the profile
 * @param [out] p_tab pointer to definition of L3CA classes of service
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 */
int profile_l3ca_get(const char *id, const struct pqos_cap_l3ca *l3ca,
                     unsigned *p_num, const char * const **p_tab);


#ifdef __cplusplus
}
#endif

#endif /* __PROFILES_H__ */
