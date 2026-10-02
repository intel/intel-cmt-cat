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
 * @brief Platform QoS operations logger for info, warnings and errors.
 */

#ifndef __PQOS_LOG_H__
#define __PQOS_LOG_H__

#include <stdarg.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "types.h"

#define LOG_VER_SILENT        (-1)
#define LOG_VER_DEFAULT       (0)
#define LOG_VER_VERBOSE       (1)
#define LOG_VER_SUPER_VERBOSE (2)

#define LOG_RETVAL_OK    0 /**< everything OK */
#define LOG_RETVAL_ERROR 1 /**< generic error */

/**
 * The payload buffer of log_printf(). vsnprintf() is given AP_BUFFER_SIZE - 1
 * of it and spends one byte of that on the NUL, so a message reaches the log
 * callback and the log file as at most AP_BUFFER_SIZE - 2 characters.
 */
#define AP_BUFFER_SIZE 320

#define LOG_OPT_INFO  (1 << 0)
#define LOG_OPT_WARN  (1 << 1)
#define LOG_OPT_ERROR (1 << 2)
#define LOG_OPT_DEBUG (1 << 3)

#define LOG_OPT_SILENT  (-1)
#define LOG_OPT_DEFAULT (LOG_OPT_WARN | LOG_OPT_ERROR)
#define LOG_OPT_VERBOSE (LOG_OPT_WARN | LOG_OPT_ERROR | LOG_OPT_INFO)
#define LOG_OPT_SUPER_VERBOSE                                                  \
        (LOG_OPT_WARN | LOG_OPT_ERROR | LOG_OPT_INFO | LOG_OPT_DEBUG)

#define LOG_INFO(str...)  log_printf(LOG_OPT_INFO, "INFO: " str)
#define LOG_WARN(str...)  log_printf(LOG_OPT_WARN, "WARN: " str)
#define LOG_ERROR(str...) log_printf(LOG_OPT_ERROR, "ERROR: " str)
#define LOG_DEBUG(str...) log_printf(LOG_OPT_DEBUG, "DEBUG: " str)

/**
 * @brief Initializes PQoS log module
 * There are five typical use cases for this function
 *  [1] log to file descriptor only
 *  @note log_init(fd_log, NULL, NULL, LOG_VER_DEFAULT);
 *  [2] use callback function to capture logs
 *  @note log_init(-1, custom_callback, NULL, LOG_VER_DEFAULT);
 *  [3] use callback with a custom context
 *  @note log_init(-1, custom_callback, anything, LOG_VER_DEFAULT);
 *  [4] use both a callback and file descriptor
 *  @note log_init(fd_log, custom_callback, NULL, LOG_VER_DEFAULT);
 *  [5] keep all logging silent
 *  @note log_init(-1, NULL, NULL, LOG_VER_SILENT);
 *
 * @param [in] fd_log file descriptor to be used as library log
 * @param [in] callback_log pointer to an application callback function
 *         void *       - An application context - it can point to a structure
 *                        or an object that an application may find useful
 *                        when receiving the callback
 *         const size_t - the size of the log message
 *         const char * - the log message
 * @param [in] context_log application specific data that is provided
 *                    to the callback function. It can be NULL if application
 *                    doesn't require it.
 * @param [in] verbosity logging options
 *         LOG_VER_SILENT         - no messages
 *         LOG_VER_DEFAULT        - warning and error messages
 *         LOG_VER_VERBOSE        - warning, error and info messages
 *         LOG_VER_SUPER_VERBOSE  - warning, error, info and debug messages
 *
 * @return Operation status
 * @retval LOG_RETVAL_OK on success
 */
PQOS_LOCAL int
log_init(int fd_log,
         void (*callback_log)(void *, const size_t, const char *),
         void *context_log,
         int verbosity);

/**
 * @brief Shuts down PQoS log module
 *
 * @return Operation status
 * @retval LOG_RETVAL_OK on success
 */
PQOS_LOCAL int log_fini(void);

/**
 * @brief Drops the messages this thread logs until log_resume()
 *
 * For a path that may run before pqos_init(), or beside it, and must not write
 * to the log either way - pqos_hybrid_discover() is the one. Two things stand
 * in the way of such a path logging at all, and this answers both:
 *
 * - log_printf() asserts that the log has been initialized, and before
 *   pqos_init() it has not, so a DEBUG build would abort on a message this path
 *   had no destination for anyway.
 * - where the log *is* initialized, the destination is the application's, and
 *   this path runs outside pqos_init() and pqos_fini(). Writing to it would
 *   mean either writing to a descriptor a concurrent pqos_fini() has let the
 *   application close, or making that pqos_fini() wait - and it holds the API
 *   lock this path deliberately does not take, so waiting for a path that may
 *   itself call into the library inverts the two locks.
 *
 * Dropping the messages on this thread costs the caller nothing it is promised:
 * what such a call reports is its return status. Nothing global is touched, so
 * there is no lifetime to coordinate and no lock to invert: this is the calling
 * thread's own state, and other threads log as usual throughout.
 *
 * Nested suspensions are counted.
 */
PQOS_LOCAL void log_suspend(void);

/**
 * @brief Symmetric operation to \a log_suspend
 */
PQOS_LOCAL void log_resume(void);

