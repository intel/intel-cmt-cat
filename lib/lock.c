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
 * @brief Implementation of PQoS API lock
 *
 * provide functions for safe access to PQoS API - this is required for
 * allocation and monitoring modules which also implement PQoS API
 */

#include "lock.h"

#include "log.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h> /* O_CREAT */
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h> /* S_Ixxx */
#include <sys/types.h>
#include <unistd.h> /* usleep(), lockf() */

/**
 * ---------------------------------------
 * Local data structures
 * ---------------------------------------
 */
#define LOCKDIR                   "/var/lock"
#define LOCKFILE_TMP              "/var/lock/myapilock.tmp"
#define LOCKFILE_PERMS            0666
#define MAX_LINE                  128
#define PROC_START_TIME_FIELD_IDX 22
#define STAT_PATH_LENGTH          64
#define PROCFILE_BUF_LEN          1024

/**
 * API thread/process safe access is secured through these locks.
 */
static int m_apilock = -1;
static pthread_mutex_t m_apilock_mutex;
static pid_t m_pid = 0;
static unsigned long m_start_time = 0;

/**
 * @brief helper: Get process start time
 *
 * @param [in] pid process id
 *
 * @return process start time
 */
static unsigned long
get_process_start_time(pid_t pid)
{
        char stat_path[STAT_PATH_LENGTH];
        char buf[PROCFILE_BUF_LEN];
        char *p = buf;
        int i;
        unsigned long start_time = 0;

        snprintf(stat_path, sizeof(stat_path), "/proc/%d/stat", pid);
        FILE *fp = fopen(stat_path, "r");

        if (!fp)
                return 0;

        if (!fgets(buf, sizeof(buf), fp)) {
                fclose(fp);
                return 0;
        }
        fclose(fp);

        for (i = 0; i < PROC_START_TIME_FIELD_IDX; ++i) {
                p = strchr(p, ' ');
                if (!p)
                        return 0;
                ++p;
        }

        return sscanf(p, "%lu", &start_time) == 1 ? start_time : 0;
}

/**
 * @brief Check if the lockfile's directory is accessible
 *
 * @return Operation status
 * retval 0 on success
 * retval -1 on failure
 */
static int
check_lockdir_access(void)
{
        if (access(LOCKDIR, R_OK | W_OK | X_OK) != 0)
                return -1;

        /* Try to create and delete a temp file */
        int fd =
            open(LOCKFILE_TMP, O_WRONLY | O_CREAT | O_EXCL, LOCKFILE_PERMS);
        if (fd == -1)
                return -1;
        close(fd);
        if (unlink(LOCKFILE_TMP) != 0)
                return -1;
        return 0;
}

/**
 * @brief Check if PID is alive and matches start time
 *
 * @param [in] pid process id
 * @param [in] start_time process start time
 *
 * @return Operation status
 * retval 0 if process is alive and started at provided time
 * retval 1 if process is dead or it start time doesn't match with provided
 */
static int
is_pid_alive(pid_t pid, unsigned long start_time)
{
        unsigned long cur_start_time = get_process_start_time(pid);

        return (cur_start_time != 0 && cur_start_time == start_time);
}

/**
 * @brief Read lock file (PID and start time)
 *
 * @param [out] pid process id
 * @param [out] process start time
 *
 * @return Operation status
 * retval 0 on success
 * retval -1 on failure
 */
static int
read_lockfile(pid_t *pid, unsigned long *start_time)
{
        FILE *fp = fopen(LOCKFILE, "r");
        char line[MAX_LINE];

        if (!fp)
                return -1;

        if (!fgets(line, sizeof(line), fp)) {
                fclose(fp);
                return -1;
        }
        fclose(fp);

        return sscanf(line, "%d %lu", pid, start_time) == 2 ? 0 : -1;
}

/**
 * @brief Write lock file (PID and start time)
 *
 * @param [in] pid process id
 * @param [in] start_time process start time
 *
 * @return Operation status
 * retval 0 on success
 * retval -1 on failure
 */
