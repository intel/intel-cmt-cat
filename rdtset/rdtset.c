/*
 *   BSD LICENSE
 *
 *   Copyright(c) 2016-2020 Intel Corporation. All rights reserved.
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
#include <signal.h>
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
#include "mba_sc.h"

static pid_t child = -1;

/**
 * @brief flushes output buffers and terminate
 *
 * @status exit status
 */
static void _exit_flush(int status) __attribute__((noreturn));

static void
_exit_flush(int status)
{
        fflush(NULL);
        _exit(status);
}

/**
 * @brief Detect if sudo was used to elevate privileges and drop them
 *
 * @return Operation status
 * @retval 0 on success
 * @retval -1 on error
 */
static int
sudo_drop(void)
{
        const char *sudo_uid = getenv("SUDO_UID");
        const char *sudo_gid = getenv("SUDO_GID");
        const char *sudo_user = getenv("SUDO_USER");
        uid_t uid;
        gid_t gid;
        char *tailp;

        /* Was sudo used to elevate privileges? */
        if (NULL == sudo_uid || NULL == sudo_gid || NULL == sudo_user)
                return 0;

        /* get user UID and GID */
        errno = 0;
        tailp = NULL;
        uid = (uid_t)strtol(sudo_uid, &tailp, 10);
        if (NULL == tailp || *tailp != '\0' || errno != 0 ||
            sudo_uid == tailp || 0 == uid)
                goto err;

        errno = 0;
        tailp = NULL;
        gid = (gid_t)strtol(sudo_gid, &tailp, 10);
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
        fprintf(stderr,
                "Failed to drop privileges to uid: %s, "
                "gid: %s!\n",
                sudo_uid, sudo_gid);
        return -1;
}

/**
 * @brief Execute command (fork, exec, wait)
 *
 * @param [in] argc number of cmd args
 * @param [in] argv cmd args
 *
 * @return Operation status
 * @retval 0 on success
 * @retval -1 on error
 */
