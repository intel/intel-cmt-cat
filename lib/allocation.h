/*
 * BSD LICENSE
 *
 * Copyright(c) 2014-2026 Intel Corporation. All rights reserved.
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

/**
 * @brief Internal header file to PQoS allocation initialization
 */

#ifndef __PQOS_ALLOC_H__
#define __PQOS_ALLOC_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "pqos.h"
#include "types.h"

#define IS_CONTIGNOUS(ca)                                                      \
        ({                                                                     \
                int ret;                                                       \
                                                                               \
                if (ca.cdp)                                                    \
                        ret = alloc_is_bitmask_contiguous(ca.u.s.data_mask) && \
                              alloc_is_bitmask_contiguous(ca.u.s.code_mask);   \
                else                                                           \
                        ret = alloc_is_bitmask_contiguous(ca.u.ways_mask);     \
                                                                               \
                ret;                                                           \
        })

/**
 * Types of possible PQoS allocation technologies
 */
enum pqos_technology {
        PQOS_TECHNOLOGY_L3CA = 1 << PQOS_CAP_TYPE_L3CA,
        PQOS_TECHNOLOGY_L2CA = 1 << PQOS_CAP_TYPE_L2CA,
        PQOS_TECHNOLOGY_MBA = 1 << PQOS_CAP_TYPE_MBA,
        PQOS_TECHNOLOGY_SMBA = 1 << PQOS_CAP_TYPE_SMBA,
        PQOS_TECHNOLOGY_ALL = -1
};

/**
 * @brief Initializes allocation sub-module of PQoS library
 *
 * @param cpu CPU topology structure
 * @param cap capabilities structure
 * @param cfg library configuration structure
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK success
 */
int pqos_alloc_init(const struct pqos_cpuinfo *cpu,
                    const struct pqos_cap *cap,
                    const struct pqos_config *cfg);

/**
 * @brief Shuts down allocation sub-module of PQoS library
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK success
 */
int pqos_alloc_fini(void);

/**
 * @brief Writes class of service associated to \a lcore
 *
 * @param [in] lcore CPU logical core id
 * @param [in] class_id class of service
 *
 * @return Operation status
 */
PQOS_LOCAL int hw_alloc_assoc_write(const unsigned lcore,
                                    const unsigned class_id);

/**
 * @brief Reads class of service associated to \a lcore
 *
 * @param [in] lcore CPU logical core id
 * @param [out] class_id class of service
 *
 * @return Operation status
 */
PQOS_LOCAL int hw_alloc_assoc_read(const unsigned lcore, unsigned *class_id);

/**
 * @brief Gets unused COS on a socket or L2 cluster
 *
 * The lowest acceptable COS is 1, as 0 is a default one
 *
 * @param [in] technology selection of allocation technologies
 * @param [in] l3cat_id L3 CAT resource id
 * @param [in] l2cat_id L2 CAT resource id
 * @param [in] mba_id MBA resource id
 * @param [in] smba_id SMBA resource id
 * @param [out] class_id unused COS
 *
 * NOTE: It is our assumption that mba id and cat ids are same for
 * a core. In future, if a core can have different mba id and cat ids
 * then, we need to change this function to handle it.
 *
 * @return Operation status
 */
PQOS_LOCAL int hw_alloc_assoc_unused(const unsigned technology,
                                     unsigned l3cat_id,
                                     unsigned l2cat_id,
                                     unsigned mba_id,
                                     unsigned smba_id,
                                     unsigned *class_id);

/**
 * @brief Hardware interface to associate \a lcore
 *        with given class of service
 *
 * @param [in] lcore CPU logical core id
 * @param [in] class_id class of service
 *
 * @return Operations status
 */
PQOS_LOCAL int hw_alloc_assoc_set(const unsigned lcore,
                                  const unsigned class_id);

/**
 * @brief Hardware interface to read association
 *        of \a lcore with class of service
 *
 * @param [in] lcore CPU logical core id
 * @param [out] class_id class of service
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 */
PQOS_LOCAL int hw_alloc_assoc_get(const unsigned lcore, unsigned *class_id);

/**
 * @brief Hardware interface to assign first available
 *        COS to cores in \a core_array
 *
 * While searching for available COS take technologies it is intended to use
 * with into account.
 * Note on \a technology and \a core_array selection:
 * - if L2 CAT technology is requested then cores need to belong to
 *   one L2 cluster (same L2ID)
 * - if only L3 CAT is requested then cores need to belong to one socket
 * - if only MBA is selected then cores need to belong to one socket
 *
 * @param [in] technology bit mask selecting technologies
 *             (1 << enum pqos_cap_type)
 * @param [in] core_array list of core ids
 * @param [in] core_num number of core ids in the \a core_array
 * @param [out] class_id place to store reserved COS id
 *
 * @return Operations status
 */
