/*
 * BSD LICENSE
 *
 * Copyright(c) 2017 Intel Corporation. All rights reserved.
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

#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>

#include <pqos.h>
#include "os_allocation.h"
#include "cap.h"
#include "log.h"
#include "types.h"

/*
 * Path to resctrl file system
 */
static const char *rctl_path = "/sys/fs/resctrl/";

/**
 * ---------------------------------------
 * Local data structures
 * ---------------------------------------
 */
static const struct pqos_cap *m_cap = NULL;
static const struct pqos_cpuinfo *m_cpu = NULL;

/*
 * Max supported number of CPU's
 */
#define MAX_CPUS 4096


/**
 * ---------------------------------------
 * CPU mask structures and utility functions
 * ---------------------------------------
 */

#define CPUMASK_BITS_PER_CHAR 4
#define CPUMASK_BITS_PER_ITEM 8
#define CPUMASK_CHARS_PER_ITEM (CPUMASK_BITS_PER_ITEM / CPUMASK_BITS_PER_CHAR)
#define CPUMASK_GET_TAB_INDEX(mask, lcore) \
	((mask->length * CPUMASK_BITS_PER_CHAR - lcore - 1) \
	/ CPUMASK_BITS_PER_ITEM)
#define CPUMASK_GET_BIT_INDEX(mask, lcore) \
	((mask->length * CPUMASK_BITS_PER_CHAR + lcore) % 8)

/**
 * @brief Structure to hold parsed cpu mask
 *
 * Structure contains table with cpu bit mask. Each table item holds
 * information about 8 bit in mask. Items in table are held in reverse order.
 *
 * Example bitmask tables:
 *  - cpus file contains 'ABC' mask = [ 0xAB, 0xC0, ... ]
 *  - cpus file containd 'ABCD' mask = [ 0xAB, 0xCD, ... ]
 */
struct cpumask {
	unsigned length;	                        /**< Length of mask
							string in cpus file */
	uint8_t tab[MAX_CPUS / CPUMASK_BITS_PER_ITEM];  /**< bit mask table */
};

/**
 * @brief Set lcore bit in cpu mask
 *
 * @param [in] lcore Core number
 * @param [in] cpumask Modified cpu mask
 */
static void
cpumask_set(const unsigned lcore, struct cpumask *mask)
{
	/* index in mask table */
	const unsigned item = CPUMASK_GET_TAB_INDEX(mask, lcore);
	const unsigned bit = CPUMASK_GET_BIT_INDEX(mask, lcore);

	/* get pointer to table item containing information about lcore */
	uint8_t *m = &(mask->tab[item]);

	/* Set lcore bit in mask table item */
	*m = *m | (1 << bit);
}

/**
 * @brief Check if lcore is set in cpu mask
 *
 * @param [in] lcore Core number
 * @param [in] cpumask Cpu mask
 *
 * @return Returns 1 when bit corresponding to lcore is set in mask
 * @retval 1 if cpu bit is set im mask
 * @retval 0 if cpu bit is not set in mask
 */
static int
cpumask_get(const unsigned lcore, const struct cpumask *mask)
{
	/* index in mask table */
	const unsigned item = CPUMASK_GET_TAB_INDEX(mask, lcore);
	const unsigned bit = CPUMASK_GET_BIT_INDEX(mask, lcore);

	/* get pointer to table item containing information about lcore */
	const uint8_t *m = &(mask->tab[item]);

	/* Check if lcore bit is set in mask table item */
	return ((*m >> bit) & 0x1) == 0x1;
}

/**
 * @brief Opens COS cpu file
 *
 * @param [in] class_is COS id
 * @param [in] mode fopen mode
 *
 * @return Pointer to the stream
 */
static FILE *
cpumask_fopen(const unsigned class_id, const char *mode)
{
	FILE *fd;
	char buf[128];
	int result;

	memset(buf, 0, sizeof(buf));
	if (class_id == 0) {
		result = snprintf(buf, sizeof(buf) - 1, "%scpus", rctl_path);
	} else
		result = snprintf(buf, sizeof(buf) - 1,
			          "%sCOS%u/cpus", rctl_path, class_id);

	if (result < 0)
		return NULL;

	fd = fopen(buf, mode);
	if (fd == NULL)
		LOG_ERROR("Could not open cpu file %s\n", buf);

	return fd;
}

