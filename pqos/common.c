/*
 * BSD LICENSE
 *
 * Copyright(c) 2019-2023 Intel Corporation. All rights reserved.
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

#include "common.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

/**
 * @brief Filter directory filenames
 *
 * This function is used by the scandir function to filter directories
 *
 * @param dir dirent structure containing directory info
 *
 * @return if directory entry should be included in scandir() output list
 * @retval 0 means don't include the entry
 * @retval 1 means include the entry
 */
int
pqos_filter_cpu(const struct dirent *dir)
{
        return fnmatch("cpu[0-9]*", dir->d_name, 0) == 0;
}

/**
 * @brief Converts string into unsigned number.
 *
 * @param [in] str string to be converted into unsigned number
 * @param [out] value Numeric value of the string representing the number
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK on success
 */
static int
pqos_parse_uint(const char *str, unsigned *value)
{
        unsigned long val;
        char *endptr = NULL;

        ASSERT(str != NULL);
        ASSERT(value != NULL);

        errno = 0;
        val = strtoul(str, &endptr, 0);
        if (!(*str != '\0' && (*endptr == '\0' || *endptr == '\n')))
                return PQOS_RETVAL_ERROR;

        if (val <= UINT_MAX) {
                *value = val;
                return PQOS_RETVAL_OK;
        }

        return PQOS_RETVAL_ERROR;
}

__attribute__((noreturn)) void
parse_error(const char *arg, const char *note)
{
        printf("Error parsing \"%s\" command line argument. %s\n",
               arg ? arg : "<null>", note ? note : "");
        exit(EXIT_FAILURE);
}

FILE *
safe_fopen(const char *name, const char *mode)
{
        int fd;
        FILE *stream = NULL;
        struct stat lstat_val;
        struct stat fstat_val;
        int new_file = 0;

        /* collect any link info about the file */
        /* coverity[fs_check_call] */
        if (lstat(name, &lstat_val) == -1) {
                if (errno != ENOENT)
                        return NULL;
                else
                        new_file = 1;
        }

        stream = fopen(name, mode);
        if (stream == NULL)
                return stream;

        if (new_file && lstat(name, &lstat_val) == -1)
                goto safe_fopen_error;

        fd = fileno(stream);
        if (fd == -1)
                goto safe_fopen_error;

        /* collect info about the opened file */
        if (fstat(fd, &fstat_val) == -1)
                goto safe_fopen_error;

        /* we should not have followed a symbolic link */
        if (lstat_val.st_mode != fstat_val.st_mode ||
            lstat_val.st_ino != fstat_val.st_ino ||
            lstat_val.st_dev != fstat_val.st_dev) {
                printf("File %s is a symlink\n", name);
                goto safe_fopen_error;
        }

        return stream;

safe_fopen_error:
        if (stream != NULL)
                fclose(stream);

        return NULL;
}

int
safe_open(const char *pathname, int flags, mode_t mode)
{
        int fd;
        struct stat lstat_val;
        struct stat fstat_val;

        /* collect any link info about the file */
        /* coverity[fs_check_call] */
        if (lstat(pathname, &lstat_val) == -1)
                return -1;

        /* open the file */
        fd = open(pathname, flags, mode);
        if (fd == -1)
                return -1;

        /* collect info about the opened file */
        if (fstat(fd, &fstat_val) == -1) {
                close(fd);
                return -1;
        }

        /* we should not have followed a symbolic link */
        if (lstat_val.st_mode != fstat_val.st_mode ||
            lstat_val.st_ino != fstat_val.st_ino ||
            lstat_val.st_dev != fstat_val.st_dev) {
                printf("File %s is a symlink\n", pathname);
                close(fd);
                return -1;
        }

        return fd;
}

int
pqos_cpu_sort(const struct dirent **dir1, const struct dirent **dir2)
{
        unsigned cpu1 = 0;
        unsigned cpu2 = 0;

        pqos_parse_uint((*dir1)->d_name + 3, &cpu1);
        pqos_parse_uint((*dir2)->d_name + 3, &cpu2);

        return cpu1 - cpu2;
}
