/*
 * BSD LICENSE
 *
 * Copyright(c) 2023-2026 Intel Corporation. All rights reserved.
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

#include "iordt.h"

#include "acpi.h"
#include "common.h"
#include "log.h"
#include "pci.h"
#include "utils.h"

#include <inttypes.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#define PQOS_IRDT_CHAN_ID(rmud_index, rcs_enum, chan_num)                      \
        (chan_num | (rcs_enum << 8) | ((rmud_index + 1) << 16))

#define PQOS_IRDT_MMIO_ID(rmud_index, rcs_enum)                                \
        ((rcs_enum << 8) | ((rmud_index + 1) << 16))

#define PQOS_IRDT_CHAN_MMIO(chan) (chan & ~0xFF)
#define PQOS_IRDT_CHAN(chan)      (chan & 0xFF)

#define MMIO_MAX_CHANNELS 8

/**
 * MMIO block information
 */
struct iordt_mmio {
        uint64_t id;          /**< MMIO id */
        uint64_t addr;        /**< MMIO physical address */
        unsigned numa;        /**< NUMA node ID in the system */
        uint16_t rmid_offset; /**< RMID block offset */
        uint16_t clos_offset; /**< CLOS block offset */
        uint64_t flags;       /**< RCS flags */
};

/**
 * MMIO information
 */
struct iordt_mmioinfo {
        unsigned num_mmio;       /**< number of MMIO blocks */
        struct iordt_mmio *mmio; /**< MMIO block information */
};

/**
 * I/O RDT topology information.
 * This pointer is allocated and initialized in this module.
 */
static struct pqos_devinfo *m_devinfo = NULL;

/**
 * MMIO topology information
 */
static struct iordt_mmioinfo *m_mmioinfo = NULL;

static int m_acpi_initialized;
static int m_pci_initialized;

/**
 * @brief Get MMIO information for the channel
 *
 * @param mmioinfo MMIO information
 * @param channel_id channel id
 *
 * @return MMIO information structure
 * @retval NULL on error
 */
static const struct iordt_mmio *
get_mmio(const struct iordt_mmioinfo *mmioinfo, pqos_channel_t channel_id)
{
        pqos_channel_t id = PQOS_IRDT_CHAN_MMIO(channel_id);
        unsigned i;

        if (mmioinfo == NULL)
                return NULL;

        for (i = 0; i < mmioinfo->num_mmio; ++i) {
                const struct iordt_mmio *mmio = &mmioinfo->mmio[i];

                if (mmio->id == id)
                        return mmio;
        }

        return NULL;
}

/**
 * @brief Check if I/O RDT is supported
 *
 * @param [in] platform QoS capabilities structure
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK if I/O RDT is supported
 */
int
iordt_check_support(const struct pqos_cap *cap)
{
        int ret;
        int supported;

        ret = pqos_l3ca_iordt_enabled(cap, &supported, NULL);
        if (ret == PQOS_RETVAL_OK && supported)
                return PQOS_RETVAL_OK;

        ret = pqos_mon_iordt_enabled(cap, &supported, NULL);
        if (ret == PQOS_RETVAL_OK && supported)
                return PQOS_RETVAL_OK;

        return PQOS_RETVAL_RESOURCE;
}

/**
 * @brief Parses IRDT DSS table to extract channels
 *
 * @param pqos_dev struct to be updated with channels' info
 * @param dev DSS table to be parsed for channels' info
 * @param rmud_idx RMUD index for DSS
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK success
 */
