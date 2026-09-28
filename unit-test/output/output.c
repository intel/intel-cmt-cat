/*
 * BSD LICENSE
 *
 * Copyright(c) 2022-2026 Intel Corporation. All rights reserved.
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
static int buffer_full = 0;
static int exit_code = 0;
static int exit_was_called = 0;

void
output_start(void)
{
        memset(buffer, 0, BUFFER_LENGTH + 1);
        chars_in_buffer = 0;
        buffer_full = 0;
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

/**
 * @brief Appends captured output to the buffer, and never past the end of it
 *
 * Every wrapper below goes through this, because they share one cursor: a
 * single unbounded write leaves chars_in_buffer past the end of the buffer, and
 * then the next write of any of them is to a pointer outside the buffer with a
 * negative remaining length - which strncpy() and snprintf() take as a size_t
 * and read as gigabytes of room. What each wrapper still returns is what the
 * function it stands in for would have returned, which is the formatted length
 * whether the capture kept it or not.
 *
 * @param [in] text what was formatted
 * @param [in] len its length, as the formatting reported it
 */
static void
append_to_buffer(const char *text, int len)
{
        int room;

        if (text == NULL || len <= 0)
                return;

        room = BUFFER_LENGTH - chars_in_buffer;
        if (room <= 0 || len > room) {
                /* said once per grab, and on the real stderr: a case that
                 * asserts on output it cannot see would otherwise pass or fail
                 * for a reason that is not in the buffer
                 */
                if (!buffer_full) {
                        buffer_full = 1;
                        fputs("output capture: the buffer is full, so the rest "
                              "of this run's output is not in it\n",
                              stderr);
                }
                if (room <= 0)
                        return;
                len = room;
        }

        memcpy(&buffer[chars_in_buffer], text, (size_t)len);
        chars_in_buffer += len;
        buffer[chars_in_buffer] = '\0';
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
                append_to_buffer(tmp_buff, str_len);
        }
        if (tmp_buff != NULL)
                free(tmp_buff);
        return str_len;
}

int __real_fprintf(FILE *stream, const char *format_string, ...);

/**
 * @brief Captures what the utility writes to a stream
 *
 * The utility reports its errors on stderr, and a case asking what it said has
 * to be able to see those too - so a write is captured while a grab is in
 * progress, whichever stream it was addressed to, and goes where it was
 * addressed otherwise.
 */
int
__wrap_fprintf(FILE *stream, const char *format_string, ...)
{
        int str_len = 0;
        char *tmp_buff = NULL;
        va_list args;

        if (!grab_in_progress) {
                va_start(args, format_string);
                str_len = vfprintf(stream, format_string, args);
                va_end(args);
                return str_len;
        }

        va_start(args, format_string);
        str_len = vasprintf(&tmp_buff, format_string, args);
        va_end(args);
        append_to_buffer(tmp_buff, str_len);
        if (tmp_buff != NULL)
                free(tmp_buff);

        return str_len;
}

int
__wrap_puts(const char *__s)
{
        if (grab_in_progress) {
                append_to_buffer(__s, (int)strlen(__s));
                append_to_buffer("\n", 1);
        }

        return strlen(__s);
}

int
__wrap_putchar(int __c)
{
        if (grab_in_progress) {
                const char one = (char)__c;

                append_to_buffer(&one, 1);
        }
        return __c;
}
