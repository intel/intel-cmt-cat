/*
 * BSD LICENSE
 *
 * Copyright(c) 2014-2022 Intel Corporation. All rights reserved.
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

#ifndef __PQOS_INTERNAL_H__
#define __PQOS_INTERNAL_H__

#include "pqos.h"

#ifdef PQOS_RMID_CUSTOM
/**
 * RMID initialization types
 */
enum pqos_rmid_type {
        PQOS_RMID_TYPE_DEFAULT = 0, /**< Default sequential init */
        PQOS_RMID_TYPE_MAP          /**< Custom Core to RMID mapping */
};
#endif /* PQOS_RMID_CUSTOM */

struct pqos_mon_options {
#ifdef PQOS_RMID_CUSTOM
        struct {
                enum pqos_rmid_type type;
                pqos_rmid_t rmid; /**< custom rmid value*/
        } rmid;
#else
        int reserved;
#endif /* PQOS_RMID_CUSTOM */
};

/**
 * @brief Starts resource monitoring on selected group of channels
 *
 * The function sets up content of the \a group structure.
 *
 * @param [in] num_cores number of cores in \a cores array
 * @param [in] cores array of logical core id's
 * @param [in] event combination of monitoring events
 * @param [in] context a pointer for application's convenience
 *            (unused by the library)
 * @param [out] group a pointer to monitoring structure
 * @param [in] opt extended options
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 */
#ifndef PQOS_RMID_CUSTOM
__attribute__((visibility("hidden")))
#endif /* PQOS_RMID_CUSTOM */
int
pqos_mon_start_cores_ext(const unsigned num_cores,
                         const unsigned *cores,
                         const enum pqos_mon_event event,
                         void *context,
                         struct pqos_mon_data **group,
                         const struct pqos_mon_options *opt);

/**
 * @brief Starts resource monitoring on selected group of channels
 *
 * The function sets up content of the \a group structure.
 *
 * @param [in] num_channels number of channels in \a channels array
 * @param [in] channels array of channel id's
 * @param [in] event combination of monitoring events
 * @param [in] context a pointer for application's convenience
 *            (unused by the library)
 * @param [out] group a pointer to monitoring structure
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 */
#ifndef PQOS_RMID_CUSTOM
__attribute__((visibility("hidden")))
#endif /* PQOS_RMID_CUSTOM */
int
pqos_mon_start_channels_ext(const unsigned num_channels,
                            const pqos_channel_t *channels,
                            const enum pqos_mon_event event,
                            void *context,
                            struct pqos_mon_data **group,
                            const struct pqos_mon_options *opt);

#endif /* __PQOS_INTERNAL_H__ */
