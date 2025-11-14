// Copyright(c) 2025 Zededa, Inc.
// SPDX-License-Identifier: BSD-3-Clause

package pqos

/*
#cgo LDFLAGS: -L${SRCDIR}/../../../lib -lpqos
#cgo CFLAGS: -I${SRCDIR}/../../../lib
#include <stdlib.h>
#include <pqos.h>

// Helper to set ways_mask in L3CA union
static inline void set_l3ca_ways_mask(struct pqos_l3ca *ca, uint64_t mask) {
	ca->u.ways_mask = mask;
}

// Helper to get ways_mask from L3CA union
static inline uint64_t get_l3ca_ways_mask(struct pqos_l3ca *ca) {
	return ca->u.ways_mask;
}

// Helper to set CDP masks in L3CA union
static inline void set_l3ca_cdp_masks(struct pqos_l3ca *ca, uint64_t code, uint64_t data) {
	ca->u.s.code_mask = code;
	ca->u.s.data_mask = data;
}

// Helper to get CDP masks from L3CA union
static inline void get_l3ca_cdp_masks(struct pqos_l3ca *ca, uint64_t *code, uint64_t *data) {
	*code = ca->u.s.code_mask;
	*data = ca->u.s.data_mask;
}

// Helper to set ways_mask in L2CA union
static inline void set_l2ca_ways_mask(struct pqos_l2ca *ca, uint64_t mask) {
	ca->u.ways_mask = mask;
}

// Helper to get ways_mask from L2CA union
static inline uint64_t get_l2ca_ways_mask(struct pqos_l2ca *ca) {
	return ca->u.ways_mask;
}

// Helper to set CDP masks in L2CA union
static inline void set_l2ca_cdp_masks(struct pqos_l2ca *ca, uint64_t code, uint64_t data) {
	ca->u.s.code_mask = code;
	ca->u.s.data_mask = data;
}

// Helper to get CDP masks from L2CA union
static inline void get_l2ca_cdp_masks(struct pqos_l2ca *ca, uint64_t *code, uint64_t *data) {
	*code = ca->u.s.code_mask;
	*data = ca->u.s.data_mask;
}
*/
import "C"
import (
	"fmt"
)

// L3CA represents L3 Cache Allocation configuration
type L3CA struct {
	// ClassID is the class of service
	ClassID uint
	// CDP indicates if code and data masks are used separately
	CDP bool
	// WaysMask is the bit mask for L3 cache ways (when CDP is false)
	WaysMask uint64
	// CodeMask is the bit mask for code (when CDP is true)
	CodeMask uint64
	// DataMask is the bit mask for data (when CDP is true)
	DataMask uint64
}

// L2CA represents L2 Cache Allocation configuration
type L2CA struct {
	// ClassID is the class of service
	ClassID uint
	// CDP indicates if code and data masks are used separately
	CDP bool
	// WaysMask is the bit mask for L2 cache ways (when CDP is false)
	WaysMask uint64
	// CodeMask is the bit mask for code (when CDP is true)
	CodeMask uint64
	// DataMask is the bit mask for data (when CDP is true)
	DataMask uint64
}

// MBA represents Memory Bandwidth Allocation configuration
type MBA struct {
	// ClassID is the class of service
	ClassID uint
	// MBMax is the maximum available bandwidth
	// - Without MBA controller: percentage (0-100)
	// - With MBA controller: MBps
	MBMax uint
	// Ctrl indicates if MBA controller is being used
	Ctrl bool
	// SMBA indicates if this is SMBA (Slow Memory Bandwidth Allocation)
	SMBA bool
}

