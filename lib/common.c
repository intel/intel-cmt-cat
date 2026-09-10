/*
 * BSD LICENSE
 *
 * Copyright(c) 2019-2026 Intel Corporation. All rights reserved.
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

/* Maximum required file descriptors per core */
#define MAX_FD_PER_CORE 5
/* pqos tool opens some file descriptors while using msr interface */
#define MAX_PQOS_FD 100

/**
 * The permissions a created file is given: 0666, which is what fopen() asks
 * for, narrowed by the umask exactly as it is there. Octal rather than the six
 * S_I* macros, which is the form the rest of this tree uses - lock.c spells the
 * same value LOCKFILE_PERMS 0666 - and the one checkpatch asks for.
 */
#define FOPEN_CREATE_PERMS 0666

/** Room for a mode string with the exclusive modifier taken out of it */
#define FOPEN_MODE_SIZE 8

/**
 * @brief Translate a mode string of fopen() into flags for open()
 *
 * Every mode C defines is translated, and the position of each character
 * matters. The first one carries the access, then '+' and 'b' in either order,
 * and last of all the exclusive 'x' that C11 adds for a 'w' - which makes the
 * exclusive modes exactly "wx", "wbx", "w+x", "w+bx" and "wb+x". Nothing else
 * is a mode C defines, so "wxb" and "wx+" are refused along with the glibc
 * extensions 'e', 'm' and 'c'; a 'b' or an 'x' accepted wherever it appeared
 * would turn "bw" into a translation of "w", and the file would be opened and
 * truncated before fdopen() refused the mode.
 *
 * 'x' asks for the file to be created and to fail if it is already there,
 * which is what O_EXCL does, and takes O_TRUNC out of the flags because a file
 * that cannot exist has nothing to truncate. It is also the one modifier with
 * no meaning for a descriptor that is already open, and glibc's fdopen()
 * refuses "w+x" outright, so it is left out of the mode the stream is made
 * with.
 *
 * This is the counterpart of the same function in the utility's pqos/common.c,
 * which safe_fopen() uses, and for now a change to the mode grammar has to be
 * made in both: pqos_fopen() is PQOS_LOCAL, so the utility cannot call it. The
 * duplication is deliberate and meant to be short-lived - the two wrappers are
 * to become one, the library's pair declared in the public header and the
 * utility's deleted - which is tracked apart from this.
 *
 * @param [in] mode the mode string as pqos_fopen() received it
 * @param [out] flags the flags to open() the file with
 * @param [out] stream_mode the mode fdopen() is given, the same string without
 *              an 'x'; at least FOPEN_MODE_SIZE bytes
 *
 * @return Operation status
 * @retval 0 the mode was translated
 * @retval -1 the mode is not one C defines
 */
static int
fopen_mode_flags(const char *mode, int *flags, char *stream_mode)
{
        int plus = 0;
        int binary = 0;
        int exclusive = 0;
        size_t used = 0;
        size_t i;

        /* refused before the walk below, which starts past the access character
         * and would read past the end of an empty mode
         */
        if (mode[0] == '\0')
                return -1;

        stream_mode[used++] = mode[0];

        for (i = 1; mode[i] != '\0'; i++) {
                if (mode[i] == '+' && !plus)
                        plus = 1;
                else if (mode[i] == 'b' && !binary)
                        binary = 1;
                else if (mode[i] == 'x' && !exclusive && mode[0] == 'w' &&
                         mode[i + 1] == '\0')
                        exclusive = 1;
                else
                        return -1;

                if (mode[i] == 'x')
                        continue;

                if (used >= FOPEN_MODE_SIZE - 1)
                        return -1;
                stream_mode[used++] = mode[i];
        }
        stream_mode[used] = '\0';

        switch (mode[0]) {
        case 'r':
                *flags = plus ? O_RDWR : O_RDONLY;
                break;
        case 'w':
                *flags = (plus ? O_RDWR : O_WRONLY) | O_CREAT;
                *flags |= exclusive ? O_EXCL : O_TRUNC;
                break;
        case 'a':
                *flags = (plus ? O_RDWR : O_WRONLY) | O_CREAT | O_APPEND;
                break;
        default:
                return -1;
        }

        return 0;
}

