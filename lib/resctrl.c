/*
 * BSD LICENSE
 *
 * Copyright(c) 2018-2026 Intel Corporation. All rights reserved.
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

#include "resctrl.h"

#include "common.h"
#include "log.h"
#include "os_common.h"

#include <errno.h>
#include <fcntl.h>
#include <mntent.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/mount.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

/*
 * fsopen(2) and fsconfig(2), called by number: the C library has wrappers for
 * them only from glibc 2.36, and <linux/mount.h>, which has these values,
 * clashes with <sys/mount.h> in older ones
 */
#define RESCTRL_FSOPEN_CLOEXEC    0x00000001
#define RESCTRL_FSCONFIG_SET_FLAG 0

/*
 * A flag no kernel defines: a resctrl that parses its options as they are
 * set refuses it, and one that parses them only when it mounts takes it
 */
#define RESCTRL_NO_SUCH_OPTION "pqos_no_such_option"

static int resctrl_lock_fd = -1; /**< File descriptor to the lockfile */

/**
 * How long a lock of the resctrl filesystem waits for another holder to give
 * it up, and how long it sleeps between two tries in that while
 */
#define RESCTRL_LOCK_TIMEOUT_NS 100000000L
#define RESCTRL_LOCK_RETRY_NS   1000000L

/**
 * @brief Sleeps the resctrl lock's retry interval, the rest of it after each
 *        signal that cuts it short
 */
static void
resctrl_lock_sleep(void)
{
        struct timespec req = {0, RESCTRL_LOCK_RETRY_NS};
        struct timespec rem;

        while (nanosleep(&req, &rem) != 0 && errno == EINTR)
                req = rem;
}

/**
 * @brief Tries to take the resctrl lock until it is had, or until the timeout
 *        has passed
 *
 * flock() is asked not to block, and asked again after a sleep while the
 * timeout has not passed: a thread that slept, or was not run, past it does
 * not try again, so the lock is not taken at a try after the timeout. A
 * blocking flock() timed out by SIGALRM, as the lock was before, put in a
 * handler of the library's in place of the application's, and replaced, then
 * cancelled, any alarm the application had pending: they share the process's
 * one ITIMER_REAL. The timeout is when the tries stop, at the first look at
 * the clock after a sleep, not a bound on how long they take: a sleep can end
 * late. One that a signal cuts short is slept out, so that the tries keep
 * their pace whatever signals the process takes.
 *
 * @param [in] fd descriptor of the resctrl filesystem
 * @param [in] type lock type
 *
 * @return 0 when the lock is had, ETIMEDOUT when the timeout has passed, or
 *         the errno of a try, or of a look at the clock, that failed
 */
static int
resctrl_lock_try(const int fd, const int type)
{
        struct timespec start;
        struct timespec now;
        int err;

        if (clock_gettime(CLOCK_MONOTONIC, &start) != 0)
                return errno;
        for (;;) {
                if (flock(fd, type | LOCK_NB) == 0)
                        return 0;
                err = errno;
                if (err != EWOULDBLOCK && err != EINTR)
                        break;
                resctrl_lock_sleep();
                if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
                        err = errno;
                        break;
                }
                if ((now.tv_sec - start.tv_sec) * 1000000000LL +
                        (now.tv_nsec - start.tv_nsec) >=
                    RESCTRL_LOCK_TIMEOUT_NS) {
                        err = ETIMEDOUT;
                        break;
                }
        }

        return err;
}

/**
 * @brief Gives the resctrl lock up, for a thread cancelled as
 *        resctrl_lock() hands it over
 *
 * @param [in] arg unused
 */
static void
resctrl_lock_cancelled(void *arg)
{
        UNUSED_PARAM(arg);

        (void)flock(resctrl_lock_fd, LOCK_UN);
        close(resctrl_lock_fd);
        resctrl_lock_fd = -1;
}

