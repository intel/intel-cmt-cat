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
        unsigned numa;        /**< numa id in the system */
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
 * IO RDT topology information.
 * This pointer is allocated and initialized in this module.
 */
static struct pqos_devinfo *m_devinfo = NULL;

/**
 * MMIO topology information
 */
static struct iordt_mmioinfo *m_mmioinfo = NULL;

/**
 * @brief Check if I/O RDT is supported
 *
 * @param [in] platform QoS capabilities structure
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK is I/O RDT is supported
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

        if (ret != PQOS_RETVAL_OK)
                return ret;

        for (chms_idx = 0, chan_idx = 0; chms_idx < chms_num; chms_idx++) {
                size_t vc_num = DIM(chms[chms_idx]->vc_map);
                size_t vc_idx;

                for (vc_idx = 0;
                     vc_idx < vc_num && chan_idx < PQOS_DEV_MAX_CHANNELS;
                     vc_idx++) {
                        uint8_t vc = chms[chms_idx]->vc_map[vc_idx];

                        /* Check if this is a valid entry */
                        if (!(vc & ACPI_TABLE_IRDT_CHMS_CHAN_VALID))
                                continue;
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
        if (!devs)
                return PQOS_RETVAL_ERROR;

        /* pqos_devinfo devs */
        for (dev_idx = 0; dev_idx < devs_num; dev_idx++) {

                /* skipping entries other than DSS */
                if (devs[dev_idx]->type != ACPI_TABLE_IRDT_TYPE_DSS)
                        continue;

                devinfo->num_devs++;
                devinfo->devs = realloc(devinfo->devs, sizeof(*devinfo->devs) *
                                                           devinfo->num_devs);
                if (devinfo->devs == NULL) {
                        free(devs);
                        return PQOS_RETVAL_ERROR;
                }

                struct pqos_dev *pqos_dev =
                    &devinfo->devs[devinfo->num_devs - 1];

                if (devs[dev_idx]->dss.device_type == 0x1)
                        pqos_dev->type = PQOS_DEVICE_TYPE_PCI;
                else if (devs[dev_idx]->dss.device_type == 0x2)
                        pqos_dev->type = PQOS_DEVICE_TYPE_PCI_BRIDGE;
                else {
                        LOG_ERROR("Unknown DSS device type!\n");
                        free(devs);
                        return PQOS_RETVAL_ERROR;
                }
                pqos_dev->segment = rmud->segment;
                pqos_dev->bdf = devs[dev_idx]->dss.enumeration_id;

                ret =
                    iordt_dev_populate_chans(pqos_dev, devs[dev_idx], rmud_idx);
                if (ret != PQOS_RETVAL_OK) {
                        LOG_ERROR("Failed to populate DSS channels!\n");
                        free(devs);
                        return PQOS_RETVAL_ERROR;
                }
        }

        if (devs)
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
        size_t dev_idx, chan_idx;

        devs = acpi_get_irdt_dev(rmud, &devs_num);
        if (!devs)
                return PQOS_RETVAL_ERROR;

        for (dev_idx = 0; dev_idx < devs_num; dev_idx++) {

                /* skipping entries other than RCS */
                if (devs[dev_idx]->type != ACPI_TABLE_IRDT_TYPE_RCS)
                        continue;

                int rmid_tag =
                    (devs[dev_idx]->rcs.flags & RCS_FLAGS_RTS) ? 1 : 0;
                int clos_tag =
                    (devs[dev_idx]->rcs.flags & RCS_FLAGS_CTS) ? 1 : 0;

                for (chan_idx = 0; chan_idx < devs[dev_idx]->rcs.channel_count;
                     chan_idx++) {

                        devinfo->num_channels++;
                        devinfo->channels = realloc(
                            devinfo->channels, sizeof(struct pqos_channel) *
                                                   devinfo->num_channels);
                        if (devinfo->channels == NULL) {
                                free(devs);
                                return PQOS_RETVAL_ERROR;
                        }

                        struct pqos_channel *pqos_chan =
                            &devinfo->channels[devinfo->num_channels - 1];

                        pqos_chan->rmid_tagging = rmid_tag;
                        pqos_chan->clos_tagging = clos_tag;
                        pqos_chan->channel_id = PQOS_IRDT_CHAN_ID(
                            rmud_idx, devs[dev_idx]->rcs.rcs_enumeration_id,
                            chan_idx);
                }
        }

        if (devs)
                free(devs);

        return PQOS_RETVAL_OK;
}

