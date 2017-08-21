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
#include <limits.h>                     /**< CHAR_BIT*/
#include <errno.h>

#include <pqos.h>
#include "os_allocation.h"
#include "cap.h"
#include "log.h"
#include "types.h"
#include "resctrl_alloc.h"

/*
 * Path to resctrl file system
 */
static const char *rctl_cpus = "cpus";
static const char *rctl_schemata = "schemata";
static const char *rctl_tasks = "tasks";

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

        ASSERT(name != NULL);
        ASSERT(mode != NULL);

	memset(buf, 0, sizeof(buf));
	if (class_id == 0)
		result = snprintf(buf, sizeof(buf) - 1,
		                  "%s/%s", RESCTRL_ALLOC_PATH, name);
	else
		result = snprintf(buf, sizeof(buf) - 1, "%s/COS%u/%s",
		                  RESCTRL_ALLOC_PATH, class_id, name);

	if (result < 0)
		return NULL;

	fd = fopen(buf, mode);
	if (fd == NULL)
		LOG_ERROR("Could not open %s file %s for COS %u\n",
		          name, buf, class_id);

	return fd;
}

/**
 * @brief Closes COS file in resctl filesystem
 *
 * @param[in] fd File descriptor
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK on success
 */
static int
rctl_fclose(FILE *fd)
{
        if (fd == NULL)
                return PQOS_RETVAL_PARAM;

        if (fclose(fd) == 0)
                return PQOS_RETVAL_OK;

        switch (errno) {
        case EBADF:
                LOG_ERROR("Invalid file descriptor!\n");
                break;
        case EINVAL:
                LOG_ERROR("Invalid file arguments!\n");
                break;
        default:
                LOG_ERROR("Error closing file!\n");
        }

        return PQOS_RETVAL_ERROR;
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

/**
 * @brief Structure to hold parsed cpu mask
 *
 * Structure contains table with cpu bit mask. Each table item holds
 * information about 8 bit in mask.
 *
 * Example bitmask tables:
 *  - cpus file contains 'ABC' mask = [ ..., 0x0A, 0xBC ]
 *  - cpus file contains 'ABCD' mask = [ ..., 0xAB, 0xCD ]
 */
struct cpumask {
	uint8_t tab[MAX_CPUS / CHAR_BIT];  /**< bit mask table */
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
	const unsigned item = (sizeof(mask->tab) - 1) - (lcore / CHAR_BIT);
	const unsigned bit = lcore % CHAR_BIT;

	/* Set lcore bit in mask table item */
	mask->tab[item] = mask->tab[item] | (1 << bit);
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
	const unsigned item = (sizeof(mask->tab) - 1) - (lcore / CHAR_BIT);
	const unsigned bit = lcore % CHAR_BIT;

	/* Check if lcore bit is set in mask table item */
	return (mask->tab[item] >> bit) & 0x1;
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

	for (i = 0; i < sizeof(mask->tab); i++) {
		const unsigned value = (unsigned) mask->tab[i];

		if (fprintf(fd, "%02x", value) < 0) {
			LOG_ERROR("Failed to write cpu mask\n");
                        break;
		}
                if ((i + 1) % 4 == 0)
                        if (fprintf(fd, ",") < 0) {
                                LOG_ERROR("Failed to write cpu mask\n");
                                break;
                        }
	}
	ret = rctl_fclose(fd);

        /* check if error occured in loop */
        if (i < sizeof(mask->tab))
                return PQOS_RETVAL_ERROR;

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
        int i, hex_offset, idx;
	FILE *fd;
        size_t num_chars = 0;
        char cpus[MAX_CPUS / CHAR_BIT];

        memset(mask, 0, sizeof(struct cpumask));
        memset(cpus, 0, sizeof(cpus));
	fd = rctl_fopen(class_id, rctl_cpus, "r");
	if (fd == NULL)
                return PQOS_RETVAL_ERROR;

        /** Read the entire file into memory. */
        num_chars = fread(cpus, sizeof(char), sizeof(cpus), fd);

        if (ferror(fd) != 0) {
                LOG_ERROR("Error reading CPU file\n");
                rctl_fclose(fd);
                return PQOS_RETVAL_ERROR;
        }
        cpus[sizeof(cpus) - 1] = '\0'; /** Just to be safe. */
        if (rctl_fclose(fd) != PQOS_RETVAL_OK)
                return PQOS_RETVAL_ERROR;

        /**
         *  Convert the cpus array into hex, skip any non hex chars.
         *  Store the hex values in the mask tab.
         */
        for (i = num_chars - 1, hex_offset = 0, idx = sizeof(mask->tab) - 1;
             i >= 0; i--) {
                const char c = cpus[i];
                int hex_num;

                if ('0' <= c && c <= '9')
                        hex_num = c - '0';
                else if ('a' <= c && c <= 'f')
                        hex_num = 10 + c - 'a';
                else if ('A' <= c && c <= 'F')
                        hex_num = 10 + c - 'A';
                else
                        continue;

                if (!hex_offset)
                        mask->tab[idx] = (uint8_t) hex_num;
                else {
                        mask->tab[idx] |= (uint8_t) (hex_num << 4);
                        idx--;
                }
                hex_offset ^= 1;
        }

        return PQOS_RETVAL_OK;
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
	unsigned mba_num;       /**< Number of MBA COS held in struct */
	struct pqos_mba *mba;   /**< MBA COS definitions */
};

/*
 * @brief Deallocate memory of schemata struct
 *
 * @param[in] schemata Schemata structure
 */
static void
schemata_fini(struct schemata *schemata)
{
	if (schemata->l2ca != NULL) {
		free(schemata->l2ca);
		schemata->l2ca = NULL;
	}
	if (schemata->l3ca != NULL) {
		free(schemata->l3ca);
		schemata->l3ca = NULL;
	}
	if (schemata->mba != NULL) {
		free(schemata->mba);
		schemata->mba = NULL;
	}
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
schemata_init(const unsigned class_id, struct schemata *schemata)
{
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

	/* MBA */
	retval = pqos_mba_get_cos_num(m_cap, &num_cos);
	if (retval == PQOS_RETVAL_OK && class_id < num_cos) {
		unsigned *sockets = NULL;

		sockets = pqos_cpu_get_sockets(m_cpu, &num_ids);
		if (sockets == NULL) {
			ret = PQOS_RETVAL_ERROR;
			goto schemata_init_exit;
		}

		free(sockets);

		schemata->mba_num = num_ids;
		schemata->mba = calloc(num_ids, sizeof(struct pqos_mba));
		if (schemata->mba == NULL) {
			ret = PQOS_RETVAL_ERROR;
			goto schemata_init_exit;
		}

		/* fill class_id */
		for (i = 0; i < num_ids; i++) {
			schemata->mba[i].class_id = class_id;
			schemata->mba[i].mb_rate = 100;
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
	SCHEMATA_TYPE_MB,     /**< MBA data */
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
	else if (strcasecmp(str, "MB") == 0)
		type = SCHEMATA_TYPE_MB;

	return type;
}

/**
 * @brief Fill schemata structure
 *
 * @param [in] res_id Resource id
 * @param [in] value Ways mask/Memory B/W rate
 * @param [in] type Schemata type
 * @param [out] schemata Schemata structure
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK on success
 */
static int
schemata_set(const unsigned res_id,
             const uint64_t value,
             const int type,
             struct schemata *schemata)
{
	if (type == SCHEMATA_TYPE_L2) {
		if (schemata->l2ca_num <= res_id)
			return PQOS_RETVAL_ERROR;
		schemata->l2ca[res_id].ways_mask = value;

	} else if (type == SCHEMATA_TYPE_L3) {
		if (schemata->l3ca_num <= res_id || schemata->l3ca[res_id].cdp)
			return PQOS_RETVAL_ERROR;
		schemata->l3ca[res_id].u.ways_mask = value;

	} else if (type == SCHEMATA_TYPE_L3CODE) {
		if (schemata->l3ca_num <= res_id || !schemata->l3ca[res_id].cdp)
			return PQOS_RETVAL_ERROR;
		schemata->l3ca[res_id].u.s.code_mask = value;

	} else if (type == SCHEMATA_TYPE_L3DATA) {
		if (schemata->l3ca_num <= res_id || !schemata->l3ca[res_id].cdp)
			return PQOS_RETVAL_ERROR;
		schemata->l3ca[res_id].u.s.data_mask = value;

	} else if (type == SCHEMATA_TYPE_MB) {
		if (schemata->mba_num <= res_id)
			return PQOS_RETVAL_ERROR;
		schemata->mba[res_id].mb_rate = value;
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
		q = buf;
		/**
		 * Trim white spaces
		 */
		while (isspace(*q))
			q++;

		/**
		 * Determine allocation type
		 */
		p = strchr(q, ':');
		if (p == NULL) {
			ret = PQOS_RETVAL_ERROR;
			break;
		}
		*p = '\0';
		type = schemata_type_get(q);

		/* Skip unknown label */
		if (type == SCHEMATA_TYPE_NONE)
			continue;

		/**
		 * Parse COS masks
		 */
		for (++p; ; p = NULL) {
			char *token = NULL;
			uint64_t id = 0;
			uint64_t value = 0;
			unsigned base = (type == SCHEMATA_TYPE_MB ? 10 : 16);

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

			ret = strtouint64(q + 1, base, &value);
			if (ret != PQOS_RETVAL_OK)
				goto schemata_read_exit;

			ret = schemata_set(id, value, type, schemata);
			if (ret != PQOS_RETVAL_OK)
				goto schemata_read_exit;
		}
	}

 schemata_read_exit:
        /* check if error occured */
        if (ret != PQOS_RETVAL_OK)
                rctl_fclose(fd);
        else
                ret = rctl_fclose(fd);

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
		rctl_fclose(fd);
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

	/* MBA */
	if (schemata->mba_num > 0) {
		fprintf(fd, "MB:");
		for (i = 0; i < schemata->mba_num; i++) {
			if (i > 0)
				fprintf(fd, ";");
			fprintf(fd, "%u=%u", i, schemata->mba[i].mb_rate);
		}
		fprintf(fd, "\n");
	}

	ret = rctl_fclose(fd);

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
		/* get MBA COS num */
		if (p_cap->type == PQOS_CAP_TYPE_MBA) {
			ret = pqos_mba_get_cos_num(cap, &num_cos);
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
        if (mount("resctrl", RESCTRL_ALLOC_PATH, "resctrl", 0, cdp_option) != 0)
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
        if (access(RESCTRL_ALLOC_PATH"/cpus", F_OK) != 0) {
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
static int
os_alloc_prep(void)
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
                             "%s/COS%d", RESCTRL_ALLOC_PATH, (int) i) < 0)
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

	ASSERT(class_id != NULL);
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

unsigned *
os_pid_get_pid_assoc(const unsigned class_id, unsigned *count)
{
        FILE *fd;
        unsigned *tasks = NULL, idx = 0, grps;
        int ret;
        char buf[128];
        struct linked_list {
                uint64_t task_id;
                struct linked_list *next;
        } head, *current = NULL;

	ASSERT(m_cap != NULL);
        ASSERT(count != NULL);
        ret = os_get_max_rctl_grps(m_cap, &grps);
	if (ret != PQOS_RETVAL_OK)
		return NULL;

	if (class_id >= grps)
		/* class_id is out of bounds */
		return NULL;

        /* Open resctrl tasks file */
        fd = rctl_fopen(class_id, rctl_tasks, "r");
        if (fd == NULL)
                return NULL;

        head.next = NULL;
        current = &head;
        memset(buf, 0, sizeof(buf));
        while (fgets(buf, sizeof(buf), fd) != NULL) {
                uint64_t tmp;
                struct linked_list *p = NULL;

                ret = strtouint64(buf, 10, &tmp);
                if (ret != PQOS_RETVAL_OK)
                        goto exit_clean;
                p = malloc(sizeof(head));
                if (p == NULL)
                        goto exit_clean;
                p->task_id = tmp;
                p->next = NULL;
                current->next = p;
                current = p;
                idx++;
        }

        /* if no pids found then allocate empty buffer to be returned */
        if (idx == 0)
                tasks = (unsigned *) calloc(1, sizeof(tasks[0]));
        else
                tasks = (unsigned *) malloc(idx * sizeof(tasks[0]));
        if (tasks == NULL)
                goto exit_clean;

        *count = idx;
        current = head.next;
        idx = 0;
        while (current != NULL) {
                tasks[idx++] = current->task_id;
                current = current->next;
        }

 exit_clean:
        rctl_fclose(fd);
        current = head.next;
        while (current != NULL) {
                struct linked_list *tmp = current->next;

                free(current);
                current = tmp;
        }
        return tasks;
}

/**
 * @brief Function to search a COS tasks file and check if this file is blank
 *
 * @param [in] class_id COS containing task ID
 * @param [out] found flag
 *                    0 if no Task ID is found
 *                    1 if a Task ID is found
 *
 * @return Operation status
 */
static int
task_file_check(const unsigned class_id, unsigned *found)
{
        FILE *fd;
        char buf[128];

        /* Open resctrl tasks file */
        fd = rctl_fopen(class_id, rctl_tasks, "r");
        if (fd == NULL)
                return PQOS_RETVAL_ERROR;

        /* Search tasks file for any task ID */
        memset(buf, 0, sizeof(buf));
        if (fgets(buf, sizeof(buf), fd) != NULL)
                *found = 1;

        if (rctl_fclose(fd) != PQOS_RETVAL_OK)
                return PQOS_RETVAL_ERROR;

        return PQOS_RETVAL_OK;
}

/**
 * @brief Gets unused COS
 *
 * The lowest acceptable COS is 1, as 0 is a default one
 *
 * @param [in] hi_class_id highest acceptable COS id
 * @param [out] class_id unused COS
 *
 * @return Operation status
 */
static int
get_unused_cos(const unsigned hi_class_id,
               unsigned *class_id)
{
        unsigned used_classes[hi_class_id + 1];
        unsigned i, cos;
        int ret;

        if (class_id == NULL)
                return PQOS_RETVAL_PARAM;

        memset(used_classes, 0, sizeof(used_classes));

        for (i = hi_class_id; i != 0; i--) {
                struct cpumask mask;
                unsigned j;

                ret = cpumask_read(i, &mask);
                if (ret != PQOS_RETVAL_OK)
			return ret;

                for (j = 0; j < sizeof(mask.tab); j++)
                        if (mask.tab[j] > 0) {
                                used_classes[i] = 1;
                                break;
                        }

                if (used_classes[i] == 1)
                        continue;

                ret = task_file_check(i, &used_classes[i]);
                if (ret != PQOS_RETVAL_OK)
			return ret;
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
        unsigned i, num_rctl_grps = 0;
        int ret;

        ASSERT(core_num > 0);
        ASSERT(core_array != NULL);
        ASSERT(class_id != NULL);
        ASSERT(m_cap != NULL);
        UNUSED_PARAM(technology);

        /* obtain highest class id for all requested technologies */
        ret = os_get_max_rctl_grps(m_cap, &num_rctl_grps);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        if (num_rctl_grps == 0)
                return PQOS_RETVAL_ERROR;

        /* find an unused class from highest down */
        ret = get_unused_cos(num_rctl_grps - 1, class_id);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        /* assign cores to the unused class */
        for (i = 0; i < core_num; i++) {
                ret = os_alloc_assoc_set(core_array[i], *class_id);
                if (ret != PQOS_RETVAL_OK)
                        return ret;
        }

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
        ret = umount2(RESCTRL_ALLOC_PATH, 0);
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
	unsigned num_grps = 0, l3ca_num;
	int cdp_enabled = 0;

	ASSERT(ca != NULL);
	ASSERT(num_cos != 0);
	ASSERT(m_cap != NULL);
	ASSERT(m_cpu != NULL);

	ret = pqos_l3ca_get_cos_num(m_cap, &l3ca_num);
	if (ret != PQOS_RETVAL_OK)
		return PQOS_RETVAL_RESOURCE; /* L3 CAT not supported */

	ret = os_get_max_rctl_grps(m_cap, &num_grps);
	if (ret != PQOS_RETVAL_OK)
		return ret;

	if (num_cos > num_grps)
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
			struct pqos_l3ca *l3ca = &(schmt.l3ca[socket]);

			if (cdp_enabled == 1 && ca[i].cdp == 0) {
				l3ca->cdp = 1;
				l3ca->u.s.data_mask = ca[i].u.ways_mask;
				l3ca->u.s.code_mask = ca[i].u.ways_mask;
			} else
				*l3ca = ca[i];

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
		return PQOS_RETVAL_RESOURCE; /* L3 CAT not supported */

	ret = os_get_max_rctl_grps(m_cap, &count);
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
os_l3ca_get_min_cbm_bits(unsigned *min_cbm_bits)
{
	int ret = PQOS_RETVAL_OK;
	char buf[128];
	const struct pqos_capability *l3_cap = NULL;
	FILE *fd;

	ASSERT(m_cap != NULL);
	ASSERT(min_cbm_bits != NULL);

	/**
	 * Get L3 CAT capabilities
	 */
	ret = pqos_cap_get_type(m_cap, PQOS_CAP_TYPE_L3CA, &l3_cap);
	if (ret != PQOS_RETVAL_OK)
		return PQOS_RETVAL_RESOURCE; /* L3 CAT not supported */

	memset(buf, 0, sizeof(buf));
	snprintf(buf, sizeof(buf) - 1, "%s/info/L3/min_cbm_bits",
	         RESCTRL_ALLOC_PATH);

	fd = fopen(buf, "r");
	if (fd == NULL)
		return PQOS_RETVAL_ERROR;

	if (fscanf(fd, "%u", min_cbm_bits) != 1)
		ret = PQOS_RETVAL_ERROR;

	fclose(fd);

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
	unsigned num_grps = 0, l2ca_num;

	ASSERT(m_cap != NULL);
	ASSERT(ca != NULL);
	ASSERT(num_cos != 0);

	ret = pqos_l2ca_get_cos_num(m_cap, &l2ca_num);
	if (ret != PQOS_RETVAL_OK)
		return PQOS_RETVAL_RESOURCE; /* L2 CAT not supported */

	ret = os_get_max_rctl_grps(m_cap, &num_grps);
	if (ret != PQOS_RETVAL_OK)
		return ret;

	if (num_cos > num_grps)
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

	ret = os_get_max_rctl_grps(m_cap, &count);
	if (ret != PQOS_RETVAL_OK)
		return ret;

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

int
os_mba_set(const unsigned socket,
           const unsigned num_cos,
           const struct pqos_mba *requested,
           struct pqos_mba *actual)
{
	int ret;
	unsigned sockets_num = 0;
	unsigned *sockets = NULL;
	unsigned i, step = 0;
	unsigned num_grps = 0;
	const struct pqos_capability *mba_cap = NULL;

	ASSERT(requested != NULL);
	ASSERT(num_cos != 0);
	ASSERT(m_cap != NULL);
	ASSERT(m_cpu != NULL);

	/**
	 * Check if MBA is supported
	 */
	ret = pqos_cap_get_type(m_cap, PQOS_CAP_TYPE_MBA, &mba_cap);
	if (ret != PQOS_RETVAL_OK)
		return PQOS_RETVAL_RESOURCE; /* MBA not supported */

	ret = os_get_max_rctl_grps(m_cap, &num_grps);
	if (ret != PQOS_RETVAL_OK)
		return ret;

	if (num_cos > num_grps)
		return PQOS_RETVAL_PARAM;

        step = mba_cap->u.mba->throttle_step;

        /**
	 * Check if class id's are within allowed range.
	 */
	for (i = 0; i < num_cos; i++)
		if (requested[i].class_id >= num_grps) {
			LOG_ERROR("MBA COS%u is out of range (COS%u is max)!\n",
			          requested[i].class_id, num_grps - 1);
			return PQOS_RETVAL_PARAM;
		}

	/* Get number of sockets in the system */
	sockets = pqos_cpu_get_sockets(m_cpu, &sockets_num);
	if (sockets == NULL || sockets_num == 0 || socket >= sockets_num) {
		ret = PQOS_RETVAL_ERROR;
		goto os_l3ca_set_exit;
	}

	for (i = 0; i < num_cos; i++) {
		struct schemata schmt;

		ret = schemata_init(requested[i].class_id, &schmt);

		/* read schemata file */
		if (ret == PQOS_RETVAL_OK)
			ret = schemata_read(requested[i].class_id, &schmt);

		/* update and write schemata */
		if (ret == PQOS_RETVAL_OK) {
			struct pqos_mba *mba = &(schmt.mba[socket]);

			*mba = requested[i];
                        mba->mb_rate = (((requested[i].mb_rate
                                          + (step/2)) / step) * step);
			if (mba->mb_rate == 0)
				mba->mb_rate = step;

			ret = schemata_write(requested[i].class_id, &schmt);
		}

		if (actual != NULL) {
			/* read actual schemata */
			if (ret == PQOS_RETVAL_OK)
				ret = schemata_read(requested[i].class_id,
					            &schmt);

			/* update actual schemata */
			if (ret == PQOS_RETVAL_OK)
				actual[i] = schmt.mba[socket];
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
os_mba_get(const unsigned socket,
           const unsigned max_num_cos,
           unsigned *num_cos,
           struct pqos_mba *mba_tab)
{
	int ret;
	unsigned class_id;
	unsigned count = 0;
	unsigned sockets_num = 0;
	unsigned *sockets = NULL;
	const struct pqos_capability *mba_cap = NULL;

	ASSERT(num_cos != NULL);
	ASSERT(max_num_cos != 0);
	ASSERT(mba_tab != NULL);
	ASSERT(m_cap != NULL);
	ASSERT(m_cpu != NULL);

	/**
	 * Check if MBA is supported
	 */
	ret = pqos_cap_get_type(m_cap, PQOS_CAP_TYPE_MBA, &mba_cap);
	if (ret != PQOS_RETVAL_OK)
		return PQOS_RETVAL_RESOURCE; /* MBA not supported */

	ret = os_get_max_rctl_grps(m_cap, &count);
	if (ret != PQOS_RETVAL_OK)
		return ret;

	if (count > max_num_cos)
		return PQOS_RETVAL_ERROR;

	sockets = pqos_cpu_get_sockets(m_cpu, &sockets_num);
	if (sockets == NULL || sockets_num == 0 || socket >= sockets_num) {
		ret = PQOS_RETVAL_ERROR;
		goto os_mba_get_exit;
	}

	for (class_id = 0; class_id < count; class_id++) {
		struct schemata schmt;

		ret = schemata_init(class_id, &schmt);
		if (ret == PQOS_RETVAL_OK)
			ret = schemata_read(class_id, &schmt);

		if (ret == PQOS_RETVAL_OK)
			mba_tab[class_id] = schmt.mba[socket];

		schemata_fini(&schmt);

		if (ret != PQOS_RETVAL_OK)
			goto os_mba_get_exit;
	}
	*num_cos = count;

 os_mba_get_exit:
	if (sockets != NULL)
		free(sockets);

	return ret;
}

/**
 * ---------------------------------------
 * Task utility functions
 * ---------------------------------------
 */

/**
 * @brief Function to validate if \a task is a valid task ID
 *
 * @param task task ID to validate
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK on success
 */
static int
task_validate(const pid_t task)
{
        char buf[128];

        memset(buf, 0, sizeof(buf));
        snprintf(buf, sizeof(buf)-1, "/proc/%d", (int)task);
        if (access(buf, F_OK) != 0) {
                LOG_ERROR("Task %d does not exist!\n", (int)task);
                return PQOS_RETVAL_ERROR;
        }

        return PQOS_RETVAL_OK;
}

/**
 * @brief Function to write task ID to resctrl COS tasks file
 *        Used to associate a task with COS
 *
 * @param class_id COS tasks file to write to
 * @param task task ID to write to tasks file
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK on success
 */
static int
task_write(const unsigned class_id, const pid_t task)
{
        FILE *fd;
        int ret;

        /* Check if task exists */
        ret = task_validate(task);
        if (ret != PQOS_RETVAL_OK)
                return PQOS_RETVAL_PARAM;

        /* Open resctrl tasks file */
        fd = rctl_fopen(class_id, rctl_tasks, "w");
	if (fd == NULL)
		return PQOS_RETVAL_ERROR;

        /* Write task ID to file */
        if (fprintf(fd, "%d\n", task) < 0) {
                LOG_ERROR("Failed to write to task %d to file!\n", (int) task);
                rctl_fclose(fd);
                return PQOS_RETVAL_ERROR;
        }
        ret = rctl_fclose(fd);

        return ret;
}

/**
 * @brief Function to search a COS tasks file for a task ID
 *
 * @param [out] class_id COS containing task ID
 * @param [in] task task ID to search for
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK on success
 */
static int
task_search(unsigned *class_id, const pid_t task)
{
        FILE *fd;
        unsigned i, max_cos = 0;
        int ret;

        /* Check if task exists */
        ret = task_validate(task);
        if (ret != PQOS_RETVAL_OK)
                return PQOS_RETVAL_PARAM;

        /* Get number of COS */
        ret = os_get_max_rctl_grps(m_cap, &max_cos);
	if (ret != PQOS_RETVAL_OK)
		return ret;

        /**
         * Starting at highest COS - search all COS tasks files for task ID
         */
        for (i = (max_cos - 1); (int)i >= 0; i--) {
                uint64_t tid = 0;
                char buf[128];

                /* Open resctrl tasks file */
                fd = rctl_fopen(i, rctl_tasks, "r");
                if (fd == NULL)
                        return PQOS_RETVAL_ERROR;

                /* Search tasks file for specified task ID */
                memset(buf, 0, sizeof(buf));
                while (fgets(buf, sizeof(buf), fd) != NULL) {
                        ret = strtouint64(buf, 10, &tid);
                        if (ret != PQOS_RETVAL_OK)
                                continue;

                        if (task == (pid_t)tid) {
                                *class_id = i;
                                if (rctl_fclose(fd) != PQOS_RETVAL_OK)
                                        return PQOS_RETVAL_ERROR;

                                return PQOS_RETVAL_OK;
                        }
                }
                if (rctl_fclose(fd) != PQOS_RETVAL_OK)
                        return PQOS_RETVAL_ERROR;
        }
        /* If not found in any COS group - return error */
        LOG_ERROR("Failed to get association for task %d!\n", (int)task);
        return PQOS_RETVAL_ERROR;
}

int
os_alloc_assoc_set_pid(const pid_t task,
                       const unsigned class_id)
{
        int ret;
	unsigned max_cos = 0;

        ASSERT(m_cap != NULL);

	/* Get number of COS */
        ret = os_get_max_rctl_grps(m_cap, &max_cos);
	if (ret != PQOS_RETVAL_OK)
		return ret;

        if (class_id >= max_cos) {
                LOG_ERROR("COS out of bounds for task %d\n", (int)task);
                return PQOS_RETVAL_PARAM;
        }

        /* Write to tasks file */
	return task_write(class_id, task);
}

int
os_alloc_assoc_get_pid(const pid_t task,
                       unsigned *class_id)
{
        ASSERT(class_id != NULL);

        /* Search tasks files */
        return task_search(class_id, task);
}

int
os_alloc_assign_pid(const unsigned technology,
                    const pid_t *task_array,
                    const unsigned task_num,
                    unsigned *class_id)
{
        unsigned i, num_rctl_grps = 0;
        int ret;

        ASSERT(task_num > 0);
        ASSERT(task_array != NULL);
        ASSERT(class_id != NULL);
        ASSERT(m_cap != NULL);
        UNUSED_PARAM(technology);

        /* obtain highest class id for all requested technologies */
        ret = os_get_max_rctl_grps(m_cap, &num_rctl_grps);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        if (num_rctl_grps == 0)
                return PQOS_RETVAL_ERROR;

        /* find an unused class from highest down */
        ret = get_unused_cos(num_rctl_grps - 1, class_id);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        /* assign tasks to the unused class */
        for (i = 0; i < task_num; i++) {
                ret = task_write(*class_id, task_array[i]);
                if (ret != PQOS_RETVAL_OK)
                        return ret;
        }

        return ret;
}

int
os_alloc_release_pid(const pid_t *task_array,
                     const unsigned task_num)
{
        unsigned i;

        ASSERT(task_array != NULL);
        ASSERT(task_num != 0);

        /**
         * Write all tasks to default COS#0 tasks file
         * - return on error
         * - otherwise try next task in array
         */
        for (i = 0; i < task_num; i++)
                if (task_write(0, task_array[i]) == PQOS_RETVAL_ERROR)
                        return PQOS_RETVAL_ERROR;

        return PQOS_RETVAL_OK;
}
