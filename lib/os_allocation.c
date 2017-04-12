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
#include <ctype.h>
#include <stdlib.h>
#include <sys/mount.h>

#include <pqos.h>
#include "os_allocation.h"
#include "cap.h"
#include "log.h"
#include "types.h"

/*
 * Path to resctrl file system
 */
static const char *rctl_path = "/sys/fs/resctrl/";
static const char *rctl_cpus = "cpus";
static const char *rctl_schemata = "schemata";

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
 * @brief Opens COS file in resctl filesystem
 *
 * @param [in] class_id COS id
 * @param [in] name File name
 * @param [in] mode fopen mode
 *
 * @return Pointer to the stream
 * @retval Pointer on success
 * @retval NULL on error
 */
static FILE *
rctl_fopen(const unsigned class_id, const char *name, const char *mode)
{
	FILE *fd;
	char buf[128];
	int result;

	memset(buf, 0, sizeof(buf));
	if (class_id == 0)
		result = snprintf(buf, sizeof(buf) - 1,
		                  "%s%s", rctl_path, name);
	else
		result = snprintf(buf, sizeof(buf) - 1,
			          "%sCOS%u/%s", rctl_path, class_id, name);

	if (result < 0)
		return NULL;

	fd = fopen(buf, mode);
	if (fd == NULL)
		LOG_ERROR("Could not open %s file %s for COS %u\n",
		          name, buf, class_id);

	return fd;
}

/**
 * @brief Converts string into 64-bit unsigned number.
 *
 * Numbers can be in decimal or hexadecimal format.
 *
 * @param [in] s string to be converted into 64-bit unsigned number
 * @param [in] base Numerical base
 * @param [out] Numeric value of the string representing the number
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK on success
 */
static int
strtouint64(const char *s, int base, uint64_t *value)
{
	char *endptr = NULL;

	ASSERT(s != NULL);
	if (strncasecmp(s, "0x", 2) == 0) {
		base = 16;
		s += 2;
	}

	*value = strtoull(s, &endptr, base);
	if (!(*s != '\0' && (*endptr == '\0' || *endptr == '\n')))
		return PQOS_RETVAL_ERROR;

	return PQOS_RETVAL_OK;
}

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
 * @retval 1 if cpu bit is set in mask
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
 * @brief Write CPU mask to file
 *
 * @param [in] class_id COS id
 * @param [in] mask CPU mask to write
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK on success
 */
