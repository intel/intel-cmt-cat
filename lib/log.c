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
#include <stdint.h> /* int64_t */
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/**
 * ---------------------------------------
 * Local data structures
 * ---------------------------------------
 */

/**
 * Where a message goes: the descriptor, the callback and the callback's
 * context, in one object so that a message takes all of it or none of it. The
 * object is never modified once published - an install makes a new one - so a
 * message that has taken it has a consistent destination and needs nothing
 * else.
 *
 * A message holds one for the length of its emission, counted in \a refs, and
 * the last holder of a retired destination is what frees it. A destination
 * nobody holds is freed by the teardown that retires it, which is every
 * teardown with no message beside it.
 *
 * A process that exits with the log still installed leaves one allocated, the
 * way it leaves every other module's state at exit.
 */
struct log_destination {
        int opt; /**< which message types reach it */
        int fd;  /**< descriptor to write to, or -1 for none */
        /** where a message is handed to, or NULL for none */
        void (*callback)(void *cb_context,
                         const size_t cb_size,
                         const char *cb_message);
        void *context;                /**< handed back to \a callback */
        unsigned refs;                /**< messages using it right now */
        int retired;                  /**< taken out of service */
        struct log_destination *next; /**< the retired list */
};

/**
 * The published destination, the destinations retired and not yet freed, and
 * the mutex that guards both of them together with every reference count.
 *
 * A lock here and not an atomic pointer: what a message needs is not only a
 * consistent pointer but a destination that stays alive while it uses it, and
 * taking a reference is two operations - read the pointer, count the reference
 * - which an atomic pointer cannot make one. The retired list is a second
 * reason: an install or a teardown links to it under the API lock while a drain
 * may be walking it with that lock released, so the list needs a lock of its
 * own whatever the pointer does.
 *
 * What this lock must never do is what the module refused a lock for before: it
 * is *not held across the write or the application's callback*, and *not held
 * while anything waits*. So nothing a callback does can be stopped by this
 * lock: it may log, and it may finalize, and a teardown waiting for a message
 * to finish takes this only to look and sleeps without it.
 *
 * Which is this module's half of the question and not the whole of it. What a
 * callback may call is decided by the lock the *library* holds while it logs -
 * pqos.h is where that is written down, and it is narrower: a callback reached
 * from inside a pqos API cannot call one, because that lock is not recursive.
 */
static pthread_mutex_t m_dest_mutex = PTHREAD_MUTEX_INITIALIZER;
static struct log_destination *m_dest;
static struct log_destination *m_retired;

/** How deeply this thread is inside an emission, and which destination each of
 *  those emissions is using. Thread local because it describes a call and not
 *  the log: a teardown reached from a log callback must not wait for the
 *  emission it is itself inside - but it must still wait for the emissions
 *  other threads are inside, including ones using the same destination, so it
 *  is not enough to know that this thread is somewhere inside one. What the
 *  drain needs is the difference between the references a destination has and
 *  the references this thread is holding on it, and that is what this records.
 *
 *  Four is how many frames are tracked, not how many an application can reach:
 *  a callback that logs, whose callback logs, is nesting the application chose,
 *  and pqos.h describes a caller recursing through pqos_open() failures. So
 *  indices 0 to 3 are recorded and a fifth emission is counted without being
 *  recorded, which makes the drain wait for a reference it could have skipped -
 *  a teardown reached that deep pays the whole bound. Waiting too long is
 *  bounded; not waiting is not, which is why the limit errs this way.
 */
#define LOG_NEST_MAX 4
static __thread unsigned m_emitting = 0;
static __thread const struct log_destination *m_held[LOG_NEST_MAX];

/** how deeply logging is suspended on this thread, which is thread local
 *  because it describes a call and not the log
 */
static __thread unsigned m_suspended = 0;

/** How long log_wait_quiescent() waits for the messages using a retired
 *  destination to finish: a second, as a deadline rather than as a count of
 *  sleeps. A count of sleeps is not a duration - a hundred microseconds is
 * below what the kernel can be relied on to give back, so ten thousand of them
 * take well over a second on a loaded machine - and what this promises is a
 *  duration. Bounded deliberately: a finalization that waited for ever would
 *  hand the application a hang in place of a race, and what the wait is for is
 *  the last few instructions of a message, not an unbounded callback.
 *
 *  A millisecond between looks, which is a thousand wakeups in the worst case
 *  rather than ten thousand. The interval decides only how long after the last
 *  message this returns, and a millisecond of that is nothing beside the second
 *  it is bounded by - while ten thousand syscalls in every finalization is not
 *  nothing. A condition variable signalled by the last release would cost none
 *  at all, and is deliberately not here: it is a second synchronisation object
 *  in a module whose one rule is that nothing is held across the application's
 *  callback, for a saving of a few milliseconds per teardown.
 */