/**
 * @brief Write CPU mask to file
 *
 * @param [in] class_is COS id
 * @param [in] maks CPU mask to write
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK on success
 */
static int
cpumask_write(const unsigned class_id, const struct cpumask *mask) {
	int ret = PQOS_RETVAL_OK;
	FILE *fd;
	unsigned  i;

	fd = cpumask_fopen(class_id, "w");
	if (fd == NULL)
		return PQOS_RETVAL_ERROR;

	for (i = 0; i < mask->length; i++) {
		const uint8_t *item = &(mask->tab[i / CPUMASK_CHARS_PER_ITEM]);
		unsigned value = *item;

		/* Determine which hex value of the hex pairing is to be
		 * written. e.g 0xfe, are you writing f or e */
		value >>= ((i + 1) % CPUMASK_CHARS_PER_ITEM)
			* CPUMASK_BITS_PER_CHAR;
		/* Remove the hex value you are not going to write from
		 * the hex pairing */
		value &= (1 << CPUMASK_BITS_PER_CHAR) - 1;

		if (fprintf(fd, "%01x", value) < 0) {
			LOG_ERROR("Failed to write cpu mask\n");
			fclose(fd);
			return PQOS_RETVAL_ERROR;
		}
	}

	fclose(fd);

	return ret;
}

/**
 * @brief Read CPU mask from file
 *
 * @param [in] class_is COS id
 * @param [out] maks CPU mask to write
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK on success
 */
static int
cpumask_read(const unsigned class_id, struct cpumask *mask) {
	int ret = PQOS_RETVAL_OK;
	FILE *fd;
	unsigned character;

	fd = cpumask_fopen(class_id, "r");
	if (fd == NULL)
		return PQOS_RETVAL_ERROR;

	memset(mask, 0, sizeof(struct cpumask));
	while (fscanf(fd, "%1x", &character) == 1) {
		const unsigned index = mask->length / CPUMASK_CHARS_PER_ITEM;
		uint8_t *item = &(mask->tab[index]);

		/* Determine where in the hex pairing the value is to be read
		 * into e.g 0xfe, are you reading in pos f or pos e */
		character <<= CPUMASK_BITS_PER_CHAR
			* ((mask->length + 1) % CPUMASK_CHARS_PER_ITEM);
		/* Ensure you dont loose any hex values read in already,
		 * e.g 0xf0 should become 0xfe when e is read in */
		*item |= (uint8_t) character;
		mask->length++;
	}

	if (!feof(fd))
		ret = PQOS_RETVAL_ERROR;

	fclose(fd);

	return ret;
}

/**
 * @brief Function to find the maximum number of resctrl groups allowed
 *
 * @param cap pqos capabilities structure
 * @param num_rctl_grps place to store result
 * @return Operational Status
 */
static int
os_get_max_rctl_grps(const struct pqos_cap *cap,
		     unsigned *num_rctl_grps)
{
	unsigned i;
	unsigned max_rctl_grps = 0;
	int ret = PQOS_RETVAL_OK;

	ASSERT(cap != NULL);
	ASSERT(num_rctl_grps != NULL);

	/*
	 * Loop through all caps that have OS support
	 * Find max COS supported by all
	 */
	for (i = 0; i < cap->num_cap; i++) {
		unsigned num_cos = 0;
		const struct pqos_capability *p_cap = &cap->capabilities[i];

		if (!p_cap->os_support)
			continue;

		/* get L3 CAT COS num */
		if (p_cap->type == PQOS_CAP_TYPE_L3CA) {
			ret = pqos_l3ca_get_cos_num(cap, &num_cos);
			if (ret != PQOS_RETVAL_OK)
				return ret;

			if (max_rctl_grps == 0)
				max_rctl_grps = num_cos;
			else if (num_cos < max_rctl_grps)
				max_rctl_grps = num_cos;
		}
		/* get L2 CAT COS num */
		if (p_cap->type == PQOS_CAP_TYPE_L2CA) {
			ret = pqos_l2ca_get_cos_num(cap, &num_cos);
			if (ret != PQOS_RETVAL_OK)
				return ret;

			if (max_rctl_grps == 0)
				max_rctl_grps = num_cos;
			else if (num_cos < max_rctl_grps)
				max_rctl_grps = num_cos;
		}
	}
	*num_rctl_grps = max_rctl_grps;
	return PQOS_RETVAL_OK;
}

