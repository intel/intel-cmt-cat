/*
 * BSD LICENSE
 *
 * Copyright(c) 2020-2026 Intel Corporation. All rights reserved.
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

#include "mock_common.h"

#include "mock_test.h"
#include "pqos.h"

#include <string.h>

FILE *
__wrap_pqos_fopen(const char *name, const char *mode)
{
        check_expected(name);
        check_expected(mode);

        if (strcmp(mode, "r") == 0) {
                const char *data = mock_ptr_type(const char *);

                if (data == NULL)
                        return NULL;
                else {
                        FILE *fd = tmpfile();

                        assert_non_null(fd);

                        fprintf(fd, "%s", data);
                        fseek(fd, 0, SEEK_SET);

                        return fd;
                }
        }

        return mock_ptr_type(FILE *);
}

int
__wrap_pqos_fclose(FILE *fd)
{
        function_called();
        assert_non_null(fd);

        if (fd != NULL && fd != (FILE *)0xDEAD)
                fclose(fd);

        return mock_type(int);
}

int
__wrap_pqos_fread_uint(const char *path, unsigned *value)
{
        int ret;

        check_expected(path);
        assert_non_null(value);

        ret = mock_type(int);
        if (ret == PQOS_RETVAL_OK)
                *value = mock_type(unsigned);

        return ret;
}

int
__wrap_pqos_fread_uint64(const char *fname, unsigned base, uint64_t *value)
{
        int ret;

        check_expected(fname);
        check_expected(base);
        assert_non_null(value);

        ret = mock_type(int);
        if (ret == PQOS_RETVAL_OK)
                *value = mock_type(uint64_t);

        return ret;
}

int
__wrap_pqos_file_exists(const char *path)
{
        check_expected(path);

        return mock_type(int);
}

int
__wrap_pqos_dir_exists(const char *path)
{
        check_expected(path);

        return mock_type(int);
}

int
__wrap_pqos_file_contains(const char *fname, const char *str, int *found)
{
        int ret;

        check_expected(fname);
        check_expected(str);

        ret = mock_type(int);
        if (ret == PQOS_RETVAL_OK)
                *found = mock_type(int);

        return ret;
}

size_t
__wrap_fread(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
        char *data;
        size_t ret;

        assert_int_equal(nmemb, 1);
        assert_non_null(ptr);
        assert_non_null(stream);

        data = mock_ptr_type(char *);
        ret = strlen(data);

        if (ret > 0) {
                assert_true(size >= strlen(data));
                strncpy(ptr, data, size);
        }

        return ret;
}