/**
 * @brief Parses IRDT table to extract MMIO address
 *
 * @param mmio MMIO information structure
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
        if (!devs)
                return PQOS_RETVAL_ERROR;

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

                mmio = realloc(mmioinfo->mmio, (mmioinfo->num_mmio + 1) *
                                                   sizeof(struct iordt_mmio));
                if (mmio == NULL) {
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
                if (ret != PQOS_RETVAL_OK)
                        goto iordt_populate_mmio_exit;

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

int
iordt_init(const struct pqos_cap *cap, struct pqos_devinfo **devinfo)
{
        int ret;

        if (cap == NULL || devinfo == NULL)
                return PQOS_RETVAL_PARAM;

        ret = iordt_check_support(cap);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        ret = acpi_init();
        if (ret != PQOS_RETVAL_OK) {
                LOG_WARN("Could not initialize ACPI!\n");
                return ret;
        }

        ret = pci_init();
        if (ret != PQOS_RETVAL_OK) {
                LOG_WARN("Could not initialize PCI!\n");
                return ret;
        }

        struct acpi_table *table = acpi_get_sig(ACPI_TABLE_SIG_IRDT);

        if (table == NULL) {
                LOG_WARN("Could not obtain %s table\n", ACPI_TABLE_SIG_IRDT);
                return PQOS_RETVAL_RESOURCE;
        }

        acpi_print(table);

        m_devinfo = (struct pqos_devinfo *)calloc(sizeof(*m_devinfo), 1);
        if (m_devinfo == NULL) {
                acpi_free(table);
                return PQOS_RETVAL_ERROR;
        }

        m_mmioinfo = (struct iordt_mmioinfo *)calloc(sizeof(*m_mmioinfo), 1);
        if (m_mmioinfo == NULL) {
                acpi_free(table);
                free(m_devinfo);
                return PQOS_RETVAL_ERROR;
        }

        size_t rmud_idx;
        size_t rmuds_num = 0;
        struct acpi_table_irdt_rmud **rmuds = NULL;

        rmuds = acpi_get_irdt_rmud(table->irdt, &rmuds_num);
        if (!rmuds) {
                acpi_free(table);
                LOG_WARN("Could not get RMUDs!\n");
                return PQOS_RETVAL_ERROR;
        }

        for (rmud_idx = 0; rmud_idx < rmuds_num; rmud_idx++) {
                ret = iordt_populate_devs(m_devinfo, rmuds[rmud_idx], rmud_idx);
                if (ret != PQOS_RETVAL_OK)
                        break;

                ret =
                    iordt_populate_chans(m_devinfo, rmuds[rmud_idx], rmud_idx);
                if (ret != PQOS_RETVAL_OK)
                        break;

                ret =
                    iordt_populate_mmio(m_mmioinfo, rmuds[rmud_idx], rmud_idx);
                if (ret != PQOS_RETVAL_OK)
                        break;
        }

        if (rmuds)
                free(rmuds);

        acpi_free(table);

        *devinfo = m_devinfo;

        return ret;
}

int
iordt_fini(void)
{
        int ret;

        ret = pci_fini();
        if (ret != PQOS_RETVAL_OK) {
                LOG_WARN("Could not finalize PCI!\n");
                return ret;
        }

        ret = acpi_fini();
        if (ret != PQOS_RETVAL_OK) {
                LOG_WARN("Could not finalize IO RDT!\n");
                return ret;
        }

        if (m_devinfo != NULL) {
                if (m_devinfo->channels)
                        free(m_devinfo->channels);
                if (m_devinfo->devs)
                        free(m_devinfo->devs);
                free(m_devinfo);
                m_devinfo = NULL;
        }

        if (m_mmioinfo != NULL) {
                free(m_mmioinfo->mmio);
                free(m_mmioinfo);
                m_mmioinfo = NULL;
        }

        return PQOS_RETVAL_OK;
}

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

int
iordt_get_numa(const struct pqos_devinfo *devinfo,
               pqos_channel_t channel_id,
               unsigned *numa)
{
        const struct iordt_mmio *mmio = get_mmio(m_mmioinfo, channel_id);
        unsigned i;
        int ret = PQOS_RETVAL_RESOURCE;

        if (mmio == NULL)
                return PQOS_RETVAL_PARAM;

        /* read socket information basing on bdf in RCS */
        if (mmio->numa != PCI_NUMA_INVALID) {
                *numa = mmio->numa;
                return PQOS_RETVAL_OK;
        }

        /* Check socket info in DSS */
        for (i = 0; i < devinfo->num_devs; ++i) {
                unsigned ch;
                const struct pqos_dev *dev = &devinfo->devs[i];

                for (ch = 0; ch < PQOS_DEV_MAX_CHANNELS; ++ch) {
                        struct pci_dev *pci;

                        if (dev->channel[ch] != channel_id)
                                continue;

                        pci = pci_dev_get(dev->segment, dev->bdf);
                        if (pci == NULL) {
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
                if (retval != PQOS_RETVAL_OK)
                        ret = retval;
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

        addr = mmio->addr + mmio->clos_offset;
        size = MMIO_REGW(mmio) * MMIO_MAX_CHANNELS;
        index = PQOS_IRDT_CHAN(channel);
        cef = enable && ((mmio->flags & RCS_FLAGS_CEF) != 0);

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
                if (retval != PQOS_RETVAL_OK)
                        ret = PQOS_RETVAL_ERROR;
        }

        return ret;
}
