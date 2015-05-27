/*
 * BSD LICENSE
 * 
 * Copyright(c) 2014-2015 Intel Corporation. All rights reserved.
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
 * @brief Platform QoS sample LLC occupancy monitoring application
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include "pqos.h"
/**
 * Defines
 */
#define PQOS_MAX_SOCKETS      2
#define PQOS_MAX_SOCKET_CORES 64
#define PQOS_MAX_CORES        (PQOS_MAX_SOCKET_CORES*PQOS_MAX_SOCKETS)
#define PQOS_MAX_MON_EVENTS   1
/**
 * Number of cores that are selected in config string
 * for monitoring LLC occupancy
 */
static int sel_monitor_num = 0;
/**
 * The mask to tell which events to display
 */
static enum pqos_mon_event sel_events_max = 0;
/**
 * Maintains a table of core, event, number of events that are selected in
 * config string for monitoring LLC occupancy
 */
static struct {
        unsigned core;
        struct pqos_mon_data *pgrp;
        enum pqos_mon_event events;
} sel_monitor_tab[PQOS_MAX_CORES];
static struct pqos_mon_data m_mon_grps[PQOS_MAX_CORES];
/**
 * Stop monitoring indicator for infinite monitoring loop
 */
static int stop_monitoring_loop = 0;
/**
 * @brief CTRL-C handler for infinite monitoring loop
 *
 * @param signo signal number
 */
static void monitoring_ctrlc(int signo)
{
	printf("\nExiting[%d]... Press Enter\n", signo);
        stop_monitoring_loop = 1;
}
/**
 * @brief Gets scale factors to display events data
 *
 * LLC factor is scaled to kilobytes (1024 bytes = 1KB)
 * MBM factors are scales to megabytes (1024x104 bytes = 1MB)
 *
 * @param cap capability structure
 * @param llc_factor cache occupancy monitoring data
 * @param mbr_factor remote memory bandwidth monitoring data
 * @param mbl_factor local memory bandwidth monitoring data
 * @return operation status
 * @retval PQOS_RETVAL_OK on success
 */
static int
get_event_factors(const struct pqos_cap * const cap,
                  double * const llc_factor,
                  double * const mbr_factor,
                  double * const mbl_factor)
{
        if ((cap == NULL) || (llc_factor == NULL) ||
            (mbr_factor == NULL) || (mbl_factor == NULL))
                return PQOS_RETVAL_PARAM;
        const struct pqos_monitor *l3mon = NULL, *mbr_mon = NULL,
                *mbl_mon = NULL;
        int ret = PQOS_RETVAL_OK;

        if (sel_events_max & PQOS_MON_EVENT_L3_OCCUP) {
                ret = pqos_cap_get_event(cap, PQOS_MON_EVENT_L3_OCCUP, &l3mon);
                if (ret != PQOS_RETVAL_OK) {
                        printf("Failed to obtain LLC occupancy event data!\n");
                        return PQOS_RETVAL_ERROR;
                }
                *llc_factor = ((double)l3mon->scale_factor) / 1024.0;
        } else {
                *llc_factor = 1.0;
        }
        if (sel_events_max & PQOS_MON_EVENT_RMEM_BW) {
                ret = pqos_cap_get_event(cap, PQOS_MON_EVENT_RMEM_BW, &mbr_mon);
                if (ret != PQOS_RETVAL_OK) {
                        printf("Failed to obtain MBR event data!\n");
                        return PQOS_RETVAL_ERROR;
                }
                *mbr_factor = ((double) mbr_mon->scale_factor)
                        / (1024.0*1024.0);
        } else {
                *mbr_factor = 1.0;
        }
        if (sel_events_max & PQOS_MON_EVENT_LMEM_BW) {
                ret = pqos_cap_get_event(cap, PQOS_MON_EVENT_LMEM_BW, &mbl_mon);
                if (ret != PQOS_RETVAL_OK) {
                        printf("Failed to obtain MBL occupancy event data!\n");
                        return PQOS_RETVAL_ERROR;
                }
                *mbl_factor = ((double)mbl_mon->scale_factor) / (1024.0*1024.0);
        } else {
                *mbl_factor = 1.0;
        }
        return PQOS_RETVAL_OK;
}
/**
 * @brief Verifies and translates monitoring config string into
 *        internal monitoring configuration.
 *
 * @param argc Number of arguments in input command
 * @param argv Input arguments for LLC occupancy monitoring
 */
static void
monitoring_get_input(int argc, char *argv[])
{
	int numberOfCores = argc-1, i = 0;

	if (numberOfCores == 0)
		sel_monitor_num = 0;
	else if (!strcmp(argv[1], "-h") || !strcmp(argv[1], "-H")) {
		printf("Usage: %s [<core1> <core2> <core3> ...]\n", argv[0]);
		printf("Eg   : %s 1 2 6\n\n", argv[0]);
		sel_monitor_num = 0;
	} else {
		for (i = 0; i < numberOfCores; i++) {
			sel_monitor_tab[i].core = (unsigned)atoi(argv[i+1]);
			sel_monitor_tab[i].pgrp = &m_mon_grps[i];
			sel_monitor_num = (int) numberOfCores;
		}
	}
}
/**
 * @brief Starts monitoring on selected cores
 *
 * @param cpu_info cpu information structure
 *
 * @return Operation status
 * @retval 0 OK
 * @retval -1 error
 */