// L3CASet configures L3 cache allocation for a given L3 cluster
//
// Parameters:
//   - l3CatID: L3 cache allocation ID (cluster)
//   - cos: slice of L3CA configurations to set
//
// Returns error if operation fails
func (p *PQoS) L3CASet(l3CatID uint, cos []L3CA) error {
	p.mu.Lock()
	defer p.mu.Unlock()

	if !p.initialized {
		return ErrNotInitialized
	}

	if len(cos) == 0 {
		return fmt.Errorf("empty COS array")
	}

	// Convert Go slice to C array
	cCos := make([]C.struct_pqos_l3ca, len(cos))
	for i, ca := range cos {
		cCos[i].class_id = C.uint(ca.ClassID)
		cCos[i].cdp = C.int(0)
		if ca.CDP {
			cCos[i].cdp = C.int(1)
			C.set_l3ca_cdp_masks(&cCos[i], C.uint64_t(ca.CodeMask), C.uint64_t(ca.DataMask))
		} else {
			C.set_l3ca_ways_mask(&cCos[i], C.uint64_t(ca.WaysMask))
		}
	}

	ret := C.pqos_l3ca_set(C.uint(l3CatID), C.uint(len(cos)), &cCos[0])
	if ret != C.PQOS_RETVAL_OK {
		return retvalToError(ret, "pqos_l3ca_set")
	}

	return nil
}

// L3CAGet retrieves L3 cache allocation configuration for a given L3 cluster
//
// Parameters:
//   - l3CatID: L3 cache allocation ID (cluster)
//   - maxNumCOS: maximum number of classes to retrieve
//
// Returns slice of L3CA configurations and error if operation fails
func (p *PQoS) L3CAGet(l3CatID uint, maxNumCOS uint) ([]L3CA, error) {
	p.mu.Lock()
	defer p.mu.Unlock()

	if !p.initialized {
		return nil, ErrNotInitialized
	}

	if maxNumCOS == 0 {
		return nil, fmt.Errorf("maxNumCOS must be greater than 0")
	}

	// Allocate C array
	cCos := make([]C.struct_pqos_l3ca, maxNumCOS)
	var numCos C.uint

	ret := C.pqos_l3ca_get(C.uint(l3CatID), C.uint(maxNumCOS), &numCos, &cCos[0])
	if ret != C.PQOS_RETVAL_OK {
		return nil, retvalToError(ret, "pqos_l3ca_get")
	}

	// Convert C array to Go slice
	result := make([]L3CA, numCos)
	for i := C.uint(0); i < numCos; i++ {
		ca := &cCos[i]
		result[i].ClassID = uint(ca.class_id)
		result[i].CDP = int(ca.cdp) != 0

		if result[i].CDP {
			var code, data C.uint64_t
			C.get_l3ca_cdp_masks(ca, &code, &data)
			result[i].CodeMask = uint64(code)
			result[i].DataMask = uint64(data)
		} else {
			result[i].WaysMask = uint64(C.get_l3ca_ways_mask(ca))
		}
	}

	return result, nil
}

// L3CAGetMinCBMBits retrieves the minimum number of bits that must be set in cache bitmask
func (p *PQoS) L3CAGetMinCBMBits() (uint, error) {
	p.mu.Lock()
	defer p.mu.Unlock()

	if !p.initialized {
		return 0, ErrNotInitialized
	}

	var minBits C.uint
	ret := C.pqos_l3ca_get_min_cbm_bits(&minBits)
	if ret != C.PQOS_RETVAL_OK {
		return 0, retvalToError(ret, "pqos_l3ca_get_min_cbm_bits")
	}

	return uint(minBits), nil
}

// L2CASet configures L2 cache allocation for a given L2 cluster
//
// Parameters:
//   - l2ID: L2 cache ID (cluster)
//   - cos: slice of L2CA configurations to set
//
// Returns error if operation fails
func (p *PQoS) L2CASet(l2ID uint, cos []L2CA) error {
	p.mu.Lock()
	defer p.mu.Unlock()

	if !p.initialized {
		return ErrNotInitialized
	}

	if len(cos) == 0 {
		return fmt.Errorf("empty COS array")
	}

	// Convert Go slice to C array
	cCos := make([]C.struct_pqos_l2ca, len(cos))
	for i, ca := range cos {
		cCos[i].class_id = C.uint(ca.ClassID)
		cCos[i].cdp = C.int(0)
		if ca.CDP {
			cCos[i].cdp = C.int(1)
			C.set_l2ca_cdp_masks(&cCos[i], C.uint64_t(ca.CodeMask), C.uint64_t(ca.DataMask))
		} else {
			C.set_l2ca_ways_mask(&cCos[i], C.uint64_t(ca.WaysMask))
		}
	}

	ret := C.pqos_l2ca_set(C.uint(l2ID), C.uint(len(cos)), &cCos[0])
	if ret != C.PQOS_RETVAL_OK {
		return retvalToError(ret, "pqos_l2ca_set")
	}

	return nil
}

