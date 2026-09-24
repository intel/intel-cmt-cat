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
/** how deeply logging is suspended on this thread, which is thread local
 *  because it describes a call and not the log
 */
static __thread unsigned m_suspended = 0;

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

        pthread_mutex_unlock(&m_log_mutex);

        return ret;
}

int
log_fini(void)
{
        int ret;

        if (pthread_mutex_lock(&m_log_mutex) != 0)
                return LOG_RETVAL_ERROR;

        ret = log_fini_unlocked();

        pthread_mutex_unlock(&m_log_mutex);

        return ret;
}

void
log_suspend(void)
{
        m_suspended++;
}

void
log_resume(void)
{
        if (m_suspended > 0)
                m_suspended--;
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

        /* a caller that has suspended logging on this thread wants no message
         * written and no assertion about a log it is not using - see
         * log_suspend()
         */
        if (m_suspended > 0)
                return;

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

        /* the descriptor first: the callback belongs to the application, and an
         * application that closes its log descriptor from inside it - having
         * just finalized the library, say - would leave this write addressed to
         * a descriptor that is closed, or worse, reused
         */
        if (fd >= 0) {
                if (write(fd, ap_buffer, size) < 0)
                        fprintf(stderr, "%s: printing to file failed\n",
                                __func__);
        }

        /* and the callback outside the lock, so that what it does with the
         * message - including calling back into this library - is not for the
         * log to serialize
         */
        if (callback != NULL)
                callback(context, size, ap_buffer);
}