/**
 * @brief Puts the caller's cancellation state back once the lock is held, and
 *        acts on a cancellation asked for while it was being taken, with the
 *        lock given up
 *
 * The thread's cancellation was off from before the open until the lock was
 * held, so one asked for meanwhile is pending. Acted on at the caller's next
 * cancellation point, it would leave the lock held and its descriptor open,
 * with nothing to give either up: the API lock's destructor gives the API
 * lock back alone, and the next lock would wait out its timeout on this one.
 * The handler is in before the state is put back, so a caller whose
 * cancellation is asynchronous, and acted on as soon as it is on, gives the
 * lock up as well. On its own, so that the handler's scope holds nothing else.
 *
 * @param [in] state the caller's cancellation state
 */
static void
resctrl_lock_handed_over(const int state)
{
        int off;

        pthread_cleanup_push(resctrl_lock_cancelled, NULL);
        (void)pthread_setcancelstate(state, &off);
        pthread_testcancel();
        pthread_cleanup_pop(0);
}

/**
 * @brief Obtain lock on resctrl filesystem
 *
 * Gives another holder 100 ms to give the lock up, as resctrl_lock_try()
 * counts them. A cancellation asked for before the call is acted on at its
 * start, before anything is opened. The thread's cancellation is then off
 * from the open of the descriptor until the lock is held, or the descriptor
 * closed: one acted on in a sleep of the wait would leave the descriptor open
 * with nothing to close it. It is put back before a failure is logged, since
 * a log written to a pipe, or a callback, can block, and a thread that cannot
 * be cancelled there would keep the API lock. A cancellation asked for since
 * the start is acted on, where the lock was taken, before it is handed over,
 * with the lock given up - see resctrl_lock_handed_over(). Where the wait
 * failed, it is acted on in the log where the log reaches a cancellation
 * point, as a write() does; where it does not, as at a level that writes
 * nothing, the call returns with it still pending, for the caller's next
 * cancellation point, with nothing left open.
 *
 * @param[in] type lock type
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK on success
 */
static int
resctrl_lock(const int type)
{
        int state;
        int err;

        ASSERT(type == LOCK_SH || type == LOCK_EX);

        pthread_testcancel();
        (void)pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &state);

        /* O_CLOEXEC: held for as long as the resctrl lock is */
        resctrl_lock_fd = open(RESCTRL_PATH, O_DIRECTORY | O_CLOEXEC);
        if (resctrl_lock_fd < 0) {
                (void)pthread_setcancelstate(state, &state);
                LOG_ERROR("Could not open %s directory\n", RESCTRL_PATH);
                return PQOS_RETVAL_ERROR;
        }

        err = resctrl_lock_try(resctrl_lock_fd, type);
        if (err == 0) {
                resctrl_lock_handed_over(state);
                return PQOS_RETVAL_OK;
        }
        close(resctrl_lock_fd);
        resctrl_lock_fd = -1;
        (void)pthread_setcancelstate(state, &state);

        if (err == ETIMEDOUT)
                LOG_ERROR("Failed to acquire lock on resctrl filesystem - "
                          "timeout occurred\n");
        else
                LOG_ERROR("Failed to acquire lock on resctrl filesystem - "
                          "%s\n",
                          strerror(err));

        return PQOS_RETVAL_ERROR;
}

int
resctrl_lock_shared(void)
{
        return resctrl_lock(LOCK_SH);
}

int
resctrl_lock_exclusive(void)
{
        return resctrl_lock(LOCK_EX);
}

int
resctrl_lock_release(void)
{
        if (resctrl_lock_fd < 0) {
                LOG_ERROR("Resctrl filesystem not locked\n");
                return PQOS_RETVAL_ERROR;
        }

        if (flock(resctrl_lock_fd, LOCK_UN) != 0)
                LOG_WARN("Failed to release lock on resctrl filesystem\n");

        close(resctrl_lock_fd);
        resctrl_lock_fd = -1;

        return PQOS_RETVAL_OK;
}

