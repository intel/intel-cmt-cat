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

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pqos.h>

#include "rdt.h"
#include "cpu.h"

static const struct pqos_cap *m_cap;
static const struct pqos_cpuinfo *m_cpu;
static const struct pqos_capability *m_cap_l3ca;
static struct cat_config m_config[CPU_SETSIZE];
static unsigned m_config_count;

/* Return number of set bits in bitmask */
static unsigned
bits_count(uint64_t bitmask)
{
	unsigned count = 0;

	for (; bitmask != 0; count++)
		bitmask &= bitmask - 1;

	return count;
}

/* Test if bitmask is contiguous */
static int
is_contiguous(uint64_t bitmask)
{
	/* check if bitmask is contiguous */
	unsigned i = 0, j = 0;
	const unsigned max_idx = (sizeof(bitmask) * CHAR_BIT);

	if (bitmask == 0)
		return 0;

	for (i = 0; i < max_idx; i++) {
		if (((1ULL << i) & bitmask) != 0)
			j++;
		else if (j > 0)
			break;
	}

	if (bits_count(bitmask) != j) {
		printf("L3: mask 0x%llx is not contiguous.\n",
			(unsigned long long) bitmask);
		return 0;
	}

	return 1;
}

static int
xstr_to_uint(const char *xstr, uint64_t *value)
{
	const char *xstr_start = xstr;
	char *xstr_end = NULL;

	while (isblank(*xstr_start))
		xstr_start++;

	if (!isxdigit(*xstr_start))
		return -1;

	errno = 0;
	*value = strtoul(xstr_start, &xstr_end, 16);
	if (errno != 0 || xstr_end == NULL || xstr_end == xstr_start ||
			*value == 0)
		return -1;

	return xstr_end - xstr;
}

static int
parse_mask_set(const char *cbm, uint64_t *mask, uint64_t *cmask)
{
	int offset = 0;
	const char *cbm_start = cbm;

	/* parse mask_set from start point */
	if (*cbm_start == '(') {
		cbm_start++;

		offset = xstr_to_uint(cbm_start, cmask);
		if (offset < 0)
			goto err;

		cbm_start += offset;

		while (isblank(*cbm_start))
			cbm_start++;

		if (*cbm_start != ',')
			goto err;

		cbm_start++;
	}

	offset = xstr_to_uint(cbm_start, mask);
	if (offset < 0)
		goto err;

	return cbm_start + offset - cbm;

err:
	return -1;

}

int
parse_l3(const char *l3ca)
{
	unsigned idx = 0;
	const char *end = NULL;

	if (l3ca == NULL)
		goto err;

	/* Get cbm */
	do {
		uint64_t mask = 0, cmask = 0;
		const char *cbm_start = NULL;
		int offset = 0;

		while (isblank(*l3ca))
			l3ca++;

		if (*l3ca == '\0')
			goto err;

		/* record mask_set start point */
		cbm_start = l3ca;

		/* go across a complete bracket */
		if (*cbm_start == '(') {
			l3ca += strcspn(l3ca, ")");
			if (*l3ca++ == '\0')
				goto err;
		}

		/* scan the separator '@' */
		l3ca += strcspn(l3ca, "@");

		if (*l3ca != '@')
			goto err;

		l3ca++;

		/* explicit assign cpu_set */
		offset = parse_cpu_set(l3ca, &m_config[idx].cpumask);
		if (offset < 0 || CPU_COUNT(&m_config[idx].cpumask) == 0)
			goto err;

		end = l3ca + offset;

		if (*end != ',' && *end != '\0')
			goto err;

		offset = parse_mask_set(cbm_start, &mask, &cmask);
		if (offset < 0 || 0 == mask)
			goto err;

		if (is_contiguous(mask) == 0 ||
				(cmask != 0 && is_contiguous(cmask) == 0))
			goto err;

		if (cmask != 0) {
			m_config[idx].cdp = 1;
			m_config[idx].code_mask = cmask;
			m_config[idx].data_mask = mask;
		} else
			m_config[idx].mask = mask;

		m_config_count++;

		l3ca = end + 1;
		idx++;
	} while (*end != '\0' && idx < CPU_SETSIZE);

	if (m_config_count == 0)
		goto err;

	return 0;

err:
	return -EINVAL;
}

