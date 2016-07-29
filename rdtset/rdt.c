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
static const struct pqos_capability *m_cap_l2ca;
static const struct pqos_capability *m_cap_l3ca;

/**
 * @brief Prints L2 or L3 configuration in \a ca
 *
 * @param [in] stream to write output to
 * @param [in] ca CAT COS configuration
 */
static void
rdt_ca_print(FILE *stream, const struct rdt_ca ca)
{
	if (!(PQOS_CAP_TYPE_L2CA == ca.type || PQOS_CAP_TYPE_L3CA == ca.type) ||
			NULL == ca.u.generic_ptr)
		return;

	if (PQOS_CAP_TYPE_L2CA == ca.type)
		fprintf(stream, "MASK: 0x%llx",
			(unsigned long long) ca.u.l2->ways_mask);
	else if (ca.u.l3->cdp == 1)
		fprintf(stream, "code MASK: 0x%llx, data MASK: 0x%llx",
			(unsigned long long) ca.u.l3->u.s.code_mask,
			(unsigned long long) ca.u.l3->u.s.data_mask);
	else
		fprintf(stream, "MASK: 0x%llx",
			(unsigned long long) ca.u.l3->u.ways_mask);
}
/**
 * @brief Gets short string representation of configuration type of \a ca
 *
 * @param [in] ca CAT COS configuration
 *
 * @return String representation of configuration type
 */
static const char *
rdt_ca_get_type_str(const struct rdt_ca ca)
{
	if (!(PQOS_CAP_TYPE_L2CA == ca.type || PQOS_CAP_TYPE_L3CA == ca.type) ||
			NULL == ca.u.generic_ptr)
		return "ERROR!";

	return PQOS_CAP_TYPE_L2CA == ca.type ? "L2" : "L3";
}

/**
 * @brief Validates configuration in \a ca
 *
 * @param [in] ca CAT COS configuration
 *
 * @return Result
 * @retval 1 on success
 * @retval 0 on failure
 */
static int
rdt_ca_is_valid(const struct rdt_ca ca)
{
	if (!(PQOS_CAP_TYPE_L2CA == ca.type || PQOS_CAP_TYPE_L3CA == ca.type) ||
			NULL == ca.u.generic_ptr)
		return 0;

	if (PQOS_CAP_TYPE_L2CA == ca.type)
		return NULL != ca.u.l2 && 0 != ca.u.l2->ways_mask;

	return NULL != ca.u.l3 &&
		((1 == ca.u.l3->cdp && 0 != ca.u.l3->u.s.code_mask &&
		0 != ca.u.l3->u.s.data_mask) ||
		(0 == ca.u.l3->cdp && 0 != ca.u.l3->u.ways_mask));
}

/**
 * @brief Returns number of set bits in \a bitmask
 *
 * @param [in] bitmask
 *
 * @return bits count
 */
static unsigned
bits_count(uint64_t bitmask)
{
	unsigned count = 0;

	for (; bitmask != 0; count++)
		bitmask &= bitmask - 1;

	return count;
}

/**
 * @brief Tests if \a bitmask is contiguous
 *
 * @param [in] cat_type string to identify CAT type (L2 or L3) on error
 * @param [in] bitmask bitmask to be tested
 *
 * @return Result
 * @retval 1 on success
 * @retval 0 on failure
 */
static int
is_contiguous(const char *cat_type, const uint64_t bitmask)
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
		fprintf(stderr, "CAT: %s mask 0x%llx is not contiguous.\n",
			cat_type, (unsigned long long) bitmask);
		return 0;
	}

	return 1;
}

/**
 * @brief Converts hex string \a xstr to UINT \a value
 *
 * @param [in] xstr hex string
 * @param [out] value UINT value
 *
 * @return number of parsed characters
 * @retval >0 on success
 * @retval negative on error (-errno)
 */
static int
xstr_to_uint(const char *xstr, uint64_t *value)
{
	const char *xstr_start = xstr;
	char *xstr_end = NULL;

	if (NULL == xstr || NULL == value)
		return -EINVAL;

	while (isblank(*xstr_start))
		xstr_start++;

	if (!isxdigit(*xstr_start))
		return -EINVAL;

	errno = 0;
	*value = strtoul(xstr_start, &xstr_end, 16);
	if (errno != 0 || xstr_end == NULL || xstr_end == xstr_start ||
			*value == 0)
		return -EINVAL;

	return xstr_end - xstr;
}

