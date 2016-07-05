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
#include <string.h>
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

	if (g_cfg.verbose)
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
 * @param [in] argc number of cmd args
 * @param [in] argv cmd args
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

	if (g_cfg.verbose) {
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
		int status = EXIT_FAILURE;
		/* Wait for child */
		waitpid(pid, &status, 0);

		if (EXIT_SUCCESS != status)
			return -1;
	} else {
		/* set cpu affinity */
		if (0 != set_affinity(0)) {
			fprintf(stderr, "%s,%s:%d Failed to set core "
				"affinity!\n", __FILE__, __func__,
				__LINE__);
			_Exit(EXIT_FAILURE);
		}

		/* drop elevated root privileges */
		if (0 == g_cfg.sudo_keep && 0 != sudo_drop())
			_Exit(EXIT_FAILURE);

		errno = 0;
		/* execute command */
		execvp(argv[0], argv);

		fprintf(stderr, "%s,%s:%d Failed to execute %s, %s (%i) !\n",
				__FILE__, __func__, __LINE__,
				argv[0], strerror(errno), errno);

		_Exit(EXIT_FAILURE);
	}

	return 0;
}

/**
 * @brief Prints help page about usage
 *
 * @param [in] prgname executable name to be printed out
 * @param [in] short_usage flag to print short version
 */
static void
print_usage(char *prgname, unsigned short_usage)
{
	printf("Usage: %s <-t feature=value;...cpu=cpulist>... -c <cpulist> "
		"(-p <pid> | [-k] cmd [<args>...])\n"
		"       %s -r <cpulist> <-t feature=value;...cpu=cpulist>... "
		"-c <cpulist> (-p <pid> | [-k] cmd [<args>...])\n"
		"       %s -r <cpulist> -c <cpulist> "
		"(-p <pid> | [-k] cmd [<args>...])\n"
		"       %s -r <cpulist> <-t feature=value;...cpu=cpulist>... "
		"-p <pid>\n\n",
		prgname, prgname, prgname, prgname);

	printf("Options:\n"
		" -t/--rdt feature=value;...cpu=cpulist "
		"specify RDT configuration\n"
		"  Features:\n"
		"   2, l2\n"
		"   3, l3\n"
		" -c <cpulist>, --cpu <cpulist>         "
		"specify CPUs (affinity)\n"
		" -p <pid>, --pid <pid>                 "
		"operate on existing given pid\n"
		" -r <cpulist>, --reset <cpulist>       "
		"reset CAT for CPUs\n"
		" -k, --sudokeep                        "
		"do not drop sudo elevated privileges\n"
		" -v, --verbose                         "
		"prints out additional logging information\n"
		" -h, --help                            "
		"display help\n\n");

	if (short_usage) {
		printf("For more help run with -h/--help\n");
		return;
	}

	printf("Run \"id\" command on CPU 1 using four L3 cache-ways (mask 0xf)"
		",\nkeeping sudo elevated privileges:\n"
		"    -t 'l3=0xf;cpu=1' -c 1 -k id\n\n");

	printf("Examples CAT configuration strings:\n"
		"    -t 'l3=0xf;cpu=1'\n"
		"        CPU 1 uses four L3 cache-ways (mask 0xf)\n\n"

		"    -t 'l2=0x1;l3=0xf;cpu=1'\n"
		"        CPU 1 uses one L2 (mask 0x1) and four L3 (mask 0xf) "
		"cache-ways\n\n"

		"    -t 'l2=0x1;l3=0xf;cpu=1' -t 'l2=0x1;cpu=2'\n"
		"        CPU 1 uses one L2 (mask 0x1) and four L3 (mask 0xf) "
		"cache-ways\n"
		"        CPU 2 uses one L2 (mask 0x1) and default number of L3 "
		"cache-ways\n"
		"        L2 cache-ways used by CPU 1 and 2 are overlapping\n\n"

		"    -t 'l3=0xf;cpu=2' -t 'l3=0xf0;cpu=3,4,5'\n"
		"        CPU 2 uses four L3 cache-ways (mask 0xf), "
		"CPUs 3-5 share four L3 cache-ways\n"
		"        (mask 0xf0), L3 cache-ways used by CPU 2 and 3-4 are "
		"non-overlapping\n\n"

		"    -t 'l3=0xf;cpu=0-2' -t 'l3=0xf0;cpu=3,4,5'\n"
		"        CPUs 0-2 share four L3 cache-ways (mask 0xf), "
		"CPUs 3-5 share four L3 cache-ways\n"
		"        (mask 0xf0), L3 cache-ways used by CPUs 0-2 and 3-5 "
		"are non-overlapping\n\n"

		"    -t 'l3=0xf,0xf0;cpu=1'\n"
		"        On CDP enabled system, CPU 1 uses four L3 cache-ways "
		"for code (mask 0xf)\n"
		"        and four L3 cache-ways for data (mask 0xf0),\n"
		"        data and code L3 cache-ways are non-overlapping\n\n");

	printf("Example CPUs configuration string:\n"
		"    -c 0-3,4,5\n"
		"        CPUs 0,1,2,3,4,5\n\n");

	printf("Example RESET configuration string:\n"
		"    -r 0-3,4,5\n"
		"        reset CAT for CPUs 0,1,2,3,4,5\n\n");

	printf("Example usage of RESET option:\n"
		"    -t 'l3=0xf;cpu=0-2' -t 'l3=0xf0;cpu=3,4,5' -c 0-5 -p $BASHPID\n"
		"        Configure CAT and CPU affinity for BASH process\n\n"
		"    -r 0-5 -t 'l3=0xff;cpu=0-5' -c 0-5 -p $BASHPID\n"
		"        Change CAT configuration of CPUs used by BASH "
		"process\n\n"
		"    -r 0-5 -p $BASHPID\n"
		"        Reset CAT configuration of CPUs used by BASH "
		"process\n\n");
}
/**
 * @brief Validates arguments combination
 *
 * @param [in] f_r flag for -r argument
 * @param [in] f_t flag for -t arguments
 * @param [in] f_c flag for -c argument
 * @param [in] f_p flag for -p argument
 * @param [in] cmd flag for command to be executed
 *
 * @return Result
 * @retval 1 on success
 * @retval 0 on failure
 */
