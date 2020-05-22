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

/**
 * @brief Internal header file for common functions
 */

#ifndef __PQOS_COMMON_H__
#define __PQOS_COMMON_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

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
FILE * pqos_fopen(const char *name, const char *mode);
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
char *pqos_strcat(char *dst, const char *src, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* __PQOS_COMMON_H__ */
