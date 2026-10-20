/*
 * BSD LICENSE
 *
 * Copyright(c) 2014-2026 Intel Corporation. All rights reserved.
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

/**
 * Monitoring module
 */

#include "pqos.h"

#include <stdint.h>
#include <stdio.h>

#ifndef MONITOR_H_
#define MONITOR_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Verifies and translates multiple monitoring config strings into
 *        internal PID monitoring configuration
 *
 * @param arg argument passed to -p command line option
 */
void selfn_monitor_pids(const char *arg);

/**
 * @brief Looks for processes with highest CPU usage on the system and
 *        starts monitoring for them. Processes are displayed and sorted
 *        afterwards by LLC occupancy
 */
void selfn_monitor_top_pids(void);

/**
 * @brief Selects top-like monitoring format
 *
 * @param arg not used
 */
void selfn_monitor_top_like(const char *arg);

/**
 * @brief Selects monitoring interval
 *
 * @param arg string passed to -i command line option
 */
void selfn_monitor_interval(const char *arg);

/**
 * @brief Selects monitoring time
 *
 * @param arg string passed to -t command line option
 */
void selfn_monitor_time(const char *arg);

/**
 * @brief Selects type of monitoring output file
 *
 * @param arg string passed to -u command line option
 */
void selfn_monitor_file_type(const char *arg);

/**
 * @brief Selects monitoring output file
 *
 * @param arg string passed to -o command line option
 */
void selfn_monitor_file(const char *arg);

/**
 * @brief Translates multiple monitoring request strings into
 *        internal monitoring request structures
 *
 * @param str string passed to -m command line option
 */
void selfn_monitor_cores(const char *arg);

#ifdef PQOS_RMID_CUSTOM
/**
 * @brief Selects custom RMID mapping for cores
 *
 * @param arg string passed to --rmid command line option
 */
void selfn_monitor_rmid_cores(const char *arg);
#endif

/**
 * @brief Translates multiple monitoring request strings into
 *        internal device monitoring request structures
 *
 * @param str string passed to --mon-dev command line option
 */
void selfn_monitor_devs(const char *arg);

/**
 * @brief Translates multiple monitoring request strings into
 *        internal channel monitoring request structures
 *
 * @param str string passed to --mon-channel command line option
 */
void selfn_monitor_channels(const char *arg);

/**
 * @brief Translates multiple ddio monitoring request strings into
 *        internal monitoring request structures
 *
 * @param str string passed to --mon-ddio command line option
 */
void selfn_monitor_uncore(const char *arg);

/**
 * @brief Allows to use LLC-percent representation (by default LLC
 *        values are represented in kilobytes)
 *
 */
void selfn_monitor_set_llc_percent(void);

/*
 * @brief Allows to disable IPC monitoring
 */
void selfn_monitor_disable_ipc(const char *arg);

/*
 * @brief Allows to disable LLC misses monitoring
 */
void selfn_monitor_disable_llc_miss(const char *arg);

#ifdef PQOS_RMID_CUSTOM
/**
 * @brief Selects custom RMID mapping for channels
 *
 * @param arg string passed to --rmid-channels command line option
 */
void selfn_monitor_rmid_channels(const char *arg);
#endif

/**
 * @brief Selects memory regions for monitoring
 *
 * @param arg not used
 */
void selfn_mon_mem_regions(const char *arg);

/**
 * @brief Check to determine if processes are monitored
 *
 * @return Process monitoring mode status
 * @retval 0 not monitoring processes
 * @retval 1 monitoring processes
 */
int monitor_process_mode(void);

/**
 * @brief Check to determine if cores are monitored
 *
 * @return Core monitoring mode status
 * @retval 0 not monitoring cores
 * @retval 1 monitoring cores
 */
int monitor_core_mode(void);

/**
 * @brief Check to determine if channel/devices are monitored
 *
 * @return Process monitoring mode status
 * @retval 0 not monitoring channel/devices
 * @retval 1 monitoring channel/devices
 */
int monitor_iordt_mode(void);

/**
 * @brief Check to determine if uncore monitoring is started
 *
 * @return Uncore monitoring mode status
 */
int monitor_uncore_mode(void);

/**
 * @brief Check to determine if combined core and I/O (MMIO) monitoring is used
 *
 * @return Mixed monitoring mode status
 * @retval 0 not in mixed core + I/O monitoring mode
 * @retval 1 in mixed core + I/O monitoring mode
 */
int monitor_mixed_mode(void);

