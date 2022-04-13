/*
 * BSD LICENSE
 *
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
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

#include "monitor_text.h"

#include "common.h"
#include "monitor.h"
#include "monitor_utils.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

void
monitor_text_begin(FILE *fp)
{
        UNUSED_ARG(fp);
}

void
monitor_text_header(FILE *fp, const char *timestamp)
{
        enum pqos_mon_event events = monitor_get_events();
        enum monitor_llc_format format = monitor_get_llc_format();

        ASSERT(fp != NULL);
        ASSERT(timestamp != NULL);

        if (isatty(fileno(fp)))
                fprintf(fp, "\033[2J"     /* Clear screen */
                            "\033[0;0H"); /* move to position 0:0 */

        fprintf(fp, "TIME %s\n", timestamp);

        if (monitor_core_mode()) {
                fprintf(fp, "    CORE");
#ifdef PQOS_RMID_CUSTOM
                enum pqos_interface iface;

                pqos_get_inter(&iface);
                if (iface == PQOS_INTER_MSR)
                        fprintf(fp, " RMID");
#endif
        } else if (monitor_process_mode())
                fprintf(fp, "     PID     CORE");
        else if (monitor_uncore_mode())
                fprintf(fp, "  SOCKET");

        if (events & PQOS_PERF_EVENT_IPC)
                fprintf(fp, "         IPC");
        if (events & PQOS_PERF_EVENT_LLC_MISS)
                fprintf(fp, "      MISSES");
        if (events & PQOS_PERF_EVENT_LLC_REF)
                fprintf(fp, "  REFERENCES");
        if (events & PQOS_MON_EVENT_L3_OCCUP) {
                if (format == LLC_FORMAT_KILOBYTES)
                        fprintf(fp, "     LLC[KB]");
                else
                        fprintf(fp, "      LLC[%%]");
        }
        if (events & PQOS_MON_EVENT_LMEM_BW)
                fprintf(fp, "   MBL[MB/s]");
        if (events & PQOS_MON_EVENT_RMEM_BW)
                fprintf(fp, "   MBR[MB/s]");
        if (events & PQOS_MON_EVENT_TMEM_BW)
                fprintf(fp, "   MBT[MB/s]");

        if (events & PQOS_PERF_EVENT_LLC_MISS_PCIE_READ)
                fprintf(fp, " %11s", "MISS_READ");
        if (events & PQOS_PERF_EVENT_LLC_MISS_PCIE_WRITE)
                fprintf(fp, " %11s", "MISS_WRITE");
        if (events & PQOS_PERF_EVENT_LLC_REF_PCIE_READ)
                fprintf(fp, " %11s", "REF_READ");
        if (events & PQOS_PERF_EVENT_LLC_REF_PCIE_WRITE)
                fprintf(fp, " %11s", "REF_WRITE");
}

/**
 * @brief Fills in single text column in the monitoring table
 *
 * @param val numerical value to be put into the column
 * @param data place to put formatted column into
 * @param sz_data available size for the column
 * @param is_monitored if true then \a val holds valid data
 * @param is_column_present if true then corresponding event is
 *        selected for display
 * @return Number of characters added to \a data excluding NULL
 */
static size_t
fillin_text_column(const char *format,
                   const double val,
                   char data[],
                   const size_t sz_data,
                   const int is_monitored,
                   const int is_column_present)
{
        static const char blank_column[] = "            ";
        size_t offset = 0;

        if (sz_data <= sizeof(blank_column))
                return 0;

        if (is_monitored) {
                /**
                 * This is monitored and we have the data
                 */
                snprintf(data, sz_data - 1, format, val);
                offset = strlen(data);
        } else if (is_column_present) {
                /**
                 * The column exists though there's no data
                 */
                strncpy(data, blank_column, sz_data - 1);
                offset = strlen(data);
        }

        return offset;
}

void
monitor_text_row(FILE *fp,
                 const char *timestamp,
                 const struct pqos_mon_data *mon_data)
{
        const size_t sz_data = 256;
        char data[sz_data];
        size_t offset = 0;
        char core_list[16];
        enum pqos_mon_event events = monitor_get_events();
        unsigned i;

        ASSERT(fp != NULL);
        ASSERT(data != NULL);
        UNUSED_ARG(timestamp);

        memset(data, 0, sz_data);

#ifdef PQOS_RMID_CUSTOM
        enum pqos_interface iface;

        pqos_get_inter(&iface);
        if (iface == PQOS_INTER_MSR) {
                pqos_rmid_t rmid;
                int ret = pqos_mon_assoc_get(mon_data->cores[0], &rmid);

                offset += fillin_text_column(
                    " %4.0f", (double)rmid, data + offset, sz_data - offset,
                    ret == PQOS_RETVAL_OK, sel_interface == PQOS_INTER_MSR);
        }
#endif
        struct {
                enum pqos_mon_event event;
                unsigned unit;
                const char *format;
        } output[] = {
            {.event = PQOS_PERF_EVENT_IPC, .unit = 1, .format = " %11.2f"},
            {.event = PQOS_PERF_EVENT_LLC_MISS,
             .unit = 1000,
             .format = " %10.0fk"},
            {.event = PQOS_PERF_EVENT_LLC_REF,
             .unit = 1000,
             .format = " %10.0fk"},
            {.event = PQOS_MON_EVENT_L3_OCCUP, .unit = 1, .format = " %11.1f"},
            {.event = PQOS_MON_EVENT_LMEM_BW, .unit = 1, .format = " %11.1f"},
            {.event = PQOS_MON_EVENT_RMEM_BW, .unit = 1, .format = " %11.1f"},
            {.event = PQOS_MON_EVENT_TMEM_BW, .unit = 1, .format = " %11.1f"},
            {.event = PQOS_PERF_EVENT_LLC_MISS_PCIE_READ,
             .unit = 1000,
             .format = " %10.0fk"},
            {.event = PQOS_PERF_EVENT_LLC_MISS_PCIE_WRITE,
             .unit = 1000,
             .format = " %10.0fk"},
            {.event = PQOS_PERF_EVENT_LLC_REF_PCIE_READ,
             .unit = 1000,
             .format = " %10.0fk"},
            {.event = PQOS_PERF_EVENT_LLC_REF_PCIE_WRITE,
             .unit = 1000,
             .format = " %10.0fk"},
        };

        for (i = 0; i < DIM(output); i++) {
                double value =
                    monitor_utils_get_value(mon_data, output[i].event) /
                    output[i].unit;

                offset += fillin_text_column(output[i].format, value,
                                             data + offset, sz_data - offset,
                                             mon_data->event & output[i].event,
                                             events & output[i].event);
        }

        if (monitor_core_mode() || monitor_uncore_mode())
                fprintf(fp, "\n%8.8s%s", (char *)mon_data->context, data);
        else if (monitor_process_mode()) {
                memset(core_list, 0, sizeof(core_list));

                if (monitor_utils_get_pid_cores(mon_data, core_list,
                                                sizeof(core_list)) == -1) {
                        strncpy(core_list, "err", sizeof(core_list) - 1);
                }

                fprintf(fp, "\n%8.8s %8.8s%s", (char *)mon_data->context,
                        core_list, data);
        }
}

void
monitor_text_footer(FILE *fp)
{
        ASSERT(fp != NULL);

        if (!isatty(fileno(fp)))
                fputs("\n", fp);
}

void
monitor_text_end(FILE *fp)
{
        ASSERT(fp != NULL);

        if (isatty(fileno(fp)))
                fputs("\n\n", fp);
}
