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

/**
 * @brief Internal header file to share PQoS API lock mechanism
 * and library initialization status.
 */

#ifndef __PQOS_LOCK_H__
#define __PQOS_LOCK_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "types.h"

#ifndef LOCKFILE
#ifdef __linux__
#define LOCKFILE "/var/lock/libpqos"
#endif
#ifdef __FreeBSD__
#define LOCKFILE "/var/tmp/libpqos.lockfile"
#endif
#endif /*!LOCKFILE*/

/**
 * @brief Initializes API locks
 *
 * @return Operation status
 * @retval 0 success
 * @retval -1 error
 */
PQOS_LOCAL int lock_init(void);

/**
 * @brief Uninitializes API locks
 *
 * @return Operation status
 * @retval 0 success
 * @retval -1 error
 */
PQOS_LOCAL int lock_fini(void);

/**
 * @brief Acquires lock for PQoS API use
 *
 * Only one thread at a time is allowed to use the API.
 * Each PQoS API need to use api_lock and api_unlock functions.
 */
PQOS_LOCAL void lock_get(void);

/**
 * @brief Symmetric operation to \a lock_get to release the lock
 */
PQOS_LOCAL void lock_release(void);

#ifdef __cplusplus
}
#endif

#endif /* __PQOS_LOCK_H__ */