static int
validate_args(const int f_r, __attribute__((unused)) const int f_t,
		const int f_c, const int f_p, const int cmd)
{
	return (f_c && !f_p && cmd) ||
		(f_c && f_p && !cmd) ||
		(f_r && f_p && !cmd);
}

/**
 * @brief Parse the argument given in the command line of the application
 *
 * @param [in] argc number of args
 * @param [in] argv args
 *
 * @return 0 on success
 * @retval -EINVAL on error
 */
static int
parse_args(int argc, char **argv)
{
	int opt = 0;
	int retval = 0;
	char **argvopt = argv;

	static const struct option lgopts[] = {
		{ "cpu",	required_argument,	0, 'c' },
		{ "pid",	required_argument,	0, 'p' },
		{ "reset",	required_argument,	0, 'r' },
		{ "rdt",	required_argument,	0, 't' },
		{ "sudokeep",	no_argument,		0, 'k' },
		{ "verbose",	no_argument,		0, 'v' },
		{ "help",	no_argument,		0, 'h' },
		{ NULL, 0, 0, 0 } };

	while (-1 != (opt = getopt_long
			(argc, argvopt, "+c:p:r:t:kvh", lgopts, NULL))) {
		if (opt == 'c') {
			retval = parse_cpu(optarg);
			if (retval != 0) {
				fprintf(stderr, "Invalid CPU parameters!\n");
				goto exit;
			}
		} else if (opt == 'p') {
			char *tailp = NULL;

			errno = 0;
			g_cfg.pid = (pid_t) strtol(optarg, &tailp, 10);
			if (NULL == tailp || *tailp != '\0' || errno != 0 ||
					optarg == tailp || 0 == g_cfg.pid) {
				fprintf(stderr, "Invalid PID parameter!\n");
				retval = -EINVAL;
				goto exit;
			}
		} else if (opt == 'r') {
			retval = parse_reset(optarg);
			if (retval != 0) {
				fprintf(stderr, "Invalid RESET parameters!\n");
				goto exit;
			}
		} else if (opt == 't') {
			retval = parse_rdt(optarg);
			if (retval != 0) {
				fprintf(stderr, "Invalid RDT parameters!\n");
				goto exit;
			}
		} else if (opt == 'k') {
			g_cfg.sudo_keep = 1;
		} else if (opt == 'v') {
			g_cfg.verbose = 1;
		} else if (opt == '?') {
			retval = -EINVAL;
			goto exit;
		} else if (opt == 'h') {
			retval = -EAGAIN;
			goto exit;
		}
	}

exit:
	return retval;
}