// L2CAGet retrieves L2 cache allocation configuration for a given L2 cluster
//
// Parameters:
//   - l2ID: L2 cache ID (cluster)
//   - maxNumCOS: maximum number of classes to retrieve
//
// Returns slice of L2CA configurations and error if operation fails
func (p *PQoS) L2CAGet(l2ID uint, maxNumCOS uint) ([]L2CA, error) {
	p.mu.Lock()
	defer p.mu.Unlock()

	if !p.initialized {
		return nil, ErrNotInitialized
	}

	if maxNumCOS == 0 {
		return nil, fmt.Errorf("maxNumCOS must be greater than 0")
	}

	// Allocate C array
	cCos := make([]C.struct_pqos_l2ca, maxNumCOS)
	var numCos C.uint

	ret := C.pqos_l2ca_get(C.uint(l2ID), C.uint(maxNumCOS), &numCos, &cCos[0])
	if ret != C.PQOS_RETVAL_OK {
		return nil, retvalToError(ret, "pqos_l2ca_get")
	}

	// Convert C array to Go slice
	result := make([]L2CA, numCos)
	for i := C.uint(0); i < numCos; i++ {
		ca := &cCos[i]
		result[i].ClassID = uint(ca.class_id)
		result[i].CDP = int(ca.cdp) != 0

		if result[i].CDP {
			var code, data C.uint64_t
			C.get_l2ca_cdp_masks(ca, &code, &data)
			result[i].CodeMask = uint64(code)
			result[i].DataMask = uint64(data)
		} else {
			result[i].WaysMask = uint64(C.get_l2ca_ways_mask(ca))
		}
	}

	return result, nil
}

// L2CAGetMinCBMBits retrieves the minimum number of bits that must be set in cache bitmask
func (p *PQoS) L2CAGetMinCBMBits() (uint, error) {
	p.mu.Lock()
	defer p.mu.Unlock()

	if !p.initialized {
		return 0, ErrNotInitialized
	}

	var minBits C.uint
	ret := C.pqos_l2ca_get_min_cbm_bits(&minBits)
	if ret != C.PQOS_RETVAL_OK {
		return 0, retvalToError(ret, "pqos_l2ca_get_min_cbm_bits")
	}

	return uint(minBits), nil
}

// MBASet configures memory bandwidth allocation for a given MBA ID
//
// Parameters:
//   - mbaID: MBA ID
//   - requested: slice of requested MBA configurations
//
// Returns actual MBA configurations applied and error if operation fails
func (p *PQoS) MBASet(mbaID uint, requested []MBA) ([]MBA, error) {
	p.mu.Lock()
	defer p.mu.Unlock()

	if !p.initialized {
		return nil, ErrNotInitialized
	}

	if len(requested) == 0 {
		return nil, fmt.Errorf("empty MBA array")
	}

	// Convert Go slice to C arrays
	cRequested := make([]C.struct_pqos_mba, len(requested))
	cActual := make([]C.struct_pqos_mba, len(requested))

	for i, mba := range requested {
		cRequested[i].class_id = C.uint(mba.ClassID)
		cRequested[i].mb_max = C.uint(mba.MBMax)
		cRequested[i].ctrl = C.int(0)
		if mba.Ctrl {
			cRequested[i].ctrl = C.int(1)
		}
		cRequested[i].smba = C.int(0)
		if mba.SMBA {
			cRequested[i].smba = C.int(1)
		}
	}

	ret := C.pqos_mba_set(C.uint(mbaID), C.uint(len(requested)), &cRequested[0], &cActual[0])
	if ret != C.PQOS_RETVAL_OK {
		return nil, retvalToError(ret, "pqos_mba_set")
	}

	// Convert actual values to Go slice
	result := make([]MBA, len(requested))
	for i := 0; i < len(requested); i++ {
		result[i].ClassID = uint(cActual[i].class_id)
		result[i].MBMax = uint(cActual[i].mb_max)
		result[i].Ctrl = int(cActual[i].ctrl) != 0
		result[i].SMBA = int(cActual[i].smba) != 0
	}

	return result, nil
}

