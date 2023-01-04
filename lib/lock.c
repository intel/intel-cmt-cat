/*
 * BSD LICENSE
 *
 * Copyright(c) 2022-2023 Intel Corporation. All rights reserved.
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
 * @brief Implementation of PQoS API lock
 *
 * provide functions for safe access to PQoS API - this is required for
 * allocation and monitoring modules which also implement PQoS API
 */

#include "lock.h"

#include "log.h"

#include <fcntl.h> /* O_CREAT */
#include <pthread.h>
#include <sys/stat.h> /* S_Ixxx */
#include <unistd.h>   /* usleep(), lockf() */

/**
 * ---------------------------------------
 * Local data structures
 * ---------------------------------------
 */

/**
 * API thread/process safe access is secured through these locks.
 */
static int m_apilock = -1;
static pthread_mutex_t m_apilock_mutex;

int
lock_init(void)
{

        const char *lock_filename = LOCKFILE;

        if (m_apilock != -1)
                return -1;

        m_apilock = open(lock_filename, O_WRONLY | O_CREAT,
                         S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        if (m_apilock == -1)
                return -1;

        if (pthread_mutex_init(&m_apilock_mutex, NULL) != 0) {
                close(m_apilock);
                m_apilock = -1;
                return -1;
        }

        return 0;
}

int
lock_fini(void)
{
        int ret = 0;

        if (close(m_apilock) != 0)
                ret = -1;

        if (pthread_mutex_destroy(&m_apilock_mutex) != 0)
                ret = -1;

        m_apilock = -1;

        return ret;
}

void
lock_get(void)
{
        int err = 0;

        if (lockf(m_apilock, F_LOCK, 0) != 0)
                err = 1;

        if (pthread_mutex_lock(&m_apilock_mutex) != 0)
                err = 1;

        if (err)
                LOG_ERROR("API lock error!\n");
}

void
lock_release(void)
{
        int err = 0;

        if (lockf(m_apilock, F_ULOCK, 0) != 0)
                err = 1;

        if (pthread_mutex_unlock(&m_apilock_mutex) != 0)
                err = 1;

        if (err)
                LOG_ERROR("API unlock error!\n");
}