int
resctrl_mount(const enum pqos_cdp_config l3_cdp_cfg,
              const enum pqos_cdp_config l2_cdp_cfg,
              const enum pqos_mba_config mba_cfg)
{
        const char *options = NULL;
        char buf[32] = "";

        ASSERT(l3_cdp_cfg == PQOS_REQUIRE_CDP_ON ||
               l3_cdp_cfg == PQOS_REQUIRE_CDP_OFF);
        ASSERT(l2_cdp_cfg == PQOS_REQUIRE_CDP_ON ||
               l2_cdp_cfg == PQOS_REQUIRE_CDP_OFF);
        ASSERT(mba_cfg == PQOS_MBA_DEFAULT || mba_cfg == PQOS_MBA_CTRL);

        /* l3 cdp mount option */
        if (l3_cdp_cfg == PQOS_REQUIRE_CDP_ON) {
                pqos_strcat(buf, "cdp", sizeof(buf));
                options = buf;
        }

        /* l2 cdp mount option */
        if (l2_cdp_cfg == PQOS_REQUIRE_CDP_ON) {
                if (options != NULL)
                        pqos_strcat(buf, ",", sizeof(buf));
                pqos_strcat(buf, "cdpl2", sizeof(buf));
                options = buf;
        }

        /* mba mount option */
        if (mba_cfg == PQOS_MBA_CTRL) {
                if (options != NULL)
                        pqos_strcat(buf, ",", sizeof(buf));
                pqos_strcat(buf, "mba_MBps", sizeof(buf));
                options = buf;
        }

        if (mount("resctrl", RESCTRL_PATH, "resctrl", 0, options) != 0) {
                LOG_DEBUG("resctrl mount failed with error %d - %m\n", errno);
                return PQOS_RETVAL_ERROR;
        }

        return PQOS_RETVAL_OK;
}

int
resctrl_umount(void)
{
        if (umount2(RESCTRL_PATH, 0) != 0) {
                /* EINVAL: not a mount point, ENOENT: no such path */
                const int none = errno == EINVAL || errno == ENOENT;

                LOG_ERROR("Could not umount resctrl filesystem!\n");
                return none ? PQOS_RETVAL_RESOURCE : PQOS_RETVAL_ERROR;
        }

        return PQOS_RETVAL_OK;
}

int
resctrl_mounted_with(const char *option, int *found)
{
        FILE *mounts;
        struct mntent entry;
        struct mntent *ent;
        /* getmntent() answers in storage the whole process shares, which a
         * thread of the caller's can overwrite before the entry is read;
         * getmntent_r() answers here
         */
        char strings[4096];

        if (option == NULL || found == NULL)
                return PQOS_RETVAL_PARAM;

        *found = 0;

        /* "e" for close-on-exec, which glibc's setmntent() adds of itself
         * and musl's does not: a thread of the caller's that starts a
         * program meanwhile would otherwise hand it the descriptor
         */
        mounts = setmntent(PROC_MOUNTS, "re");
        if (mounts == NULL) {
                LOG_DEBUG("%s not read.\n", PROC_MOUNTS);
                return PQOS_RETVAL_OK;
        }

        for (;;) {
                errno = 0;
                ent = getmntent_r(mounts, &entry, strings, sizeof(strings));
                if (ent == NULL) {
                        /* a line too long for the buffer: glibc goes on to
                         * the next line itself, and musl skips the line and
                         * answers NULL with ERANGE, which is no end of file
                         */
                        if (errno == ERANGE && !feof(mounts))
                                continue;
                        break;
                }
                if (strcmp(ent->mnt_type, "resctrl") == 0 &&
                    hasmntopt(ent, option) != NULL) {
                        *found = 1;
                        break;
                }
        }

        endmntent(mounts);

        return PQOS_RETVAL_OK;
}