// MBAGet retrieves memory bandwidth allocation configuration for a given MBA ID
//
// Parameters:
//   - mbaID: MBA ID
//   - maxNumCOS: maximum number of classes to retrieve
//
// Returns slice of MBA configurations and error if operation fails
func (p *PQoS) MBAGet(mbaID uint, maxNumCOS uint) ([]MBA, error) {
	p.mu.Lock()
	defer p.mu.Unlock()

	if !p.initialized {
		return nil, ErrNotInitialized
	}

	if maxNumCOS == 0 {
		return nil, fmt.Errorf("maxNumCOS must be greater than 0")
	}

	// Allocate C array
	cMBA := make([]C.struct_pqos_mba, maxNumCOS)
	var numCos C.uint

	ret := C.pqos_mba_get(C.uint(mbaID), C.uint(maxNumCOS), &numCos, &cMBA[0])
	if ret != C.PQOS_RETVAL_OK {
		return nil, retvalToError(ret, "pqos_mba_get")
	}

	// Convert C array to Go slice
	result := make([]MBA, numCos)
	for i := C.uint(0); i < numCos; i++ {
		result[i].ClassID = uint(cMBA[i].class_id)
		result[i].MBMax = uint(cMBA[i].mb_max)
		result[i].Ctrl = int(cMBA[i].ctrl) != 0
		result[i].SMBA = int(cMBA[i].smba) != 0
	}

	return result, nil
}

// AllocAssocSet associates a logical core with a class of service
//
// Parameters:
//   - lcore: logical core ID
//   - classID: class of service
//
// Returns error if operation fails
func (p *PQoS) AllocAssocSet(lcore uint, classID uint) error {
	p.mu.Lock()
	defer p.mu.Unlock()

	if !p.initialized {
		return ErrNotInitialized
	}

	ret := C.pqos_alloc_assoc_set(C.uint(lcore), C.uint(classID))
	if ret != C.PQOS_RETVAL_OK {
		return retvalToError(ret, "pqos_alloc_assoc_set")
	}

	return nil
}

// AllocAssocGet retrieves the class of service associated with a logical core
//
// Parameters:
//   - lcore: logical core ID
//
// Returns class ID and error if operation fails
func (p *PQoS) AllocAssocGet(lcore uint) (uint, error) {
	p.mu.Lock()
	defer p.mu.Unlock()

	if !p.initialized {
		return 0, ErrNotInitialized
	}

	var classID C.uint
	ret := C.pqos_alloc_assoc_get(C.uint(lcore), &classID)
	if ret != C.PQOS_RETVAL_OK {
		return 0, retvalToError(ret, "pqos_alloc_assoc_get")
	}

	return uint(classID), nil
}

// AllocAssocSetPID associates a task (process) with a class of service (OS interface)
//
// Parameters:
//   - pid: process ID
//   - classID: class of service
//
// Returns error if operation fails
func (p *PQoS) AllocAssocSetPID(pid int, classID uint) error {
	p.mu.Lock()
	defer p.mu.Unlock()

	if !p.initialized {
		return ErrNotInitialized
	}

	ret := C.pqos_alloc_assoc_set_pid(C.pid_t(pid), C.uint(classID))
	if ret != C.PQOS_RETVAL_OK {
		return retvalToError(ret, "pqos_alloc_assoc_set_pid")
	}

	return nil
}

// AllocAssocGetPID retrieves the class of service associated with a task (OS interface)
//
// Parameters:
//   - pid: process ID
//
// Returns class ID and error if operation fails
func (p *PQoS) AllocAssocGetPID(pid int) (uint, error) {
	p.mu.Lock()
	defer p.mu.Unlock()

	if !p.initialized {
		return 0, ErrNotInitialized
	}

	var classID C.uint
	ret := C.pqos_alloc_assoc_get_pid(C.pid_t(pid), &classID)
	if ret != C.PQOS_RETVAL_OK {
		return 0, retvalToError(ret, "pqos_alloc_assoc_get_pid")
	}

	return uint(classID), nil
}

