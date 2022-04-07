/*
 * BSD LICENSE
 *
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
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
#ifndef __OUTPUT_H__
#define __OUTPUT_H__
#include <setjmp.h>

#define run_void_function(function_name, ...)                                  \
        do {                                                                   \
                output_start();                                                \
                if (!setjmp(jump_buff))                                        \
                        function_name(__VA_ARGS__);                            \
                output_stop();                                                 \
        } while (0)

#define run_function(function_name, ret_var, ...)                              \
        do {                                                                   \
                output_start();                                                \
                if (!setjmp(jump_buff))                                        \
                        retvar = function_name(__VA_ARGS__);                   \
                output_stop();                                                 \
        } while (0)

jmp_buf jump_buff;

void output_start(void);
void output_stop(void);
const char *output_get(void);
int output_exit_was_called(void);
int output_get_exit_status(void);
void __wrap_exit(int __status);
int __wrap_printf(const char *format_string, ...);
int __wrap_puts(const char *__s);
#endif /* __OUTPUT_H__ */