/**
 * @brief Prepares and authenticates resctrl file system
 *        used for OS allocation interface
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK success
 */
static int os_alloc_prep(void)
{
        unsigned i, num_grps = 0;
        int ret;

        ASSERT(m_cap != NULL);
        ret = os_get_max_rctl_grps(m_cap, &num_grps);
	if (ret != PQOS_RETVAL_OK)
		return ret;
        /*
         * Detect/Create all available COS resctrl groups
         */
	for (i = 1; i < num_grps; i++) {
		char buf[128];
		struct stat st;

		memset(buf, 0, sizeof(buf));
		if (snprintf(buf, sizeof(buf) - 1,
                             "%sCOS%d", rctl_path, (int) i) < 0)
			return PQOS_RETVAL_ERROR;

		/* if resctrl group doesn't exist - create it */
		if (stat(buf, &st) == 0) {
			LOG_DEBUG("resctrl group COS%d detected\n", i);
			continue;
		}

		if (mkdir(buf, 0755) == -1) {
			LOG_DEBUG("Failed to create resctrl group %s!\n", buf);
			return PQOS_RETVAL_BUSY;
		}
		LOG_DEBUG("resctrl group COS%d created\n", i);
	}
	return PQOS_RETVAL_OK;
}

int
os_alloc_init(const struct pqos_cpuinfo *cpu, const struct pqos_cap *cap)
{
	if (cpu == NULL || cap == NULL)
		return PQOS_RETVAL_PARAM;

	m_cap = cap;
	m_cpu = cpu;

        return os_alloc_prep();
}

int
os_alloc_fini(void)
{
        int ret = PQOS_RETVAL_OK;

        m_cap = NULL;
        m_cpu = NULL;
        return ret;
}

int
os_alloc_assoc_set(const unsigned lcore,
                   const unsigned class_id)
{
	int ret = PQOS_RETVAL_OK;
	unsigned num_l2_cos = 0, num_l3_cos = 0;
	struct cpumask mask;

	ASSERT(m_cpu != NULL);
	ret = pqos_cpu_check_core(m_cpu, lcore);
	if (ret != PQOS_RETVAL_OK)
		return PQOS_RETVAL_PARAM;

	ASSERT(m_cap != NULL);
	ret = pqos_l3ca_get_cos_num(m_cap, &num_l3_cos);
	if (ret != PQOS_RETVAL_OK && ret != PQOS_RETVAL_RESOURCE)
		return ret;

	ret = pqos_l2ca_get_cos_num(m_cap, &num_l2_cos);
	if (ret != PQOS_RETVAL_OK && ret != PQOS_RETVAL_RESOURCE)
		return ret;

	if (class_id >= num_l3_cos && class_id >= num_l2_cos)
		/* class_id is out of bounds */
		return PQOS_RETVAL_PARAM;

	ret = cpumask_read(class_id, &mask);
	if (ret != PQOS_RETVAL_OK)
		return ret;

	cpumask_set(lcore, &mask);

	ret = cpumask_write(class_id, &mask);

	return ret;
}

int
os_alloc_assoc_get(const unsigned lcore,
                   unsigned *class_id)
{
	int ret;
	unsigned grps, i;
	struct cpumask mask;

	if (class_id == NULL)
		return PQOS_RETVAL_PARAM;

	ASSERT(m_cpu != NULL);
	ret = pqos_cpu_check_core(m_cpu, lcore);
	if (ret != PQOS_RETVAL_OK)
		return PQOS_RETVAL_PARAM;

	ret = os_get_max_rctl_grps(m_cap, &grps);
	if (ret != PQOS_RETVAL_OK)
		return ret;

	for (i = 0; i < grps; i++) {
		ret = cpumask_read(i, &mask);
		if (ret != PQOS_RETVAL_OK)
			return ret;

		if (cpumask_get(lcore, &mask)) {
			*class_id = i;
			return PQOS_RETVAL_OK;
		}
	}

	return PQOS_RETVAL_ERROR;
}