// AllocAssign assigns first available COS to cores
//
// Parameters:
//   - technology: bit mask selecting technologies (1 << CapType)
//   - cores: list of core IDs
//
// Returns assigned class ID and error if operation fails
func (p *PQoS) AllocAssign(technology uint, cores []uint) (uint, error) {
	p.mu.Lock()
	defer p.mu.Unlock()

	if !p.initialized {
		return 0, ErrNotInitialized
	}

	if len(cores) == 0 {
		return 0, fmt.Errorf("empty cores array")
	}

	// Convert Go slice to C array
	cCores := make([]C.uint, len(cores))
	for i, core := range cores {
		cCores[i] = C.uint(core)
	}

	var classID C.uint
	ret := C.pqos_alloc_assign(C.uint(technology), &cCores[0], C.uint(len(cores)), &classID)
	if ret != C.PQOS_RETVAL_OK {
		return 0, retvalToError(ret, "pqos_alloc_assign")
	}

	return uint(classID), nil
}

// AllocRelease reassigns cores to default COS#0
//
// Parameters:
//   - cores: list of core IDs
//
// Returns error if operation fails
func (p *PQoS) AllocRelease(cores []uint) error {
	p.mu.Lock()
	defer p.mu.Unlock()

	if !p.initialized {
		return ErrNotInitialized
	}

	if len(cores) == 0 {
		return fmt.Errorf("empty cores array")
	}

	// Convert Go slice to C array
	cCores := make([]C.uint, len(cores))
	for i, core := range cores {
		cCores[i] = C.uint(core)
	}

	ret := C.pqos_alloc_release(&cCores[0], C.uint(len(cores)))
	if ret != C.PQOS_RETVAL_OK {
		return retvalToError(ret, "pqos_alloc_release")
	}

	return nil
}

// AllocAssignPID assigns first available COS to tasks (OS interface)
//
// Parameters:
//   - technology: bit mask selecting technologies (1 << CapType)
//   - pids: list of process IDs
//
// Returns assigned class ID and error if operation fails
func (p *PQoS) AllocAssignPID(technology uint, pids []int) (uint, error) {
	p.mu.Lock()
	defer p.mu.Unlock()

	if !p.initialized {
		return 0, ErrNotInitialized
	}

	if len(pids) == 0 {
		return 0, fmt.Errorf("empty pids array")
	}

	// Convert Go slice to C array
	cPIDs := make([]C.pid_t, len(pids))
	for i, pid := range pids {
		cPIDs[i] = C.pid_t(pid)
	}

	var classID C.uint
	ret := C.pqos_alloc_assign_pid(C.uint(technology), &cPIDs[0], C.uint(len(pids)), &classID)
	if ret != C.PQOS_RETVAL_OK {
		return 0, retvalToError(ret, "pqos_alloc_assign_pid")
	}

	return uint(classID), nil
}

// AllocReleasePID reassigns tasks to default COS#0 (OS interface)
//
// Parameters:
//   - pids: list of process IDs
//
// Returns error if operation fails
func (p *PQoS) AllocReleasePID(pids []int) error {
	p.mu.Lock()
	defer p.mu.Unlock()

	if !p.initialized {
		return ErrNotInitialized
	}

	if len(pids) == 0 {
		return fmt.Errorf("empty pids array")
	}

	// Convert Go slice to C array
	cPIDs := make([]C.pid_t, len(pids))
	for i, pid := range pids {
		cPIDs[i] = C.pid_t(pid)
	}

	ret := C.pqos_alloc_release_pid(&cPIDs[0], C.uint(len(pids)))
	if ret != C.PQOS_RETVAL_OK {
		return retvalToError(ret, "pqos_alloc_release_pid")
	}

	return nil
}

// AllocReset resets allocation configuration to default
func (p *PQoS) AllocReset() error {
	p.mu.Lock()
	defer p.mu.Unlock()

	if !p.initialized {
		return ErrNotInitialized
	}

	// Use PQOS_REQUIRE_CDP_ANY and PQOS_MBA_ANY for default reset
	ret := C.pqos_alloc_reset(C.PQOS_REQUIRE_CDP_ANY, C.PQOS_REQUIRE_CDP_ANY, C.PQOS_MBA_ANY)
	if ret != C.PQOS_RETVAL_OK {
		return retvalToError(ret, "pqos_alloc_reset")
	}

	return nil
}
