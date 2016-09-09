/*
 * BSD LICENSE
 *
 * Copyright(c) 2014-2016 Intel Corporation. All rights reserved.
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
 * @brief Library operations logger for info, warnings and errors.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#ifdef __linux__
#include <error.h>
#endif /* __linux__ */
#include <errno.h>

#include "types.h"
#include "log.h"

/**
 * ---------------------------------------
 * Local data types
 * ---------------------------------------
 */


/**
 * ---------------------------------------
 * Local data structures
 * ---------------------------------------
 */

static FILE *m_file = NULL;             /**< file that log writes to */
static int m_opt = 0;                   /**< log options */
static int m_fd = -1;			/**< log file descriptor */
/**
 * ---------------------------------------
 * Local functions
 * ---------------------------------------
 */


/**
 * =======================================
 * initialize and shutdown
 * =======================================
 */

int
log_init(int fd, int opt)
{
        int ret = LOG_RETVAL_OK;

        if (m_file != NULL)
                return LOG_RETVAL_ERROR;

        m_file = fdopen(fd, "a");
        if (m_file == NULL) {
                fprintf(stderr, "%s: fdopen() failed: %s\n",
                       __func__, strerror(errno));
                return LOG_RETVAL_ERROR;
        }

        m_opt = opt;
		m_fd = fd;
        return ret;
}

int log_fini(void)
{
        int ret = LOG_RETVAL_OK;

        if (m_file == NULL)
                return LOG_RETVAL_ERROR;

        if ((m_fd != STDOUT_FILENO) && (m_fd != STDERR_FILENO))
                close(m_fd);

	m_file = NULL;
        m_opt = 0;
	m_fd = -1;

        return ret;
}

void log_printf(int type, const char *str, ...)
{
        va_list ap;

        if (m_file == NULL)
                return;

        if ((m_opt&type) == 0)
                return;

        va_start(ap, str);
        vfprintf(m_file, str, ap);
        va_end(ap);
}