int
resctrl_mount_option_supported(const char *option, int *supported)
{
        if (option == NULL || supported == NULL)
                return PQOS_RETVAL_PARAM;

        *supported = -1;

#if defined(SYS_fsopen) && defined(SYS_fsconfig)
        {
                int state;

                /* the thread's cancellation is off from the open until the
                 * descriptor is closed: close() is a cancellation point, and
                 * a cancellation acted on there, asked for at any time since
                 * the open, would leave the descriptor and its filesystem
                 * context open, with nothing to close them. It is put back
                 * before anything is logged, as resctrl_lock() puts it back
                 */
                (void)pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &state);

                const int fd =
                    (int)syscall(SYS_fsopen, "resctrl", RESCTRL_FSOPEN_CLOEXEC);

                if (fd < 0) {
                        const int err = errno;

                        (void)pthread_setcancelstate(state, &state);
                        errno = err;
                        LOG_DEBUG("resctrl option %s not asked: fsopen error "
                                  "%d - %m\n",
                                  option, err);
                        return PQOS_RETVAL_OK;
                }

                /* the answers are kept, 0 or the errno, and the descriptor
                 * closed before anything is logged: a log callback that ends
                 * the thread would otherwise leave it open
                 */
                const int bogus =
                    syscall(SYS_fsconfig, fd, RESCTRL_FSCONFIG_SET_FLAG,
                            RESCTRL_NO_SUCH_OPTION, NULL, 0) == 0
                        ? 0
                        : errno;
                int answer = -1;

                if (bogus == EINVAL)
                        answer =
                            syscall(SYS_fsconfig, fd, RESCTRL_FSCONFIG_SET_FLAG,
                                    option, NULL, 0) == 0
                                ? 0
                                : errno;
                close(fd);
                (void)pthread_setcancelstate(state, &state);

                if (bogus == 0)
                        LOG_DEBUG("resctrl option %s not asked: options are "
                                  "parsed only when resctrl mounts\n",
                                  option);
                else if (bogus != EINVAL) {
                        errno = bogus;
                        LOG_DEBUG("resctrl option %s not asked: fsconfig "
                                  "error %d for an unknown option - %m\n",
                                  option, bogus);
                } else if (answer == 0)
                        *supported = 1;
                else if (answer == EINVAL)
                        *supported = 0;
                else {
                        errno = answer;
                        LOG_DEBUG("resctrl option %s not asked: fsconfig "
                                  "error %d - %m\n",
                                  option, answer);
                }
        }
#endif

        return PQOS_RETVAL_OK;
}

/*
 * ---------------------------------------
 * CPU mask structures and utility functions
 * ---------------------------------------
 */

void
resctrl_cpumask_set(const unsigned lcore, struct resctrl_cpumask *mask)
{
        /* index in mask table */
        const unsigned item = (sizeof(mask->tab) - 1) - (lcore / CHAR_BIT);
        const unsigned bit = lcore % CHAR_BIT;

        /* a core past the mask has no bit in it */
        if (lcore >= RESCTRL_MAX_CPUS)
                return;

        /* Set lcore bit in mask table item */
        mask->tab[item] = mask->tab[item] | (1 << bit);
}

void
resctrl_cpumask_unset(const unsigned lcore, struct resctrl_cpumask *mask)
{
        /* index in mask table */
        const unsigned item = (sizeof(mask->tab) - 1) - (lcore / CHAR_BIT);
        const unsigned bit = lcore % CHAR_BIT;

        /* a core past the mask has no bit in it */
        if (lcore >= RESCTRL_MAX_CPUS)
                return;

        /* Clear lcore bit in mask table item */
        mask->tab[item] &= ~(1 << bit);
}

int
resctrl_cpumask_get(const unsigned lcore, const struct resctrl_cpumask *mask)
{
        /* index in mask table */
        const unsigned item = (sizeof(mask->tab) - 1) - (lcore / CHAR_BIT);
        const unsigned bit = lcore % CHAR_BIT;

        /* a core past the mask has no bit in it */
        if (lcore >= RESCTRL_MAX_CPUS)
                return 0;

        /* Check if lcore bit is set in mask table item */
        return (mask->tab[item] >> bit) & 0x1;
}

