/*
 * BSD LICENSE
 *
 * Copyright(c) 2014-2026 Intel Corporation. All rights reserved.
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
 * @brief Library operations logger for info, warnings and errors.
 */

#include "log.h"

#include "types.h"

#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/**
 * ---------------------------------------
 * Local data structures
 * ---------------------------------------
 */

static int m_opt = 0;              /**< log options */
static int m_fd = -1;              /**< log file descriptor */
static void *m_context_log = NULL; /**< log callback context */
/**
 *  log callback
 */
static void (*m_callback_log)(void *, const size_t, const char *);
static int log_init_successful = 0; /**< log init gatekeeper */

/**
 * The log is one object per process, and the library's initialization is not
 * the only thing that installs it: a read-only path may be called before
 * pqos_init(), with no log of its own, and still has to report what it found.
 * All of the state above is therefore guarded by this mutex, so that an
 * install, a teardown and a message cannot see each other half done.
 */
static pthread_mutex_t m_log_mutex = PTHREAD_MUTEX_INITIALIZER;
/** signalled when the last holder leaves, for a log_fini() waiting on it */
static pthread_cond_t m_log_released = PTHREAD_COND_INITIALIZER;
/** how many pre-initialization readers are logging through it right now */
static unsigned m_holders = 0;
/** the log currently installed is the silent one a holder brought up */
static int m_holder_installed = 0;

/**
 * ---------------------------------------
 * Local functions
 * ---------------------------------------
 */

/**
 * =======================================
 * initialize and shutdown
 * =======================================
 */

/**
 * @brief Installs the log, with \a m_log_mutex already held
 *
 * @param [in] fd_log file descriptor to write to, or -1 for none
 * @param [in] callback_log callback to hand messages to, or NULL for none
 * @param [in] context_log context passed back to \a callback_log
 * @param [in] verbosity one of the LOG_VER_* levels
 *
 * @return Operation status
 * @retval LOG_RETVAL_OK success
 */
static int
log_init_unlocked(int fd_log,
                  void (*callback_log)(void *, const size_t, const char *),
                  void *context_log,
                  int verbosity)
{
        /**
         * Set log message verbosity
         */
        switch (verbosity) {
        case LOG_VER_SILENT:
                m_opt = LOG_OPT_SILENT;
                log_init_successful = 1;
                return LOG_RETVAL_OK;
        case LOG_VER_DEFAULT:
                m_opt = LOG_OPT_DEFAULT;
                break;
        case LOG_VER_VERBOSE:
                m_opt = LOG_OPT_VERBOSE;
                break;
        case LOG_VER_SUPER_VERBOSE:
                m_opt = LOG_OPT_SUPER_VERBOSE;
                break;
        default:
                m_opt = LOG_OPT_SUPER_VERBOSE;
                break;
        }

        if (fd_log < 0 && callback_log == NULL) {
                fprintf(stderr, "%s: no LOG destination selected\n", __func__);
                return LOG_RETVAL_ERROR;
        }

        m_fd = fd_log;
        m_callback_log = callback_log;
        m_context_log = context_log;
        log_init_successful = 1;

        return LOG_RETVAL_OK;
}

/**
 * @brief Takes the log down, with \a m_log_mutex already held
 *
 * @return Operation status
 * @retval LOG_RETVAL_OK success
 */
static int
log_fini_unlocked(void)
{
        if (m_opt == LOG_OPT_SILENT) {
                log_init_successful = 0;
                return LOG_RETVAL_OK;
        }

        m_opt = 0;
        m_fd = -1;
        m_callback_log = NULL;
        m_context_log = NULL;
        log_init_successful = 0;

        return LOG_RETVAL_OK;
}

int
log_init(int fd_log,
         void (*callback_log)(void *, const size_t, const char *),
         void *context_log,
         int verbosity)
{
        int ret;

        if (pthread_mutex_lock(&m_log_mutex) != 0)
                return LOG_RETVAL_ERROR;

        ret = log_init_unlocked(fd_log, callback_log, context_log, verbosity);
        /* whatever a holder may have installed, this log is the caller's now:
         * releasing the hold must leave it alone
         */
        if (ret == LOG_RETVAL_OK)
                m_holder_installed = 0;

        pthread_mutex_unlock(&m_log_mutex);

        return ret;
}