/**
 * @brief Parses CBM (Capacity Bit Mask) string \a cbm
 *        and stores result in \a mask and \a cmask
 *
 * @param [in] cbm CBM string
 * @param [in] force_dual_mask expect two masks separated with ','
 * @param [out] mask common (L2 or L3 non CDP) or data mask (L3 CDP)
 * @param [out] cmask code mask (L3 CDP)
 *
 * @return status
 * @retval 0 on success
 * @retval negative on error (-errno)
 */
static int
parse_mask_set(const char *cbm, const int force_dual_mask, uint64_t *mask,
		uint64_t *cmask)
{
	int offset = 0;
	const char *cbm_start = cbm;

	/* parse mask_set from start point */
	if (*cbm_start == '(' || force_dual_mask) {
		while (!isxdigit(*cbm_start))
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
	return -EINVAL;
}

int
parse_reset(const char *cpustr)
{
	unsigned cpustr_len = strlen(cpustr);
	int ret = 0;

	ret = str_to_cpuset(cpustr, cpustr_len, &g_cfg.reset_cpuset);
	return ret > 0 ? 0 : ret;
}

/**
 * @brief Parses CBMs string \a param and stores in \a ca
 *
 * @param [in] param CBMs string
 * @param [out] ca to store result
 *
 * @return status
 * @retval 0 on success
 * @retval negative on error (-errno)
 */
static int
rdt_ca_str_to_cbm(const char *param, struct rdt_ca ca)
{
	uint64_t mask = 0, mask2 = 0;
	int ret, force_dual_mask;

	if (!(PQOS_CAP_TYPE_L2CA == ca.type || PQOS_CAP_TYPE_L3CA == ca.type) ||
			NULL == ca.u.generic_ptr || NULL == param)
		return -EINVAL;

	force_dual_mask = (NULL != strchr(param, ','));
	ret = parse_mask_set(param, force_dual_mask, &mask, &mask2);
	if (ret < 0)
		return -EINVAL;

	if (0 == mask || is_contiguous(rdt_ca_get_type_str(ca), mask) == 0)
		return -EINVAL;

	if (PQOS_CAP_TYPE_L2CA == ca.type) {
		if (mask2 != 0)
			return -EINVAL;
		ca.u.l2->ways_mask = mask;
	} else {
		if (mask2 != 0 && is_contiguous("L3", mask2) == 0)
			return -EINVAL;
		if (mask2 != 0) {
			ca.u.l3->cdp = 1;
			ca.u.l3->u.s.data_mask = mask;
			ca.u.l3->u.s.code_mask = mask2;
		} else
			ca.u.l3->u.ways_mask = mask;
	}

	return 0;
}

/*
* @brief Simplifies feature string
*
* @param [in] feature feature string
*
* @return simplified feature as char
* @retval char representation of feature
* @retval '?' on error
*/
static char
simplify_feature_str(const char *feature)
{
	static const struct {
		const char *feature_long;
		const char feature_char;
	} feature_lut[] = {
		{"cpu", 'c'},
		{"l2", '2'},
		{"l3", '3'},
		{NULL, 0}
	};


	if (feature == NULL)
		return '?';

	if (strlen(feature) > 1) {
		unsigned i = 0;

		while (feature_lut[i].feature_long != NULL) {
			if (strcmp(feature, feature_lut[i].feature_long) == 0)
				return feature_lut[i].feature_char;

			i++;
		}
	} else
		return feature[0];

	return '?';
}

int
parse_rdt(char *rdtstr)
{
	char *rdtstr_saveptr = NULL;
	char *group = NULL;
	unsigned idx = g_cfg.config_count;
	const struct rdt_ca l2ca = wrap_l2ca(&g_cfg.config[idx].l2);
	const struct rdt_ca l3ca = wrap_l3ca(&g_cfg.config[idx].l3);
	const unsigned min_len_arg = strlen("3=f;c=0");
	const unsigned min_len_group = strlen("3=f");
	int ret;

	if (rdtstr == NULL)
		return -EINVAL;

	/* Min len check, 1 feature + 1 CPU e.g. "3=f;c=0" */
	if (strlen(rdtstr) < min_len_arg) {
		fprintf(stderr, "Invalid argument: \"%s\"\n", rdtstr);
		return -EINVAL;
	}

	group = strtok_r(rdtstr, ";", &rdtstr_saveptr);
	while (group != NULL) {
		char *group_saveptr = NULL;

		/* Min len check, 1 feature "3=f" */
		if (strlen(group) < min_len_group) {
			fprintf(stderr, "Invalid option: \"%s\"\n", group);
			return -EINVAL;
		}

		const char *feature = strtok_r(group, "=", &group_saveptr);
		const char *param = strtok_r(NULL, "=", &group_saveptr);

		if (feature == NULL || param == NULL) {
			fprintf(stderr, "Invalid option: \"%s\"\n", group);
			return -EINVAL;
		}

		switch (simplify_feature_str(feature)) {
		case '2':
			if (rdt_ca_is_valid(l2ca))
				return -EINVAL;

			ret = rdt_ca_str_to_cbm(param, l2ca);
			if (ret < 0)
				return ret;
			break;

		case '3':
			if (rdt_ca_is_valid(l3ca))
				return -EINVAL;

			ret = rdt_ca_str_to_cbm(param, l3ca);
			if (ret < 0)
				return ret;
			break;

		case 'c':
			if (CPU_COUNT(&g_cfg.config[idx].cpumask) != 0)
				return -EINVAL;

			ret = str_to_cpuset(param, strlen(param),
				&g_cfg.config[idx].cpumask);
			if (ret <= 0)
				return -EINVAL;

			if (CPU_COUNT(&g_cfg.config[idx].cpumask) == 0)
				return -EINVAL;
			break;

		default:
			fprintf(stderr, "Invalid option: \"%s\"\n", feature);
			return -EINVAL;
		}

		group = strtok_r(NULL, ";", &rdtstr_saveptr);
	}

	if (CPU_COUNT(&g_cfg.config[idx].cpumask) == 0 ||
			(!rdt_ca_is_valid(l2ca) && !rdt_ca_is_valid(l3ca)))
		return -EINVAL;

	g_cfg.config_count++;

	return 0;
}

/**
 * @brief Checks if configured CPU sets are overlapping
 *
 * @return status
 * @retval 0 on success
 * @retval negative on error (-errno)
 */
static int
check_cpus_overlapping(void)
{
	unsigned i = 0;

	for (i = 0; i < g_cfg.config_count; i++) {
		unsigned j = 0;

		for (j = i + 1; j < g_cfg.config_count; j++) {
			unsigned overlapping = 0;
#ifdef __linux__
			cpu_set_t mask;

			CPU_AND(&mask, &g_cfg.config[i].cpumask,
				&g_cfg.config[j].cpumask);
			overlapping = CPU_COUNT(&mask) != 0;
#endif
#ifdef __FreeBSD__
			overlapping = CPU_OVERLAP(&g_cfg.config[i].cpumask,
				&g_cfg.config[j].cpumask);
#endif
			if (1 == overlapping) {
				fprintf(stderr, "CAT: Requested CPUs sets are "
					"overlapping.\n");
				return -EINVAL;
			}
		}
	}

	return 0;
}

/**
 * @brief Checks if configured CPUs are valid and have no COS associated
 *
 * @return status
 * @retval 0 on success
 * @retval negative on error (-errno)
 */
static int
check_cpus(void)
{
	unsigned i = 0;
	int ret = 0;

	for (i = 0; i < g_cfg.config_count; i++) {
		unsigned cpu_id = 0;

		for (cpu_id = 0; cpu_id < CPU_SETSIZE; cpu_id++) {
			unsigned cos_id = 0;

			if (CPU_ISSET(cpu_id, &g_cfg.config[i].cpumask) == 0)
				continue;

			ret = pqos_cpu_check_core(m_cpu, cpu_id);
			if (ret != PQOS_RETVAL_OK) {
				fprintf(stderr, "CAT: %u is not a valid "
					"logical core id.\n", cpu_id);
				return -ENODEV;
			}

			ret = pqos_alloc_assoc_get(cpu_id, &cos_id);
			if (ret != PQOS_RETVAL_OK) {
				fprintf(stderr, "CAT: Failed to read cpu %u "
					"COS association.\n", cpu_id);
				return -EFAULT;
			}

			/*
			 * Check if COS assigned to lcore is different
			 * then default one (#0)
			 */
			if (cos_id != 0) {
				fprintf(stderr, "CAT: cpu %u has already "
					"associated COS#%u. Please reset CAT."
					"\n", cpu_id, cos_id);
				return -EBUSY;
			}
		}
	}

	return 0;
}

/**
 * @brief Checks if CPU supports requested CDP configuration
 *
 * @return status
 * @retval 0 on success
 * @retval negative on error (-errno)
 */
static int
check_cdp_support(void)
{
	unsigned i = 0;
	const int cdp_supported = m_cap_l3ca != NULL &&
		m_cap_l3ca->u.l3ca->cdp == 1;
	const int cdp_enabled = cdp_supported &&
		m_cap_l3ca->u.l3ca->cdp_on == 1;

	if (cdp_enabled)
		return 0;

	for (i = 0; i < g_cfg.config_count; i++) {
		if (0 == g_cfg.config[i].l3.cdp)
			continue;

		if (!cdp_supported)
			fprintf(stderr, "CAT: CDP requested but not supported."
				"\n");
		else
			fprintf(stderr, "CAT: CDP requested but not enabled. "
				"Please enable CDP.\n");

		return -ENOTSUP;
	}

	return 0;
}

/**
 * @brief Checks if CAT configuration requested by a user via cmd line
 *        is supported by the system.
 *
 * @return status
 * @retval 0 on success
 * @retval negative on error (-errno)
 */
static int
check_supported(void)
{
	unsigned i = 0;

	for (i = 0; i < g_cfg.config_count; i++) {
		if (rdt_ca_is_valid(wrap_l3ca(&g_cfg.config[i].l3)) &&
				NULL == m_cap_l3ca) {
			fprintf(stderr, "CAT: L3CA requested but not supported "
				"by system!\n");
			return -ENOTSUP;
		}

		if (rdt_ca_is_valid(wrap_l2ca(&g_cfg.config[i].l2)) &&
				NULL == m_cap_l2ca) {
			fprintf(stderr, "CAT: L2CA requested but not supported "
				"by system!\n");
			return -ENOTSUP;
		}
	}

	return 0;
}

/**
 * @brief Returns negation of max CBM for requested \a type (L2 or L3)
 *
 * @param [in] type type of pqos CAT capability, L2 or L3
 *
 * @return mask
 * @retval UINT64_MAX on error
 */
static uint64_t
get_not_cbm(const enum pqos_cap_type type)
{
	if (PQOS_CAP_TYPE_L2CA == type && NULL != m_cap_l2ca)
		return (UINT64_MAX << (m_cap_l2ca->u.l2ca->num_ways));
	else if (PQOS_CAP_TYPE_L3CA == type && NULL != m_cap_l3ca)
		return (UINT64_MAX << (m_cap_l3ca->u.l3ca->num_ways));
	else
		return UINT64_MAX;
}

/**
 * @brief Returns contention mask for requested \a type (L2 or L3)
 *
 * @param [in] type type of pqos CAT capability, L2 or L3
 *
 * @return contention mask
 * @retval UINT64_MAX on error
 *
 */
static uint64_t
get_contention_mask(const enum pqos_cap_type type)
{
	if (PQOS_CAP_TYPE_L2CA == type && NULL != m_cap_l2ca)
		return m_cap_l2ca->u.l2ca->way_contention;
	else if (PQOS_CAP_TYPE_L3CA == type && NULL != m_cap_l3ca)
		return m_cap_l3ca->u.l3ca->way_contention;
	else
		return UINT64_MAX;
}

/**
 * @brief Returns cumulative mask for \a ca CAT config
 *
 * For L3 CDP config returns code_mask | data_mask,
 * for L2 or L3 non-CDP config returns ways_mask
 *
 * @param [in] ca CAT COS configuration
 *
 * @return cumulative mask
 * @retval UINT64_MAX on error
 */
static uint64_t
rdt_ca_get_cumulative_cbm(const struct rdt_ca ca)
{
	if (!(PQOS_CAP_TYPE_L2CA == ca.type || PQOS_CAP_TYPE_L3CA == ca.type) ||
			NULL == ca.u.generic_ptr || 0 == rdt_ca_is_valid(ca))
		return UINT64_MAX;

	if (PQOS_CAP_TYPE_L2CA == ca.type)
		return ca.u.l2->ways_mask;
	if (ca.u.l3->cdp == 1)
		return ca.u.l3->u.s.code_mask | ca.u.l3->u.s.data_mask;

	return ca.u.l3->u.ways_mask;
}

/**
 * @brief Checks if requested CBMs of \a type are supported by system.
 *
 * Warn if CBM overlaps contention mask
 *
 * @param [in] type type of pqos CAT capability, L2 or L3
 *
 * @return status
 * @retval 0 on success
 * @retval negative on error (-errno)
 */
static int
check_cbm_len_and_contention(const enum pqos_cap_type type)
{
	unsigned i = 0;
	struct rdt_ca ca;
	uint64_t mask = 0;

	if (!(PQOS_CAP_TYPE_L2CA == type || PQOS_CAP_TYPE_L3CA == type))
		return -EINVAL;

	const uint64_t not_cbm = get_not_cbm(type);
	const uint64_t contention_cbm = get_contention_mask(type);

	if (UINT64_MAX == not_cbm || UINT64_MAX == contention_cbm)
		return -EINVAL;

	for (i = 0; i < g_cfg.config_count; i++) {
		if (PQOS_CAP_TYPE_L2CA == type)
			ca = wrap_l2ca(&g_cfg.config[i].l2);
		else
			ca = wrap_l3ca(&g_cfg.config[i].l3);

		if (!rdt_ca_is_valid(ca))
			continue;

		mask = rdt_ca_get_cumulative_cbm(ca);
		if (UINT64_MAX == mask)
			return -EFAULT;

		if ((mask & not_cbm) != 0) {
			fprintf(stderr, "CAT: One or more of requested %s CBMs "
				"(", rdt_ca_get_type_str(ca));
			rdt_ca_print(stderr, ca);
			fprintf(stderr, ") not supported by system "
				"(too long).\n");
			return -ENOTSUP;
		}

		/* Just a note */
		if ((mask & contention_cbm) != 0) {
			printf("CAT: One or more of requested %s CBMs "
				"(", rdt_ca_get_type_str(ca));
			rdt_ca_print(stdout, ca);
			printf(") overlap contention mask.\n");
		}
	}

	return 0;
}

/**
 * @brief Checks if requested CBMs (of all types) are supported by system
 *
 * Warn if CBM overlaps contention mask
 *
 * @return status
 * @retval 0 on success
 * @retval negative on error (-errno)
 */
static int
check_cbm_len_and_contention_all(void)
{
	int ret;

	if (NULL != m_cap_l2ca) {
		ret = check_cbm_len_and_contention(PQOS_CAP_TYPE_L2CA);
		if (ret < 0)
			return ret;
	}

	if (NULL != m_cap_l3ca) {
		ret = check_cbm_len_and_contention(PQOS_CAP_TYPE_L3CA);
		if (ret < 0)
			return ret;
	}

	return 0;
}


/**
 * @brief Validates requested CAT configuration
 *
 * @return status
 * @retval 0 on success
 * @retval negative on error (-errno)
 */
static int
cat_validate(void)
{
	int ret = 0;

	ret = check_cpus();
	if (ret != 0)
		return ret;

	ret = check_supported();
	if (ret != 0)
		return ret;

	ret = check_cdp_support();
	if (ret != 0)
		return ret;

	ret = check_cbm_len_and_contention_all();
	if (ret != 0)
		return ret;

	ret = check_cpus_overlapping();
	if (ret != 0)
		return ret;

	return 0;
}

/**
 * @brief Gets cores from \a cores set which belong to \a socket
 *
 * @param [in] cores set of cores
 * @param [in] socket_id socket to get cores from
 * @param [out] core_num number of cores in \a core_array
 * @param [out] core_array array of cores
 *
 * @return status
 * @retval 0 on success
 * @retval negative on error (-errno)
 */
static int
get_socket_cores(const cpu_set_t *cores, const unsigned socket_id,
		unsigned *core_num, unsigned core_array[CPU_SETSIZE])
{
	unsigned i;

	if (cores == NULL || core_num == NULL || core_array == NULL)
		return -EINVAL;

	if (m_cpu == NULL)
		return -EFAULT;

	if (CPU_COUNT(cores) == 0) {
		*core_num = 0;
		return 0;
	}

	for (i = 0, *core_num = 0; i < m_cpu->num_cores; i++) {
		if (m_cpu->cores[i].socket != socket_id ||
				0 == CPU_ISSET(m_cpu->cores[i].lcore, cores))
			continue;

		core_array[(*core_num)++] = m_cpu->cores[i].lcore;
	}

	return 0;
}

/**
 * @brief pqos_alloc_release wrapper
 *
 * @param [in] cores set of cores
 *
 * @return status
 * @retval 0 on success
 * @retval negative on error (-errno)
 */
static int
alloc_release(const cpu_set_t *cores)
{
	unsigned core_array[CPU_SETSIZE];
	unsigned core_num, i;
	int ret;

	if (cores == NULL)
		return -EINVAL;

	if (m_cpu == NULL)
		return -EFAULT;

	for (i = 0, core_num = 0; i < m_cpu->num_cores; i++) {
		if (0 == CPU_ISSET(m_cpu->cores[i].lcore, cores))
			continue;

		core_array[core_num++] = m_cpu->cores[i].lcore;
	}

	ret = pqos_alloc_release(core_array, core_num);
	if (ret != PQOS_RETVAL_OK)
		return -EFAULT;

	return 0;
}

/**
 * @brief Get default COS
 *
 * @param [out] l2_def L2 COS
 * @param [out] l3_def L3 COS
 *
 * @return status
 * @retval 0 on success
 * @retval negative on error (-errno)
 */
static int
cat_get_default_cos(struct pqos_l2ca *l2_def, struct pqos_l3ca *l3_def)
{
	if (l2_def == NULL && l3_def == NULL)
		return -EINVAL;

	if (m_cap_l2ca == NULL && m_cap_l3ca == NULL)
		return -EFAULT;

	if (m_cap_l2ca != NULL && l2_def != NULL) {
		memset(l2_def, 0, sizeof(*l2_def));
		l2_def->ways_mask =
			(1ULL << m_cap_l2ca->u.l2ca->num_ways) - 1ULL;
	}

	if (m_cap_l3ca != NULL  && l3_def != NULL) {
		const uint64_t def_mask =
			(1ULL << m_cap_l3ca->u.l3ca->num_ways) - 1ULL;

		memset(l3_def, 0, sizeof(*l3_def));

		if (m_cap_l3ca->u.l3ca->cdp_on == 1) {
			l3_def->cdp = 1;
			l3_def->u.s.code_mask = def_mask;
			l3_def->u.s.data_mask = def_mask;
		} else
			l3_def->u.ways_mask = def_mask;
	}

	return 0;
}

/**
 * @brief Configures COS \a cos_id defined by \a l2ca and \a l3ca on
 *        \a socket_id
 *
 * @param [in] l2ca L2 classes configuration
 * @param [in] l3ca L3 classes configuration
 * @param [in] socket_id socket to configure COS on
 * @param [in] cos_id id of COS to be configured
 *
 * @return status
 * @retval 0 on success
 * @retval negative on error (-errno)
 */
static int
cat_configure_cos(const struct pqos_l2ca *l2ca, const struct pqos_l3ca *l3ca,
		const unsigned socket_id, const unsigned cos_id)
{
	struct pqos_l2ca l2_defs;
	struct pqos_l3ca l3_defs;
	int ret;

	if (NULL == l2ca || NULL == l3ca)
		return -EINVAL;

	if (NULL == m_cap_l2ca && NULL == m_cap_l3ca)
		return -EFAULT;

	/* Get default COS values */
	ret = cat_get_default_cos(&l2_defs, &l3_defs);
	if (ret != 0)
		return ret;

	/* Configure COS */
	if (m_cap_l3ca != NULL && m_cap_l3ca->u.l3ca->num_classes > cos_id) {
		struct pqos_l3ca ca = *l3ca;

		/* if COS is not configured, set it to default */
		if (!rdt_ca_is_valid(wrap_l3ca(&ca)))
			ca = l3_defs;

		/* set proper COS id */
		ca.class_id = cos_id;

		ret = pqos_l3ca_set(socket_id, 1, &ca);
		if (ret != PQOS_RETVAL_OK) {
			fprintf(stderr, "Error configuring L3 COS#%u on socket "
				"%u!\n", cos_id, socket_id);
			return -EFAULT;
		}
	}

	if (m_cap_l2ca != NULL  && m_cap_l2ca->u.l2ca->num_classes > cos_id) {
		struct pqos_l2ca ca = *l2ca;

		/* if COS is not configured, set it to default */
		if (!rdt_ca_is_valid(wrap_l2ca(&ca)))
			ca = l2_defs;

		/* set proper COS id */
		ca.class_id = cos_id;

		ret = pqos_l2ca_set(socket_id, 1, &ca);
		if (ret != PQOS_RETVAL_OK) {
			fprintf(stderr, "Error configuring L2 COS#%u on socket "
				"%u!\n", cos_id, socket_id);
			return -EFAULT;
		}
	}

	return 0;
}

/**
 * @brief Sets L2/L3 configuration.
 *
 * pqos_l*ca.class_id values are ignored, available/unused COS are used.
 *
 * @param [in] num number of configuration entries in \a l2ca , \a l3ca
 *             and \a cores
 * @param [in] l2ca L2 CAT configuration table
 * @param [in] l3ca L3 CAT configuration table
 * @param [in] cores cores configuration table
 *
 * @return status
 * @retval 0 on success
 * @retval negative on error (-errno)
 */
static int
cat_set(const unsigned num, struct pqos_l2ca l2ca[const],
		struct pqos_l3ca l3ca[const], cpu_set_t cores[const])
{
	int i, ret;

	if (num == 0 || NULL == l2ca || NULL == l3ca || NULL == cores)
		return -EINVAL;

	for (i = 0; (unsigned)i < num; i++) {
		unsigned j, technology = 0;

		if (rdt_ca_is_valid(wrap_l2ca(&l2ca[i])))
			technology |= (1 << PQOS_CAP_TYPE_L2CA);

		if (rdt_ca_is_valid(wrap_l3ca(&l3ca[i])))
			technology |= (1 << PQOS_CAP_TYPE_L3CA);

		for (j = 0; j < RDT_MAX_SOCKETS; j++) {
			unsigned core_array[CPU_SETSIZE] = {0};
			unsigned core_num, cos_id;

			/* Get cores on socket j */
			ret = get_socket_cores(&cores[i], j, &core_num,
				core_array);
			if (ret != 0)
				goto err;

			if (core_num == 0)
				continue;

			/* Assign COS to those cores */
			ret = pqos_alloc_assign(technology, core_array,
				core_num, &cos_id);
			if (ret != PQOS_RETVAL_OK) {
				if (ret == PQOS_RETVAL_RESOURCE)
					fprintf(stderr, "No free COS available "
						"on socket %u.\n", j);
				else
					fprintf(stderr, "Unable to assign COS "
						"on socket %u!\n", j);

				goto err;
			}

			/* Configure COS on socket j */
			ret = cat_configure_cos(&l2ca[i], &l3ca[i], j, cos_id);
			if (ret != 0)
				goto err;
		}
	}

	return 0;

err:
	printf("Reverting CAT configuration...\n");
	for (; i >= 0; i--)
		(void)alloc_release(&cores[i]);

	return ret;
}

int
cat_configure(void)
{
	struct pqos_l3ca l3ca[g_cfg.config_count];
	struct pqos_l2ca l2ca[g_cfg.config_count];
	cpu_set_t cpu[g_cfg.config_count];
	unsigned i = 0;
	int ret = 0;

	/* Validate cmd line configuration */
	ret = cat_validate();
	if (ret != 0) {
		fprintf(stderr, "CAT: Requested CAT configuration is not valid!"
			"\n");
		return ret;
	}

	for (i = 0; i < g_cfg.config_count; i++) {
		l3ca[i] = g_cfg.config[i].l3;
		l2ca[i] = g_cfg.config[i].l2;
		cpu[i] = g_cfg.config[i].cpumask;
	}

	ret = cat_set(g_cfg.config_count, l2ca, l3ca, cpu);
	if (ret != 0)
		fprintf(stderr, "CAT: Failed to configure CAT!\n");

	return ret;
}

int
cat_reset(void)
{
	unsigned i = 0;
	int ret = 0;

	/* Associate COS#0 to cores */
	for (i = 0; i < CPU_SETSIZE; i++) {
		if (0 == CPU_ISSET(i, &g_cfg.reset_cpuset))
			continue;

		ret = pqos_alloc_assoc_set(i, 0);
		if (ret != PQOS_RETVAL_OK) {
			fprintf(stderr, "Error associating COS,"
				"core: %u, COS: 0!\n", i);
			return -EFAULT;
		}
	}

	return 0;
}

void
cat_fini(void)
{
	int ret = 0;

	if (g_cfg.verbose)
		printf("Shutting down PQoS library...\n");

	/* deallocate all the resources */
	ret = pqos_fini();
	if (ret != PQOS_RETVAL_OK && ret != PQOS_RETVAL_INIT)
		fprintf(stderr, "Error shutting down PQoS library!\n");

	m_cap = NULL;
	m_cpu = NULL;
	m_cap_l3ca = NULL;
	memset(g_cfg.config, 0, sizeof(g_cfg.config));
	g_cfg.config_count = 0;
}

void
cat_exit(void)
{
	unsigned i = 0;

	/* if lib is not initialized, do nothing */
	if (m_cap == NULL && m_cpu == NULL)
		return;

	if (g_cfg.verbose)
		printf("CAT: Reverting CAT configuration...\n");

	for (i = 0; i < g_cfg.config_count; i++)
		if (alloc_release(&g_cfg.config[i].cpumask) != 0)
			fprintf(stderr, "Failed to release COS!\n");

	cat_fini();
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
		fprintf(stderr, "CAT: CAT module already initialized!\n");
		return -EEXIST;
	}

	/* PQoS Initialization - Check and initialize CAT capability */
	memset(&cfg, 0, sizeof(cfg));
	cfg.fd_log = STDOUT_FILENO;
	cfg.verbose = 0;
	ret = pqos_init(&cfg);
	if (ret != PQOS_RETVAL_OK) {
		fprintf(stderr, "CAT: Error initializing PQoS library!\n");
		ret = -EFAULT;
		goto err;
	}

	/* Get capability and CPU info pointer */
	ret = pqos_cap_get(&m_cap, &m_cpu);
	if (ret != PQOS_RETVAL_OK || m_cap == NULL || m_cpu == NULL) {
		fprintf(stderr, "CAT: Error retrieving PQoS capabilities!\n");
		ret = -EFAULT;
		goto err;
	}

	/* Get L2CA capabilities */
	ret = pqos_cap_get_type(m_cap, PQOS_CAP_TYPE_L2CA, &m_cap_l2ca);
	if (g_cfg.verbose && (ret != PQOS_RETVAL_OK || m_cap_l2ca == NULL))
		printf("CAT: L2 CAT capability not supported.\n");

	/* Get L3CA capabilities */
	ret = pqos_cap_get_type(m_cap, PQOS_CAP_TYPE_L3CA, &m_cap_l3ca);
	if (g_cfg.verbose && (ret != PQOS_RETVAL_OK || m_cap_l3ca == NULL))
		printf("CAT: L3 CAT capability not supported.\n");

	if (m_cap_l3ca == NULL && m_cap_l2ca == NULL) {
		fprintf(stderr, "CAT: L2 CAT, L3 CAT "
			"capabilities not supported!\n");
		ret = -EFAULT;
		goto err;
	}

	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);

	ret = atexit(cat_exit);
	if (ret != 0) {
		ret = -EFAULT;
		fprintf(stderr, "CAT: Cannot set exit function\n");
		goto err;
	}

	return 0;