static int
iordt_dev_populate_chans(struct pqos_dev *pqos_dev,
                         struct acpi_table_irdt_device *dev,
                         size_t rmud_idx)
{
        if (!pqos_dev || !dev)
                return PQOS_RETVAL_PARAM;

        memset(pqos_dev->channel, 0, sizeof(pqos_dev->channel));

        size_t chms_num = 0;
        struct acpi_table_irdt_chms **chms = NULL;
        size_t chms_idx, chan_idx;
        int ret = acpi_get_irdt_chms(dev, &chms, &chms_num);

        if (ret != PQOS_RETVAL_OK) {
                LOG_ERROR("Failed to get DSS channel mappings: %d\n", ret);
                free(chms);
                return ret;
        }

        for (chms_idx = 0, chan_idx = 0; chms_idx < chms_num; chms_idx++) {
                size_t vc_num = DIM(chms[chms_idx]->vc_map);
                size_t vc_idx;

                for (vc_idx = 0; vc_idx < vc_num; vc_idx++) {
                        uint8_t vc = chms[chms_idx]->vc_map[vc_idx];

                        /* Check if this is a valid entry */
                        if (!(vc & ACPI_TABLE_IRDT_CHMS_CHAN_VALID))
                                continue;
                        if (chan_idx >= PQOS_DEV_MAX_CHANNELS) {
                                LOG_ERROR("Too many channels for I/O RDT "
                                          "device\n");
                                free(chms);
                                return PQOS_RETVAL_ERROR;
                        }
                        /* remove flags */
                        vc &= ~(ACPI_TABLE_IRDT_CHMS_CHAN_MASK);

                        pqos_dev->channel[chan_idx++] = PQOS_IRDT_CHAN_ID(
                            rmud_idx, chms[chms_idx]->rcs_enum_id, vc);
                }
        }

        if (chms)
                free(chms);

        return PQOS_RETVAL_OK;
}

/**
 * @brief Parses IRDT table to extract DSS info
 *
 * @param devinfo struct to be updated with DSS' info
 * @param rmud RMUD table to be parsed for DSS' info
 * @param rmud_idx RMUD index
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK success
 */
static int
iordt_populate_devs(struct pqos_devinfo *devinfo,
                    struct acpi_table_irdt_rmud *rmud,
                    size_t rmud_idx)
{
        size_t devs_num = 0;
        struct acpi_table_irdt_device **devs;
        size_t dev_idx;
        int ret;

        devs = acpi_get_irdt_dev(rmud, &devs_num);
        if (devs == NULL) {
                LOG_ERROR("Failed to get IRDT devices\n");
                return PQOS_RETVAL_ERROR;
        }

        for (dev_idx = 0; dev_idx < devs_num; dev_idx++) {
                struct pqos_dev *new_devs;
                struct pqos_dev *pqos_dev;

                if (devs[dev_idx]->type != ACPI_TABLE_IRDT_TYPE_DSS)
                        continue;
                if (devinfo->num_devs == UINT_MAX) {
                        LOG_ERROR("Too many I/O RDT devices\n");
                        free(devs);
                        return PQOS_RETVAL_ERROR;
                }

                new_devs = realloc(devinfo->devs, (devinfo->num_devs + 1) *
                                                      sizeof(*devinfo->devs));
                if (new_devs == NULL) {
                        LOG_ERROR("Can't allocate I/O RDT devices\n");
                        free(devs);
                        return PQOS_RETVAL_ERROR;
                }

                devinfo->devs = new_devs;
                pqos_dev = &devinfo->devs[devinfo->num_devs++];
                memset(pqos_dev, 0, sizeof(*pqos_dev));

                if (devs[dev_idx]->dss.device_type == 0x1)
                        pqos_dev->type = PQOS_DEVICE_TYPE_PCI;
                else if (devs[dev_idx]->dss.device_type == 0x2)
                        pqos_dev->type = PQOS_DEVICE_TYPE_PCI_BRIDGE;
                else {
                        LOG_ERROR("Unknown DSS device type 0x%x\n",
                                  devs[dev_idx]->dss.device_type);
                        free(devs);
                        return PQOS_RETVAL_ERROR;
                }
                pqos_dev->segment = rmud->segment;
                pqos_dev->bdf = devs[dev_idx]->dss.enumeration_id;

                ret =
                    iordt_dev_populate_chans(pqos_dev, devs[dev_idx], rmud_idx);
                if (ret != PQOS_RETVAL_OK) {
                        LOG_ERROR("Failed to populate DSS channels: %d\n", ret);
                        free(devs);
                        return ret;
                }
        }

        free(devs);
        return PQOS_RETVAL_OK;
}

/**
 * @brief Parses IRDT table to extract RCS
 *
 * @param devinfo struct to be updated with RCS' info
 * @param rmud RMUD table to be parsed for RCS' info
 * @param rmud_idx RMUD index
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK success
 */
