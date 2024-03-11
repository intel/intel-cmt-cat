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

#include "log.h"
#include "pqos.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <unistd.h>

FILE *
pqos_fopen(const char *name, const char *mode)
{
        int fd;
        FILE *stream = NULL;
        struct stat lstat_val;
        struct stat fstat_val;

        /* collect any link info about the file */
        /* coverity[fs_check_call] */
        if (lstat(name, &lstat_val) == -1)
                return NULL;

        stream = fopen(name, mode);
        if (stream == NULL)
                return stream;

        fd = fileno(stream);
        if (fd == -1)
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

int
pqos_fclose(FILE *stream)
{
        return fclose(stream);
}

int
pqos_open(const char *pathname, int flags)
{
        int fd;
        struct stat lstat_val;
        struct stat fstat_val;

        /* collect any link info about the file */
        /* coverity[fs_check_call] */
        if (lstat(pathname, &lstat_val) == -1)
                return -1;

        /* open the file */
        fd = open(pathname, flags);
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

char *
pqos_strcat(char *dst, const char *src, size_t size)
{
        return strncat(dst, src, size - strnlen(dst, size));
}

char *
pqos_fgets(char *s, int n, FILE *stream)
{
        char *line = NULL;
        size_t line_len = 0;
        ssize_t line_read;
        ssize_t i;

        line_read = getline(&line, &line_len, stream);
        if (line_read != -1) {
                char *p = strchr(line, '\n');

                if (p == NULL)
                        goto pqos_fgets_error;
                *p = '\0';

                if ((ssize_t)strlen(line) != line_read - 1 || line_read > n)
                        goto pqos_fgets_error;
                for (i = 0; i < line_read - 1; ++i)
                        if (!isascii(line[i]))
                                goto pqos_fgets_error;

                strncpy(s, line, n - 1);
                s[n - 1] = '\0';

                free(line);
                return s;
        }

pqos_fgets_error:
        if (line != NULL)
                free(line);

        return NULL;
}

/**
 * @brief Read integer from file
 *
 * @param [in] path file path
 * @param [out] value parsed value
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 */
int
pqos_fread_uint(const char *path, unsigned *value)
{
        int ret = PQOS_RETVAL_OK;
        FILE *fd;

        fd = pqos_fopen(path, "r");
        if (fd != NULL) {
                if (fscanf(fd, "%u", value) != 1)
                        ret = PQOS_RETVAL_ERROR;
                pqos_fclose(fd);
        } else
                return PQOS_RETVAL_RESOURCE;

        return ret;
}

int
pqos_fread_uint64(const char *fname, unsigned base, uint64_t *value)
{
        FILE *fd;
        char buf[17] = "\0";
        char *s = buf;
        char *endptr = NULL;
        size_t bytes;
        unsigned long long val;

        ASSERT(fname != NULL);
        ASSERT(value != NULL);

        fd = pqos_fopen(fname, "r");
        if (fd == NULL)
                return PQOS_RETVAL_ERROR;

        bytes = fread(buf, sizeof(buf) - 1, 1, fd);
        if (bytes == 0 && !feof(fd)) {
                pqos_fclose(fd);
                return PQOS_RETVAL_ERROR;
        }
        pqos_fclose(fd);

        val = strtoull(s, &endptr, base);

        if (!((*s != '\0' && *s != '\n') &&
              (*endptr == '\0' || *endptr == '\n'))) {
                LOG_ERROR("Error converting '%s' to unsigned number!\n", buf);
                return PQOS_RETVAL_ERROR;
        }
        if (val < UINT64_MAX) {
                *value = val;
                return PQOS_RETVAL_OK;
        }

        return PQOS_RETVAL_ERROR;
}

int
pqos_file_exists(const char *path)
{
        return access(path, F_OK) == 0;
}

/**
 * @brief Checks if directory exists
 *
 * @param [in] path file path
 *
 * @return If file exists
 * @retval 1 if file exists
 * @retval 0 if file not exists
 */
int
pqos_dir_exists(const char *path)
{
        struct stat st;

        return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

int
pqos_file_contains(const char *fname, const char *str, int *found)
{
        FILE *fd;
        char temp[1024];
        int check_symlink = 1;

        if (fname == NULL || str == NULL || found == NULL)
                return PQOS_RETVAL_PARAM;

        if (strncmp(fname, "/proc/", 6) == 0)
                check_symlink = 0;

        if (check_symlink)
                fd = pqos_fopen(fname, "r");
        else
                fd = fopen(fname, "r");

        if (fd == NULL) {
                LOG_DEBUG("%s not found.\n", fname);
                *found = 0;
                return PQOS_RETVAL_OK;
        }

        *found = 0;
        while (fgets(temp, sizeof(temp), fd) != NULL) {
                if (strstr(temp, str) != NULL) {
                        *found = 1;
                        break;
                }
        }

        fclose(fd);
        return PQOS_RETVAL_OK;
}

#define DEV_MEM "/dev/mem"

uint8_t *
pqos_mmap_read(uint64_t address, const uint64_t size)
{
        uint32_t offset;
        uint64_t page_size;
        uint8_t *mem;
        int fd;

        fd = pqos_open(DEV_MEM, O_RDONLY);
        if (fd < 0) {
                LOG_ERROR("Could not open %s\n", DEV_MEM);
                return NULL;
        }

        page_size = sysconf(_SC_PAGESIZE);
        offset = address % page_size;
        mem = mmap(NULL, size + offset, PROT_READ, MAP_PRIVATE, fd,
                   address - offset);

        if (mem == MAP_FAILED) {
                LOG_ERROR("Memory map failed, address=%llx size=%llu\n",
                          (unsigned long long)address,
                          (unsigned long long)size);
                close(fd);
                return NULL;
        }

        close(fd);
        return mem + offset;
}

uint8_t *
pqos_mmap_write(uint64_t address, const uint64_t size)
{
        uint32_t offset;
        uint64_t page_size;
        uint8_t *mem;
        int fd;

        fd = pqos_open(DEV_MEM, O_RDWR);
        if (fd < 0) {
                LOG_ERROR("Could not open %s\n", DEV_MEM);
                return NULL;
        }

        page_size = sysconf(_SC_PAGESIZE);
        offset = address % page_size;
        mem = mmap(NULL, size + offset, PROT_READ | PROT_WRITE, MAP_SHARED, fd,
                   address - offset);

        if (mem == MAP_FAILED) {
                LOG_ERROR("Memory map failed, address=%llx size=%llu\n",
                          (unsigned long long)address,
                          (unsigned long long)size);
                close(fd);
                return NULL;
        }

        close(fd);
        return mem + offset;
}

void
pqos_munmap(void *mem, const uint64_t size)
{
        uint64_t offset;
        uint64_t page_size;

        page_size = sysconf(_SC_PAGESIZE);
        offset = (uint64_t)mem % page_size;
        munmap((uint8_t *)mem - offset, size + offset);
}

ssize_t
pqos_read(int fd, void *buf, size_t count)
{
        int len = count;
        char *byte_ptr = (char *)buf;
        int ret;

        if (buf == NULL)
                return -1;

        while (len != 0 && (ret = read(fd, byte_ptr, len)) != 0) {
                if (ret == -1) {
                        if (errno == EINTR)
                                continue;
                        return ret;
                }

                len -= ret;
                byte_ptr += ret;
        }

        return count;
}

int
pqos_set_no_files_limit(unsigned long max_core_count)
{
        struct rlimit files_limit;

        if (getrlimit(RLIMIT_NOFILE, &files_limit))
                return PQOS_RETVAL_ERROR;

        /* Check if rlim_max can handle the number of CPUs */
        if (files_limit.rlim_max < max_core_count * 4)
                return PQOS_RETVAL_ERROR;

        if (files_limit.rlim_cur < max_core_count * 4) {
                files_limit.rlim_cur = max_core_count * 4;
                if (setrlimit(RLIMIT_NOFILE, &files_limit))
                        return PQOS_RETVAL_ERROR;
        }

        return PQOS_RETVAL_OK;
}