PQOS_LOCAL int hw_alloc_assign(const unsigned technology,
                               const unsigned *core_array,
                               const unsigned core_num,
                               unsigned *class_id);

/**
 * @brief Hardware interface to reassign cores
 *        in \a core_array to default COS#0
 *
 * @param [in] core_array list of core ids
 * @param [in] core_num number of core ids in the \a core_array
 *
 * @return Operations status
 */
PQOS_LOCAL int hw_alloc_release(const unsigned *core_array,
                                const unsigned core_num);

/**
 * @brief Hardware interface to reset configuration
 *        of allocation technologies
 *
 * Reverts CAT/MBA state to the one after reset:
 * - all cores associated with COS0
 * - all COS are set to give access to entire resource
 * - all device channels associated with COS0
 *
 * As part of allocation reset CDP, MBA, I/O RDT reconfiguration
 * can be performed. This can be requested via \a cfg.
 *
 * @param [in] cfg requested configuration
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 */
PQOS_LOCAL int hw_alloc_reset(const struct pqos_alloc_config *cfg);

/**
 * @brief Enables or disables L3 CDP across selected CPU sockets
 *
 * @param [in] l3cat_id_num dimension of \a sockets array
 * @param [in] l3cat_ids array with socket ids to change CDP config on
 * @param [in] enable CDP enable/disable flag, 1 - enable, 0 - disable
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 * @retval PQOS_RETVAL_ERROR on failure, MSR read/write error
 */
PQOS_LOCAL int hw_alloc_reset_l3cdp(const unsigned l3cat_id_num,
                                    const unsigned *l3cat_ids,
                                    const int enable);

/**
 * @brief Enables or disables L3 I/O RDT across selected CPU sockets
 *
 * @param [in] l3cat_id_num dimension of \a l3cat_ids array
 * @param [in] l3cat_ids array with L3 ids to change I/O RDT config on
 * @param [in] enable I/O RDT enable/disable flag, 1 - enable, 0 - disable
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 * @retval PQOS_RETVAL_ERROR on failure, MSR read/write error
 */
PQOS_LOCAL int hw_alloc_reset_l3iordt(const unsigned l3cat_id_num,
                                      const unsigned *l3cat_ids,
                                      const int enable);

/**
 * @brief Enables or disables L2 CDP across selected CPU clusters
 *
 * @param [in] l2id_num dimension of \a l2ids array
 * @param [in] l2ids array with clusters ids to change CDP config on
 * @param [in] enable CDP enable/disable flag, 1 - enable, 0 - disable
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 * @retval PQOS_RETVAL_ERROR on failure, MSR read/write error
 */
PQOS_LOCAL int hw_alloc_reset_l2cdp(const unsigned l2id_num,
                                    const unsigned *l2ids,
                                    const int enable);

/**
 * @brief Associates each of the cores and channels with COS0
 *
 * Operates on m_cpu structure.
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 * @retval PQOS_RETVAL_ERROR on error
 */
PQOS_LOCAL int hw_alloc_reset_assoc(void);

/**
 * @brief Associates each of the cores with COS0
 *
 * Operates on m_cpu structure.
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 * @retval PQOS_RETVAL_ERROR on MSR write error
 */
PQOS_LOCAL int hw_alloc_reset_assoc_cores(void);

/**
 * @brief Associates each of the channels with COS0
 *
 * Operates on m_cpu structure.
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 * @retval PQOS_RETVAL_ERROR on error
 */
PQOS_LOCAL int hw_alloc_reset_assoc_channels(void);

/**
 * @brief Writes range of MBA/CAT COS MSR's with \a msr_val value
 *
 * Used as part of CAT/MBA reset process.
 *
 * @param [in] msr_start First MSR to be written
 * @param [in] msr_num Number of MSR's to be written
 * @param [in] coreid Core ID to be used for MSR write operations
 * @param [in] msr_val Value to be written to MSR's
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 * @retval PQOS_RETVAL_ERROR on MSR write error
 */
PQOS_LOCAL int hw_alloc_reset_cos(const unsigned msr_start,
                                  const unsigned msr_num,
                                  const unsigned coreid,
                                  const uint64_t msr_val);

