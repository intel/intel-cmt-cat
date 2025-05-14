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

int
cores_domains_init(unsigned int num_cores,
                   const struct pqos_erdt_info *erdt,
                   struct pqos_cores_domains **cores_domains)
{
        unsigned int *domains;
        struct pqos_erdt_cacd cacd;

        ASSERT(num_cores != NULL);
        ASSERT(erdt != NULL);
        ASSERT(cores_domains != NULL);

        domains = (unsigned int *)malloc(num_cores * sizeof(unsigned int));
        if (domains == NULL)
                return PQOS_RETVAL_ERROR;

        memset(domains, 0, num_cores * sizeof(unsigned int));

        for (unsigned i = 0; i < erdt->num_cpu_agents; i++) {
                cacd = erdt->cpu_agents[i].cacd;
                for (unsigned j = 0; j < cacd.enum_ids_length; j++)
                        domains[cacd.enumeration_ids[j]] = cacd.rmdd_domain_id;
        }

        m_cores_domains.num_cores = num_cores;
        m_cores_domains.domains = domains;

        *cores_domains = &m_cores_domains;

        return PQOS_RETVAL_OK;
}

int
cores_domains_fini(void)
{
        ASSERT(m_cores_domains.domains != NULL);
        free(m_cores_domains.domains);

        return PQOS_RETVAL_OK;
}
