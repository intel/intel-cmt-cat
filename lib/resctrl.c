/*
 * BSD LICENSE
 *
 * Copyright(c) 2018 Intel Corporation. All rights reserved.
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
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <stdlib.h>
#include <unistd.h>
#include <sys/file.h>
#include <signal.h>
#include <sys/mount.h>
#include <errno.h>

#include "pqos.h"
#include "log.h"
#include "types.h"
#include "resctrl.h"


static int resctrl_lock_fd = -1; /**< File descriptor to the lockfile */
static int shared = 0;
static int exclusive = 0;
/**
 * @brief Handle SIGALRM
 */
static void
resctrl_lock_signalhandler(int signal)
{
	UNUSED_PARAM(signal);
	/* File lock timeout */
}

/**
 * @brief Obtain lock on resctrl filesystem
 *
 * @param[in] type type of lock
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK on success
 */
static int
resctrl_lock(const int type)
{
	struct sigaction sa;
	int ret = PQOS_RETVAL_ERROR;

	ASSERT(type == LOCK_SH || type == LOCK_EX);

	resctrl_lock_fd = open(RESCTRL_PATH, O_DIRECTORY);
	if (resctrl_lock_fd < 0) {
		LOG_ERROR("Could not open %s directory\n", RESCTRL_PATH);
		return PQOS_RETVAL_ERROR;
	}

	/* Handle SIGALRM */
	sigaction(SIGALRM, NULL, &sa);
	sa.sa_handler = resctrl_lock_signalhandler;
	sa.sa_flags = 0;
	sigaction(SIGALRM, &sa, NULL);

	/* Set alarm when 100ms timeout occurs */
	ualarm(100000, 0);

	if (flock(resctrl_lock_fd, type) == 0) {
		ret = PQOS_RETVAL_OK;
		goto resctrl_lock_exit;
	}

	if (errno ==  EINTR)
		LOG_ERROR("Failed to acquire lock on resctrl filesystem - "
			  "timeout occured\n");
	else
		LOG_ERROR("Failed to acquire lock on resctrl filesystem - "
			  "%m\n");

	close(resctrl_lock_fd);
	resctrl_lock_fd = -1;

 resctrl_lock_exit:
	/* disable alarm */
	ualarm(0, 0);
	sa.sa_handler = SIG_DFL;

	return ret;
}

int
resctrl_lock_shared(void)
{
	int ret = PQOS_RETVAL_OK;

	if (exclusive == 0 && shared == 0)
		ret = resctrl_lock(LOCK_SH);

	if (ret == PQOS_RETVAL_OK)
		shared++;

	return ret;
}

int
resctrl_lock_exclusive(void)
{
	int ret = PQOS_RETVAL_OK;

	if (exclusive == 0)
		ret = resctrl_lock(LOCK_EX);

	if (ret == PQOS_RETVAL_OK)
		exclusive++;

	return ret;
}

int
resctrl_lock_release(void)
{
	if (shared > 0)
		shared--;
	else if (exclusive > 0)
		exclusive--;

	if (shared > 0 || exclusive > 0)
		return PQOS_RETVAL_OK;

	if (resctrl_lock_fd < 0) {
		LOG_ERROR("Resctrl filesystem not locked\n");
		return PQOS_RETVAL_ERROR;
	}

	if (flock(resctrl_lock_fd, LOCK_UN) != 0)
		LOG_WARN("Failed to release lock on resctrl filesystem\n");

	close(resctrl_lock_fd);
	resctrl_lock_fd = -1;

	return PQOS_RETVAL_OK;
}

int
resctrl_mount(const enum pqos_cdp_config l3_cdp_cfg,
              const enum pqos_cdp_config l2_cdp_cfg)
{
	const char *cdp_option = NULL; /**< cdp_off default */

	ASSERT(l3_cdp_cfg == PQOS_REQUIRE_CDP_ON ||
	       l3_cdp_cfg == PQOS_REQUIRE_CDP_OFF);
	ASSERT(l2_cdp_cfg == PQOS_REQUIRE_CDP_ON ||
	       l2_cdp_cfg == PQOS_REQUIRE_CDP_OFF);

        /* cdp mount options */
        if (l3_cdp_cfg == PQOS_REQUIRE_CDP_ON &&
                l2_cdp_cfg == PQOS_REQUIRE_CDP_ON)
                cdp_option = "cdp,cdpl2"; /**< both L3 CDP and L2 CDP on */
        else if (l3_cdp_cfg == PQOS_REQUIRE_CDP_ON)
                cdp_option = "cdp";  /**< L3 CDP on */
        else if (l2_cdp_cfg == PQOS_REQUIRE_CDP_ON)
                cdp_option = "cdpl2";  /**< L2 CDP on */

        if (mount("resctrl", RESCTRL_PATH, "resctrl", 0, cdp_option) != 0)
                return PQOS_RETVAL_ERROR;

        return PQOS_RETVAL_OK;
}

int
resctrl_umount(void)
{
        if (umount2(RESCTRL_PATH, 0) != 0) {
                LOG_ERROR("Could not umount resctrl filesystem!\n");
                return PQOS_RETVAL_ERROR;
        }

        return PQOS_RETVAL_OK;
}