#define LOG_DRAIN_NS       1000000000L
#define LOG_DRAIN_SLEEP_NS 1000000

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
 * @brief Takes the destination a message is to use, and a reference to it
 *
 * The pointer and the reference are taken together, which is the whole reason
 * this is not an atomic pointer: between reading a pointer and counting a
 * reference to what it points at, a teardown could otherwise free it.
 *
 * @return the destination, with a reference held for the caller, or NULL where
 *         there is none
 */
static struct log_destination *
log_dest_acquire(void)
{
        struct log_destination *dest;

        pthread_mutex_lock(&m_dest_mutex);
        dest = m_dest;
        if (dest != NULL)
                dest->refs++;
        pthread_mutex_unlock(&m_dest_mutex);

        return dest;
}

/**
 * @brief Gives back a reference, freeing a retired destination nobody holds
 *
 * The last holder of a retired destination is what frees it, whether that is a
 * message finishing or the teardown that retired it.
 *
 * @param [in] dest what log_dest_acquire() returned, or NULL
 */
static void
log_dest_release(struct log_destination *dest)
{
        struct log_destination **link;

        if (dest == NULL)
                return;

        pthread_mutex_lock(&m_dest_mutex);

        dest->refs--;
        if (dest->retired && dest->refs == 0) {
                for (link = &m_retired; *link != NULL; link = &(*link)->next)
                        if (*link == dest) {
                                *link = dest->next;
                                free(dest);
                                break;
                        }
        }

        pthread_mutex_unlock(&m_dest_mutex);
}

/**
 * @brief Takes a destination out of service
 *
 * Freed here where no message is using it, which is every teardown with none
 * beside it; otherwise it goes on the retired list and the last message to let
 * it go frees it. Called with m_dest_mutex held.
 *
 * @param [in] dest the destination, or NULL
 */
static void
log_dest_retire(struct log_destination *dest)
{
        if (dest == NULL)
                return;

        if (dest->refs == 0) {
                free(dest);
                return;
        }

        dest->retired = 1;
        dest->next = m_retired;
        m_retired = dest;
}

/**
 * @brief Installs the log
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
log_install(int fd_log,
            void (*callback_log)(void *, const size_t, const char *),
            void *context_log,
            int verbosity)
{
        struct log_destination *dest;
        struct log_destination *replaced;
        int opt;

        /**
         * Set log message verbosity
         */
        switch (verbosity) {
        case LOG_VER_SILENT:
                opt = LOG_OPT_SILENT;
                break;
        case LOG_VER_DEFAULT:
                opt = LOG_OPT_DEFAULT;
                break;
        case LOG_VER_VERBOSE:
                opt = LOG_OPT_VERBOSE;
                break;
        case LOG_VER_SUPER_VERBOSE:
                opt = LOG_OPT_SUPER_VERBOSE;
                break;
        default:
                opt = LOG_OPT_SUPER_VERBOSE;
                break;
        }

        /* a silent log writes nowhere, so it needs no destination to write to;
         * every other level does
         */
        if (opt != LOG_OPT_SILENT && fd_log < 0 && callback_log == NULL) {
                fprintf(stderr, "%s: no LOG destination selected\n", __func__);
                return LOG_RETVAL_ERROR;
        }

        dest = calloc(1, sizeof(*dest));
        if (dest == NULL) {
                fprintf(stderr, "%s: out of memory\n", __func__);
                return LOG_RETVAL_ERROR;
        }
        dest->opt = opt;
        dest->fd = opt == LOG_OPT_SILENT ? -1 : fd_log;
        dest->callback = opt == LOG_OPT_SILENT ? NULL : callback_log;
        dest->context = opt == LOG_OPT_SILENT ? NULL : context_log;

        /* Published under the lock, which is also what retires the destination
         * this replaces: an install over one that is still there has to retire
         * it as a teardown would, since a message may be using it and nothing
         * else would ever free it.
         */
        pthread_mutex_lock(&m_dest_mutex);
        replaced = m_dest;
        m_dest = dest;
        log_dest_retire(replaced);
        pthread_mutex_unlock(&m_dest_mutex);

        return LOG_RETVAL_OK;
}

