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

#include "monitor_xml.h"

#include "common.h"
#include "monitor.h"
#include "monitor_utils.h"

#include <string.h>

static const char *xml_root_open = "<records>";
static const char *xml_root_close = "</records>";
static const char *xml_child_open = "<record>";
static const char *xml_child_close = "</record>";

void
monitor_xml_begin(FILE *fp)
{
        ASSERT(fp != NULL);

        fprintf(fp, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n%s\n",
                xml_root_open);
}

void
monitor_xml_header(FILE *fp, const char *timestamp)
{
        UNUSED_ARG(fp);
        UNUSED_ARG(timestamp);
}

/**
 * @brief Fills in single XML column in the monitoring table
 *
 * @param format numerical value format
 * @param val numerical value to be put into the column
 * @param data place to put formatted column into
 * @param sz_data available size for the column
 * @param is_monitored if true then \a val holds valid data
 * @param is_column_present if true then corresponding event is
 *        selected for display
 * @param node_name defines XML node name for the column
 * @return Number of characters added to \a data excluding NULL
 */
static size_t
fillin_xml_column(const char *const format,
                  const double val,
                  char data[],
                  const size_t sz_data,
                  const int is_monitored,
                  const int is_column_present,
                  const char node_name[])
{
        size_t offset = 0;

        if (is_monitored) {
                char formatted_val[16];

                snprintf(formatted_val, 15, format, val);

                /**
                 * This is monitored and we have the data
                 */
                snprintf(data, sz_data - 1, "\t<%s>%s</%s>\n", node_name,
                         formatted_val, node_name);
                offset = strlen(data);
        } else if (is_column_present) {
                /**
                 * The column exists though there's no data
                 */
                snprintf(data, sz_data - 1, "\t<%s></%s>\n", node_name,
                         node_name);
                offset = strlen(data);
        }

        return offset;
}

void
monitor_xml_row(FILE *fp,
                const char *timestamp,
                const struct pqos_mon_data *mon_data)
{
        enum pqos_mon_event events = monitor_get_events();
        enum monitor_llc_format format = monitor_get_llc_format();
        const size_t sz_data = 256;
        char data[sz_data];
        char core_list[1024];
        size_t offset = 0;
        const char *l3_text = NULL;

        ASSERT(fp != NULL);
        ASSERT(timestamp != NULL);
        ASSERT(mon_data != NULL);

        switch (format) {
        case LLC_FORMAT_KILOBYTES:
                l3_text = "l3_occupancy_kB";
                break;
        case LLC_FORMAT_PERCENT:
                l3_text = "l3_occupancy_percent";
                break;
        }

#ifdef PQOS_RMID_CUSTOM
        enum pqos_interface iface;

        pqos_get_inter(&iface);
        if (iface == PQOS_INTER_MSR) {
                pqos_rmid_t rmid;
                int ret = pqos_mon_assoc_get(mon_data->cores[0], &rmid);

                offset +=
                    fillin_xml_column("%.0f", rmid, data + offset,
                                      sz_data - offset, ret == PQOS_RETVAL_OK,
                                      sel_interface == PQOS_INTER_MSR, "rmid");
        }
#endif

        offset += fillin_xml_column(
            "%.2f", monitor_utils_get_value(mon_data, PQOS_PERF_EVENT_IPC),
            data + offset, sz_data - offset,
            mon_data->event & PQOS_PERF_EVENT_IPC, events & PQOS_PERF_EVENT_IPC,
            "ipc");

        offset += fillin_xml_column(
            "%.0f", monitor_utils_get_value(mon_data, PQOS_PERF_EVENT_LLC_MISS),
            data + offset, sz_data - offset,
            mon_data->event & PQOS_PERF_EVENT_LLC_MISS,
            events & PQOS_PERF_EVENT_LLC_MISS, "llc_misses");

        offset += fillin_xml_column(
            "%.0f", monitor_utils_get_value(mon_data, PQOS_PERF_EVENT_LLC_REF),
            data + offset, sz_data - offset,
            mon_data->event & PQOS_PERF_EVENT_LLC_REF,
            events & PQOS_PERF_EVENT_LLC_REF, "llc_references");

        offset += fillin_xml_column(
            "%.1f", monitor_utils_get_value(mon_data, PQOS_MON_EVENT_L3_OCCUP),
            data + offset, sz_data - offset,
            mon_data->event & PQOS_MON_EVENT_L3_OCCUP,
            events & PQOS_MON_EVENT_L3_OCCUP, l3_text);

        offset += fillin_xml_column(
            "%.1f", monitor_utils_get_value(mon_data, PQOS_MON_EVENT_LMEM_BW),
            data + offset, sz_data - offset,
            mon_data->event & PQOS_MON_EVENT_LMEM_BW,
            events & PQOS_MON_EVENT_LMEM_BW, "mbm_local_MB");

        offset += fillin_xml_column(
            "%.1f", monitor_utils_get_value(mon_data, PQOS_MON_EVENT_RMEM_BW),
            data + offset, sz_data - offset,
            mon_data->event & PQOS_MON_EVENT_RMEM_BW,
            events & PQOS_MON_EVENT_RMEM_BW, "mbm_remote_MB");

        fillin_xml_column(
            "%.1f", monitor_utils_get_value(mon_data, PQOS_MON_EVENT_TMEM_BW),
            data + offset, sz_data - offset,
            mon_data->event & PQOS_MON_EVENT_TMEM_BW,
            events & PQOS_MON_EVENT_TMEM_BW, "mbm_total_MB");

        fprintf(fp, "%s\n", xml_child_open);
        if (!monitor_process_mode())
                fprintf(fp,
                        "\t<time>%s</time>\n"
                        "\t<core>%s</core>\n"
                        "%s",
                        timestamp, (char *)mon_data->context, data);
        else {
                memset(core_list, 0, sizeof(core_list));

                if (monitor_utils_get_pid_cores(mon_data, core_list,
                                                sizeof(core_list)) == -1) {
                        strncpy(core_list, "err", sizeof(core_list) - 1);
                }

                fprintf(fp,
                        "\t<time>%s</time>\n"
                        "\t<pid>%s</pid>\n"
                        "\t<core>%s</core>\n"
                        "%s",
                        timestamp, (char *)mon_data->context, core_list, data);
        }
        fprintf(fp, "%s\n", xml_child_close);
}

void
monitor_xml_footer(FILE *fp)
{
        UNUSED_ARG(fp);
}

void
monitor_xml_end(FILE *fp)
{
        ASSERT(fp != NULL);

        fprintf(fp, "%s\n", xml_root_close);
}