static int
iordt_populate_chans(struct pqos_devinfo *devinfo,
                     struct acpi_table_irdt_rmud *rmud,
                     size_t rmud_idx)
{
        size_t devs_num = 0;
        struct acpi_table_irdt_device **devs;
        size_t dev_idx;
        size_t chan_idx;

        devs = acpi_get_irdt_dev(rmud, &devs_num);
        if (devs == NULL) {
                LOG_ERROR("Failed to get IRDT devices\n");
                return PQOS_RETVAL_ERROR;
        }

        for (dev_idx = 0; dev_idx < devs_num; dev_idx++) {
                const struct acpi_table_irdt_device *dev = devs[dev_idx];
                int rmid_tag;
                int clos_tag;
                int cxld;

                if (dev->type != ACPI_TABLE_IRDT_TYPE_RCS)
                        continue;

                rmid_tag = !!(dev->rcs.flags & RCS_FLAGS_RTS);
                clos_tag = !!(dev->rcs.flags & RCS_FLAGS_CTS);
                cxld = !!(dev->rcs.flags & RCS_FLAGS_CXLD);

                if (dev->rcs.channel_count > MMIO_MAX_CHANNELS) {
                        LOG_ERROR("Invalid I/O RDT channel count %u\n",
                                  (unsigned)dev->rcs.channel_count);
                        free(devs);
                        return PQOS_RETVAL_ERROR;
                }

                for (chan_idx = 0; chan_idx < dev->rcs.channel_count;
                     chan_idx++) {
                        const struct iordt_mmio *mmio;
                        struct pqos_channel *channels;
                        struct pqos_channel *channel;

                        if (devinfo->num_channels == UINT_MAX) {
                                LOG_ERROR("Too many I/O RDT channels\n");
                                free(devs);
                                return PQOS_RETVAL_ERROR;
                        }

                        channels = realloc(devinfo->channels,
                                           (devinfo->num_channels + 1) *
                                               sizeof(*devinfo->channels));
                        if (channels == NULL) {
                                LOG_ERROR("Can't allocate I/O RDT channels\n");
                                free(devs);
                                return PQOS_RETVAL_ERROR;
                        }

                        devinfo->channels = channels;
                        channel = &devinfo->channels[devinfo->num_channels++];
                        memset(channel, 0, sizeof(*channel));
                        channel->rmid_tagging = rmid_tag;
                        channel->clos_tagging = clos_tag;
                        channel->cxld = cxld;
                        channel->channel_id = PQOS_IRDT_CHAN_ID(
                            rmud_idx, dev->rcs.rcs_enumeration_id, chan_idx);

                        mmio = get_mmio(m_mmioinfo, channel->channel_id);
                        if (mmio == NULL) {
                                LOG_ERROR("No MMIO information for channel "
                                          "0x%" PRIx64 "\n",
                                          channel->channel_id);
                                free(devs);
                                return PQOS_RETVAL_ERROR;
                        }
                        channel->mmio_addr = mmio->addr;
                        channel->numa = mmio->numa;
                }
        }

        free(devs);
        return PQOS_RETVAL_OK;
}

/**
 * @brief Parses IRDT table to extract MMIO address
 *
 * @param mmioinfo MMIO information structure
 * @param rmud RMUD table to be parsed for RCS' info
 * @param rmud_idx RMUD index
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK success
 */