/**
 * @brief Check if monitoring was requested on the command line
 *
 * True when a monitoring target was selected with one of -m, -p, --mon-core,
 * --mon-pid, --mon-dev, --mon-channel or --mon-uncore, and also when only a
 * monitoring option that relies on the default all core monitoring was given,
 * for example -t, -i, -T, -o, -u, --percent-llc, --mon-mem-regions or the
 * --disable-mon-* opt-outs.
 *
 * @return Monitoring request status
 * @retval 0 no monitoring option selected
 * @retval 1 monitoring option selected
 */
int monitor_requested(void);

/**
 * @brief Retrieve the number of memory regions selected for monitoring
 *
 * @return number of memory regions
 */
int monitor_get_num_mem_regions(void);

/**
 * @brief Stops monitoring on selected core(s)/pid(s)
 */
void monitor_stop(void);

/**
 * @brief Starts monitoring on selected core(s)/pid(s)
 *
 * @param [in] cpu_info cpu information structure
 * @param [in] cap_mon monitoring capability
 * @param [in] dev_info IO RDT topology
 *
 * @return Operation status
 * @retval 0 OK
 * @retval -1 error
 */
int monitor_setup(const struct pqos_cpuinfo *cpu_info,
                  const struct pqos_capability *const cap_mon,
                  const struct pqos_devinfo *dev_info);

/**
 * @brief Frees any allocated memory during parameter selection and
 *        monitoring setup.
 */
void monitor_cleanup(void);

/**
 * @brief Monitors resources and writes data into selected stream.
 *
 * @return 0 when the monitoring ran until its timeout or a stop, or until
 *         more intervals passed between two reads of its timer than it counts
 *         - the process was stopped, or its output blocked - which it reports
 *         as "Failed to read timer" but is no failure of the run, and its
 *         output took all that was flushed to it: the end of the output is
 *         left for monitor_flush() to write, and, once a stop is requested,
 *         what the loop has not flushed. A write that failed to stdout, which
 *         a counter's overflow is reported on, where -o named another output,
 *         does not end the monitoring: the loop leaves it to monitor_flush()
 *         to say, and reports no overflow there from then on.
 *         -1 where it could not start or an error ended it, having said why,
 *         or where a write to its output failed, as to a terminal that has
 *         hung up, a file on a full file system or a pipe whose reader has
 *         gone, which it leaves monitor_flush() to say.
 *         Either way the caller stops the monitoring and finalises the
 *         library, which an exit from in here would skip, and then calls
 *         monitor_flush()
 */
int monitor_loop(void);

/**
 * @brief Where the loop left the end of the monitoring output, writes it,
 *        which closes the XML root, flushes it, and closes a file that -o
 *        named, flushing stdout then as well; and says what write failed
 *
 * monitor_loop() leaves the end to this: a pipe whose reader has stopped
 * reading blocks its write - at once where the stream is line buffered or
 * unbuffered, at the flush where it is fully buffered - and the groups and
 * the library lock would be held until it read again. The caller writes it
 * here, once it has stopped the monitoring and finalised the library. The
 * loop leaves the failure of a write to this as well, for the same reason: a
 * pipe on stderr whose reader has stopped reading blocks the message.
 *
 * The file is closed here rather than by monitor_cleanup(), as a file
 * system such as NFS can report a write that failed only at the close.
 * stdout is left open, for exit() to close; but where -o named a file, stdout
 * is flushed here and checked, the loop flushing nothing once a stop is
 * requested: a stop that comes with a counter's overflow leaves the
 * overflow's message there.
 *
 * Where the loop left no end - a write to its output failed, or it did not
 * get as far as its output - this writes none, and flushes and closes
 * nothing: a file that -o named is left for monitor_cleanup() to close,
 * unchecked. It then only says the failures that the loop recorded.
 *
 * @return Operation status
 * @retval 0 no write failed: the output took its end, or the loop left no
 *         end to write, not having got as far as its output
 * @retval -1 a write failed - in the loop, to the output, or to stdout where
 *         -o named another output; or here, the end's, the file's close or
 *         stdout's flush - which this says on stderr, the first failure of
 *         each stream only
 */
int monitor_flush(void);

/**
 * @brief Retrieve monitoring interval
 *
 * @return monitoring interval
 */
int monitor_get_interval(void);

/**
 * @brief List of events being monitored
 *
 * @return monitoring events
 */
enum pqos_mon_event monitor_get_events(void);

/** Monitor LLC format */
enum monitor_llc_format {
        LLC_FORMAT_KILOBYTES = 0, /**< LLC in kB */
        LLC_FORMAT_PERCENT        /**< LLC in percent */
};

enum monitor_llc_format monitor_get_llc_format(void);

#ifdef __cplusplus
}
#endif

#endif /* MONITOR_H_ */