/* Check are configured CPU sets overlapping */
static int
check_cpus_overlapping(void)
{
	unsigned i = 0;

	for (i = 0; i < m_config_count; i++) {
		unsigned j = 0;

		for (j = i + 1; j < m_config_count; j++) {
			unsigned overlapping = 0;
#ifdef __linux__
			cpu_set_t mask;

			CPU_AND(&mask, &m_config[i].cpumask,
				&m_config[j].cpumask);
			overlapping = CPU_COUNT(&mask) != 0;
#endif
#ifdef __FreeBSD__
			overlapping = CPU_OVERLAP(&m_config[i].cpumask,
				&m_config[j].cpumask);
#endif
			if (1 == overlapping) {
				printf("L3: Requested CPUs sets are "
					"overlapping.\n");
				return -EINVAL;
			}
		}
	}

	return 0;
}

/* Check are configured CPUs valid and have no COS associated */
static int
check_cpus(void)
{
	unsigned i = 0;
	int ret = 0;

	for (i = 0; i < m_config_count; i++) {
		unsigned cpu_id = 0;

		for (cpu_id = 0; cpu_id < CPU_SETSIZE; cpu_id++) {
			unsigned cos_id = 0;

			if (CPU_ISSET(cpu_id, &m_config[i].cpumask) == 0)
				continue;

			ret = pqos_cpu_check_core(m_cpu, cpu_id);
			if (ret != PQOS_RETVAL_OK) {
				printf("L3: %u is not a valid logical core id."
					"\n", cpu_id);
				return -ENODEV;
			}

			ret = pqos_alloc_assoc_get(cpu_id, &cos_id);
			if (ret != PQOS_RETVAL_OK) {
				printf("L3: Failed to read cpu %u COS "
                                       "association.\n", cpu_id);
				return -EFAULT;
			}

			/*
			 * Check if COS assigned to lcore is different
			 * then default one (#0)
			 */
			if (cos_id != 0) {
				printf("L3: cpu %u has already associated "
					"COS#%u. Please reset L3CA.\n",
					cpu_id, cos_id);
				return -EBUSY;
			}
		}
	}

	return 0;
}

/*
 * Check if CPU supports requested CDP configuration,
 * note user if it supported but not enabled
 */
static int
check_cdp(void)
{
	unsigned i = 0;

	for (i = 0; i < m_config_count; i++) {
		if (m_config[i].cdp == 1 && m_cap_l3ca->u.l3ca->cdp_on == 0) {
			if (m_cap_l3ca->u.l3ca->cdp == 0)
				printf("L3: CDP requested but not "
					"supported.\n");
			else
				printf("L3: CDP requested but not enabled. "
					"Please enable CDP.\n");

			return -ENOTSUP;
		}
	}

	return 0;
}

/*
 * Check are requested CBMs supported by system,
 * warn if CBM overlaps contention mask
 */
static int
check_cbm_len_and_contention(void)
{
	unsigned i = 0;

	for (i = 0; i < m_config_count; i++) {
		const uint64_t not_cbm =
			(UINT64_MAX << (m_cap_l3ca->u.l3ca->num_ways));
		const uint64_t contention_cbm =
			m_cap_l3ca->u.l3ca->way_contention;
		uint64_t mask = 0;

		if (m_config[i].cdp == 1)
			mask = m_config[i].code_mask | m_config[i].data_mask;
		else
			mask = m_config[i].mask;

		if ((mask & not_cbm) != 0) {
			printf("L3: One or more of requested CBM masks not "
				"supported by system (too long).\n");
			return -ENOTSUP;
		}

		/* Just a note */
		if ((mask & contention_cbm) != 0)
			printf("L3: One or more of requested CBM  masks "
				"overlap CBM contention mask.\n");

	}

	return 0;
}

static int
l3ca_get_avail(const unsigned socket, const unsigned max_num_ca,
		unsigned *num_ca, struct pqos_l3ca *ca)
{
	unsigned cores[CPU_SETSIZE];
	unsigned cores_count = 0, k = 0, num = 0;
	int ret = PQOS_RETVAL_OK;

	ret = pqos_l3ca_get(socket, max_num_ca, &num, ca);

	if (ret != PQOS_RETVAL_OK) {
		printf("Error retrieving L3CA configuration!\n");
		return ret;
	}

	ret = pqos_cpu_get_cores(m_cpu, socket, CPU_SETSIZE, &cores_count,
		&cores[0]);
	if (ret != PQOS_RETVAL_OK) {
		printf("Error retrieving core information!\n");
		return ret;
	}

	/* Get already associated COS and remove them from list */
	for (k = 0; k < cores_count; k++) {
		unsigned class_id = 0;
		unsigned i = 0, j = 0;

		/* Get COS associated to core */
		ret = pqos_alloc_assoc_get(cores[k], &class_id);
		if (ret != PQOS_RETVAL_OK) {
			printf("Error reading class association!\n");
			return ret;
		}

		/* Removed associated COS from the list */
		for (i = 0; i < num; i++)
			if (ca[i].class_id == class_id) {
				for (j = i + 1; j < num; j++)
					ca[j-1] = ca[j];

				num--;
				break;
			}
	}

	*num_ca = num;

	return ret;
}