static int
cpumask_write(const unsigned class_id, const struct cpumask *mask)
{
	int ret = PQOS_RETVAL_OK;
	FILE *fd;
	unsigned  i;

	fd = rctl_fopen(class_id, rctl_cpus, "w");
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
 * @param [in] class_id COS id
 * @param [out] mask CPU mask to write
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK on success
 */
static int
cpumask_read(const unsigned class_id, struct cpumask *mask)
{
	int ret = PQOS_RETVAL_OK;
	FILE *fd;
	unsigned character;

	fd = rctl_fopen(class_id, rctl_cpus, "r");
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
 * ---------------------------------------
 * Schemata structures and utility functions
 * ---------------------------------------
 */

/*
 * @brief Structure to hold parsed schemata
 */
struct schemata {
	unsigned l3ca_num;      /**< Number of L3 COS held in struct */
	struct pqos_l3ca *l3ca; /**< L3 COS definitions */
	unsigned l2ca_num;      /**< Number of L2 COS held in struct */
	struct pqos_l2ca *l2ca; /**< L2 COS definitions */
};

/*
 * @brief Deallocate memory of schemata struct
 *
 * @param[in] schemata Schemata structure
 */
static void
schemata_fini(struct schemata *schemata) {
	if (schemata->l2ca != NULL)
		free(schemata->l2ca);
	if (schemata->l3ca != NULL)
		free(schemata->l3ca);
}

/**
 * @brief Allocates memory of schemata struct
 *
 * @param[in] class_id COS id
 * @param[out] schemata Schemata structure
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK on success
 */
static int
schemata_init(const unsigned class_id, struct schemata *schemata) {
	int ret = PQOS_RETVAL_OK;
	int retval;
	unsigned num_cos, num_ids, i;

	ASSERT(schemata != NULL);

	memset(schemata, 0, sizeof(struct schemata));

	/* L2 */
	retval = pqos_l2ca_get_cos_num(m_cap, &num_cos);
	if (retval == PQOS_RETVAL_OK && class_id < num_cos) {
		unsigned *l2ids = NULL;

		l2ids = pqos_cpu_get_l2ids(m_cpu, &num_ids);
		if (l2ids == NULL) {
			ret = PQOS_RETVAL_ERROR;
			goto schemata_init_exit;
		}

		free(l2ids);

		schemata->l2ca_num = num_ids;
		schemata->l2ca = calloc(num_ids, sizeof(struct pqos_l2ca));
		if (schemata->l2ca == NULL) {
			ret = PQOS_RETVAL_ERROR;
			goto schemata_init_exit;
		}

		/* fill class_id */
		for (i = 0; i < num_ids; i++)
			schemata->l2ca[i].class_id = class_id;
	}

	/* L3 */
	retval = pqos_l3ca_get_cos_num(m_cap, &num_cos);
	if (retval == PQOS_RETVAL_OK && class_id < num_cos) {
		unsigned *sockets = NULL;
		int cdp_enabled;

		sockets = pqos_cpu_get_sockets(m_cpu, &num_ids);
		if (sockets == NULL) {
			ret = PQOS_RETVAL_ERROR;
			goto schemata_init_exit;
		}

		free(sockets);

		schemata->l3ca_num = num_ids;
		schemata->l3ca = calloc(num_ids, sizeof(struct pqos_l3ca));
		if (schemata->l3ca == NULL) {
			ret = PQOS_RETVAL_ERROR;
			goto schemata_init_exit;
		}

		ret = pqos_l3ca_cdp_enabled(m_cap, NULL, &cdp_enabled);
		if (ret != PQOS_RETVAL_OK)
			goto schemata_init_exit;

		/* fill class_id and cdp values */
		for (i = 0; i < num_ids; i++) {
			schemata->l3ca[i].class_id = class_id;
			schemata->l3ca[i].cdp = cdp_enabled;
		}
	}

 schemata_init_exit:
	/* Deallocate memory in case of error */
	if (ret != PQOS_RETVAL_OK)
		schemata_fini(schemata);

	return ret;
}

/**
 * @brief Schemata type
 */
enum schemata_type {
	SCHEMATA_TYPE_NONE,   /**< unknown */
	SCHEMATA_TYPE_L2,     /**< L2 CAT */
	SCHEMATA_TYPE_L3,     /**< L3 CAT without CDP */
	SCHEMATA_TYPE_L3CODE, /**< L3 CAT code */
	SCHEMATA_TYPE_L3DATA, /**< L3 CAT data */
};


/**
 * @brief Determine allocation type
 *
 * @param [in] str resctrl label
 *
 * @return Allocation type
 */
static int
schemata_type_get(const char *str)
{
	enum schemata_type type = SCHEMATA_TYPE_NONE;

	if (strcasecmp(str, "L2") == 0)
		type = SCHEMATA_TYPE_L2;
	else if (strcasecmp(str, "L3") == 0)
		type = SCHEMATA_TYPE_L3;
	else if (strcasecmp(str, "L3CODE") == 0)
		type = SCHEMATA_TYPE_L3CODE;
	else if (strcasecmp(str, "L3DATA") == 0)
		type = SCHEMATA_TYPE_L3DATA;

	return type;
}

/**
 * @brief Fill schemata structure
 *
 * @param [in] class_is COS id
 * @param [in] res_id Resource id
 * @param [in] mask Ways mask
 * @param [in] type Schemata type
 * @param [out] schemata Schemata structure
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK on success
 */
static int
schemata_set(const unsigned class_id,
             const unsigned res_id,
	     const uint64_t mask,
	     const int type,
	     struct schemata *schemata)
{
	if (type == SCHEMATA_TYPE_L2) {
		if (schemata->l2ca_num <= res_id)
			return PQOS_RETVAL_ERROR;
		schemata->l2ca[res_id].class_id = class_id;
		schemata->l2ca[res_id].ways_mask = mask;

	} else if (type == SCHEMATA_TYPE_L3) {
		if (schemata->l3ca_num <= res_id || schemata->l3ca[res_id].cdp)
			return PQOS_RETVAL_ERROR;
		schemata->l3ca[res_id].class_id = class_id;
		schemata->l3ca[res_id].u.ways_mask = mask;

	} else if (type == SCHEMATA_TYPE_L3CODE) {
		if (schemata->l3ca_num <= res_id || !schemata->l3ca[res_id].cdp)
			return PQOS_RETVAL_ERROR;
		schemata->l3ca[res_id].class_id = class_id;
		schemata->l3ca[res_id].u.s.code_mask = mask;

	} else if (type == SCHEMATA_TYPE_L3DATA) {
		if (schemata->l3ca_num <= res_id || !schemata->l3ca[res_id].cdp)
			return PQOS_RETVAL_ERROR;
		schemata->l3ca[res_id].class_id = class_id;
		schemata->l3ca[res_id].u.s.data_mask = mask;
	}

	return PQOS_RETVAL_OK;
}

/**
 * @brief Read resctrl schemata from file
 *
 * @param [in] class_is COS id
 * @param [out] schemata Parsed schemata
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK on success
 */
static int
schemata_read(const unsigned class_id, struct schemata *schemata)
{
	int ret = PQOS_RETVAL_OK;
	FILE *fd;
	enum schemata_type type = SCHEMATA_TYPE_NONE;
	char buf[16 * 1024];
	char *p = NULL, *q = NULL, *saveptr = NULL;

	ASSERT(schemata != NULL);

	fd = rctl_fopen(class_id, rctl_schemata, "r");
	if (fd == NULL)
		return PQOS_RETVAL_ERROR;

	if ((schemata->l3ca_num > 0 && schemata->l3ca == NULL)
	    || (schemata->l2ca_num > 0 && schemata->l2ca == NULL)) {
		ret = PQOS_RETVAL_ERROR;
		goto schemata_read_exit;
	}

	memset(buf, 0, sizeof(buf));
	while (fgets(buf, sizeof(buf), fd) != NULL) {
		/**
		 * Determine allocation type
		 */
		p =  strchr(buf, ':');
		if (p == NULL) {
			ret = PQOS_RETVAL_ERROR;
			break;
		}
		*p = '\0';
		type = schemata_type_get(buf);

		/* Skip unknown label */
		if (type == SCHEMATA_TYPE_NONE)
			continue;

		/**
		 * Parse COS masks
		 */
		for (++p; ; p = NULL) {
			char *token = NULL;
			uint64_t id = 0;
			uint64_t mask = 0;

			token = strtok_r(p, ";", &saveptr);
			if (token == NULL)
				break;

			q = strchr(token, '=');
			if (q == NULL) {
				ret = PQOS_RETVAL_ERROR;
				goto schemata_read_exit;
			}
			*q = '\0';

			ret = strtouint64(token, 10, &id);
			if (ret != PQOS_RETVAL_OK)
				goto schemata_read_exit;

			ret = strtouint64(q + 1, 16, &mask);
			if (ret != PQOS_RETVAL_OK)
				goto schemata_read_exit;

			ret = schemata_set(class_id, id, mask, type, schemata);
			if (ret != PQOS_RETVAL_OK)
				goto schemata_read_exit;
		}
	}

 schemata_read_exit:
	fclose(fd);

	return ret;
}

/**
 * @brief Write resctrl schemata to file
 *
 * @param [in] class_id COS id
 * @param [in] schemata Schemata to write
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK on success
 */
static int
schemata_write(const unsigned class_id, const struct schemata *schemata)
{
	int ret = PQOS_RETVAL_OK;
	unsigned i;
	FILE *fd;
	char buf[16 * 1024];

	ASSERT(schemata != NULL);

	fd = rctl_fopen(class_id, rctl_schemata, "w");
	if (fd == NULL)
		return PQOS_RETVAL_ERROR;

	/* Enable fully buffered output. File won't be flushed until 16kB
	 * buffer is full */
	if (setvbuf(fd, buf, _IOFBF, sizeof(buf)) != 0) {
		fclose(fd);
		return PQOS_RETVAL_ERROR;
	}

	/* L2 */
	if (schemata->l2ca_num > 0) {
		fprintf(fd, "L2:");
		for (i = 0; i < schemata->l2ca_num; i++) {
			if (i > 0)
				fprintf(fd, ";");
			fprintf(fd, "%u=%x", i, schemata->l2ca[i].ways_mask);
		}
		fprintf(fd, "\n");
	}

	/* L3 without CDP */
	if (schemata->l3ca_num > 0 && !schemata->l3ca[0].cdp) {
		fprintf(fd, "L3:");
		for (i = 0; i < schemata->l3ca_num; i++) {
			if (i > 0)
				fprintf(fd, ";");
			fprintf(fd, "%u=%llx", i, (unsigned long long)
			        schemata->l3ca[i].u.ways_mask);
		}
		fprintf(fd, "\n");
	}

	/* L3 with CDP */
	if (schemata->l3ca_num > 0 && schemata->l3ca[0].cdp) {
		fprintf(fd, "L3CODE:");
		for (i = 0; i < schemata->l3ca_num; i++) {
			if (i > 0)
				fprintf(fd, ";");
			fprintf(fd, "%u=%llx", i, (unsigned long long)
				schemata->l3ca[i].u.s.code_mask);
		}
		fprintf(fd, "\nL3DATA:");
		for (i = 0; i < schemata->l3ca_num; i++) {
			if (i > 0)
				fprintf(fd, ";");
			fprintf(fd, "%u=%llx", i, (unsigned long long)
			        schemata->l3ca[i].u.s.data_mask);
		}
		fprintf(fd, "\n");
	}
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
 * @brief Function to mount the resctrl file system with CDP option
 *
 * @param l3_cdp_cfg CDP option
 * @return Operational status
 * @retval PQOS_RETVAL_OK on success
 */
static int
os_interface_mount(const enum pqos_cdp_config l3_cdp_cfg)
{
        const struct pqos_cap_l3ca *l3_cap = NULL;
        const struct pqos_capability *alloc_cap = NULL;
        const char *cdp_option = NULL; /**< cdp_off default */

        if (l3_cdp_cfg != PQOS_REQUIRE_CDP_ON &&
            l3_cdp_cfg != PQOS_REQUIRE_CDP_OFF) {
                LOG_ERROR("Invalid CDP mounting setting %d!\n",
                          l3_cdp_cfg);
                return PQOS_RETVAL_PARAM;
        }

        if (l3_cdp_cfg == PQOS_REQUIRE_CDP_OFF)
                goto mount;

        /* Get L3 CAT capabilities */
        (void) pqos_cap_get_type(m_cap, PQOS_CAP_TYPE_L3CA, &alloc_cap);
        if (alloc_cap != NULL)
                l3_cap = alloc_cap->u.l3ca;

        if (l3_cap != NULL && !l3_cap->cdp) {
                /* Check against erroneous CDP request */
                LOG_ERROR("CDP requested but not supported by the platform!\n");
                return PQOS_RETVAL_PARAM;
        }
        cdp_option = "cdp";  /**< cdp_on */

 mount:
        if (mount("resctrl", rctl_path, "resctrl", 0, cdp_option) != 0)
                return PQOS_RETVAL_ERROR;

        return PQOS_RETVAL_OK;
}

/**
 * @brief Check to see if resctrl is supported.
 *        If it is attempt to mount the file system.
 *
 * @return Operational status
 */
static int
os_alloc_check(void)
{
        int ret;
        unsigned i, supported = 0;

        /**
         * Check if resctrl is supported
         */
        for (i = 0; i < m_cap->num_cap; i++) {
                if (m_cap->capabilities[i].os_support == 1) {
                        if (m_cap->capabilities[i].type == PQOS_CAP_TYPE_L3CA)
                                supported = 1;
                        if (m_cap->capabilities[i].type == PQOS_CAP_TYPE_L2CA)
                                supported = 1;
                        if (m_cap->capabilities[i].type == PQOS_CAP_TYPE_MBA)
                                supported = 1;
                }
        }

        if (!supported)
                return PQOS_RETVAL_OK;
        /**
         * Check if resctrl is mounted
         */
        if (access("/sys/fs/resctrl/cpus", 0) != 0) {
                const struct pqos_capability *alloc_cap = NULL;
                int cdp_mount = PQOS_REQUIRE_CDP_OFF;
                /* Get L3 CAT capabilities */
                (void) pqos_cap_get_type(m_cap, PQOS_CAP_TYPE_L3CA, &alloc_cap);
                if (alloc_cap != NULL)
                        cdp_mount = alloc_cap->u.l3ca->cdp_on;

                ret = os_interface_mount(cdp_mount);
                if (ret != PQOS_RETVAL_OK) {
                        LOG_INFO("Unable to mount resctrl\n");
                        return PQOS_RETVAL_RESOURCE;
                }
        }

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
	int ret;

        if (cpu == NULL || cap == NULL)
		return PQOS_RETVAL_PARAM;

	m_cap = cap;
	m_cpu = cpu;

        ret = os_alloc_check();
        if (ret != PQOS_RETVAL_OK)
                return ret;

        ret = os_alloc_prep();

        return ret;
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
	int ret;
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

/**
 * @brief Gets unused COS on a socket or L2 cluster
 *
 * The lowest acceptable COS is 1, as 0 is a default one
 *
 * @param [in] id socket or L2 cache ID to search for unused COS on
 * @param [in] technology selection of allocation technologies
 * @param [in] hi_class_id highest acceptable COS id
 * @param [out] class_id unused COS
 *
 * @return Operation status
 */
static int
get_unused_cos(const unsigned id,
               const unsigned technology,
               const unsigned hi_class_id,
               unsigned *class_id)
{
        const int l2_req = ((technology & (1 << PQOS_CAP_TYPE_L2CA)) != 0);
        unsigned used_classes[hi_class_id + 1];
        unsigned i, cos;
        int ret;

        if (class_id == NULL)
                return PQOS_RETVAL_PARAM;

        memset(used_classes, 0, sizeof(used_classes));

        /* Create a list of COS used on socket/L2 cluster */
        for (i = 0; i < m_cpu->num_cores; i++) {

                if (l2_req) {
                        /* L2 requested so looking in L2 cluster scope */
                        if (m_cpu->cores[i].l2_id != id)
                                continue;
                } else {
                        /* L2 not requested so looking at socket scope */
                        if (m_cpu->cores[i].socket != id)
                                continue;
                }

                ret = os_alloc_assoc_get(m_cpu->cores[i].lcore, &cos);
                if (ret != PQOS_RETVAL_OK)
                        return ret;

                if (cos > hi_class_id)
                        continue;

                /* Mark as used */
                used_classes[cos] = 1;
        }

        /* Find unused COS */
        for (cos = hi_class_id; cos != 0; cos--) {
                if (used_classes[cos] == 0) {
                        *class_id = cos;
                        return PQOS_RETVAL_OK;
                }
        }

        return PQOS_RETVAL_RESOURCE;
}

int
os_alloc_assign(const unsigned technology,
                const unsigned *core_array,
                const unsigned core_num,
                unsigned *class_id)
{
        const int l2_req = ((technology & (1 << PQOS_CAP_TYPE_L2CA)) != 0);
        unsigned i, hi_cos_id;
        unsigned socket = 0, l2id = 0;
        int ret;

        ASSERT(core_num > 0);
        ASSERT(core_array != NULL);
        ASSERT(class_id != NULL);
        ASSERT(technology != 0);

        /* Check if core belongs to one resource entity */
        for (i = 0; i < core_num; i++) {
                const struct pqos_coreinfo *pi = NULL;

                pi = pqos_cpu_get_core_info(m_cpu, core_array[i]);
                if (pi == NULL) {
                        ret = PQOS_RETVAL_PARAM;
                        goto os_alloc_assign_exit;
                }

                if (l2_req) {
                        /* L2 is requested
                         * The smallest manageable entity is L2 cluster
                         */
                        if (i != 0 && l2id != pi->l2_id) {
                                ret = PQOS_RETVAL_PARAM;
                                goto os_alloc_assign_exit;
                        }
                        l2id = pi->l2_id;
                } else {
                        if (i != 0 && socket != pi->socket) {
                                ret = PQOS_RETVAL_PARAM;
                                goto os_alloc_assign_exit;
                        }
                        socket = pi->socket;
                }
        }

        /* obtain highest class id for all requested technologies */
        ret = os_get_max_rctl_grps(m_cap, &hi_cos_id);
        if (ret != PQOS_RETVAL_OK)
                goto os_alloc_assign_exit;

        /* find an unused class from highest down */
        if (!l2_req)
                ret = get_unused_cos(socket, technology,
                                     hi_cos_id - 1, class_id);
        else
                ret = get_unused_cos(l2id, technology, hi_cos_id - 1, class_id);

        if (ret != PQOS_RETVAL_OK)
                goto os_alloc_assign_exit;

        /* assign cores to the unused class */
        for (i = 0; i < core_num; i++) {
                ret = os_alloc_assoc_set(core_array[i], *class_id);
                if (ret != PQOS_RETVAL_OK)
                        goto os_alloc_assign_exit;
        }

 os_alloc_assign_exit:
        return ret;
}

int
os_alloc_release(const unsigned *core_array, const unsigned core_num)
{
        int ret;
        unsigned i, cos0 = 0;
        struct cpumask mask;

	ASSERT(m_cpu != NULL);
        ASSERT(core_num > 0 && core_array != NULL);
        /**
         * Set the CPU assoc back to COS0
         */
        ret = cpumask_read(cos0, &mask);
        if (ret != PQOS_RETVAL_OK)
                return ret;
        for (i = 0; i < core_num; i++) {
		if (core_array[i] >= m_cpu->num_cores)
			return PQOS_RETVAL_ERROR;
                cpumask_set(core_array[i], &mask);
	}

        ret = cpumask_write(cos0, &mask);
        if (ret != PQOS_RETVAL_OK)
                LOG_ERROR("CPU assoc reset failed\n");

        return ret;
}

int
os_alloc_reset(const enum pqos_cdp_config l3_cdp_cfg)
{
        const struct pqos_capability *alloc_cap = NULL;
        const struct pqos_cap_l3ca *l3_cap = NULL;
        const struct pqos_cap_l2ca *l2_cap = NULL;
        int ret, cdp_mount, cdp_current = 0;
        unsigned i, cos0 = 0;
        struct cpumask mask;

        ASSERT(l3_cdp_cfg == PQOS_REQUIRE_CDP_ON ||
               l3_cdp_cfg == PQOS_REQUIRE_CDP_OFF ||
               l3_cdp_cfg == PQOS_REQUIRE_CDP_ANY);

        /* Get L3 CAT capabilities */
        (void) pqos_cap_get_type(m_cap, PQOS_CAP_TYPE_L3CA, &alloc_cap);
        if (alloc_cap != NULL) {
                l3_cap = alloc_cap->u.l3ca;
                cdp_current = l3_cap->cdp_on;
        }

        /* Get L2 CAT capabilities */
        alloc_cap = NULL;
        (void) pqos_cap_get_type(m_cap, PQOS_CAP_TYPE_L2CA, &alloc_cap);
        if (alloc_cap != NULL)
                l2_cap = alloc_cap->u.l2ca;

        /* Check if either L2 CAT or L3 CAT is supported */
        if (l2_cap == NULL && l3_cap == NULL) {
                LOG_ERROR("L2 CAT/L3 CAT not present!\n");
                ret = PQOS_RETVAL_RESOURCE; /* no L2/L3 CAT present */
                goto os_alloc_reset_exit;
        }
        /* Check L3 CDP requested while not present */
        if (l3_cap == NULL && l3_cdp_cfg != PQOS_REQUIRE_CDP_ANY) {
                LOG_ERROR("L3 CDP setting requested but no L3 CAT present!\n");
                ret = PQOS_RETVAL_RESOURCE;
                goto os_alloc_reset_exit;
        }
        /* Check against erroneous CDP request */
        if (l3_cdp_cfg == PQOS_REQUIRE_CDP_ON && !l3_cap->cdp) {
                LOG_ERROR("CAT/CDP requested but not supported by the "
                          "platform!\n");
                ret = PQOS_RETVAL_PARAM;
                goto os_alloc_reset_exit;
        }

        /**
         * Set the CPU assoc back to COS0
         */
        ret = cpumask_read(cos0, &mask);
	if (ret != PQOS_RETVAL_OK)
		return ret;
        for (i = 0; i < m_cpu->num_cores; i++)
                cpumask_set(i, &mask);

        ret = cpumask_write(cos0, &mask);
        if (ret != PQOS_RETVAL_OK) {
                LOG_ERROR("CPU assoc reset failed\n");
                return ret;
        }

        /**
         * Umount resctrl to reset schemata
         */
        ret = umount2(rctl_path, 0);
        if (ret != 0) {
                LOG_ERROR("Umount OS interface error!\n");
                goto os_alloc_reset_exit;
        }
        /**
         * Turn L3 CDP ON or OFF
         */
        if (l3_cdp_cfg == PQOS_REQUIRE_CDP_ON)
                cdp_mount = 1;
        else if (l3_cdp_cfg == PQOS_REQUIRE_CDP_ANY)
                cdp_mount = cdp_current;
        else
                cdp_mount = 0;

        /**
         * Mount now with CDP option.
         */
        ret = os_interface_mount(cdp_mount);
        if (ret != PQOS_RETVAL_OK) {
                LOG_ERROR("Mount OS interface error!\n");
                goto os_alloc_reset_exit;
        }
        if (cdp_mount != cdp_current)
                _pqos_cap_l3cdp_change(cdp_current, cdp_mount);
        /**
         * Create the COS dir's in resctrl.
         */
        ret = os_alloc_prep();
        if (ret != PQOS_RETVAL_OK)
                LOG_ERROR("OS alloc prep error!\n");

 os_alloc_reset_exit:
        return ret;
}

int
os_l3ca_set(const unsigned socket,
            const unsigned num_cos,
            const struct pqos_l3ca *ca)
{
	int ret;
	unsigned sockets_num = 0;
	unsigned *sockets = NULL;
	unsigned i;
	unsigned l3ca_num = 0;
	int cdp_enabled = 0;

	ASSERT(ca != NULL);
	ASSERT(num_cos != 0);
	ASSERT(m_cap != NULL);
	ASSERT(m_cpu != NULL);

	ret = pqos_l3ca_get_cos_num(m_cap, &l3ca_num);
	if (ret != PQOS_RETVAL_OK)
		return ret;

	if (num_cos > l3ca_num)
		return PQOS_RETVAL_ERROR;

	/* Get number of sockets in the system */
	sockets = pqos_cpu_get_sockets(m_cpu, &sockets_num);
	if (sockets == NULL || sockets_num == 0) {
		ret = PQOS_RETVAL_ERROR;
		goto os_l3ca_set_exit;
	}

	if (socket >= sockets_num) {
		ret = PQOS_RETVAL_PARAM;
		goto os_l3ca_set_exit;
	}

	ret = pqos_l3ca_cdp_enabled(m_cap, NULL, &cdp_enabled);
	if (ret != PQOS_RETVAL_OK)
		goto os_l3ca_set_exit;

	for (i = 0; i < num_cos; i++) {
		struct schemata schmt;

		if (ca[i].cdp == 1 && cdp_enabled == 0) {
			LOG_ERROR("Attempting to set CDP COS while CDP "
			          "is disabled!\n");
			ret = PQOS_RETVAL_ERROR;
			goto os_l3ca_set_exit;
		}

		ret = schemata_init(ca[i].class_id, &schmt);

		/* read schemata file */
		if (ret == PQOS_RETVAL_OK)
			ret = schemata_read(ca[i].class_id, &schmt);

		/* update and write schemata */
		if (ret == PQOS_RETVAL_OK) {
			schmt.l3ca[socket] = ca[i];
			ret = schemata_write(ca[i].class_id, &schmt);
		}

		schemata_fini(&schmt);

		if (ret != PQOS_RETVAL_OK)
			goto os_l3ca_set_exit;
	}

 os_l3ca_set_exit:
	if (sockets != NULL)
		free(sockets);

	return ret;
}

int
os_l3ca_get(const unsigned socket,
            const unsigned max_num_ca,
            unsigned *num_ca,
            struct pqos_l3ca *ca)
{
	int ret;
	unsigned class_id;
	unsigned count = 0;
	unsigned sockets_num = 0;
	unsigned *sockets = NULL;

	ASSERT(num_ca != NULL);
	ASSERT(ca != NULL);
	ASSERT(max_num_ca != 0);
	ASSERT(m_cap != NULL);
	ASSERT(m_cpu != NULL);

	ret = pqos_l3ca_get_cos_num(m_cap, &count);
	if (ret != PQOS_RETVAL_OK)
		return ret;

	if (count > max_num_ca)
		return PQOS_RETVAL_ERROR;

	sockets = pqos_cpu_get_sockets(m_cpu, &sockets_num);
	if (sockets == NULL || sockets_num == 0) {
		ret = PQOS_RETVAL_ERROR;
		goto os_l3ca_get_exit;
	}

	if (socket >= sockets_num) {
		ret = PQOS_RETVAL_PARAM;
		goto os_l3ca_get_exit;
	}

	for (class_id = 0; class_id < count; class_id++) {
		struct schemata schmt;

		ret = schemata_init(class_id, &schmt);
		if (ret == PQOS_RETVAL_OK)
			ret = schemata_read(class_id, &schmt);

		if (ret == PQOS_RETVAL_OK)
			ca[class_id] = schmt.l3ca[socket];

		schemata_fini(&schmt);

		if (ret != PQOS_RETVAL_OK)
			goto os_l3ca_get_exit;
	}
	*num_ca = count;

 os_l3ca_get_exit:
	if (sockets != NULL)
		free(sockets);

	return ret;
}

int
os_l2ca_set(const unsigned l2id,
            const unsigned num_cos,
            const struct pqos_l2ca *ca)
{
	int ret;
	unsigned i;
	unsigned l2ids_num = 0;
	unsigned *l2ids = NULL;
	unsigned l2ca_num;

	ASSERT(m_cap != NULL);
	ASSERT(ca != NULL);
	ASSERT(num_cos != 0);

	ret = pqos_l2ca_get_cos_num(m_cap, &l2ca_num);
	if (ret != PQOS_RETVAL_OK)
		return ret;

	if (num_cos > l2ca_num)
		return PQOS_RETVAL_PARAM;

	/*
	 * Check if class id's are within allowed range.
	 */
	for (i = 0; i < num_cos; i++) {
		if (ca[i].class_id >= l2ca_num) {
			LOG_ERROR("L2 COS%u is out of range (COS%u is max)!\n",
			          ca[i].class_id, l2ca_num - 1);
			return PQOS_RETVAL_PARAM;
		}
	}

	/* Get number of L2 ids in the system */
	l2ids = pqos_cpu_get_l2ids(m_cpu, &l2ids_num);
	if (l2ids == NULL || l2ids_num == 0) {
		ret = PQOS_RETVAL_ERROR;
		goto os_l2ca_set_exit;
	}

	if (l2id >= l2ids_num) {
		ret = PQOS_RETVAL_PARAM;
		goto os_l2ca_set_exit;
	}

	for (i = 0; i < num_cos; i++) {
		struct schemata schmt;

		ret = schemata_init(ca[i].class_id, &schmt);

		/* read schemata file */
		if (ret == PQOS_RETVAL_OK)
			ret = schemata_read(ca[i].class_id, &schmt);

		if (ret == PQOS_RETVAL_OK) {
			schmt.l2ca[l2id] = ca[i];
			ret = schemata_write(ca[i].class_id, &schmt);
		}

		schemata_fini(&schmt);

		if (ret != PQOS_RETVAL_OK)
			goto os_l2ca_set_exit;
	}

 os_l2ca_set_exit:
	if (l2ids != NULL)
		free(l2ids);

	return ret;
}

int
os_l2ca_get(const unsigned l2id,
            const unsigned max_num_ca,
            unsigned *num_ca,
            struct pqos_l2ca *ca)
{
	int ret;
	unsigned class_id;
	unsigned count = 0;
	unsigned l2ids_num = 0;
	unsigned *l2ids = NULL;

	ASSERT(num_ca != NULL);
	ASSERT(ca != NULL);
	ASSERT(max_num_ca != 0);
	ASSERT(m_cap != NULL);
	ASSERT(m_cpu != NULL);

	ret = pqos_l2ca_get_cos_num(m_cap, &count);
	if (ret != PQOS_RETVAL_OK)
		return PQOS_RETVAL_RESOURCE; /* L2 CAT not supported */

	if (count > max_num_ca)
		/* Not enough space to store the classes */
		return PQOS_RETVAL_PARAM;

	l2ids = pqos_cpu_get_l2ids(m_cpu, &l2ids_num);
	if (l2ids == NULL || l2ids_num == 0) {
		ret = PQOS_RETVAL_ERROR;
		goto os_l2ca_get_exit;
	}

	if (l2id >= l2ids_num) {
		ret = PQOS_RETVAL_PARAM;
		goto os_l2ca_get_exit;
	}

	for (class_id = 0; class_id < count; class_id++) {
		struct schemata schmt;

		ret = schemata_init(class_id, &schmt);
		if (ret == PQOS_RETVAL_OK)
			ret = schemata_read(class_id, &schmt);

		if (ret == PQOS_RETVAL_OK)
			ca[class_id] = schmt.l2ca[l2id];

		schemata_fini(&schmt);

		if (ret != PQOS_RETVAL_OK)
			goto os_l2ca_get_exit;
	}
	*num_ca = count;

 os_l2ca_get_exit:
	if (l2ids != NULL)
		free(l2ids);

	return ret;
}
