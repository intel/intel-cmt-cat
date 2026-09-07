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

#ifdef DEBUG
#include <assert.h>
#endif

#include "pqos.h"

#include <dirent.h> /**< scandir() */
#include <fnmatch.h>
#include <stdio.h>
#include <sys/stat.h>

#ifdef DEBUG
#define ASSERT assert
#else
#define ASSERT(x)
#endif

/**
 * Macros
 */
#ifndef DIM
#define DIM(x) (sizeof(x) / sizeof(x[0]))
#endif

#define UNUSED_ARG(_x) ((void)(_x))

#define MAX_DOMAINS 65535

#define MAX_DOMAIN_IDS 128
#define MAX_RMIDS      1024

#define PQOS_SYSTEM_CPU "/sys/devices/system/cpu"

/**
 * @brief Wrapper around fopen() that additionally checks if a given path
 * contains any symbolic links and fails if it does.
 *
 * @param [in] name a path to a file
 * @param [in] mode a file access mode
 *
 * @return Pointer to a file
 * @retval A valid pointer to a file or NULL on error (e.g. when the path
 * contains any symbolic links).
 */
/* clang-format off */
FILE *safe_fopen(const char *name, const char *mode);
/* clang-format on */

/**
 * @brief Wrapper around open() that fails if the file it names is a symbolic
 * link.
 *
 * The name itself, that is: a directory in the path leading to it is
 * resolved as open() would resolve it, links included, so this is not a
 * check on the whole path.
 *
 * @param [in] pathname a path to a file
 * @param [in] flags file access flags
 * @param [in] mode file mode bits
 *
 * @return A file descriptor
 * @retval A valid file descriptor or -1 on error, with errno ELOOP when the
 * name is a symbolic link
 */
int safe_open(const char *pathname, int flags, mode_t mode);

/**
 * @brief Common function to handle string parsing errors
 *
 * On error, this function causes process to exit with FAILURE code.
 *
 * @param arg string that caused error when parsing
 * @param note context and information about encountered error
 */
void parse_error(const char *arg, const char *note) __attribute__((noreturn));

/**
 * @brief Parse an unsigned integer without terminating the process
 *
 * Decimal is used by default. A 0x or 0X prefix selects hexadecimal.
 *
 * @param [in] text integer text
 * @param [out] value parsed value
 *
 * @retval 0 on success
 * @retval -1 on error
 */
int pqos_parse_uint64(const char *text, uint64_t *value);

/**
 * @brief Parse a list of memory regions
 *
 * An entry of the list is a single region or a first-last range, so "0",
 * "0,2" and "0-2" are all accepted, as they are by the allocation and the
 * domain options of the same commands.
 *
 * @param [in] arg memory region list
 * @param [out] regions parsed memory regions
 * @param [in] max_regions size of the regions array
 *
 * @return Number of parsed memory regions
 * @retval -1 on error
 */
int pqos_parse_mem_regions(const char *arg, int *regions, unsigned max_regions);

/**
 * @brief Retrieves the number of memory regions supported by the platform
 *
 * Wrapper around pqos_get_num_mem_regions() that reports the failure to the
 * user, so it is only usable after the library has been initialized. Valid
 * memory region numbers are 0 to *num_mem_regions - 1.
 *
 * @param [out] num_mem_regions number of supported memory regions
 *
 * @return Operation status
 * @retval 0 on success
 * @retval -1 if the information is not available
 */
int pqos_platform_mem_regions(unsigned *num_mem_regions);

/**
 * @brief Parse and validate a PCI identifier
 *
 * @param [in] arg PCI ID in [segment:]bus:device.function format
 * @param [in] allow_vc whether a virtual channel suffix is accepted
 * @param [out] segment PCI segment
 * @param [out] bdf encoded bus, device and function
 * @param [out] vc virtual channel suffix or NULL when not provided
 *
 * @retval 0 on success
 * @retval -1 on error
 */
int pqos_parse_pci_id(char *arg,
                      int allow_vc,
                      uint16_t *segment,
                      uint16_t *bdf,
                      char **vc);

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
int pqos_filter_cpu(const struct dirent *dir);

/**
 * @brief Sort directory filenames
 *
 * @param dir1 dirent structure containing directory info
 * @param dir2 dirent structure containing directory info
 *
 * @return directory names comparison result
 */
int pqos_cpu_sort(const struct dirent **dir1, const struct dirent **dir2);

#ifdef __cplusplus
}
#endif

#endif /* __PQOS_COMMON_H__ */
