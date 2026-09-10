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

/**
 * @brief Internal header file for common functions
 */

#ifndef __PQOS_COMMON_H__
#define __PQOS_COMMON_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "types.h"

#include <stdint.h>
#include <stdio.h>
#include <sys/stat.h>

/**
 * @brief Wrapper around fopen() that fails if the file it names is a symbolic
 * link, refusing it before anything is opened, created or truncated
 *
 * The file is opened with O_NOFOLLOW and the stream is made from the
 * descriptor, so the name is resolved once and the kernel refuses a link. A
 * directory in the path leading to the file is resolved as open() would resolve
 * it, symbolic links included, which is the same promise pqos_open() makes.
 *
 * Every mode C defines is accepted - a first character of 'r', 'w' or 'a', then
 * '+' and 'b' in either order, and last of all the exclusive 'x' that C11 adds
 * for a 'w', making those modes "wx", "wbx", "w+x", "w+bx" and "wb+x". Any
 * other mode fails before the file is opened, so a caller asking for one of
 * the glibc extensions ('e', 'm', 'c') is told no, as is one writing 'b' or
 * 'x' where C does not put it, which cannot then turn into a mode that
 * truncates. An append stream is left where the fopen() of the platform would
 * have left it.
 *
 * @param [in] name a path to a file
 * @param [in] mode a file access mode
 *
 * @return Pointer to a file
 * @retval A valid pointer to a file, or NULL with errno set to the reason: the
 * errno of open() or fdopen(), which for a link is ELOOP on Linux, EMLINK on
 * FreeBSD and EEXIST under an 'x' mode - the flags come from the mode string
 * alone here, so those are all of them - and EINVAL where the mode is not one C
 * defines. A refused link is also reported through LOG_ERROR.
 */
PQOS_LOCAL FILE *pqos_fopen(const char *name, const char *mode);

/**
 * @brief Wrapper around fclose()
 *
 * @param [in] stream file descriptor
 *
 * @return Operational status
 * @retval 0 on success
 */
PQOS_LOCAL int pqos_fclose(FILE *stream);

/**
 * @brief Wrapper around open() that fails if the file it names is a symbolic
 * link
 *
 * The name itself, that is: O_NOFOLLOW is added to the flags, so the kernel
 * refuses a link at the name and resolves it once, while a directory in the
 * path leading to it is resolved as open() would resolve it, links included.
 * This is not a check on the whole path.
 *
 * @param [in] pathname a path to a file
 * @param [in] flags file access flags
 *
 * @return A file descriptor
 * @retval A valid file descriptor, or -1. errno is whatever open() reported,
 * and for a link at the name that is not one value but a function of the flags:
 * on Linux ELOOP for a plain open, ENOTDIR with O_DIRECTORY, EEXIST with
 * O_CREAT | O_EXCL, and on FreeBSD EMLINK. That is why nothing here decides
 * anything by reading it - a refused link is recognised by lstat()ing the name
 * after the failure and reported through LOG_ERROR. Two errnos this function
 * does set itself: EINVAL where no name was given, and ELOOP where O_PATH made
 * open() succeed on the link rather than refuse it.
 */
PQOS_LOCAL int pqos_open(const char *pathname, int flags);

/**
 * @brief Wrapper around strcat
 *
 * @param [out] dst Output buffer
 * @param [in] src Input buffer
 * @param [in] size Output buffer size
 *
 * @return output buffer
 */
PQOS_LOCAL char *pqos_strcat(char *dst, const char *src, size_t size);

/**
 * @brief Wrapper around fgets
 */
PQOS_LOCAL char *pqos_fgets(char *s, int n, FILE *stream);

/**
 * @brief Read integer from file
 *
 * @param [in] path file path
 * @param [out] value parsed value
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 */
PQOS_LOCAL int pqos_fread_uint(const char *path, unsigned *value);

/**
 * @brief Read uint64 from file
 *
 * @param [in] fname name of the file
 * @param [in] base numerical base
 * @param [out] value uint64 value
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK success
 */
PQOS_LOCAL int
pqos_fread_uint64(const char *fname, unsigned base, uint64_t *value);

/**
 * @brief Checks if file exists
 *
 * @param [in] path file path
 *
 * @return If file exists
 * @retval 1 if file exists
 * @retval 0 if file not exists
 */
PQOS_LOCAL int pqos_file_exists(const char *path);

/**
 * @brief Checks if directory exists
 *
 * @param [in] path directory path
 *
 * @return If file exists
 * @retval 1 if directory exists
 * @retval 0 if directory not exists
 */
PQOS_LOCAL int pqos_dir_exists(const char *path);

/**
 * @brief Checks file fname to detect str and sets a flag
 *
 * @param [in] fname name of the file to be searched
 * @param [in] str string being searched for
 * @param [out] found pointer to search result
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK success
 */
PQOS_LOCAL int
pqos_file_contains(const char *fname, const char *str, int *found);

/**
 * @brief Map physical memory for reading
 * @param[in] address Physical memory address
 * @param[in] size Memory size
 *
 * @return Mapped memory
 */
PQOS_LOCAL uint8_t *pqos_mmap_read(uint64_t address, const uint64_t size);

/**
 * @brief Map physical memory for writing
 * @param[in] address Physical memory address
 * @param[in] size Memory size
 *
 * @return Mapped memory
 */
PQOS_LOCAL uint8_t *pqos_mmap_write(uint64_t address, const uint64_t size);

/**
 * @brief Unmap physical memory
 *
 * @param[in] mem logical memory address
 * @param[in] size Memory size
 */
PQOS_LOCAL void pqos_munmap(void *mem, const uint64_t size);

/**
 * @brief Wrapper around read(2) that reads exactly count bytes
 * from the file handling exceptional situations and fails if it
 * cannot revover from those
 *
 * @param [in] fd file descriptor to read from
 * @param [in] buf buffer to read to
 * @param [in] count number of bytes to read
 *
 * @return Number of bytes read
 * @retval Number of bytes read equal 'count' param
 * @retval -1 if it couldn't read 'count' bytes
 */
PQOS_LOCAL ssize_t pqos_read(int fd, void *buf, size_t count);

/**
 * @brief Increase the number of open files limit to handle more
 * than 256 CPUs.
 *
 * @param [in] Max CPUs on the system
 *
 * @return PQOS_RETVAL_OK for success
 * @retval PQOS_RETVAL_ERROR for failure
 */
PQOS_LOCAL int pqos_set_no_files_limit(unsigned long max_core_count);

#ifdef __cplusplus
}
#endif

#endif /* __PQOS_COMMON_H__ */