static int
iordt_populate_mmio(struct iordt_mmioinfo *mmioinfo,
                    struct acpi_table_irdt_rmud *rmud,
                    size_t rmud_idx)
{
        size_t devs_num = 0;
        struct acpi_table_irdt_device **devs;
        size_t dev_idx;
        int ret = PQOS_RETVAL_OK;

        devs = acpi_get_irdt_dev(rmud, &devs_num);
        if (devs == NULL) {
                LOG_ERROR("Failed to get IRDT devices\n");
                return PQOS_RETVAL_ERROR;
        }

        for (dev_idx = 0; dev_idx < devs_num; dev_idx++) {
                struct iordt_mmio *mmio;
                struct acpi_table_irdt_device *dev = devs[dev_idx];
                uint64_t addr;
                unsigned numa = PCI_NUMA_INVALID;

                /* skipping entries other than RCS */
                if (dev->type != ACPI_TABLE_IRDT_TYPE_RCS)
                        continue;

                /* mmio physical address */
                addr = dev->rcs.rcs_block_mmio_location;

                if (mmioinfo->num_mmio == UINT_MAX) {
                        LOG_ERROR("Too many IRDT MMIO blocks\n");
                        ret = PQOS_RETVAL_ERROR;
                        goto iordt_populate_mmio_exit;
                }
                mmio = realloc(mmioinfo->mmio, (mmioinfo->num_mmio + 1) *
                                                   sizeof(*mmioinfo->mmio));
                if (mmio == NULL) {
                        LOG_ERROR("Can't allocate IRDT MMIO blocks\n");
                        ret = PQOS_RETVAL_ERROR;
                        goto iordt_populate_mmio_exit;
                }
                mmio[mmioinfo->num_mmio].addr = addr;
                mmio[mmioinfo->num_mmio].id =
                    PQOS_IRDT_MMIO_ID(rmud_idx, dev->rcs.rcs_enumeration_id);
                mmio[mmioinfo->num_mmio].numa = numa;
                mmio[mmioinfo->num_mmio].rmid_offset =
                    dev->rcs.rmid_block_offset;
                mmio[mmioinfo->num_mmio].clos_offset =
                    dev->rcs.clos_block_offset;
                mmio[mmioinfo->num_mmio].flags = dev->rcs.flags;
                mmioinfo->mmio = mmio;
                mmioinfo->num_mmio++;
        }

        /* Find socket */
        for (dev_idx = 0; dev_idx < devs_num; dev_idx++) {
                struct acpi_table_irdt_device *dev = devs[dev_idx];
                struct acpi_table_irdt_chms **chms = NULL;
                size_t chms_idx;
                size_t chms_num;
                unsigned i;

                /* skipping entries other than DSS */
                if (dev->type != ACPI_TABLE_IRDT_TYPE_DSS)
                        continue;

                ret = acpi_get_irdt_chms(dev, &chms, &chms_num);
                if (ret != PQOS_RETVAL_OK) {
                        LOG_ERROR("Failed to get IRDT channel mappings: %d\n",
                                  ret);
                        free(chms);
                        goto iordt_populate_mmio_exit;
                }

                for (chms_idx = 0; chms_idx < chms_num; chms_idx++) {
                        const uint16_t domain = rmud->segment;
                        const uint64_t mmio_id = PQOS_IRDT_MMIO_ID(
                            rmud_idx, chms[chms_idx]->rcs_enum_id);
                        const uint16_t bdf = dev->dss.enumeration_id;
                        struct iordt_mmio *mmio = NULL;
                        struct pci_dev *pci;

                        for (i = 0; i < mmioinfo->num_mmio; ++i)
                                if (mmioinfo->mmio[i].id == mmio_id)
                                        mmio = &mmioinfo->mmio[i];

                        if (mmio == NULL || mmio->numa != PCI_NUMA_INVALID)
                                continue;

                        pci = pci_dev_get(domain, bdf);
                        if (pci == NULL)
                                continue;

                        mmio->numa = pci->numa;

                        pci_dev_release(pci);
                }

                if (chms)
                        free(chms);
        }

iordt_populate_mmio_exit:
        if (devs)
                free(devs);

        return ret;
}

/**
 * @brief Frees I/O RDT topology information
 */
static void
iordt_free(void)
{
        if (m_devinfo != NULL) {
                free(m_devinfo->channels);
                free(m_devinfo->devs);
                free(m_devinfo);
                m_devinfo = NULL;
        }

        if (m_mmioinfo != NULL) {
                free(m_mmioinfo->mmio);
                free(m_mmioinfo);
                m_mmioinfo = NULL;
        }
}

