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

#include "log.h"
#include "test.h"

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* What a message longer than the payload buffer has to arrive as: all of what
 * fitted, so the buffer's capacity, and not merely something shorter than the
 * message started out as.
 */
#define LOG_PAYLOAD_MAX (AP_BUFFER_SIZE - 2)

/* what log_printf() hands its callback, per call */
static size_t callback_size;
static char callback_text[8 * 1024];
static unsigned callback_calls;

static void
log_callback(void *context __attribute__((unused)),
             const size_t size,
             const char *message)
{
        callback_calls++;
        callback_size = size;

        assert_true(size < sizeof(callback_text));
        memcpy(callback_text, message, size);
        callback_text[size] = '\0';
}

/* a message of a chosen length, built from one repeated character */
static char *
message_of(const size_t length)
{
        char *text = malloc(length + 1);

        assert_non_null(text);
        memset(text, 'x', length);
        text[length] = '\0';

        return text;
}

static int
log_setup(void **state __attribute__((unused)))
{
        callback_calls = 0;
        callback_size = 0;
        memset(callback_text, 0, sizeof(callback_text));

        assert_int_equal(
            log_init(-1, log_callback, NULL, LOG_VER_SUPER_VERBOSE),
            LOG_RETVAL_OK);

        return 0;
}

static int
log_teardown(void **state __attribute__((unused)))
{
        log_fini();

        return 0;
}

static void
test_log_printf_reports_what_it_wrote(void **state __attribute__((unused)))
{
        const char *text = "a short line\n";

        log_printf(LOG_OPT_INFO, "%s", text);

        assert_int_equal(callback_calls, 1);
        assert_int_equal(callback_size, strlen(text));
        assert_string_equal(callback_text, text);
}

static void
test_log_printf_clamps_a_long_message(void **state __attribute__((unused)))
{
        /* longer than the payload buffer, so vsnprintf() truncates and returns
         * the length the message needed rather than the length it wrote
         */
        char *text = message_of(4 * 1024);

        log_printf(LOG_OPT_INFO, "%s", text);
        free(text);

        assert_int_equal(callback_calls, 1);
        /* the length has to describe the text that was written - all of it, so
         * the buffer's capacity, and not some other value below what the
         * message started as
         */
        assert_int_equal(callback_size, LOG_PAYLOAD_MAX);
        assert_int_equal(callback_size, strlen(callback_text));
        /* and the text is the message, as far as it fitted */
        assert_int_equal(strspn(callback_text, "x"), LOG_PAYLOAD_MAX);
}

static void
test_log_printf_writes_the_same_length_to_a_file(void **state
                                                 __attribute__((unused)))
{
        char path[] = "/tmp/pqos-test-log-XXXXXX";
        char *text = message_of(4 * 1024);
        char written[8 * 1024];
        ssize_t length;
        int fd = mkstemp(path);

        assert_true(fd >= 0);
        log_fini();
        assert_int_equal(
            log_init(fd, log_callback, NULL, LOG_VER_SUPER_VERBOSE),
            LOG_RETVAL_OK);

        log_printf(LOG_OPT_INFO, "%s", text);
        free(text);

        assert_int_equal(lseek(fd, 0, SEEK_SET), 0);
        length = read(fd, written, sizeof(written));
        close(fd);
        unlink(path);

        /* the file gets exactly the truncated text, not the length the message
         * would have needed - which is what read past the end of the buffer
         */
        assert_true(length >= 0);
        assert_int_equal((size_t)length, LOG_PAYLOAD_MAX);
        assert_int_equal((size_t)length, callback_size);
        written[length] = '\0';
        assert_int_equal(strspn(written, "x"), LOG_PAYLOAD_MAX);
}

int
main(void)
{
        int result = 0;

        const struct CMUnitTest tests_log[] = {
            cmocka_unit_test_setup_teardown(
                test_log_printf_reports_what_it_wrote, log_setup, log_teardown),
            cmocka_unit_test_setup_teardown(
                test_log_printf_clamps_a_long_message, log_setup, log_teardown),
            cmocka_unit_test_setup_teardown(
                test_log_printf_writes_the_same_length_to_a_file, log_setup,
                log_teardown)};

        result += cmocka_run_group_tests(tests_log, NULL, NULL);

        return result;
}
