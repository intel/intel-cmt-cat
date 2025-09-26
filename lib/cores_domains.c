/*
 * BSD LICENSE
 *
 * Copyright(c) 2023 Intel Corporation. All rights reserved.
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

#include "cores_domains.h"

#include <stdlib.h>
#include <string.h>

static struct pqos_cores_domains m_cores_domains;
uint32_t *cpu2apic;

/**
 * @brief Reads /proc/cpuinfo fills apic to cpu map for logical CPU
 *
 * @param [in] apic2cpu Array to be filled with APIC to CPU mapping
 * @param [in] num_cpu Number of logical CPUs in the system
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK success
 */
static int
build_apic_to_cpu_map(uint32_t *cpu2apic, size_t num_cpus)
{
        FILE *f = fopen("/proc/cpuinfo", "r");

        if (!f)
                return PQOS_RETVAL_ERROR;

        char line[256];
        unsigned int cur_cpu = 0;

        while (fgets(line, sizeof(line), f)) {
                // Look for processor entry
                if (strncmp(line, "processor", 9) == 0) {
                        // New processor entry
                        // Extract logical cpu number
                        char *sep = strchr(line, ':');

                        if (sep) {
                                unsigned int cpu_id = atoi(sep + 1);

                                cur_cpu = cpu_id;
                        }
                } else if (strstr(line, "apicid") || strstr(line, "x2apicid")) {
                        // Extract apicid/x2apicid value
                        char *sep = strchr(line, ':');

                        if (sep && cur_cpu < num_cpus) {
                                uint32_t apic_id = (uint32_t)atoi(sep + 1);

                                cpu2apic[cur_cpu] = apic_id;
                        }
                }
        }
        fclose(f);

        return PQOS_RETVAL_OK;
}

/**
 * @brief Finds the logical CPU number corresponding to a given APIC ID.

 * @param [in] apic2cpu Array to be filled with APIC to CPU mapping
 * @param [in] num_cpu Number of logical CPUs in the system
 * @param [in] apic_id The APIC ID to search for.
 * Returns the logical CPU number if found, or -1 if not found.
 */
static int
get_cpu_by_apic_id(const uint32_t *cpu2apic,
                   unsigned int num_cpus,
                   uint32_t apic_id)
{
        for (unsigned int cpu = 0; cpu < num_cpus; ++cpu) {
                if (cpu2apic[cpu] == apic_id)
                        return (int)cpu;
        }

        return -1; // Not found
}

int
cores_domains_init(unsigned int num_cores,
                   const struct pqos_erdt_info *erdt,
                   struct pqos_cores_domains **cores_domains)
{
        uint16_t *domains;
        struct pqos_erdt_cacd cacd;

        ASSERT(num_cores > 0);
        ASSERT(erdt != NULL);
        ASSERT(cores_domains != NULL);

        cpu2apic = (uint32_t *)malloc(num_cores * sizeof(uint32_t));
        if (cpu2apic == NULL)
                return PQOS_RETVAL_ERROR;

        domains = (uint16_t *)malloc(num_cores * sizeof(uint16_t));
        if (domains == NULL)
                goto out_apic2cpu;

        memset(domains, 0, num_cores * sizeof(uint16_t));
        memset(cpu2apic, 0, num_cores * sizeof(uint32_t));

        if (build_apic_to_cpu_map(cpu2apic, num_cores) != PQOS_RETVAL_OK)
                goto out_domains;

        for (unsigned i = 0; i < erdt->num_cpu_agents; i++) {
                cacd = erdt->cpu_agents[i].cacd;
                for (unsigned j = 0; j < cacd.enum_ids_length; j++) {
                        int cpu = get_cpu_by_apic_id(cpu2apic, num_cores,
                                                     cacd.enumeration_ids[j]);
                        if (cpu == -1)
                                goto out_domains;
                        domains[cpu] = cacd.rmdd_domain_id;
                }
        }

        m_cores_domains.num_cores = num_cores;
        m_cores_domains.domains = domains;

        *cores_domains = &m_cores_domains;

        return PQOS_RETVAL_OK;

out_domains:
        free(domains);
out_apic2cpu:
        free(cpu2apic);

        return PQOS_RETVAL_ERROR;
}

void
cores_domains_fini(void)
{
        ASSERT(m_cores_domains.domains != NULL);
        free(m_cores_domains.domains);
        free(cpu2apic);
}