static int
setup_monitoring(const struct pqos_cpuinfo *cpu_info,
                 const struct pqos_capability const *cap_mon)
{
	unsigned i;

	for (i = 0; (unsigned)i < cap_mon->u.mon->num_events; i++)
                sel_events_max |= (cap_mon->u.mon->events[i].type);
        if (sel_monitor_num == 0) {
                for (i = 0; i < cpu_info->num_cores; i++) {
                        unsigned lcore = cpu_info->cores[i].lcore;

                        sel_monitor_tab[sel_monitor_num].core = lcore;
                        sel_monitor_tab[sel_monitor_num].events =
                                sel_events_max;
                        sel_monitor_tab[sel_monitor_num].pgrp =
                                &m_mon_grps[sel_monitor_num];
                        sel_monitor_num++;
                }
        }
	for (i = 0; i < (unsigned) sel_monitor_num; i++) {
		unsigned lcore = sel_monitor_tab[i].core;
                int ret;

		ret = pqos_mon_start(1, &lcore,
                                     sel_events_max,
                                     NULL,
                                     sel_monitor_tab[i].pgrp);
		if (ret != PQOS_RETVAL_OK) {
			printf("Monitoring start error on core %u,"
                               "status %d\n", lcore, ret);
			return ret;
		}
	}
	return PQOS_RETVAL_OK;
}
/**
 * @brief Stops monitoring on selected cores
 *
 */
static void stop_monitoring(void)
{
	unsigned i;

	for (i = 0; i < (unsigned) sel_monitor_num; i++) {
                int ret;

		ret = pqos_mon_stop(&m_mon_grps[i]);
		if (ret != PQOS_RETVAL_OK)
			printf("Monitoring stop error!\n");
	}
}
/**
 * @brief Reads monitoring event data at given \a interval for \a sel_time time span
 *
 * @param cap detected PQoS capabilites
 */
static void monitoring_loop(const struct pqos_cap *cap)
{
	double llc_factor = 1, mbr_factor = 1, mbl_factor = 1;
	int ret = PQOS_RETVAL_OK;
	struct pqos_mon_data mon_data[PQOS_MAX_CORES];
	unsigned mon_number = (unsigned)sel_monitor_num;
	unsigned i = 0;

	if (signal(SIGINT, monitoring_ctrlc) == SIG_ERR)
		printf("Failed to catch SIGINT!\n");
	ret = get_event_factors(cap, &llc_factor, &mbr_factor, &mbl_factor);
	if (ret != PQOS_RETVAL_OK)
		return;
	while (!stop_monitoring_loop) {
		ret = pqos_mon_poll(m_mon_grps, (unsigned)sel_monitor_num);
		if (ret != PQOS_RETVAL_OK) {
			printf("Failed to poll monitoring data!\n");
			return;
		}
		memcpy(mon_data, m_mon_grps, sel_monitor_num *
                       sizeof(m_mon_grps[0]));
		printf("SOCKET     CORE     RMID    LLC[KB]"
                       "    MBL[MB]    MBR[MB]\n");
		for (i = 0; i < mon_number; i++) {
			double llc =
                                ((double)(mon_data[i].values.llc * llc_factor));
			double mbr =
                                ((double)mon_data[i].values.mbm_remote_delta) *
                                mbr_factor;
			double mbl =
                                ((double)mon_data[i].values.mbm_local_delta) *
                                mbl_factor;

			printf("%6u %8u %8u %10.1f %10.1f %10.1f\n",
                               mon_data[i].socket,
                               mon_data[i].cores[0],
                               mon_data[i].rmid,
                               llc,
                               mbl,
                               mbr);
		}
		printf("\nPress Enter to continue or Ctrl+c to exit");
		if (getchar() != '\n')
			break;
		printf("\e[1;1H\e[2J");
	}
}

int main(int argc, char *argv[])
{
	struct pqos_config config;
	const struct pqos_cpuinfo *p_cpu = NULL;
	const struct pqos_cap *p_cap = NULL;
	int ret, exit_val = EXIT_SUCCESS;
	const struct pqos_capability *cap_mon = NULL;

	memset(&config, 0, sizeof(config));
        config.fd_log = STDOUT_FILENO;
        config.verbose = 0;
	/* PQoS Initialization - Check and initialize CAT and CMT capability */
	ret = pqos_init(&config);
	if (ret != PQOS_RETVAL_OK) {
		printf("Error initializing PQoS library!\n");
		exit_val = EXIT_FAILURE;
		goto error_exit;
	}
	/* Get CMT capability and CPU info pointer */
	ret = pqos_cap_get(&p_cap, &p_cpu);
	if (ret != PQOS_RETVAL_OK) {
		printf("Error retrieving PQoS capabilities!\n");
		exit_val = EXIT_FAILURE;
		goto error_exit;
	}
	/* Get input from user */
	monitoring_get_input(argc, argv);
	ret = pqos_cap_get_type(p_cap, PQOS_CAP_TYPE_MON, &cap_mon);
	/* Setup the monitoring resources */
	ret = setup_monitoring(p_cpu, cap_mon);
	if (ret != PQOS_RETVAL_OK) {
		printf("Error Setting up monitoring!\n");
                exit_val = EXIT_FAILURE;
                goto error_exit;
        }
	/* Start Monitoring */
	monitoring_loop(p_cap);
	/* Stop Monitoring */
	stop_monitoring();
 error_exit:
	ret = pqos_fini();
	if (ret != PQOS_RETVAL_OK)
		printf("Error shutting down PQoS library!\n");
	return exit_val;
}