int
iordt_init(const struct pqos_cap *cap, struct pqos_devinfo **devinfo)
{
        struct acpi_table_irdt_rmud **rmuds = NULL;
        struct acpi_table *table = NULL;
        size_t rmuds_num = 0;
        size_t rmud_idx;
        int ret;

        if (cap == NULL || devinfo == NULL)
                return PQOS_RETVAL_PARAM;
        *devinfo = NULL;

        ret = iordt_check_support(cap);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        ret = acpi_init();
        if (ret != PQOS_RETVAL_OK) {
                LOG_WARN("Could not initialize ACPI: %d\n", ret);
                return ret;
        }
        m_acpi_initialized = 1;

        ret = pci_init();
        if (ret != PQOS_RETVAL_OK) {
                LOG_WARN("Could not initialize PCI: %d\n", ret);
                goto error;
        }
        m_pci_initialized = 1;

        table = acpi_get_sig(ACPI_TABLE_SIG_IRDT);
        if (table == NULL) {
                LOG_WARN("Could not obtain %s table\n", ACPI_TABLE_SIG_IRDT);
                ret = PQOS_RETVAL_RESOURCE;
                goto error;
        }

        acpi_print(table);
        m_devinfo = calloc(1, sizeof(*m_devinfo));
        m_mmioinfo = calloc(1, sizeof(*m_mmioinfo));
        if (m_devinfo == NULL || m_mmioinfo == NULL) {
                LOG_ERROR("Can't allocate I/O RDT topology information\n");
                ret = PQOS_RETVAL_ERROR;
                goto error;
        }

        rmuds = acpi_get_irdt_rmud(table->irdt, &rmuds_num);
        if (rmuds == NULL) {
                LOG_ERROR("Could not get IRDT RMUD structures\n");
                ret = PQOS_RETVAL_ERROR;
                goto error;
        }

        for (rmud_idx = 0; rmud_idx < rmuds_num; rmud_idx++) {
                ret =
                    iordt_populate_mmio(m_mmioinfo, rmuds[rmud_idx], rmud_idx);
                if (ret != PQOS_RETVAL_OK)
                        goto error;

                ret = iordt_populate_devs(m_devinfo, rmuds[rmud_idx], rmud_idx);
                if (ret != PQOS_RETVAL_OK)
                        goto error;

                ret =
                    iordt_populate_chans(m_devinfo, rmuds[rmud_idx], rmud_idx);
                if (ret != PQOS_RETVAL_OK)
                        goto error;
        }

        LOG_DEBUG("Parsed %u I/O devices and %u channels\n",
                  m_devinfo->num_devs, m_devinfo->num_channels);
        free(rmuds);
        acpi_free(table);
        *devinfo = m_devinfo;

        return PQOS_RETVAL_OK;

error:
        free(rmuds);
        if (table != NULL)
                acpi_free(table);
        (void)iordt_fini();
        return ret;
}

int
iordt_fini(void)
{
        int ret = PQOS_RETVAL_OK;
        int retval;

        if (m_pci_initialized) {
                retval = pci_fini();
                if (retval != PQOS_RETVAL_OK) {
                        LOG_WARN("Could not finalize PCI: %d\n", retval);
                        ret = retval;
                } else
                        m_pci_initialized = 0;
        }

        if (m_acpi_initialized) {
                retval = acpi_fini();
                if (retval != PQOS_RETVAL_OK) {
                        LOG_WARN("Could not finalize I/O RDT ACPI: %d\n",
                                 retval);
                        if (ret == PQOS_RETVAL_OK)
                                ret = retval;
                } else
                        m_acpi_initialized = 0;
        }

        iordt_free();
        return ret;
}

int
iordt_get_numa(const struct pqos_devinfo *devinfo,
               pqos_channel_t channel_id,
               unsigned *numa)
{
        const struct iordt_mmio *mmio;
        unsigned i;
        int ret = PQOS_RETVAL_RESOURCE;

        if (devinfo == NULL || numa == NULL)
                return PQOS_RETVAL_PARAM;

        mmio = get_mmio(m_mmioinfo, channel_id);
        if (mmio == NULL)
                return PQOS_RETVAL_RESOURCE;

        if (mmio->numa != PCI_NUMA_INVALID) {
                *numa = mmio->numa;
                return PQOS_RETVAL_OK;
        }

        for (i = 0; i < devinfo->num_devs; ++i) {
                unsigned ch;
                const struct pqos_dev *dev = &devinfo->devs[i];

                for (ch = 0; ch < PQOS_DEV_MAX_CHANNELS; ++ch) {
                        struct pci_dev *pci;

                        if (dev->channel[ch] != channel_id)
                                continue;

                        pci = pci_dev_get(dev->segment, dev->bdf);
                        if (pci == NULL) {
                                LOG_DEBUG("No PCI information for segment "
                                          "0x%x BDF 0x%x\n",
                                          dev->segment, dev->bdf);
                                ret = PQOS_RETVAL_ERROR;
                                continue;
                        }

                        if (pci->numa != PCI_NUMA_INVALID) {
                                *numa = pci->numa;
                                pci_dev_release(pci);
                                return PQOS_RETVAL_OK;
                        }
                        pci_dev_release(pci);
                }
        }

        return ret;
}

#define MMIO_REGW(mmio) ((mmio->flags & RCS_FLAGS_REGW) ? 2 : 4)

typedef uint16_t *uint16_p;
typedef uint32_t *uint32_p;