/**
 * @brief Waits for the messages already on their way to a retired destination
 *
 * A message takes the destination - the descriptor, the callback and its
 * context
 * - together with a reference to it, and uses it afterwards, so a teardown
 * alone does not end a message that had already started: the write and the
 * callback are still to come, addressed to state the application is free to
 * reclaim the moment pqos_fini() returns. This is what closes that window.
 *
 * What it waits for is a message using a destination this library has
 * **retired**, and nothing else. A message using the destination installed now
 * - an application logging from a thread of its own, which may never stop - is
 * not waited for, so this does not establish quiescence of logging in general
 * and must not be read as doing so. It establishes that nothing is still using
 * what a teardown took out of service, which is the only part a caller can be
 * harmed by.
 *
 * Call it from pqos_fini() *after* the API lock has been released. Called with
 * that lock held it could wait for an emission whose callback wants the lock,
 * which is why log_fini() itself does not wait.
 *
 * The wait is bounded by a monotonic deadline of one second, so an application
 * cannot be hung here by a callback of its own that never returns. Where it
 * runs out, the messages still in flight keep using the destination they
 * acquired and that destination is freed by the last of them to let it go -
 * there is no reap, and no later install or teardown is involved; that residual
 * window is the documented limit of the guarantee.
 *
 * A caller that is itself inside a log callback does not wait for its **own**
 * emission - that emission cannot finish until this returns, so waiting for it
 * would wait for the caller. Every other reference is waited for as usual,
 * including one another thread holds on the same retired destination, so such a
 * caller is not promised a prompt return: with another thread inside a callback
 * it pays the bound like anybody else.
 *
 * One requirement on the application, which this module cannot check: the log
 * callback must not leave by a non-local jump. A callback that leaves by
 * longjmp(), or by a C++ exception thrown through the C frame, never gives back
 * the reference its message holds - so the destination that message was using
 * is never freed, and every later wait spends the whole bound looking for it.
 * Messages and teardowns after that one are unaffected, because each
 * destination is counted on its own.
 *
 * pthread_exit() and cancellation are **not** in that list, with two
 * qualifications. Both run the registered pthread cleanup handlers, and the
 * emission registers one, so those paths give the reference back like a
 * callback that returns: a thread cancelled inside a log callback, or one that
 * exits from it, costs *this module* nothing.
 *
 * What it costs the rest of the library is another matter, and the public
 * contract in pqos.h is where that is stated: a message is usually written from
 * inside an API call holding the API lock, and no handler gives that lock back.
 *
 * That is cancellation in the deferred mode, which is PTHREAD_CANCEL_DEFERRED,
 * the mode a thread has unless it asks for the other one. A thread with
 * PTHREAD_CANCEL_ASYNCHRONOUS set is not covered and cannot be: asynchronous
 * cancellation can land on any instruction, so it can land inside
 * log_dest_acquire() with the module's mutex held - which stops every later
 * emission and every later wait - or in the few instructions between taking the
 * reference and registering the handler, which strands that reference for the
 * life of the process. Nothing can be made async-cancel-safe by arranging the
 * code differently, which is why POSIX lists almost no function as safe to call
 * in that mode at all. An application that enables it must leave it off around
 * calls into this library.
 */
PQOS_LOCAL void log_wait_quiescent(void);

/**
 * @brief PQoS log function
 *
 * @param [in] type log type to be made
 * @param [in] str format string compatible with printf().
 *             Variadic arguments to follow depending on \a str.
 */
PQOS_LOCAL void log_printf(int type, const char *str, ...);

/**
 * @brief PQoS log function for a caller that may run before the log exists
 *
 * log_printf() with the assertion left out: where there is no destination this
 * says nothing and returns, rather than holding the caller to the rule that
 * nothing logs before pqos_init().
 *
 * Asking in one operation is the point. A caller that tested
 * log_is_initialized() and then called log_printf() could have the log taken
 * down between the two - pqos_open() and pqos_fopen() are public and log
 * without the API lock - and would then assert on a destination that was there
 * when it looked. This takes the destination, or finds there is none, once.
 *
 * @param [in] type log type to be made
 * @param [in] str format string compatible with printf().
 *             Variadic arguments to follow depending on \a str.
 */
PQOS_LOCAL void log_printf_if_init(int type, const char *str, ...);

/**
 * @brief Whether the log has been initialized
 *
 * What state the log is in, which is not the same as what a message would do
 * with it: a log initialized to write nowhere is initialized all the same.
 * What this answers is whether log_printf() may be called at all - see
 * LOG_ERROR_IF_INIT() below, which is the only reason it exists.
 *
 * @retval 1 when log_init() has succeeded and log_fini() has not run since
 * @retval 0 otherwise
 */
PQOS_LOCAL int log_is_initialized(void);

/**
 * @brief Logs an error from a function that may run before the log exists
 *
 * log_printf() holds library code to the rule that nothing logs before
 * pqos_init(): it returns without writing, and a DEBUG build asserts. The rule
 * has two exceptions, and they are public - pqos_open() and pqos_fopen() are
 * how the utility opens its own log file and its configuration file, which it
 * must do before it can initialize the library. Those two ask whether there is
 * anywhere to write instead of assuming it, and say nothing when there is not.
 *
 * One call and not a test followed by a call: a teardown between the two would
 * leave the second asserting on a destination that was there when the first
 * looked, and those two callers log without the API lock, so that window is
 * reachable. log_printf_if_init() takes the destination or finds there is none,
 * once.
 *
 * One call also means the arguments are evaluated whether or not there is a
 * log, where a test in front of them would not evaluate them at all. That is
 * fine for what this macro is for: the three callers pass a pointer they
 * already hold. A caller with an argument expensive enough to want deferring
 * should ask log_is_initialized() itself rather than have every call here pay
 * for the question - and nothing is unsafe either way, since this entry point
 * says nothing where there is no destination.
 *
 * @param [in] str format string compatible with printf().
 *             Variadic arguments to follow depending on \a str.
 */
#define LOG_ERROR_IF_INIT(str...)                                              \
        log_printf_if_init(LOG_OPT_ERROR, "ERROR: " str)

#ifdef __cplusplus
}
#endif

#endif /* __PQOS_LOG_H__ */