static int
write_lockfile(pid_t pid, unsigned long start_time)
{
        FILE *fp = fopen(LOCKFILE, "w");

        if (!fp)
                return -1;
        fprintf(fp, "%d %lu\n", pid, start_time);
        fclose(fp);
        return 0;
}

int
lock_init(void)
{
        pid_t pid;
        unsigned long stime;
        static pthread_mutex_t init_mutex = PTHREAD_MUTEX_INITIALIZER;

        pthread_mutex_lock(&init_mutex);

        /* Check directory access */
        if (check_lockdir_access() != 0) {
                fprintf(stderr,
                        "Cannot access lock directory: \"%s\". Run program as "
                        "root!\n",
                        LOCKDIR);
                pthread_mutex_unlock(&init_mutex);
                return -1;
        }

        /* Open lock file atomically (create if not exists) */
        m_apilock = open(LOCKFILE, O_RDWR | O_CREAT | O_EXCL, LOCKFILE_PERMS);
        if (m_apilock == -1) {
                if (errno == EEXIST) {
                        /* Lock file exists, check if it is stale */
                        if (read_lockfile(&pid, &stime) == -1 ||
                            !is_pid_alive(pid, stime)) {
                                /* File is stale, try to remove */
                                if (unlink(LOCKFILE) != 0) {
                                        fprintf(
                                            stderr,
                                            "Cannot remove stale lock file: "
                                            "%s. Error: \"%s\". You should "
                                            "remove the stale file manually or "
                                            "run program as root!\n",
                                            LOCKFILE, strerror(errno));
                                        pthread_mutex_unlock(&init_mutex);
                                        return -1;
                                }
                                /* Creating the lock file after removing the
                                 * stale one */
                                m_apilock =
                                    open(LOCKFILE, O_RDWR | O_CREAT | O_EXCL,
                                         LOCKFILE_PERMS);
                                if (m_apilock == -1) {
                                        fprintf(stderr,
                                                "Couldn't create lock file: "
                                                "\"%s\". Error: \"%s\".\n",
                                                LOCKFILE, strerror(errno));
                                        pthread_mutex_unlock(&init_mutex);
                                        return -1;
                                }
                        } else {
                                /* Lock file is valid and owned by a living
                                 * process */
                                if (read_lockfile(&pid, &stime) == 0) {
                                        fprintf(stderr,
                                                "Lock file \"%s\" is already "
                                                "in use "
                                                "by PID %d\n",
                                                LOCKFILE, pid);
                                } else {
                                        fprintf(
                                            stderr,
                                            "Lock file is already in use: %s\n",
                                            LOCKFILE);
                                }
                                pthread_mutex_unlock(&init_mutex);
                                return -1;
                        }
                } else {
                        fprintf(stderr,
                                "Couldn't create lock file: %s. Error: %s\n",
                                LOCKFILE, strerror(errno));
                        pthread_mutex_unlock(&init_mutex);
                        return -1;
                }
        }

        /* Record PID and start time in lock file */
        m_pid = getpid();
        m_start_time = get_process_start_time(m_pid);

        if (write_lockfile(m_pid, m_start_time) != 0) {
                close(m_apilock);
                unlink(LOCKFILE);
                m_apilock = -1;
                fprintf(stderr, "Couldn't write lock file: %s.\n", LOCKFILE);
                pthread_mutex_unlock(&init_mutex);
                return -1;
        }

        if (pthread_mutex_init(&m_apilock_mutex, NULL) != 0) {
                close(m_apilock);
                unlink(LOCKFILE);
                m_apilock = -1;
                pthread_mutex_unlock(&init_mutex);
                return -1;
        }

        pthread_mutex_unlock(&init_mutex);
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

        /* Remove lock file on exit */
        if (unlink(LOCKFILE) != 0)
                ret = -1;

        m_apilock = -1;
        return ret;
}

void
lock_get(void)
{
        if (pthread_mutex_lock(&m_apilock_mutex) != 0)
                fprintf(stderr, "API mutex lock error: %s\n", strerror(errno));
}

void
lock_release(void)
{
        if (pthread_mutex_unlock(&m_apilock_mutex) != 0)
                fprintf(stderr, "API mutex unlock error: %s\n",
                        strerror(errno));
}