static int
l3ca_select(const unsigned socket, const unsigned num_cos, struct pqos_l3ca *ca)
{
	struct pqos_l3ca avail_ca[PQOS_MAX_L3CA_COS];
	unsigned num = 0, i = 0;
	int ret = PQOS_RETVAL_OK;

	ret = l3ca_get_avail(socket, PQOS_MAX_L3CA_COS, &num, avail_ca);
	if (ret != PQOS_RETVAL_OK) {
		printf("Error retrieving list of available L3CA COS!\n");
		return ret;
	}

	if (num < num_cos) {
		printf("Not enough available COS!\n");
		return PQOS_RETVAL_RESOURCE;
	}

	for (i = 0; i < num_cos; i++)
		ca[i].class_id = avail_ca[i].class_id;

	return ret;
}

static int
l3ca_atomic_set(const unsigned num, struct pqos_l3ca *ca, cpu_set_t *cpu)
{
	unsigned i = 0, j = 0;
	cpu_set_t cpusets[num][RDT_MAX_SOCKETS];
	int ret = 0;

	memset(cpusets, 0, sizeof(cpusets));

	/*
	 * Make a table with information about cores used
	 * (by each configured class) per socket
	 */
	for (i = 0; i < num; i++) {
		for (j = 0; j < CPU_SETSIZE; j++) {
			unsigned socket = 0;

			if (0 == CPU_ISSET(j, &cpu[i]))
				continue;

			ret = pqos_cpu_get_socketid(m_cpu, j, &socket);
			if (ret != PQOS_RETVAL_OK) {
				printf("L3: Failed to get socket id of lcore "
					"%u!\n", j);
				return ret;
			}
			CPU_SET(j, &cpusets[i][socket]);
		}
	}

	/* Check do we have enough COS available */
	for (i = 0; i < num; i++) {
		for (j = 0; j < RDT_MAX_SOCKETS; j++) {
			if (0 == CPU_COUNT(&cpusets[i][j]))
				continue;

			ret = l3ca_select(j, 1, &ca[i]);
			if (ret != PQOS_RETVAL_OK) {
				printf("Error selecting available L3CA COS!\n");
				return ret;
			}
		}
	}

	/* Configure CAT */
	for (i = 0; i < num; i++) {
		for (j = 0; j < RDT_MAX_SOCKETS; j++) {
			unsigned k = 0;

			if (0 == CPU_COUNT(&cpusets[i][j]))
				continue;

			/* Get COS# to be used */
			ret = l3ca_select(j, 1, &ca[i]);
			if (ret != PQOS_RETVAL_OK) {
				printf("Error selecting available L3CA COS!\n");
				return ret;
			}

			/* Configure COS */
			ret = pqos_l3ca_set(j, 1, &ca[i]);
			if (ret != PQOS_RETVAL_OK) {
				printf("Error configuring L3CA COS!\n");
				return ret;
			}

			/* Associate COS to cores */
			for (k = 0; k < CPU_SETSIZE; k++) {
				if (0 == CPU_ISSET(k, &cpusets[i][j]))
					continue;

				ret = pqos_alloc_assoc_set(k, ca[i].class_id);
				if (ret != PQOS_RETVAL_OK) {
					printf("Error associating cpu %u with "
                                               "COS %u!\n", k, ca[i].class_id);
					return ret;
				}
			}
		}
	}

	return ret;
}

/* Validate requested L3 CAT configuration */
static int
cat_validate(void)
{
	int ret = 0;

	ret = check_cpus();
	if (ret != 0)
		return ret;

	ret = check_cdp();
	if (ret != 0)
		return ret;

	ret = check_cbm_len_and_contention();
	if (ret != 0)
		return ret;

	ret = check_cpus_overlapping();
	if (ret != 0)
		return ret;

	return 0;
}

/*
 * Check is it possible to fulfill requested L3 CAT configuration
 * and then configure system.
 */
static int
cat_set(void)
{
	struct pqos_l3ca ca[m_config_count];
	cpu_set_t cpu[m_config_count];
	unsigned i = 0;
	int ret = 0;

	for (i = 0; i < m_config_count; i++) {
		ca[i].cdp = m_config[i].cdp;
		if (1 == m_config[i].cdp) {
			ca[i].code_mask = m_config[i].code_mask;
			ca[i].data_mask = m_config[i].data_mask;
		} else
			ca[i].ways_mask = m_config[i].mask;

		cpu[i] = m_config[i].cpumask;
	}

	ret =  l3ca_atomic_set(m_config_count, ca, cpu);
	if (ret != PQOS_RETVAL_OK) {
		printf("L3: Failed to configure L3CA!\n");
		ret = -1;
	}

	return ret;
}