#define IORDT_WRITE(LENGTH)                                                    \
        static int iordt_write_uint##LENGTH(                                   \
            uint##LENGTH##_p mem, const unsigned index, const int enable,      \
            uint##LENGTH##_t value)                                            \
        {                                                                      \
                uint##LENGTH##_t mask = (uint##LENGTH##_t)(-1);                \
                                                                               \
                if (enable)                                                    \
                        mask = mask >> 1;                                      \
                                                                               \
                if ((value & mask) != value)                                   \
                        return PQOS_RETVAL_PARAM;                              \
                                                                               \
                mem[index] = value | (enable ? 1LU << (LENGTH - 1) : 0);       \
                return PQOS_RETVAL_OK;                                         \
        }

#define IORDT_READ(LENGTH)                                                     \
        static int iordt_read_uint##LENGTH(uint##LENGTH##_p mem,               \
                                           const unsigned index,               \
                                           const int enable, unsigned *value)  \
        {                                                                      \
                uint##LENGTH##_t val = mem[index];                             \
                uint##LENGTH##_t mask = (uint##LENGTH##_t)(-1);                \
                                                                               \
                if (enable)                                                    \
                        mask = mask >> 1;                                      \
                                                                               \
                /* enable bit not set */                                       \
                if (enable && (val & (1LU << (LENGTH - 1))) == 0)              \
                        return PQOS_RETVAL_RESOURCE;                           \
                                                                               \
                *value = val & mask;                                           \
                                                                               \
                return PQOS_RETVAL_OK;                                         \
        }

IORDT_WRITE(16)
IORDT_WRITE(32)
IORDT_READ(16)
IORDT_READ(32)

int
iordt_mon_assoc_write(pqos_channel_t channel, pqos_rmid_t rmid)
{
        int ret;
        const struct iordt_mmio *mmio = get_mmio(m_mmioinfo, channel);
        uint8_t *mem;
        uint64_t addr;
        uint32_t size;
        unsigned index;
        int ref;

        if (mmio == NULL)
                return PQOS_RETVAL_PARAM;
        if (PQOS_IRDT_CHAN(channel) >= MMIO_MAX_CHANNELS)
                return PQOS_RETVAL_PARAM;

        addr = mmio->addr + mmio->rmid_offset;
        size = MMIO_REGW(mmio) * MMIO_MAX_CHANNELS;
        index = PQOS_IRDT_CHAN(channel);
        ref = (rmid != 0) && ((mmio->flags & RCS_FLAGS_REF) != 0);

        mem = pqos_mmap_write(addr, size);
        if (mem == NULL)
                return PQOS_RETVAL_ERROR;

        if (mmio->flags & RCS_FLAGS_REGW)
                ret = iordt_write_uint16((uint16_t *)(void *)mem, index, ref,
                                         rmid);
        else
                ret = iordt_write_uint32((uint32_t *)(void *)mem, index, ref,
                                         rmid);

        pqos_munmap(mem, size);

        return ret;
}

int
iordt_mon_assoc_read(pqos_channel_t channel, pqos_rmid_t *rmid)
{
        int ret;
        const struct iordt_mmio *mmio = get_mmio(m_mmioinfo, channel);
        uint8_t *mem;
        uint64_t addr;
        uint32_t size;
        unsigned index;
        int ref;

        if (mmio == NULL || rmid == NULL)
                return PQOS_RETVAL_PARAM;
        if (PQOS_IRDT_CHAN(channel) >= MMIO_MAX_CHANNELS)
                return PQOS_RETVAL_PARAM;

        addr = mmio->addr + mmio->rmid_offset;
        size = MMIO_REGW(mmio) * MMIO_MAX_CHANNELS;
        index = PQOS_IRDT_CHAN(channel);
        ref = (mmio->flags & RCS_FLAGS_REF) != 0;

        mem = pqos_mmap_read(addr, size);
        if (mem == NULL)
                return PQOS_RETVAL_ERROR;

        if (mmio->flags & RCS_FLAGS_REGW)
                ret = iordt_read_uint16((uint16_t *)(void *)mem, index, ref,
                                        rmid);
        else
                ret = iordt_read_uint32((uint32_t *)(void *)mem, index, ref,
                                        rmid);

        pqos_munmap(mem, size);

        return ret;
}

