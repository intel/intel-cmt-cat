/*
 *   BSD LICENSE
 *
 *   Copyright(c) 2016 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <errno.h>
#include <getopt.h>
#include <grp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/wait.h>

#include "rdt.h"
#include "common.h"
#include "cpu.h"

/**
 * @brief Detect if sudo was used to elevate privileges and drop them
 *
 * @return 0 on success
 * @retval -1 on error
 */
static int
sudo_drop(void)
{
	const char *sudo_uid = getenv("SUDO_UID");
	const char *sudo_gid = getenv("SUDO_GID");
	const char *sudo_user = getenv("SUDO_USER");

	/* Was sudo used to elevate privileges? */
	if (NULL == sudo_uid || NULL == sudo_gid || NULL == sudo_user)
		return 0;

	/* get user UID and GID */
	errno = 0;
	char *tailp = NULL;

	const uid_t uid = (uid_t) strtol(sudo_uid, &tailp, 10);

	if (NULL == tailp || *tailp != '\0' || errno != 0 ||
			sudo_uid == tailp || 0 == uid)
		goto err;

	errno = 0;
	tailp = NULL;
	const gid_t gid = (gid_t) strtol(sudo_gid, &tailp, 10);

	if (NULL == tailp || *tailp != '\0' || errno != 0 ||
			sudo_gid == tailp || 0 == gid)
		goto err;

	/* Drop privileges */
	if (0 != setgid(gid))
		goto err;

	if (0 != initgroups(sudo_user, gid))
		goto err;

	if (0 != setuid(uid))
		goto err;

	if (g_verbose)
		printf("Privileges dropped to uid: %d, gid: %d...\n", uid, gid);

	return 0;

err:
	fprintf(stderr, "Failed to drop privileges to uid: %s, "
			"gid: %s!\n", sudo_uid, sudo_gid);
	return -1;
}

/**
 * @brief Execute command (fork, exec, wait)
 *
 * @param argc number of cmd args
 * @param argv cmd args
 *
 * @return 0 on success
 * @retval -1 on error
 */
static int
execute_cmd(int argc, char **argv)
{
	int i = 0;

	if (0 >= argc || NULL == argv)
		return -1;

	if (g_verbose) {
		printf("Trying to execute ");
		for (i = 0; i < argc; i++)
			printf("%s ", argv[i]);

		printf("\n");
	}

	pid_t pid = fork();

	if (-1 == pid) {
		fprintf(stderr, "%s,%s:%d Failed to execute %s !"
				" fork failed\n", __FILE__, __func__, __LINE__,
				argv[0]);
		return -1;
	} else if (0 < pid) {
		/* Wait for child */
		waitpid(pid, NULL, 0);
	} else {
		/* set cpu affinity */
		if (0 != set_affinity(0)) {
			fprintf(stderr, "%s,%s:%d Failed to set core "
				"affinity!\n", __FILE__, __func__, __LINE__);
			_Exit(EXIT_FAILURE);
		}

		/* drop elevated root privileges */
		if (0 == g_sudo_keep && 0 != sudo_drop())
			_Exit(EXIT_FAILURE);

		/* execute command */
		execvp(argv[0], argv);

		fprintf(stderr, "%s,%s:%d Failed to execute %s !\n",
				__FILE__, __func__, __LINE__,
				argv[0]);

		_Exit(EXIT_FAILURE);
	}

	return 0;
}

static void
print_usage(char *prgname)
{
	printf("Usage: %s -3 <cbm@cpulist> -c <cpulist> [options] "
		"(-p <pid> | cmd [<args>...]])\n\n", prgname);

	printf("Options:\n"
	" -3 <cbm@cpulist>, --l3 <cbm@cpulist>  specify L3 CAT configuration\n"
	" -c <cpulist>, --cpu <cpulist>         specify CPUs\n"
	" -p <pid>, --pid <pid>                 operate on existing given pid\n"
	" -k, --sudokeep                        do not drop sudo elevated "
	"privileges\n"
	" -v, --verbose                         prints out additional logging "
	"information\n"
	" -h, --help                            display this help\n\n");

	printf("Run \"id\" command on CPU 1 using four L3 cache-ways (mask 0xf)"
		",\nkeeping sudo elevated privileges:\n"
		"    %s -3 '0xf@1' -c 1 -k id\n\n", prgname);

	printf("Examples L3 CAT configuration strings:\n"
		"    -3 '0xf@1'\n"
		"        CPU 1 uses four cache-ways (mask 0xf)\n\n"

		"    -3 '0xf@2,0xf0@(3,4,5)'\n"
		"        CPU 2 uses four cache-ways (mask 0xf), "
		"CPUs 3-5 share four cache-ways\n"
		"        (mask 0xf0), cache-ways used by CPU 2 and 3-4 are "
		"non-overlapping\n\n"

		"    -3 '0xf@(0-2),0xf0@(3,4,5)'\n"
		"        CPUs 0-2 share four cache-ways (mask 0xf), CPUs 3-5 "
		"share four cache-ways\n"
		"        (mask 0xf0), cache-ways used by CPUs 0-2 and 3-5 are "
		"non-overlapping\n\n"

		"    -3 '(0xf,0xf0)@1'\n"
		"        On CDP enabled system, CPU 1 uses four cache-ways for "
		"code (mask 0xf)\n"
		"        and four cache-ways for data (mask 0xf0),\n"
		"        data and code cache-ways are non-overlapping\n\n");

	printf("Example CPUs configuration string:\n"
		"    -c 1-3,4,5 - CPUs 1,2,3,4,5\n\n");
}

