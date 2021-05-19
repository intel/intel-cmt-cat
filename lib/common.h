/*
 * BSD LICENSE
 *
 * Copyright(c) 2019-2021 Intel Corporation. All rights reserved.
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

/**
 * @brief Internal header file for common functions
 */

#ifndef __PQOS_COMMON_H__
#define __PQOS_COMMON_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdint.h>

#include "types.h"

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
PQOS_LOCAL FILE * pqos_fopen(const char *name, const char *mode);
/* clang-format on */

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

#ifdef __cplusplus
}
#endif

#endif /* __PQOS_COMMON_H__ */