static int
execute_cmd(int argc, char **argv)
{
        if (0 >= argc || NULL == argv)
                return -1;

        if (g_cfg.verbose) {
                int i;

                printf("Trying to execute ");
                for (i = 0; i < argc; i++)
                        printf("%s ", argv[i]);

                printf("\n");
        }

        child = fork();

        if (-1 == child) {
                fprintf(stderr,
                        "%s,%s:%d Failed to execute %s !"
                        " fork failed\n",
                        __FILE__, __func__, __LINE__, argv[0]);
                return -1;
        } else if (0 < child) {
                int status = EXIT_FAILURE;
                /* Wait for child */
                int ret = waitpid(child, &status, WNOHANG);

                if (ret < -1)
                        return -1;
                if (ret == child &&
                    (!WIFEXITED(status) && WEXITSTATUS(status) != EXIT_SUCCESS))
                        return -1;
        } else {
                if (0 != CPU_COUNT(&g_cfg.cpu_aff_cpuset))
                        /* set cpu affinity */
                        if (0 != set_affinity(0)) {
                                fprintf(stderr,
                                        "%s,%s:%d Failed to set core "
                                        "affinity!\n",
                                        __FILE__, __func__, __LINE__);
                                _exit_flush(EXIT_FAILURE);
                        }

                /* drop elevated root privileges */
                if (0 == g_cfg.sudo_keep && 0 != sudo_drop())
                        _exit_flush(EXIT_FAILURE);

                errno = 0;
                /* execute command */
                execvp(argv[0], argv);

                fprintf(stderr, "%s,%s:%d Failed to execute %s, %s (%i) !\n",
                        __FILE__, __func__, __LINE__, argv[0], strerror(errno),
                        errno);

                _exit_flush(EXIT_FAILURE);
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
        printf("Usage: %s -t <feature=value;...cpu=cpulist>... -c <cpulist> "
               "[-I] (-p <pidlist> | [-k] cmd [<args>...])\n"
               "       %s -r <cpulist> -t <feature=value;...cpu=cpulist>... "
               "-c <cpulist> [-I] (-p <pidlist> | [-k] cmd [<args>...])\n"
               "       %s -r <cpulist> -c <cpulist> "
               "(-p <pidlist> | [-k] cmd [<args>...])\n"
               "       %s -r <cpulist> -t <feature=value;...cpu=cpulist>... "
               "[-I] -p <pidlist>\n"
               "       %s -t <feature=value> -I [-c <cpulist>] "
               "(-p <pidlist> | [-k] cmd [<args>...])\n\n",
               prgname, prgname, prgname, prgname, prgname);

        printf("Options:\n"
               " -t/--rdt feature=value;...cpu=cpulist "
               "specify RDT configuration\n"
               "  Features:\n"
               "   2, l2\n"
               "   3, l3\n"
               "   m, mba\n"
               "   b, mba_max\n"
               " -c <cpulist>, --cpu <cpulist>         "
               "specify CPUs (affinity)\n"
               " -p <pidlist>, --pid <pidlist>                 "
               "operate on existing given pid\n"
               " -r <cpulist>, --reset <cpulist>       "
               "reset allocation for CPUs\n"
               " -k, --sudokeep                        "
               "do not drop sudo elevated privileges\n"
               " -v, --verbose                         "
               "prints out additional logging information\n"
               " -I, --iface-os                        "
               "set the library interface to use the kernel implementation\n"
               "                                       "
               "If not set the default implementation"
               " is to program the MSR's directly\n"
               " -h, --help                            "
               "display help\n"
               " -w, --version                         "
               "display PQoS library version\n\n");

        if (short_usage) {
                printf("For more help run with -h/--help\n");
                return;
        }

        printf("Run \"id\" command on CPU 1 using four L3 cache-ways (mask 0xf)"
               ",\nkeeping sudo elevated privileges:\n"
               "    -t 'l3=0xf;cpu=1' -c 1 -k id\n\n");

        printf(
            "Examples CAT/MBA configuration strings:\n"
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
            "        data and code L3 cache-ways are non-overlapping\n\n"

            "    -t 'mba=50;l3=0xf;cpu=1'\n"
            "        CPU 1 uses four L3 (mask 0xf) cache-ways and can utilize\n"
            "        up to 50%% of available memory bandwidth\n\n"

            "    -t 'mba_max=1200;cpu=1'\n"
            "        Use SW controller to limit local memory B/W to 1200MBps "
            "on core 1\n\n");

        printf("Example PID configuration strings:\n"
               "    -I -t 'l3=0xf' -p 23187,567-570\n"
               "        Specified processes use four L3 cache-ways (mask 0xf)\n"
               "    -I -t 'mba=50' -k memtester 10M\n"
               "        Restrict memory B/W availability to 50%% for the "
               "memtester application (using PID allocation)\n\n");

        printf("Example CPUs configuration string:\n"
               "    -c 0-3,4,5\n"
               "        CPUs 0,1,2,3,4,5\n\n");

        printf("Example RESET configuration string:\n"
               "    -r 0-3,4,5\n"
               "        reset allocation for CPUs 0,1,2,3,4,5\n\n");

        printf(
            "Example usage of RESET option:\n"
            "    -t 'l3=0xf;cpu=0-2' -t 'l3=0xf0;cpu=3,4,5' -c 0-5 -p "
            "$BASHPID\n"
            "        Configure allocation and CPU affinity for BASH process\n\n"
            "    -r 0-5 -t 'l3=0xff;cpu=0-5' -c 0-5 -p $BASHPID\n"
            "        Change allocation configuration of CPUs used by BASH "
            "process\n\n"
            "    -r 0-5 -p $BASHPID\n"
            "        Reset allocation configuration of CPUs used by BASH "
            "process\n\n");
}
/**
 * @brief Validates arguments combination
 *
 * @param [in] f_r flag for -r argument
 * @param [in] f_t flag for -t arguments
 * @param [in] f_c flag for -c argument
 * @param [in] f_p flag for -p argument
 * @param [in] f_i flag for -I argument
 * @param [in] cmd flag for command to be executed
 * @param [in] f_w flag for -w argument
 *
 * @return Operation status
 * @retval 1 on success
 * @retval 0 on error
 */
static int
validate_args(const int f_r,
              __attribute__((unused)) const int f_t,
              const int f_c,
              const int f_p,
              const int f_i,
              const int cmd,
              const int f_w)
{
        unsigned i;
        int f_n = 0; /**< non cpu (pid) config flag */

        for (i = 0; i < g_cfg.config_count; i++) {
                if (g_cfg.config[i].pid_cfg)
                        f_n++;
                /* Validate that only 1 pid config selected */
                if (f_n > 1) {
                        fprintf(stderr, "Only 1 PID config allowed!\n");
                        return 0;
                }
        }

        return (f_c && !f_p && cmd && !f_n) || (f_c && f_p && !cmd && !f_n) ||
               (f_r && f_p && !cmd) || (f_i && f_n && !f_p && cmd) ||
               (f_i && f_n && f_p && !cmd) || f_w;
}

/**
 * @brief Parse selected PIDs and add to PID table
 *
 * @param pidstr string containing list of PIDs to parse
 *
 * @return Operation status
 * @retval 0 on success
 * @retval negative on error
 */
static int
parse_pids(char *pidstr)
{
        unsigned i, n = 0;
        uint64_t pids[RDT_MAX_PIDS];

        if (pidstr == NULL)
                return -EINVAL;

        n = strlisttotab(pidstr, pids, DIM(pids));
        if (n == 0)
                return -EINVAL;

        if (n > (RDT_MAX_PIDS - g_cfg.pid_count)) {
                fprintf(stderr,
                        "Too many PIDs selected!"
                        "Max is %d...\n",
                        (int)RDT_MAX_PIDS);
                return -EINVAL;
        }

        /* Add selected PID's to config PID table */
        for (i = 0; i < n; i++)
                g_cfg.pids[g_cfg.pid_count++] = (pid_t)pids[i];

        return 0;
}

/**
 * @brief Parses the arguments given in the command line of the application
 *
 * @param [in] argc number of args
 * @param [in] argv args
 *
 * @return Operation status
 * @retval 0 on success
 * @retval -EINVAL on error
 */
static int
parse_args(int argc, char **argv)
{
        int opt = 0;
        int retval = 0;
        char **argvopt = argv;

        static const struct option lgopts[] = {
            /* clang-format off */
                { "cpu",        required_argument,      0, 'c' },
                { "pid",        required_argument,      0, 'p' },
                { "reset",      required_argument,      0, 'r' },
                { "rdt",        required_argument,      0, 't' },
                { "sudokeep",   no_argument,            0, 'k' },
                { "verbose",    no_argument,            0, 'v' },
                { "iface-os",   no_argument,            0, 'I' },
                { "help",       no_argument,            0, 'h' },
                { "version",    no_argument,            0, 'w' },
                { NULL, 0, 0, 0 }
            /* clang-format on */
        };

        while ((opt = getopt_long(argc, argvopt, "+c:p:r:t:kvIhw", lgopts,
                                  NULL)) != -1) {
                switch (opt) {
                case 'c':
                        retval = parse_cpu(optarg);
                        if (retval != 0) {
                                fprintf(stderr, "Invalid CPU parameters!\n");
                                goto exit;
                        }
                        break;
                case 'p':
                        retval = parse_pids(optarg);
                        if (retval != 0) {
                                fprintf(stderr, "Invalid PID parameters!\n");
                                goto exit;
                        }
                        break;
                case 'r':
                        retval = parse_reset(optarg);
                        if (retval != 0) {
                                fprintf(stderr, "Invalid RESET parameters!\n");
                                goto exit;
                        }
                        break;
                case 't':
                        retval = parse_rdt(optarg);
                        if (retval != 0) {
                                fprintf(stderr, "Invalid RDT parameters!\n");
                                goto exit;
                        }
                        break;
                case 'k':
                        g_cfg.sudo_keep = 1;
                        break;
                case 'v':
                        g_cfg.verbose = 1;
                        break;
                case '?':
                        retval = -EINVAL;
                        goto exit;
                case 'I':
                        g_cfg.interface = PQOS_INTER_OS;
                        break;
                case 'h':
                        retval = -EAGAIN;
                        goto exit;
                case 'w':
                        g_cfg.show_version = 1;
                        break;
                }
        }

exit:
        return retval;
}

/**
 * @brief Shut down all submodules
 */
static void
rdtset_fini(void)
{
        mba_sc_fini();
        alloc_fini();
}

/**
 * @brief Reverts settings deinitializes submodules
 */
static void
rdtset_exit(void)
{
        mba_sc_exit();
        alloc_exit();

        rdtset_fini();
}

/**
 * @brief Signal handler to do clean-up on exit on signal
 *
 * @param [in] signum signal
 */
static void
signal_handler(int signum)
{
        if (signum == SIGINT || signum == SIGTERM) {
                printf("\nRDTSET: Signal %d received, preparing to exit...\n",
                       signum);

                rdtset_exit();

                /* exit with the expected status */
                signal(signum, SIG_DFL);
                kill(getpid(), signum);
        }
}

/**
 * @brief Initialize rdtset submodules
 *
 * @return Operation status
 * @retval EXIT_SUCCESS on success
 * @retval EXIT_FAILURE on error
 */
static int
rdtset_init(void)
{
        int ret;

        /* Initialize the PQoS library and configure allocation */
        ret = alloc_init();
        if (ret < 0) {
                fprintf(stderr, "%s,%s:%d RDTSET: allocation init failed!\n",
                        __FILE__, __func__, __LINE__);
                ret = -EFAULT;
                goto err;
        }

        /* Initialize MBA SW controller */
        if (mba_sc_mode(&g_cfg)) {
                ret = mba_sc_init();
                if (ret < 0) {
                        fprintf(stderr, "%s,%s:%d MBA SC init failed!\n",
                                __FILE__, __func__, __LINE__);
                        ret = -EFAULT;
                        goto err;
                }
        }

        signal(SIGINT, signal_handler);
        signal(SIGTERM, signal_handler);

        ret = atexit(rdtset_exit);
        if (ret != 0) {
                ret = -EFAULT;
                fprintf(stderr, "%s,%s:%d Cannot set exit function\n", __FILE__,
                        __func__, __LINE__);
                goto err;
        }

        return 0;
err:
        rdtset_fini();
        return ret;
}

/**
 * @brief main function for rdtset
 *
 * Parses cmd line args and validates them,
 * initializes PQoS lib and configures CAT/MBA,
 * sets core affinity for executed command process or provided PID,
 * when running command, resets allocation configuration on exit.
 *
 * @param [in] argc number of args
 * @param [in] argv args
 *
 * @return Operation status
 * @retval EXIT_SUCCESS on success
 * @retval EXIT_FAILURE on error
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
                        fprintf(stderr, "Incorrect argument value!\n");

                print_usage(argv[0], ret == -EINVAL ? 1 : 0);
                exit(EXIT_FAILURE);
        }

        if (argc - optind >= 1)
                g_cfg.command = 1;

        if (!validate_args(0 != CPU_COUNT(&g_cfg.reset_cpuset),
                           0 != g_cfg.config_count,
                           0 != CPU_COUNT(&g_cfg.cpu_aff_cpuset),
                           0 != g_cfg.pid_count, 0 != g_cfg.interface,
                           0 != g_cfg.command, 0 != g_cfg.show_version)) {
                fprintf(stderr, "Incorrect invocation!\n");
                print_usage(argv[0], 1);
                exit(EXIT_FAILURE);
        }

        /* Print cmd line configuration */
        if (g_cfg.verbose) {
                print_cmd_line_rdt_config();
                print_cmd_line_cpu_config();
        }

        ret = rdtset_init();
        if (ret < 0)
                exit(EXIT_FAILURE);

        /* display library version */
        if (0 != g_cfg.show_version) {
                print_lib_version();
                exit(EXIT_SUCCESS);
        }

        /* reset COS association */
        if (0 != CPU_COUNT(&g_cfg.reset_cpuset)) {
                if (g_cfg.verbose)
                        printf("Allocation: Resetting allocation "
                               "configuration...\n");

                ret = alloc_reset();
                if (ret != 0) {
                        fprintf(stderr, "Allocation: Failed to reset COS "
                                        "association!\n");
                        exit(EXIT_FAILURE);
                }
        }

        /* configure CAT/MBA */
        if (0 != g_cfg.config_count) {
                if (g_cfg.verbose)
                        printf("Allocation: Configuring allocation...\n");

                ret = alloc_configure();
                if (ret != 0) {
                        fprintf(stderr, "Allocation: Failed to configure "
                                        "allocation!\n");
                        alloc_fini();
                        _exit_flush(EXIT_FAILURE);
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
        if (0 != g_cfg.pid_count && 0 != CPU_COUNT(&g_cfg.cpu_aff_cpuset)) {
                unsigned i;

                if (g_cfg.verbose)
                        printf("PID: Setting CPU affinity...\n");

                for (i = 0; i < g_cfg.pid_count; i++)
                        if (0 != set_affinity(g_cfg.pids[i])) {
                                fprintf(stderr,
                                        "%s,%s:%d Failed to set core "
                                        "affinity for pid %d!\n",
                                        __FILE__, __func__, __LINE__,
                                        (int)g_cfg.pids[i]);
                                rdtset_exit();
                                exit(EXIT_FAILURE);
                        }
        }

        if (mba_sc_mode(&g_cfg)) {
                mba_sc_main(child);

        } else if (0 != g_cfg.command) {
                int status = EXIT_FAILURE;
                /* Wait for child */
                waitpid(child, &status, 0);
                if (EXIT_SUCCESS != status)
                        exit(EXIT_FAILURE);
        }

        if (0 != g_cfg.command || mba_sc_mode(&g_cfg))
                /*
                 * If we were running some command or doing MBA SW control,
                 * do clean-up. Clean-up function is executed on process exit.
                 * (rdtset_exit() registered with atexit(...))
                 **/
                exit(EXIT_SUCCESS);
        else {
                /*
                 * If we were doing allocation only operation on PID or RESET,
                 * just deinit libpqos
                 **/
                rdtset_fini();
                _exit_flush(EXIT_SUCCESS);
        }
}
