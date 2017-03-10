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

#include <pqos.h>
#include "allocation.h"
#include "monitoring.h"
#include "cap.h"

/*
 * =======================================
 * Allocation Technology
 * =======================================
 */

int pqos_alloc_assoc_set(const unsigned lcore,
                         const unsigned class_id)
{
	int ret;

	_pqos_api_lock();

        ret = _pqos_check_init(1);
        if (ret != PQOS_RETVAL_OK) {
                _pqos_api_unlock();
                return ret;
        }

	ret = hw_alloc_assoc_set(lcore, class_id);

	_pqos_api_unlock();

	return ret;
}

int pqos_alloc_assoc_get(const unsigned lcore,
                         unsigned *class_id)
{
	int ret;

	_pqos_api_lock();

        ret = _pqos_check_init(1);
        if (ret != PQOS_RETVAL_OK) {
                _pqos_api_unlock();
                return ret;
        }

	ret = hw_alloc_assoc_get(lcore, class_id);

	_pqos_api_unlock();

	return ret;
}

int pqos_alloc_assign(const unsigned technology,
                      const unsigned *core_array,
                      const unsigned core_num,
                      unsigned *class_id)
{
	int ret;

	_pqos_api_lock();

        ret = _pqos_check_init(1);
        if (ret != PQOS_RETVAL_OK) {
                _pqos_api_unlock();
                return ret;
        }

	ret = hw_alloc_assign(technology, core_array, core_num, class_id);

	_pqos_api_unlock();

	return ret;
}

int pqos_alloc_release(const unsigned *core_array,
                       const unsigned core_num)
{
	int ret;

	_pqos_api_lock();

        ret = _pqos_check_init(1);
        if (ret != PQOS_RETVAL_OK) {
                _pqos_api_unlock();
                return ret;
        }

	ret = hw_alloc_release(core_array, core_num);

	_pqos_api_unlock();

	return ret;
}

int pqos_alloc_reset(const enum pqos_cdp_config l3_cdp_cfg)
{
	int ret;

	_pqos_api_lock();

        ret = _pqos_check_init(1);
        if (ret != PQOS_RETVAL_OK) {
                _pqos_api_unlock();
                return ret;
        }

	ret = hw_alloc_reset(l3_cdp_cfg);

	_pqos_api_unlock();

	return ret;
}

/*
 * =======================================
 * L3 cache allocation
 * =======================================
 */

int pqos_l3ca_set(const unsigned socket,
                  const unsigned num_cos,
		  const struct pqos_l3ca *ca)
{
	int ret;

	_pqos_api_lock();

        ret = _pqos_check_init(1);
        if (ret != PQOS_RETVAL_OK) {
                _pqos_api_unlock();
                return ret;
        }

	ret = hw_l3ca_set(socket, num_cos, ca);

	_pqos_api_unlock();

	return ret;
}

int pqos_l3ca_get(const unsigned socket,
                  const unsigned max_num_ca,
                  unsigned *num_ca,
                  struct pqos_l3ca *ca)
{
	int ret;

	_pqos_api_lock();

        ret = _pqos_check_init(1);
        if (ret != PQOS_RETVAL_OK) {
                _pqos_api_unlock();
                return ret;
        }

	ret = hw_l3ca_get(socket, max_num_ca, num_ca, ca);

	_pqos_api_unlock();

	return ret;
}

/*
 * =======================================
 * L2 cache allocation
 * =======================================
 */

int pqos_l2ca_set(const unsigned l2id,
                  const unsigned num_cos,
                  const struct pqos_l2ca *ca)
{
	int ret;

	_pqos_api_lock();

        ret = _pqos_check_init(1);
        if (ret != PQOS_RETVAL_OK) {
                _pqos_api_unlock();
                return ret;
        }

	ret = hw_l2ca_set(l2id, num_cos, ca);

	_pqos_api_unlock();

	return ret;
}

int pqos_l2ca_get(const unsigned l2id,
                  const unsigned max_num_ca,
                  unsigned *num_ca,
                  struct pqos_l2ca *ca)
{
	int ret;

	_pqos_api_lock();

        ret = _pqos_check_init(1);
        if (ret != PQOS_RETVAL_OK) {
                _pqos_api_unlock();
                return ret;
        }

	ret = hw_l2ca_get(l2id, max_num_ca, num_ca, ca);

	_pqos_api_unlock();

	return ret;
}

/*
 * =======================================
 * Memory Bandwidth Allocation
 * =======================================
 */

int pqos_mba_set(const unsigned socket,
                 const unsigned num_cos,
                 const struct pqos_mba *requested,
                 struct pqos_mba *actual)
{
	int ret;

	_pqos_api_lock();

        ret = _pqos_check_init(1);
        if (ret != PQOS_RETVAL_OK) {
                _pqos_api_unlock();
                return ret;
        }

	ret = hw_mba_set(socket, num_cos, requested, actual);

	_pqos_api_unlock();

	return ret;

}

int pqos_mba_get(const unsigned socket,
                 const unsigned max_num_cos,
                 unsigned *num_cos,
                 struct pqos_mba *mba_tab)
{
	int ret;

	_pqos_api_lock();

        ret = _pqos_check_init(1);
        if (ret != PQOS_RETVAL_OK) {
                _pqos_api_unlock();
                return ret;
        }

	ret = hw_mba_get(socket, max_num_cos, num_cos, mba_tab);

	_pqos_api_unlock();

	return ret;
}

int pqos_mon_reset(void)
{
        int ret;

        _pqos_api_lock();

        ret = _pqos_check_init(1);
        if (ret != PQOS_RETVAL_OK) {
                _pqos_api_unlock();
                return ret;
        }

        ret = hw_mon_reset();

        _pqos_api_unlock();

        return ret;
}

int pqos_mon_assoc_get(const unsigned lcore,
                       pqos_rmid_t *rmid)
{
        int ret;

        _pqos_api_lock();

        ret = _pqos_check_init(1);
        if (ret != PQOS_RETVAL_OK) {
                _pqos_api_unlock();
                return ret;
        }

        ret = hw_mon_assoc_get(lcore, rmid);

        _pqos_api_unlock();

        return ret;
}

int pqos_mon_start(const unsigned num_cores,
                   const unsigned *cores,
                   const enum pqos_mon_event event,
                   void *context,
                   struct pqos_mon_data *group)
{
        int ret;

        _pqos_api_lock();

        ret = _pqos_check_init(1);
        if (ret != PQOS_RETVAL_OK) {
                _pqos_api_unlock();
                return ret;
        }

        ret = hw_mon_start(num_cores, cores, event, context, group);

        _pqos_api_unlock();

        return ret;
}

int pqos_mon_stop(struct pqos_mon_data *group)
{
        int ret;

        _pqos_api_lock();

        ret = _pqos_check_init(1);
        if (ret != PQOS_RETVAL_OK) {
                _pqos_api_unlock();
                return ret;
        }

        ret = hw_mon_stop(group);

        _pqos_api_unlock();

        return ret;
}

int pqos_mon_poll(struct pqos_mon_data **groups,
                  const unsigned num_groups)
{
        int ret;

        _pqos_api_lock();

        ret = _pqos_check_init(1);
        if (ret != PQOS_RETVAL_OK) {
                _pqos_api_unlock();
                return ret;
        }

        ret = hw_mon_poll(groups, num_groups);

        _pqos_api_unlock();

        return ret;
}