int
iordt_mon_assoc_reset(const struct pqos_devinfo *dev)
{
        int ret = PQOS_RETVAL_OK;
        unsigned i;

        ASSERT(dev != NULL);

        for (i = 0; i < dev->num_channels; i++) {
                const struct pqos_channel *channel = &dev->channels[i];
                int retval;

                if (!channel->rmid_tagging)
                        continue;

                retval = iordt_mon_assoc_write(channel->channel_id, 0);
                if (retval != PQOS_RETVAL_OK) {
                        LOG_ERROR("Failed to reset RMID association for "
                                  "channel 0x%" PRIx64 ": %d\n",
                                  channel->channel_id, retval);
                        ret = retval;
                }
        }

        return ret;
}

/**
 * @brief Writes CLOS association
 *
 * @param[in] channel channel to be associated with CLOS
 * @param[in] class_id CLOS to associate channel with
 * @param[in] enable set enable bit
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK success
 */
static int
_assoc_write(pqos_channel_t channel, unsigned class_id, unsigned enable)
{
        int ret;
        const struct iordt_mmio *mmio = get_mmio(m_mmioinfo, channel);
        uint8_t *mem;
        uint64_t addr;
        uint32_t size;
        unsigned index;
        int cef;

        if (mmio == NULL)
                return PQOS_RETVAL_PARAM;
        if (PQOS_IRDT_CHAN(channel) >= MMIO_MAX_CHANNELS)
                return PQOS_RETVAL_PARAM;

        cef = enable && ((mmio->flags & RCS_FLAGS_CEF) != 0);

        /* the class is checked at its full width here, against the
         * register's width less its enable bit: the writer takes the
         * register's type, so a class wider than the register would lose
         * its upper bits at the call, before the writer's own check
         */
        if (class_id >= (1ULL << (MMIO_REGW(mmio) * 8 - cef)))
                return PQOS_RETVAL_PARAM;

        addr = mmio->addr + mmio->clos_offset;
        size = MMIO_REGW(mmio) * MMIO_MAX_CHANNELS;
        index = PQOS_IRDT_CHAN(channel);

        mem = pqos_mmap_write(addr, size);
        if (mem == NULL)
                return PQOS_RETVAL_ERROR;

        if (mmio->flags & RCS_FLAGS_REGW)
                ret = iordt_write_uint16((uint16_t *)(void *)mem, index, cef,
                                         class_id);
        else
                ret = iordt_write_uint32((uint32_t *)(void *)mem, index, cef,
                                         class_id);

        pqos_munmap(mem, size);

        return ret;
}

int
iordt_assoc_write(pqos_channel_t channel, unsigned class_id)
{
        return _assoc_write(channel, class_id, 1);
}

int
iordt_assoc_read(pqos_channel_t channel, unsigned *class_id)
{
        int ret;
        const struct iordt_mmio *mmio = get_mmio(m_mmioinfo, channel);
        uint8_t *mem;
        uint64_t addr;
        uint32_t size;
        unsigned index;
        int cef;

        if (mmio == NULL || class_id == NULL)
                return PQOS_RETVAL_PARAM;
        if (PQOS_IRDT_CHAN(channel) >= MMIO_MAX_CHANNELS)
                return PQOS_RETVAL_PARAM;

        addr = mmio->addr + mmio->clos_offset;
        size = MMIO_REGW(mmio) * MMIO_MAX_CHANNELS;
        index = PQOS_IRDT_CHAN(channel);
        cef = ((mmio->flags & RCS_FLAGS_CEF) != 0);

        mem = pqos_mmap_read(addr, size);
        if (mem == NULL)
                return PQOS_RETVAL_ERROR;

        if (mmio->flags & RCS_FLAGS_REGW)
                ret = iordt_read_uint16((uint16_t *)(void *)mem, index, cef,
                                        class_id);
        else
                ret = iordt_read_uint32((uint32_t *)(void *)mem, index, cef,
                                        class_id);

        pqos_munmap(mem, size);

        return ret;
}

int
iordt_assoc_reset(const struct pqos_devinfo *dev)
{
        int ret = PQOS_RETVAL_OK;
        unsigned i;

        ASSERT(dev != NULL);

        for (i = 0; i < dev->num_channels; ++i) {
                const struct pqos_channel *channel = &dev->channels[i];
                int retval;

                if (!channel->clos_tagging)
                        continue;

                retval = _assoc_write(channel->channel_id, 0, 0);
                if (retval != PQOS_RETVAL_OK) {
                        LOG_ERROR("Failed to reset CLOS association for "
                                  "channel 0x%" PRIx64 ": %d\n",
                                  channel->channel_id, retval);
                        ret = retval;
                }
        }

        return ret;
}