int
log_fini(void)
{
        int ret = LOG_RETVAL_OK;

        if (pthread_mutex_lock(&m_log_mutex) != 0)
                return LOG_RETVAL_ERROR;

        /* a pre-initialization reader is still reporting through this log, so
         * wait for it. Returning while it goes on logging would be worse than
         * the message it would lose: the caller is finalizing the library and
         * is free to close the descriptor and release the context this log is
         * addressed to as soon as this returns, and those are the application's
         * own. The wait is bounded by that read, which opens nothing and holds
         * no other lock
         */
        while (m_holders > 0)
                pthread_cond_wait(&m_log_released, &m_log_mutex);

        ret = log_fini_unlocked();

        pthread_mutex_unlock(&m_log_mutex);

        return ret;
}

int
log_hold(void)
{
        int ret = LOG_RETVAL_OK;

        if (pthread_mutex_lock(&m_log_mutex) != 0)
                return LOG_RETVAL_ERROR;

        if (log_init_successful == 0) {
                ret = log_init_unlocked(-1, NULL, NULL, LOG_VER_SILENT);
                if (ret == LOG_RETVAL_OK)
                        m_holder_installed = 1;
        }
        if (ret == LOG_RETVAL_OK)
                m_holders++;

        pthread_mutex_unlock(&m_log_mutex);

        return ret;
}

int
log_release(void)
{
        int ret = LOG_RETVAL_OK;

        if (pthread_mutex_lock(&m_log_mutex) != 0)
                return LOG_RETVAL_ERROR;

        if (m_holders > 0)
                m_holders--;

        /* the silent log this path brought up is its own to remove; a log
         * installed by the library's initialization outlives the hold. Either
         * way a log_fini() may be waiting for this moment
         */
        if (m_holders == 0) {
                if (m_holder_installed) {
                        ret = log_fini_unlocked();
                        m_holder_installed = 0;
                }
                pthread_cond_broadcast(&m_log_released);
        }

        pthread_mutex_unlock(&m_log_mutex);

        return ret;
}

int
log_is_initialized(void)
{
        int initialized;

        /* under the lock like every other read of this state: the answer is
         * used to decide whether log_printf() may be called, and an install or
         * a teardown in another thread must be wholly before this answer or
         * wholly after it
         */
        if (pthread_mutex_lock(&m_log_mutex) != 0)
                return 0;
        initialized = log_init_successful == 1;
        pthread_mutex_unlock(&m_log_mutex);

        return initialized;
}

void
log_printf(int type, const char *str, ...)
{
        va_list ap;
        char ap_buffer[AP_BUFFER_SIZE];
        int size;
        int initialized;
        int opt;
        int fd;
        void *context;
        void (*callback)(void *cb_context, const size_t cb_size,
                         const char *cb_message);

        /* the destination is read once, under the lock, so that a message
         * cannot be written half to one log and half to another - and so that
         * an install or a teardown running in another thread is either wholly
         * before this message or wholly after it. The assertion below reads the
         * same snapshot, for the same reason: it is there to catch a message
         * logged before initialization, not to fire on a log that was taken
         * down after this message was already on its way
         */
        if (pthread_mutex_lock(&m_log_mutex) != 0)
                return;
        initialized = log_init_successful;
        opt = m_opt;
        fd = m_fd;
        callback = m_callback_log;
        context = m_context_log;
        pthread_mutex_unlock(&m_log_mutex);

        /* If log_init has not been successful then
         * log_printf should not work. */
        ASSERT(initialized == 1);
        if (initialized == 0)
                return;

        if (opt == LOG_OPT_SILENT)
                return;

        if ((opt & type) == 0)
                return;

        ASSERT(str != NULL);
        if (str == NULL)
                return;

        va_start(ap, str);
        ap_buffer[AP_BUFFER_SIZE - 1] = '\0';
        size = vsnprintf(ap_buffer, AP_BUFFER_SIZE - 1, str, ap);
        va_end(ap);
        ASSERT(size >= 0);
        if (size < 0)
                return;

        /**
         * vsnprintf() returns the length the message needed, not the length it
         * wrote: given AP_BUFFER_SIZE - 1 it writes at most AP_BUFFER_SIZE - 2
         * characters and a NUL. Reporting the needed length would send the
         * callback and the write() below past the end of ap_buffer, and would
         * describe the truncated text by the size of the message it came from.
         */
        if (size > AP_BUFFER_SIZE - 2)
                size = AP_BUFFER_SIZE - 2;

        /* outside the lock: the callback belongs to the application, and what
         * it does with the message - including calling back into this library -
         * is not for the log to serialize
         */
        if (callback != NULL)
                callback(context, size, ap_buffer);

        if (fd >= 0) {
                if (write(fd, ap_buffer, size) < 0)
                        fprintf(stderr, "%s: printing to file failed\n",
                                __func__);
        }
}
