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

#include "output.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUFFER_LENGTH 524288
static char buffer[BUFFER_LENGTH + 1];
static int grab_in_progress = 0;
static int chars_in_buffer = 0;
static int exit_code = 0;
static int exit_was_called = 0;

void
output_start(void)
{
        memset(buffer, 0, BUFFER_LENGTH + 1);
        chars_in_buffer = 0;
        exit_code = 0;
        exit_was_called = 0;
        grab_in_progress = 1;
}

void
output_stop(void)
{
        grab_in_progress = 0;
}

const char *
output_get(void)
{
        return buffer;
}

int
output_exit_was_called(void)
{
        return exit_was_called;
}

int
output_get_exit_status(void)
{
        return exit_code;
}

int
output_has_text(const char *format_string, ...)
{
        int ret = 0;

        if (format_string != NULL) {
                int str_len = 0;
                char *tmp_buff = NULL;
                va_list args;

                va_start(args, format_string);
                str_len = vasprintf(&tmp_buff, format_string, args);
                va_end(args);
                if (tmp_buff != NULL) {
                        if (str_len > 0 && strstr(buffer, tmp_buff) != NULL)
                                ret = 1;
                        free(tmp_buff);
                }
        }
        return ret;
}

__attribute__((noreturn)) void
__wrap_exit(int __status)
{
        exit_was_called = 1;
        exit_code = __status;
        longjmp(jump_buff, 0);
}

int
__wrap_printf(const char *format_string, ...)
{
        int str_len = 0;
        char *tmp_buff = NULL;

        if (grab_in_progress) {
                va_list args;

                va_start(args, format_string);
                str_len = vasprintf(&tmp_buff, format_string, args);
                va_end(args);
                if (str_len > 0) {
                        strncpy(&buffer[chars_in_buffer], tmp_buff,
                                BUFFER_LENGTH - chars_in_buffer);
                        chars_in_buffer += str_len;
                }
        }
        if (tmp_buff != NULL)
                free(tmp_buff);
        return str_len;
}

int
__wrap_puts(const char *__s)
{
        if (grab_in_progress) {
                chars_in_buffer +=
                    snprintf(&buffer[chars_in_buffer],
                             BUFFER_LENGTH - chars_in_buffer, "%s\n", __s);
        }

        return strlen(__s);
}

int
__wrap_putchar(int __c)
{
        if (grab_in_progress)
                buffer[chars_in_buffer++] = __c;
        return __c;
}