int
resctrl_cpumask_write(FILE *fd, const struct resctrl_cpumask *mask)
{
        unsigned i;
        unsigned found_nonzero = 0;

        ASSERT(fd != NULL);
        ASSERT(mask != NULL);

        for (i = 0; i < sizeof(mask->tab); i++) {
                const unsigned value = (unsigned)mask->tab[i];

                /* skip leading 0 */
                if (!found_nonzero && value == 0x0)
                        continue;
                found_nonzero = 1;

                if (fprintf(fd, "%02x", value) < 0) {
                        LOG_ERROR("Failed to write cpu mask\n");
                        break;
                }
                if ((i + 1) % 4 == 0)
                        if (fprintf(fd, ",") < 0) {
                                LOG_ERROR("Failed to write cpu mask\n");
                                break;
                        }
        }
        if (!found_nonzero)
                fprintf(fd, "0");

        /* check if error occurred in loop */
        if (i < sizeof(mask->tab))
                return PQOS_RETVAL_ERROR;

        return PQOS_RETVAL_OK;
}

/**
 * The characters of a cpus file whose mask has RESCTRL_MAX_CPUS bits: a hex
 * digit for every 4 of them, a comma between each 32 of them and the next,
 * and its newline
 */
#define RESCTRL_CPUMASK_CHARS (RESCTRL_MAX_CPUS / 4 + RESCTRL_MAX_CPUS / 32)

int
resctrl_cpumask_read(FILE *fd, struct resctrl_cpumask *mask)
{
        int i, hex_offset, idx;
        size_t num_chars = 0;
        /* one more than such a file has, which tells a longer one */
        char cpus[RESCTRL_CPUMASK_CHARS + 1];

        ASSERT(fd != NULL);
        ASSERT(mask != NULL);

        memset(mask, 0, sizeof(struct resctrl_cpumask));
        memset(cpus, 0, sizeof(cpus));

        /** Read the entire file into memory. */
        num_chars = fread(cpus, sizeof(char), sizeof(cpus), fd);

        if (ferror(fd) != 0) {
                LOG_ERROR("Error reading CPU file\n");
                return PQOS_RETVAL_ERROR;
        }
        if (num_chars == sizeof(cpus)) {
                LOG_ERROR("CPU mask longer than one of %u CPUs\n",
                          RESCTRL_MAX_CPUS);
                return PQOS_RETVAL_ERROR;
        }

        /**
         *  Convert the cpus array into hex, skip any non hex chars.
         *  Store the hex values in the mask tab.
         */
        for (i = num_chars - 1, hex_offset = 0, idx = sizeof(mask->tab) - 1;
             i >= 0; i--) {
                const char c = cpus[i];
                int hex_num;

                if ('0' <= c && c <= '9')
                        hex_num = c - '0';
                else if ('a' <= c && c <= 'f')
                        hex_num = 10 + c - 'a';
                else if ('A' <= c && c <= 'F')
                        hex_num = 10 + c - 'A';
                else
                        continue;

                if (idx < 0) {
                        LOG_ERROR("CPU mask longer than one of %u CPUs\n",
                                  RESCTRL_MAX_CPUS);
                        return PQOS_RETVAL_ERROR;
                }
                if (!hex_offset)
                        mask->tab[idx] = (uint8_t)hex_num;
                else {
                        mask->tab[idx] |= (uint8_t)(hex_num << 4);
                        idx--;
                }
                hex_offset ^= 1;
        }

        return PQOS_RETVAL_OK;
}

int
resctrl_is_supported(void)
{
        int ret;
        int resctrl = 0;

        ret = pqos_file_contains(PROC_FILESYSTEMS, "resctrl", &resctrl);
        if (ret != PQOS_RETVAL_OK) {
                /* The log message below may not appear if log module
                   is not initialized */
                LOG_ERROR("Fatal error encountered in resctrl detection!\n");
                return ret;
        }
        /* The log message below may not appear if log module
           is not initialized */
        LOG_INFO("%s\n", resctrl ? "resctrl detected"
                                 : "resctrl not detected. "
                                   "Kernel version 4.10 or higher required");

        if (resctrl == 0)
                return PQOS_RETVAL_RESOURCE;

        return PQOS_RETVAL_OK;
}