/**
 * @brief Tell whether fopen() would leave a stream opened with this mode at the
 *        end of the file
 *
 * The C libraries this is built for do not agree: FreeBSD seeks whenever
 * O_APPEND was asked for, "a+" included; glibc positions only a stream that
 * cannot be read, so "a" ends up at the end of the file and "a+" at the start;
 * musl positions none of them. A library this was not built against is taken to
 * behave as musl does and leave the offset alone. Same reasoning, and the same
 * duplication, as the copy in pqos/common.c.
 *
 * __GLIBC__ comes from <features.h> rather than the compiler, and any glibc
 * header pulls that in, so the test below only means anything underneath one;
 * <stdio.h> is included at the top of this file.
 *
 * @param [in] mode the mode string as pqos_fopen() received it
 *
 * @return Whether the stream belongs at the end of the file
 * @retval 1 fopen() would have positioned it there
 * @retval 0 fopen() would have left it where the descriptor is
 */
static int
fopen_positions_at_end(const char *mode)
{
        if (mode[0] != 'a')
                return 0;

#ifdef __FreeBSD__
        return 1;
#elif defined(__GLIBC__)
        return strchr(mode, '+') == NULL;
#else
        return 0;
#endif
}

FILE *
pqos_fopen(const char *name, const char *mode)
{
        int fd;
        int flags = 0;
        int error;
        char stream_mode[FOPEN_MODE_SIZE] = {0};
        FILE *stream;

        if (name == NULL || mode == NULL ||
            fopen_mode_flags(mode, &flags, stream_mode) != 0) {
                errno = EINVAL;

                return NULL;
        }

        /*
         * The kernel refuses a symbolic link here, before anything is opened,
         * created or truncated. What this replaced asked lstat() whether the
         * name was a link and then handed the name to fopen(), which resolved
         * it a second time: a link planted in between was followed, and a
         * writing mode - which lib/resctrl_monitoring.c uses - truncated the
         * file it pointed at before the comparison that came afterwards could
         * refuse the stream. No race was needed for a link already sitting at
         * the name.
         */
        fd = open(name, flags | O_NOFOLLOW, FOPEN_CREATE_PERMS);
        if (fd == -1) {
                struct stat lstat_val;

                /*
                 * What open() reports for a link it refused depends on the
                 * platform and on the flags - ELOOP on Linux, EMLINK on
                 * FreeBSD, EEXIST under O_EXCL - so the name is asked about
                 * instead, after the failure, where the answer only explains
                 * it. errno is put back around that, because lstat() and the
                 * logging are both allowed to change it and the caller is
                 * entitled to the reason the call failed.
                 */
                error = errno;
                if (lstat(name, &lstat_val) == 0 && S_ISLNK(lstat_val.st_mode))
                        LOG_ERROR("File %s is a symlink\n", name);
                errno = error;

                return NULL;
        }

        stream = fdopen(fd, stream_mode);
        if (stream == NULL) {
                error = errno;
                close(fd);
                errno = error;

                return NULL;
        }

        /*
         * fdopen() takes the offset the descriptor has, which open() leaves at
         * the start of the file, while fopen() may put an append stream at the
         * end of it. Writes land at the end either way - that is what O_APPEND
         * is for - but a caller that asks ftell() before writing gets what the
         * fopen() of this platform would have given it.
         */
        if (fopen_positions_at_end(mode)) {
                error = errno;
                (void)fseek(stream, 0, SEEK_END);
                errno = error;
        }

        return stream;
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
        int error;
        int creating = 0;

        if (pathname == NULL) {
                errno = EINVAL;

                return -1;
        }

        /*
         * O_NOFOLLOW, so the kernel refuses a link at the name and the name
         * is resolved once. What this replaced lstat()ed the name, opened it
         * and compared the two answers, which meant a link planted in between
         * had already been followed by the time it was refused.
         *
         * The mode is passed whenever open() will read one as its variadic
         * argument, which the call this replaced never supplied. That is
         * O_CREAT, and also O_TMPFILE, which this library is in reach of
         * because it is built with _GNU_SOURCE and which does not carry
         * O_CREAT in its bit pattern, so a caller asking for O_TMPFILE alone
         * would otherwise land in the branch with no mode. O_TMPFILE is more
         * than one bit, so the whole pattern is compared, and it exists only
         * on Linux.
         *
         * No caller in the tree asks for either today - both use /dev/mem -
         * so this closes the gap for the next one.
         */
        if (flags & O_CREAT)
                creating = 1;
#ifdef O_TMPFILE
        if ((flags & O_TMPFILE) == O_TMPFILE)
                creating = 1;
#endif

        if (creating)
                fd = open(pathname, flags | O_NOFOLLOW, FOPEN_CREATE_PERMS);
        else
                fd = open(pathname, flags | O_NOFOLLOW);
        if (fd == -1) {
                struct stat lstat_val;

                /* the errno of a refused link is not one thing, so the name is
                 * asked about instead, and errno is kept for the caller
                 */
                error = errno;
                if (lstat(pathname, &lstat_val) == 0 &&
                    S_ISLNK(lstat_val.st_mode))
                        LOG_ERROR("File %s is a symlink\n", pathname);
                errno = error;

                return -1;
        }

#ifdef O_PATH
        /*
         * O_PATH is the one flag that makes O_NOFOLLOW stop refusing a link:
         * the pair opens the *link* rather than what it points at, and every
         * other check here would agree with it. So a descriptor opened that way
         * is asked what it describes, and a link is refused with the answer the
         * rest of this function promises. Only reachable where O_PATH exists,
         * which is Linux, and only paid for when a caller asks for it.
         */
        if (flags & O_PATH) {
                struct stat fstat_val;

                if (fstat(fd, &fstat_val) == -1) {
                        error = errno;
                        close(fd);
                        errno = error;

                        return -1;
                }
                if (S_ISLNK(fstat_val.st_mode)) {
                        LOG_ERROR("File %s is a symlink\n", pathname);
                        close(fd);
                        errno = ELOOP;

                        return -1;
                }
        }
#endif

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
        uint64_t offset;
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
        uint64_t offset;
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
        size_t len = count;
        char *byte_ptr = (char *)buf;
        ssize_t ret;

        if (buf == NULL) {
                errno = EFAULT;
                return -1;
        }

        while (len != 0 && (ret = read(fd, byte_ptr, len)) != 0) {
                if (ret < 0) {
                        if (errno == EINTR)
                                continue;
                        return ret;
                }

                len -= (size_t)ret;
                byte_ptr += ret;
        }

        return count;
}

int
pqos_set_no_files_limit(unsigned long max_core_count)
{
        struct rlimit files_limit;
        const rlim_t required_fd =
            (max_core_count * MAX_FD_PER_CORE) + MAX_PQOS_FD;

        if (getrlimit(RLIMIT_NOFILE, &files_limit))
                return PQOS_RETVAL_ERROR;

        /* Check Kernel allows to open required file descriptors */
        if (files_limit.rlim_max < required_fd ||
            files_limit.rlim_cur < required_fd) {
                if (files_limit.rlim_max < required_fd)
                        files_limit.rlim_max = required_fd;

                files_limit.rlim_cur = required_fd;

                if (setrlimit(RLIMIT_NOFILE, &files_limit))
                        return PQOS_RETVAL_ERROR;
        }

        return PQOS_RETVAL_OK;
}