/**
 * @brief Writes range of MBA/CAT COS MSR's with \a msr_val value
 *
 * Used as part of CAT/MBA reset process.
 *
 * @param [in] mba_ids_num Number of items in \a mba_ids array
 * @param [in] mba_ids Array with MBA IDs
 * @param [in] enable MBA 4.0 enable flag, 1 - enable, 0 - disable
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 * @retval PQOS_RETVAL_ERROR on failure, MSR read/write error
 */
PQOS_LOCAL int hw_alloc_reset_mba40(const unsigned mba_ids_num,
                                    const unsigned *mba_ids,
                                    const int enable);

/**
 * @brief Hardware interface to set classes of service
 *        defined by \a ca on \a l3cat_id
 *
 * @param [in] l3cat_id L3 CAT resource id
 * @param [in] num_ca number of classes of service at \a ca
 * @param [in] ca table with class of service definitions
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 */
PQOS_LOCAL int hw_l3ca_set(const unsigned l3cat_id,
                           const unsigned num_ca,
                           const struct pqos_l3ca *ca);

/**
 * @brief Hardware interface to read classes of service from \a l3cat id
 *
 * @param [in] l3cat_id L3 CAT resource id
 * @param [in] max_num_ca maximum number of classes of service
 *             that can be accommodated at \a ca
 * @param [out] num_ca number of classes of service read into \a ca
 * @param [out] ca table with read classes of service
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 */
PQOS_LOCAL int hw_l3ca_get(const unsigned l3cat_id,
                           const unsigned max_num_ca,
                           unsigned *num_ca,
                           struct pqos_l3ca *ca);

/**
 * @brief Probe hardware for minimum number of bits that must be set
 *
 * @note Uses free COS to determine lowest number of bits accepted
 * @note If no free COS is available PQOS_RETVAL_RESOURCE will be returned
 *
 * @param [out] min_cbm_bits minimum number of bits that must be set
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK on success
 * @retval PQOS_RETVAL_RESOURCE when no free COS found
 */
PQOS_LOCAL int hw_l3ca_get_min_cbm_bits(unsigned *min_cbm_bits);

/**
 * @brief Hardware interface to set classes of
 *        service defined by \a ca on \a l2id
 *
 * @param [in] l2id unique L2 cache identifier
 * @param [in] num_cos number of classes of service at \a ca
 * @param [in] ca table with class of service definitions
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 */
PQOS_LOCAL int hw_l2ca_set(const unsigned l2id,
                           const unsigned num_cos,
                           const struct pqos_l2ca *ca);

/**
 * @brief Hardware interface to read classes of service from \a l2id
 *
 * @param [in] l2id unique L2 cache identifier
 * @param [in] max_num_ca maximum number of classes of service
 *             that can be accommodated at \a ca
 * @param [out] num_ca number of classes of service read into \a ca
 * @param [out] ca table with read classes of service
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 */
PQOS_LOCAL int hw_l2ca_get(const unsigned l2id,
                           const unsigned max_num_ca,
                           unsigned *num_ca,
                           struct pqos_l2ca *ca);

/**
 * @brief Probe hardware for minimum number of bits that must be set
 *
 * @note Uses free COS to determine lowest number of bits accepted
 * @note If no free COS is available PQOS_RETVAL_RESOURCE will be returned
 *
 * @param [out] min_cbm_bits minimum number of bits that must be set
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK on success
 * @retval PQOS_RETVAL_RESOURCE when no free COS found
 */
PQOS_LOCAL int hw_l2ca_get_min_cbm_bits(unsigned *min_cbm_bits);

/**
 * @brief Hardware interface to set classes of service
 *        defined by \a MBA  on \a mba id
 *
 * @param [in]  mba_id MBA resource id
 * @param [in]  num_cos number of classes of service at \a ca
 * @param [in]  requested table with class of service definitions
 * @param [out] actual table with class of service definitions
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 */
PQOS_LOCAL int hw_mba_set(const unsigned mba_id,
                          const unsigned num_cos,
                          const struct pqos_mba *requested,
                          struct pqos_mba *actual);

/**
 * @brief Hardware interface to set classes of service
 *        defined by \a requested on \a mba_id
 * @note: This function is specific to AMD
 *
 * @param [in]  mba_id
 * @param [in]  num_cos number of classes of service at \a ca
 * @param [in]  requested table with class of service definitions
 * @param [out] actual table with class of service definitions
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 */
PQOS_LOCAL int hw_mba_set_amd(const unsigned mba_id,
                              const unsigned num_cos,
                              const struct pqos_mba *requested,
                              struct pqos_mba *actual);