/**
 * @brief Takes the log down
 *
 * The destination is retired rather than freed: a message that loaded it before
 * this runs is still using it, and this is called with the API lock held, where
 * waiting for that message could deadlock a callback that finalizes. What frees
 * it is the last message to let it go, and log_wait_quiescent() is what waits
 * for that - from pqos_fini(), after it has let the API lock go.
 *
 * @return Operation status
 * @retval LOG_RETVAL_OK success
 */
static int
log_remove(void)
{
        struct log_destination *dest;

        pthread_mutex_lock(&m_dest_mutex);
        dest = m_dest;
        m_dest = NULL;
        log_dest_retire(dest);
        pthread_mutex_unlock(&m_dest_mutex);

        return LOG_RETVAL_OK;
}

int
log_init(int fd_log,
         void (*callback_log)(void *, const size_t, const char *),
         void *context_log,
         int verbosity)
{
        return log_install(fd_log, callback_log, context_log, verbosity);
}

int
log_fini(void)
{
        return log_remove();
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
        int installed;

        pthread_mutex_lock(&m_dest_mutex);
        installed = m_dest != NULL;
        pthread_mutex_unlock(&m_dest_mutex);

        return installed;
}

/**
 * @brief How many references this thread holds on one destination
 *
 * @param [in] dest the destination to count
 *
 * @return the number of this thread's emissions using it
 */
static unsigned
log_held_by_this_thread(const struct log_destination *dest)
{
        unsigned depth = m_emitting < LOG_NEST_MAX ? m_emitting : LOG_NEST_MAX;
        unsigned held = 0;
        unsigned i;

        for (i = 0; i < depth; i++)
                if (m_held[i] == dest)
                        held++;

        return held;
}

/**
 * @brief Whether a retired destination is being used by anybody but this thread
 *
 * Its own references are left out, and nothing else is: a teardown reached from
 * a callback cannot wait for the emission it is inside - that emission cannot
 * finish until the teardown returns - but it must wait for the emissions other
 * threads are inside, including ones using the same destination. Counting only
 * "this thread is somewhere inside an emission" would abandon those.
 *
 * @return Whether a message on another thread is still holding one
 * @retval 1 one is held elsewhere
 * @retval 0 every reference left on the retired list is this thread's
 */
static int
log_retired_in_use(void)
{
        const struct log_destination *dest;
        int in_use = 0;

        pthread_mutex_lock(&m_dest_mutex);

        for (dest = m_retired; dest != NULL; dest = dest->next)
                if (dest->refs > log_held_by_this_thread(dest)) {
                        in_use = 1;
                        break;
                }

        pthread_mutex_unlock(&m_dest_mutex);

        return in_use;
}

/**
 * @brief Nanoseconds from one moment to another
 *
 * @param [in] from the earlier reading
 * @param [in] to the later reading
 *
 * @return how long separates them, in nanoseconds
 */
static int64_t
log_elapsed_ns(const struct timespec *from, const struct timespec *to)
{
        /* int64_t and not long: long is 32 bits on an ILP32 build, where two
         * and a bit seconds of nanoseconds overflow it - and a waiter
         * descheduled for that long would then compare a wrapped value against
         * the deadline and keep waiting, which is the one thing this bound
         * promises not to do
         */
        return (int64_t)(to->tv_sec - from->tv_sec) * 1000000000LL +
               (int64_t)(to->tv_nsec - from->tv_nsec);
}

void
log_wait_quiescent(void)
{
        struct timespec start;
        struct timespec now;

        /* What is waited for is a message using a destination this library has
         * retired, and nothing else: a message using the destination installed
         * now - an application logging from a thread of its own, which never
         * stops - is not what a teardown is about, and waiting for that would
         * make every pqos_fini() pay the whole bound.
         *
         * And not this thread's own references, which log_retired_in_use()
         * subtracts: a teardown reached from a log callback is inside an
         * emission that cannot finish until this returns, so waiting for it
         * would wait for this thread. Only that emission is left out, though -
         * another thread inside a callback on the same destination is waited
         * for like any other.
         */
        if (clock_gettime(CLOCK_MONOTONIC, &start) != 0)
                return;

        while (log_retired_in_use()) {
                struct timespec ts;

                if (clock_gettime(CLOCK_MONOTONIC, &now) != 0)
                        return;
                if (log_elapsed_ns(&start, &now) >= LOG_DRAIN_NS)
                        return;

                ts.tv_sec = 0;
                ts.tv_nsec = LOG_DRAIN_SLEEP_NS;
                (void)nanosleep(&ts, NULL);
        }
}