/**
 * @brief main
 *
 * @param [in] argc number of args
 * @param [in] argv args
 */
int
main(int argc, char **argv)
{
	int ret = 0;

	memset(&g_cfg, 0, sizeof(g_cfg));

	/* Parse cmd line args */
	ret = parse_args(argc, argv);
	if (ret != 0) {
		if (-EINVAL == ret)
			printf("Incorrect argument value!\n");

		print_usage(argv[0], ret == -EINVAL ? 1 : 0);
		exit(EXIT_FAILURE);
	}

	if (argc - optind >= 1)
		g_cfg.command = 1;

	if (!validate_args(0 != CPU_COUNT(&g_cfg.reset_cpuset),
			0 != g_cfg.config_count,
			0 != CPU_COUNT(&g_cfg.cpu_aff_cpuset),
			0 != g_cfg.pid,
			0 != g_cfg.command)) {
		printf("Incorrect invocation!\n");
		print_usage(argv[0], 1);
		exit(EXIT_FAILURE);
	}

	/* Print cmd line configuration */
	if (g_cfg.verbose) {
		print_cmd_line_rdt_config();
		print_cmd_line_cpu_config();
	}

	/* Initialize the PQoS library and configure CAT */
	ret = cat_init();
	if (ret < 0) {
		fprintf(stderr, "%s,%s:%d CAT init failed!\n",
				__FILE__, __func__, __LINE__);
		exit(EXIT_FAILURE);
	}

	/* reset COS association */
	if (0 != CPU_COUNT(&g_cfg.reset_cpuset)) {
		if (g_cfg.verbose)
			printf("CAT: Resetting CAT configuration...\n");

		ret = cat_reset();
		if (ret != 0) {
			printf("CAT: Failed to reset COS association!\n");
			exit(EXIT_FAILURE);
		}
	}

	/* configure CAT */
	if (0 != g_cfg.config_count) {
		if (g_cfg.verbose)
			printf("CAT: Configuring CAT...\n");

		ret = cat_set();
		if (ret != 0) {
			printf("CAT: Failed to configure CAT!\n");
			cat_fini();
			_Exit(EXIT_FAILURE);
		}
	}

	/* execute command */
	if (0 != g_cfg.command) {
		if (g_cfg.verbose)
			printf("CMD: Executing command...\n");

		if (0 != execute_cmd(argc - optind, argv + optind))
			exit(EXIT_FAILURE);
	}

	/* set core affinity */
	if (0 != g_cfg.pid && 0 != CPU_COUNT(&g_cfg.cpu_aff_cpuset)) {
		if (g_cfg.verbose)
			printf("PID: Setting CPU affinity...\n");

		if (0 != set_affinity(g_cfg.pid)) {
			fprintf(stderr, "%s,%s:%d Failed to set core "
				"affinity!\n", __FILE__, __func__,
				__LINE__);
			exit(EXIT_FAILURE);
		}
	}

	if (0 != g_cfg.command)
		/*
		 * If we were running some command, do clean-up.
		 * Clean-up function is executed on process exit.
		 * (cat_exit() registered with atexit(...))
		 **/
		exit(EXIT_SUCCESS);
	else {
		/*
		 * If we were doing operation on PID or RESET,
		 * just deinit libpqos
		 **/
		cat_fini();
		_Exit(EXIT_SUCCESS);
	}
}