err:
	/* deallocate all the resources */
	cat_fini();
	return ret;
}

void
print_cmd_line_rdt_config(void)
{
	char cpustr[CPU_SETSIZE * 3];
	unsigned i;

	if (CPU_COUNT(&g_cfg.reset_cpuset) != 0) {
		cpuset_to_str(cpustr, sizeof(cpustr), &g_cfg.reset_cpuset);
		printf("CAT Reset: CPUs: %s\n", cpustr);
	}

	for (i = 0; i < g_cfg.config_count; i++) {
		struct rdt_ca ca_array[2];
		unsigned j;

		if (CPU_COUNT(&g_cfg.config[i].cpumask) == 0)
			continue;

		cpuset_to_str(cpustr, sizeof(cpustr), &g_cfg.config[i].cpumask);

		ca_array[0] = wrap_l2ca(&g_cfg.config[i].l2);
		ca_array[1] = wrap_l3ca(&g_cfg.config[i].l3);

		for (j = 0; j < DIM(ca_array); j++) {
			if (!rdt_ca_is_valid(ca_array[j]))
				continue;
			printf("%s Allocation: CPUs: %s ",
				rdt_ca_get_type_str(ca_array[j]), cpustr);
			rdt_ca_print(stdout, ca_array[j]);
			printf("\n");
		}
	}
}