void
cat_fini(void)
{
	int ret = 0;

	if (g_verbose)
		printf("L3: Shutting down PQoS library...\n");

	/* deallocate all the resources */
	ret = pqos_fini();
	if (ret != PQOS_RETVAL_OK && ret != PQOS_RETVAL_INIT)
		printf("L3: Error shutting down PQoS library!\n");

	m_cap = NULL;
	m_cpu = NULL;
	m_cap_l3ca = NULL;
	memset(m_config, 0, sizeof(m_config));
	m_config_count = 0;
}

void
cat_exit(void)
{
	unsigned i = 0;
	int ret = 0;

	/* if lib is not initialized, do nothing */
	if (m_cap == NULL && m_cpu == NULL)
		return;

	if (g_verbose)
		printf("L3: Reverting CAT configuration...\n");

	for (i = 0; i < m_config_count; i++) {
		unsigned j = 0;

		for (j = 0; j < m_cpu->num_cores; j++) {
			unsigned cpu_id = 0;

			cpu_id = m_cpu->cores[j].lcore;
			if (CPU_ISSET(cpu_id, &m_config[i].cpumask) == 0)
				continue;

			ret = pqos_alloc_assoc_set(cpu_id, 0);
			if (ret != PQOS_RETVAL_OK)
				printf("L3: Failed to associate COS 0 to "
					"cpu %u\n", cpu_id);
		}
	}

	cat_fini();
}

/* Signal handler to do clean-up on exit on signal */
static void
signal_handler(int signum)
{
	if (signum == SIGINT || signum == SIGTERM) {
		printf("\nPQOS: Signal %d received, preparing to exit...\n",
			signum);

		cat_exit();

		/* exit with the expected status */
		signal(signum, SIG_DFL);
		kill(getpid(), signum);
	}
}

int
cat_init(void)
{
	int ret = 0;
	struct pqos_config cfg;

	if (m_cap != NULL || m_cpu != NULL) {
		printf("L3: CAT module already initialized!\n");
		return -EEXIST;
	}

	/* PQoS Initialization - Check and initialize CAT capability */
        memset(&cfg, 0, sizeof(cfg));
	cfg.fd_log = STDOUT_FILENO;
	cfg.verbose = 0;
	cfg.cdp_cfg = PQOS_REQUIRE_CDP_ANY;
	ret = pqos_init(&cfg);
	if (ret != PQOS_RETVAL_OK) {
		printf("L3: Error initializing PQoS library!\n");
		ret = -EFAULT;
		goto err;
	}

	/* Get capability and CPU info pointer */
	ret = pqos_cap_get(&m_cap, &m_cpu);
	if (ret != PQOS_RETVAL_OK || m_cap == NULL || m_cpu == NULL) {
		printf("L3: Error retrieving PQoS capabilities!\n");
		ret = -EFAULT;
		goto err;
	}

	/* Get L3CA capabilities */
	ret = pqos_cap_get_type(m_cap, PQOS_CAP_TYPE_L3CA, &m_cap_l3ca);
	if (ret != PQOS_RETVAL_OK || m_cap_l3ca == NULL) {
		printf("L3: Error retrieving PQOS_CAP_TYPE_L3CA "
			"capabilities!\n");
		ret = -EFAULT;
		goto err;
	}

	/* Validate cmd line configuration */
	ret = cat_validate();
	if (ret != 0) {
		printf("L3: Requested CAT configuration is not valid!\n");
		goto err;
	}

	/* configure system */
	ret = cat_set();
	if (ret != 0) {
		printf("L3: Failed to configure CAT!\n");
		goto err;
	}

	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);

	ret = atexit(cat_exit);
	if (ret != 0) {
		ret = -EFAULT;
		printf("L3: Cannot set exit function\n");
		goto err;
	}

	return 0;

err:
	/* deallocate all the resources */
	cat_fini();
	return ret;
}

void
print_cmd_line_l3_config(void)
{
	char cpustr[CPU_SETSIZE * 3] = { 0 };
	unsigned i = 0;

	for (i = 0; i < m_config_count; i++) {
		cpuset_to_str(cpustr, sizeof(cpustr), &m_config[i].cpumask);

		printf("L3 Allocation: CPUs: %s", cpustr);

		if (m_config[i].cdp == 1)
			printf(" code MASK: 0x%llx, data MASK: 0x%llx\n",
				(unsigned long long) m_config[i].code_mask,
				(unsigned long long) m_config[i].data_mask);
		else
			printf(" MASK: 0x%llx\n",
				(unsigned long long) m_config[i].mask);
	}
}

