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

/**
 * @brief Internal header file to PQoS OS allocation module
 */

#ifndef __PQOS_OS_ALLOC_H__
#define __PQOS_OS_ALLOC_H__

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @brief Initializes resctrl file system used for OS allocation interface
 *
 * @param cpu cpu topology structure
 * @param cap capabilities structure
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK success
 */
int os_alloc_init(const struct pqos_cpuinfo *cpu, const struct pqos_cap *cap);

/**
 * @brief Shuts down allocation sub-module for OS allocation
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK success
 */
int os_alloc_fini(void);

/**
 * @brief OS interface to assign first available
 *        COS to cores in \a core_array
 *
 * While searching for available COS take technologies it is intended to use
 * with into account.
 * Note on \a technology and \a core_array selection:
 * - if L2 CAT technology is requested then cores need to belong to
 *   one L2 cluster (same L2ID)
 * - if only L3 CAT is requested then cores need to belong to one socket
 *
 * @param [in] technology bit mask selecting technologies
 *             (1 << enum pqos_cap_type)
 * @param [in] core_array list of core ids
 * @param [in] core_num number of core ids in the \a core_array
 * @param [out] class_id place to store reserved COS id
 *
 * @return Operations status
 */
int os_alloc_assign(const unsigned technology,
                    const unsigned *core_array,
                    const unsigned core_num,
                    unsigned *class_id);

/**
 * @brief OS interface to reassign cores
 *        in \a core_array to default COS#0
 *
 * @param [in] core_array list of core ids
 * @param [in] core_num number of core ids in the \a core_array
 *
 * @return Operations status
 */
int os_alloc_release(const unsigned *core_array,
                     const unsigned core_num);
/**
 * @brief OS interface to reset configuration of allocation technologies
 *
 * Reverts allocation state to the one after reset:
 * - all cores associated with COS0
 *
 * As part of allocation reset CDP reconfiguration can be performed.
 * This can be requested via \a l3_cdp_cfg.
 *
 * @param [in] l3_cdp_cfg requested L3 CAT CDP config
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 */
int os_alloc_reset(const enum pqos_cdp_config l3_cdp_cfg);

/**
 * @brief OS interface to set classes of service
 *        defined by \a ca on \a socket
 *
 * @param [in] socket CPU socket id
 * @param [in] num_cos number of classes of service at \a ca
 * @param [in] ca table with class of service definitions
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 */
int os_l3ca_set(const unsigned socket,
                const unsigned num_cos,
                const struct pqos_l3ca *ca);

/**
 * @brief OS interface to read classes of service from \a socket
 *
 * @param [in] socket CPU socket id
 * @param [in] max_num_ca maximum number of classes of service
 *             that can be accommodated at \a ca
 * @param [out] num_ca number of classes of service read into \a ca
 * @param [out] ca table with read classes of service
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 */
int os_l3ca_get(const unsigned socket,
                const unsigned max_num_ca,
                unsigned *num_ca,
                struct pqos_l3ca *ca);

/**
 * @brief OS interface to set classes of
 *        service defined by \a ca on \a l2id
 *
 * @param [in] l2id unique L2 cache identifier
 * @param [in] num_cos number of classes of service at \a ca
 * @param [in] ca table with class of service definitions
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 */
int os_l2ca_set(const unsigned l2id,
                const unsigned num_cos,
                const struct pqos_l2ca *ca);

/**
 * @brief OS interface to read classes of service from \a l2id
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
int os_l2ca_get(const unsigned l2id,
                const unsigned max_num_ca,
                unsigned *num_ca,
                struct pqos_l2ca *ca);

/**
 * @brief OS interface to associate \a lcore
 *        with given class of service
 *
 * @param [in] lcore CPU logical core id
 * @param [in] class_id class of service
 *
 * @return Operations status
 */
int os_alloc_assoc_set(const unsigned lcore,
                       const unsigned class_id);

/**
 * @brief OS interface to read association
 *        of \a lcore with class of service
 *
 * @param [in] lcore CPU logical core id
 * @param [out] class_id class of service
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 */
int os_alloc_assoc_get(const unsigned lcore,
                       unsigned *class_id);

#ifdef __cplusplus
}
#endif

#endif /* __PQOS_OS_ALLOC_H__ */