/**
 * @brief Hardware interface to read MBA from \a mba_id
 *
 * @param [in]  mba_id MBA resource id
 * @param [in]  max_num_cos maximum number of classes of service
 *              that can be accommodated at \a mba_tab
 * @param [out] num_cos number of classes of service read into \a mba_tab
 * @param [out] mba_tab table with read classes of service
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 */
PQOS_LOCAL int hw_mba_get(const unsigned mba_id,
                          const unsigned max_num_cos,
                          unsigned *num_cos,
                          struct pqos_mba *mba_tab);

/**
 * @brief Hardware interface to read MBA from \a mba_id
 * @note: This function is specific to AMD
 *
 * @param [in]  mba_id MBA resource id
 * @param [in]  max_num_cos maximum number of classes of service
 *              that can be accommodated at \a mba_tab
 * @param [out] num_cos number of classes of service read into \a mba_tab
 * @param [out] mba_tab table with read classes of service
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 */
PQOS_LOCAL int hw_mba_get_amd(const unsigned mba_id,
                              const unsigned max_num_cos,
                              unsigned *num_cos,
                              struct pqos_mba *mba_tab);

/**
 * @brief Tests if \a bitmask is contiguous
 *
 * Zero bit mask is regarded as not contiguous.
 *
 * The function shifts out first group of contiguous 1's in the bit mask.
 * Next it checks remaining bitmask content to make a decision.
 *
 * @param bitmask bit mask to be validated for contiguity
 *
 * @return Bit mask contiguity check result
 * @retval 0 not contiguous
 * @retval 1 contiguous
 */
PQOS_LOCAL int alloc_is_bitmask_contiguous(uint64_t bitmask);

/*
 * @brief Hardware interface to read association of \a channel with
 *        class of service
 *
 * @param [in] channel Control channel
 * @param [out] class_id class of service
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 */
PQOS_LOCAL int hw_alloc_assoc_get_channel(const pqos_channel_t channel,
                                          unsigned *class_id);

/**
 * @brief Hardware interface to read association of device channel with
 *        class of service
 *
 * @param [in] bdf Device id
 * @param [in] vc Device virtual channel
 * @param [out] class_id class of service
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 */
PQOS_LOCAL int hw_alloc_assoc_get_dev(const uint16_t segment,
                                      const uint16_t bdf,
                                      const unsigned vc,
                                      unsigned *class_id);

/**
 * @brief Hardware interface to associate \a channel with given class of service
 *
 * @param [in] channel Control channel id
 * @param [in] class_id class of service
 *
 * @return Operations status
 */
PQOS_LOCAL int hw_alloc_assoc_set_channel(const pqos_channel_t channel,
                                          const unsigned class_id);

/**
 * @brief Hardware interface to associate device with given class of service
 *
 * @param [in] bdf Device id
 * @param [in] vc Device virtual channel
 * @param [in] class_id class of service
 *
 * @return Operations status
 */
PQOS_LOCAL int hw_alloc_assoc_set_dev(const uint16_t segment,
                                      const uint16_t bdf,
                                      const unsigned vc,
                                      const unsigned class_id);

/**
 * @brief Hardware interface to read SMBA from \a smba_id
 * @NOTE: This function is specific to AMD
 *
 * @param [in]  smba_id SMBA resource id
 * @param [in]  max_num_cos maximum number of classes of service
 *              that can be accommodated at \a smba_tab
 * @param [out] num_cos number of classes of service read into \a smba_tab
 * @param [out] smba_tab table with read classes of service
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 */
PQOS_LOCAL int hw_smba_get_amd(const unsigned smba_id,
                               const unsigned max_num_cos,
                               unsigned *num_cos,
                               struct pqos_mba *smba_tab);
/**
 * @brief Hardware interface to set classes of service
 *        defined by \a requested on \a smba_id
 * @NOTE: This function is specific to AMD
 *
 * @param [in]  smba_id
 * @param [in]  num_cos number of classes of service at \a ca
 * @param [in]  requested table with class of service definitions
 * @param [out] actual table with class of service definitions
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 */
PQOS_LOCAL int hw_smba_set_amd(const unsigned smba_id,
                               const unsigned num_cos,
                               const struct pqos_mba *requested,
                               struct pqos_mba *actual);

#ifdef __cplusplus
}
#endif

#endif /* __PQOS_ALLOC_H__ */