/**
 * @brief Gives back what an emission holds, however it ends
 *
 * A cancellation handler as well as the ordinary path: write() is a POSIX
 * cancellation point, so a thread cancelled while blocked in it would otherwise
 * never reach the release - and the destination it was using would be held for
 * the life of the process, leaving every later wait to spend the whole bound
 * looking for it. The application's callback is the same case, which this
 * covers too.
 *
 * @param [in] arg the destination the emission acquired, as a void pointer
 */
static void
log_emission_cleanup(void *arg)
{
        m_emitting--;
        if (m_emitting < LOG_NEST_MAX)
                m_held[m_emitting] = NULL;
        log_dest_release((struct log_destination *)arg);
}

/**
 * @brief Writes one message to a destination already acquired
 *
 * Separated from the two entry points so that the acquire, the release and the
 * cancellation handler that joins them are written once, and so that this part
 * may return wherever it likes without jumping out of that handler's scope -
 * which pthread_cleanup_push() does not permit.
 *
 * @param [in] dest the destination, or NULL where there is none
 * @param [in] type log type to be made
 * @param [in] str format string compatible with printf()
 * @param [in] ap the arguments \a str describes
 */
static void
log_emit(const struct log_destination *dest,
         int type,
         const char *str,
         va_list ap)
{
        char ap_buffer[AP_BUFFER_SIZE];
        int size;

        if (dest == NULL || dest->opt == LOG_OPT_SILENT ||
            (dest->opt & type) == 0)
                return;

        ASSERT(str != NULL);
        if (str == NULL)
                return;

        ap_buffer[AP_BUFFER_SIZE - 1] = '\0';
        size = vsnprintf(ap_buffer, AP_BUFFER_SIZE - 1, str, ap);
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

        /* the descriptor first, and the callback after it: the callback belongs
         * to the application, and one that closes the log descriptor - having
         * just finalized the library, say - would leave a write after it
         * addressed to a descriptor that is closed, or worse, reused
         */
        if (dest->fd >= 0) {
                if (write(dest->fd, ap_buffer, size) < 0)
                        fprintf(stderr, "%s: printing to file failed\n",
                                __func__);
        }

        /* and the callback with nothing held, so that what it does with the
         * message - including calling back into this library, or finalizing it
         * - is not for the log to serialize
         */
        if (dest->callback != NULL)
                dest->callback(dest->context, size, ap_buffer);
}

/**
 * @brief One message, from the acquire to the release
 *
 * @param [in] type log type to be made
 * @param [in] str format string compatible with printf()
 * @param [in] ap the arguments \a str describes
 */
static void
log_message(int type, const char *str, va_list ap)
{
        struct log_destination *dest;

        /* a caller that has suspended logging on this thread wants no message
         * written and no assertion about a log it is not using - see
         * log_suspend()
         */
        if (m_suspended > 0)
                return;

        /* The destination and a reference to it are taken together, so it
         * cannot be freed while this message is using it - and the whole
         * destination comes with one pointer, so the descriptor this message
         * tests is the descriptor it writes to and the context is the one that
         * belongs to the callback beside it.
         *
         * m_emitting says this thread is inside an emission, which is what
         * stops a teardown reached from the callback below from waiting for the
         * very message it is inside.
         */
        dest = log_dest_acquire();
        if (m_emitting < LOG_NEST_MAX)
                m_held[m_emitting] = dest;
        m_emitting++;

        pthread_cleanup_push(log_emission_cleanup, dest);

        /* If log_init has not been successful then log_printf should not
         * work
         */
        ASSERT(dest != NULL);

        log_emit(dest, type, str, ap);

        /* 1, so the handler runs here as well as on a cancellation: the normal
         * path and the cancelled one give back exactly the same things
         */
        pthread_cleanup_pop(1);
}

void
log_printf(int type, const char *str, ...)
{
        va_list ap;

        va_start(ap, str);
        log_message(type, str, ap);
        va_end(ap);
}

