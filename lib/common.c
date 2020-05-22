/*
 * BSD LICENSE
 *
 * Copyright(c) 2019-2020 Intel Corporation. All rights reserved.
 * All rights reserved.
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

#include <stdio.h>
#include <sys/stat.h>
#include <string.h>

#include "pqos.h"
#include "common.h"
#include "log.h"

FILE *
pqos_fopen(const char *name, const char *mode)
{
        int fd;
        FILE *stream = NULL;
        struct stat lstat_val;
        struct stat fstat_val;

        stream = fopen(name, mode);
        if (stream == NULL)
                return stream;

        fd = fileno(stream);
        if (fd == -1)
                goto pqos_fopen_error;

        /* collect any link info about the file */
        if (lstat(name, &lstat_val) == -1)
                goto pqos_fopen_error;

        /* collect info about the opened file */
        if (fstat(fd, &fstat_val) == -1)
                goto pqos_fopen_error;

        /* we should not have followed a symbolic link */
        if (lstat_val.st_mode != fstat_val.st_mode ||
            lstat_val.st_ino != fstat_val.st_ino ||
            lstat_val.st_dev != fstat_val.st_dev) {
                LOG_ERROR("File %s is a symlink\n", name);
                goto pqos_fopen_error;
        }

        return stream;

pqos_fopen_error:
        if (stream != NULL)
                fclose(stream);

        return NULL;
}

char *
pqos_strcat(char *dst, const char *src, size_t size)
{
        return strncat(dst, src, size - strnlen(dst, size));
}