/**
 * @brief Parse the argument given in the command line of the application
 *
 * @param argc number of args
 * @param argv args
 *
 * @return 0 on success
 * @retval -EINVAL on error
 */
static int
parse_args(int argc, char **argv)
{
	int opt = 0;
	int retval = 0;
	int flag3 = 0;
	int flagc = 0;
	char **argvopt = argv;

	g_sudo_keep = 0;
	g_verbose = 0;
	g_pid = 0;

	static const struct option lgopts[] = {
		{ "l3",		required_argument,	0, '3' },
		{ "cpu",	required_argument,	0, 'c' },
		{ "pid",	required_argument,	0, 'p' },
		{ "sudokeep",	no_argument,		0, 'k' },
		{ "verbose",	no_argument,		0, 'v' },
		{ "help",	no_argument,		0, 'h' },
		{ NULL, 0, 0, 0 } };

	while (-1 != (opt = getopt_long
				(argc, argvopt, "+3:c:p:kvh", lgopts, NULL))) {
		if (opt == '3') {
			retval = parse_l3(optarg);
			if (retval != 0) {
				fprintf(stderr, "Invalid L3 parameters!\n");
				goto exit;
			}
			flag3 = 1;
		} else if (opt == 'c') {
			retval = parse_cpu(optarg);
			if (retval != 0) {
				fprintf(stderr, "Invalid CPU parameters!\n");
				goto exit;
			}
			flagc = 1;
		} else if (opt == 'p') {
			char *tailp = NULL;

			errno = 0;
			g_pid = (pid_t) strtol(optarg, &tailp, 10);
			if (NULL == tailp || *tailp != '\0' || errno != 0 ||
					optarg == tailp || 0 == g_pid) {
				fprintf(stderr, "Invalid PID parameter!\n");
				goto exit;
			}
		} else if (opt == 'k') {
			g_sudo_keep = 1;
		} else if (opt == 'v') {
			g_verbose = 1;
		} else if (opt == '?' || opt == 'h') {
			retval = -1;
			goto exit;
		}
	}

	if (!flag3) {
		fprintf(stderr, "Missing L3 parameters!\n");
		retval = -1;
	}

	if (!flagc) {
		fprintf(stderr, "Missing CPU parameters!\n");
		retval = -1;
	}

exit:
	return retval;
}

int
main(int argc, char **argv)
{
	int ret = 0;

	/* Parse cmd line args */
	if (0 != parse_args(argc, argv)) {
		print_usage(argv[0]);
		exit(EXIT_FAILURE);
	}

	/* If expecting cmd to be run, eg. PID was not provided via -p param */
	if (0 == g_pid && argc - optind < 1) {
		fprintf(stderr, "Missing executable name!\n");
		exit(EXIT_FAILURE);
	}

	/* Print cmd line configuration */
	if (g_verbose) {
		print_cmd_line_l3_config();
		print_cmd_line_cpu_config();
	}

	/* Initialize the PQoS library and configure L3 CAT */
	ret = cat_init();
	if (ret < 0) {
		fprintf(stderr, "%s,%s:%d L3 init failed!\n",
				__FILE__, __func__, __LINE__);
		exit(EXIT_FAILURE);
	}

	/* If no PID provided via -p param, execute command */
	if (0 == g_pid) {
		if (0 != execute_cmd(argc - optind, argv + optind))
			exit(EXIT_FAILURE);
	} else if (0 != set_affinity(g_pid)) {
		fprintf(stderr, "%s,%s:%d Failed to set core "
			"affinity!\n", __FILE__, __func__, __LINE__);
		exit(EXIT_FAILURE);
	}

	if (0 == g_pid)
		/*
		 * If we were running some command, do clean-up.
		 * Clean-up function is executed on process exit.
		 * (cat_exit() registered with atexit(...))
		 **/
		exit(EXIT_SUCCESS);
	else {
		/* If we were doing operation on PID, just deinit libpqos */
		cat_fini();
		_Exit(EXIT_SUCCESS);
	}
}

