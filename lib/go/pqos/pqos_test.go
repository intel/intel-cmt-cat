// Copyright(c) 2025 Zededa, Inc.
// SPDX-License-Identifier: BSD-3-Clause

package pqos

import (
	"testing"
)

// TestGetInstance tests the singleton pattern
func TestGetInstance(t *testing.T) {
	p1 := GetInstance()
	p2 := GetInstance()

	if p1 != p2 {
		t.Error("GetInstance() should return the same instance")
	}

	if p1 == nil {
		t.Error("GetInstance() returned nil")
	}
}

// TestIsInitializedBeforeInit tests that IsInitialized returns false before Init
func TestIsInitializedBeforeInit(t *testing.T) {
	p := GetInstance()
	if p.IsInitialized() {
		t.Error("IsInitialized() should return false before Init()")
	}
}

// TestInitWithNilConfig tests initialization with nil config
func TestInitWithNilConfig(t *testing.T) {
	p := GetInstance()

	// Clean up any previous initialization
	if p.IsInitialized() {
		p.Fini()
	}

	err := p.Init(nil)
	if err != nil {
		t.Skipf("Init with nil config failed (may require hardware/privileges): %v", err)
	}
	defer p.Fini()

	if !p.IsInitialized() {
		t.Error("IsInitialized() should return true after successful Init()")
	}
}

// TestInitWithConfig tests initialization with explicit config
func TestInitWithConfig(t *testing.T) {
	p := GetInstance()

	// Clean up any previous initialization
	if p.IsInitialized() {
		p.Fini()
	}

	cfg := &Config{
		Interface: InterAuto,
		Verbose:   LogVerbositySilent,
	}

	err := p.Init(cfg)
	if err != nil {
		t.Skipf("Init failed (may require hardware/privileges): %v", err)
	}
	defer p.Fini()

	if !p.IsInitialized() {
		t.Error("IsInitialized() should return true after successful Init()")
	}
}

// TestDoubleInit tests that calling Init twice is idempotent (no error)
func TestDoubleInit(t *testing.T) {
	p := GetInstance()

	// Clean up any previous initialization
	if p.IsInitialized() {
		p.Fini()
	}

	cfg := &Config{
		Interface: InterAuto,
		Verbose:   LogVerbositySilent,
	}

	err := p.Init(cfg)
	if err != nil {
		t.Skipf("First Init failed (may require hardware/privileges): %v", err)
	}
	defer p.Fini()

	// Try to init again - should be no-op, no error
	err = p.Init(cfg)
	if err != nil {
		t.Errorf("Second Init() should be idempotent (no error), got: %v", err)
	}
}

// TestFiniWithoutInit tests that Fini without Init returns an error
func TestFiniWithoutInit(t *testing.T) {
	p := GetInstance()

	// Ensure not initialized
	if p.IsInitialized() {
		p.Fini()
	}

	err := p.Fini()
	if err == nil {
		t.Error("Fini() without Init() should return an error")
	}
}

// TestInterfaceToString tests interface name conversion
func TestInterfaceToString(t *testing.T) {
	tests := []struct {
		iface    int
		expected string
	}{
		{InterMSR, "MSR"},
		{InterOS, "OS"},
		{InterOSResctrlMon, "OS_RESCTRL_MON"},
		{InterAuto, "AUTO"},
		{999, "UNKNOWN(999)"},
	}

	for _, tt := range tests {
		result := InterfaceToString(tt.iface)
		if result != tt.expected {
			t.Errorf("InterfaceToString(%d) = %s, expected %s", tt.iface, result, tt.expected)
		}
	}
}

// TestVerbosityToString tests verbosity level name conversion
func TestVerbosityToString(t *testing.T) {
	tests := []struct {
		verbose  int
		expected string
	}{
		{LogVerbositySilent, "SILENT"},
		{LogVerbosityDefault, "DEFAULT"},
		{LogVerbosityVerbose, "VERBOSE"},
		{LogVerbositySuperVerbose, "SUPER_VERBOSE"},
		{999, "UNKNOWN(999)"},
	}

	for _, tt := range tests {
		result := VerbosityToString(tt.verbose)
		if result != tt.expected {
			t.Errorf("VerbosityToString(%d) = %s, expected %s", tt.verbose, result, tt.expected)
		}
	}
}

// TestVendorToString tests vendor name conversion
func TestVendorToString(t *testing.T) {
	tests := []struct {
		vendor   int
		expected string
	}{
		{VendorIntel, "Intel"},
		{VendorAMD, "AMD"},
		{VendorUnknown, "Unknown"},
		{999, "Unknown(999)"},
	}

	for _, tt := range tests {
		result := VendorToString(tt.vendor)
		if result != tt.expected {
			t.Errorf("VendorToString(%d) = %s, expected %s", tt.vendor, result, tt.expected)
		}
	}
}

// TestCapTypeToString tests capability type name conversion
func TestCapTypeToString(t *testing.T) {
	tests := []struct {
		capType  int
		expected string
	}{
		{CapTypeMon, "MON"},
		{CapTypeL3CA, "L3CA"},
		{CapTypeL2CA, "L2CA"},
		{CapTypeMBA, "MBA"},
		{CapTypeSMBA, "SMBA"},
		{999, "UNKNOWN(999)"},
	}

	for _, tt := range tests {
		result := CapTypeToString(tt.capType)
		if result != tt.expected {
			t.Errorf("CapTypeToString(%d) = %s, expected %s", tt.capType, result, tt.expected)
		}
	}
}

// TestMonEventToString tests monitoring event name conversion
func TestMonEventToString(t *testing.T) {
	tests := []struct {
		event    uint
		expected string
	}{
		{MonEventL3Occup, "L3_OCCUP"},
		{MonEventLMemBW, "LMEM_BW"},
		{MonEventTMemBW, "TMEM_BW"},
		{MonEventRMemBW, "RMEM_BW"},
		{PerfEventLLCMiss, "LLC_MISS"},
		{PerfEventIPC, "IPC"},
		{PerfEventLLCRef, "LLC_REF"},
	}

	for _, tt := range tests {
		result := MonEventToString(tt.event)
		if result != tt.expected {
			t.Errorf("MonEventToString(0x%x) = %s, expected %s", tt.event, result, tt.expected)
		}
	}
}

// TestGetCapabilityWithoutInit tests that GetCapability fails without Init
func TestGetCapabilityWithoutInit(t *testing.T) {
	p := GetInstance()

	// Ensure not initialized
	if p.IsInitialized() {
		p.Fini()
	}

	_, err := p.GetCapability()
	if err == nil {
		t.Error("GetCapability() without Init() should return an error")
	}
}

// TestGetInterfaceWithoutInit tests that GetInterface fails without Init
func TestGetInterfaceWithoutInit(t *testing.T) {
	p := GetInstance()

	// Ensure not initialized
	if p.IsInitialized() {
		p.Fini()
	}

	_, err := p.GetInterface()
	if err == nil {
		t.Error("GetInterface() without Init() should return an error")
	}
}

// Benchmark tests
func BenchmarkGetInstance(b *testing.B) {
	for i := 0; i < b.N; i++ {
		_ = GetInstance()
	}
}

func BenchmarkInterfaceToString(b *testing.B) {
	for i := 0; i < b.N; i++ {
		_ = InterfaceToString(InterOS)
	}
}

// TestCPUInfoL2Methods tests L2 topology discovery methods
func TestCPUInfoL2Methods(t *testing.T) {
	// Create a mock CPUInfo with some test data
	cpuInfo := &CPUInfo{
		NumCores: 8,
		Cores: []CoreInfo{
			{LCore: 0, Socket: 0, L2ID: 0, L3ID: 0},
			{LCore: 1, Socket: 0, L2ID: 0, L3ID: 0}, // Shares L2 with core 0
			{LCore: 2, Socket: 0, L2ID: 1, L3ID: 0},
			{LCore: 3, Socket: 0, L2ID: 1, L3ID: 0}, // Shares L2 with core 2
			{LCore: 4, Socket: 0, L2ID: 2, L3ID: 0},
			{LCore: 5, Socket: 0, L2ID: 2, L3ID: 0}, // Shares L2 with core 4
			{LCore: 6, Socket: 0, L2ID: 3, L3ID: 0},
			{LCore: 7, Socket: 0, L2ID: 3, L3ID: 0}, // Shares L2 with core 6
		},
	}

	// Test GetL2IDs
	l2IDs := cpuInfo.GetL2IDs()
	if len(l2IDs) != 4 {
		t.Errorf("GetL2IDs() returned %d domains, expected 4", len(l2IDs))
	}

	// Test GetNumL2Domains
	numDomains := cpuInfo.GetNumL2Domains()
	if numDomains != 4 {
		t.Errorf("GetNumL2Domains() = %d, expected 4", numDomains)
	}

	// Test GetCoresByL2ID
	cores := cpuInfo.GetCoresByL2ID(0)
	if len(cores) != 2 {
		t.Errorf("GetCoresByL2ID(0) returned %d cores, expected 2", len(cores))
	}
	if cores[0].LCore != 0 || cores[1].LCore != 1 {
		t.Errorf("GetCoresByL2ID(0) returned wrong cores")
	}

	cores = cpuInfo.GetCoresByL2ID(1)
	if len(cores) != 2 {
		t.Errorf("GetCoresByL2ID(1) returned %d cores, expected 2", len(cores))
	}
	if cores[0].LCore != 2 || cores[1].LCore != 3 {
		t.Errorf("GetCoresByL2ID(1) returned wrong cores")
	}

	// Test with non-existent L2ID
	cores = cpuInfo.GetCoresByL2ID(999)
	if len(cores) != 0 {
		t.Errorf("GetCoresByL2ID(999) should return empty slice")
	}
}

// TestCPUInfoL2MethodsComplexTopology tests L2 methods with complex topology
func TestCPUInfoL2MethodsComplexTopology(t *testing.T) {
	// Simulate complex topology where non-adjacent cores share L2
	cpuInfo := &CPUInfo{
		NumCores: 8,
		Cores: []CoreInfo{
			{LCore: 0, Socket: 0, L2ID: 0, L3ID: 0},
			{LCore: 1, Socket: 0, L2ID: 1, L3ID: 0},
			{LCore: 2, Socket: 0, L2ID: 2, L3ID: 0},
			{LCore: 3, Socket: 0, L2ID: 3, L3ID: 0},
			{LCore: 4, Socket: 0, L2ID: 0, L3ID: 0}, // Shares L2 with core 0 (non-adjacent)
			{LCore: 5, Socket: 0, L2ID: 1, L3ID: 0}, // Shares L2 with core 1
			{LCore: 6, Socket: 0, L2ID: 2, L3ID: 0}, // Shares L2 with core 2
			{LCore: 7, Socket: 0, L2ID: 3, L3ID: 0}, // Shares L2 with core 3
		},
	}

	l2IDs := cpuInfo.GetL2IDs()
	if len(l2IDs) != 4 {
		t.Errorf("GetL2IDs() returned %d domains, expected 4", len(l2IDs))
	}

	// Verify non-adjacent cores sharing L2
	cores := cpuInfo.GetCoresByL2ID(0)
	if len(cores) != 2 {
		t.Errorf("GetCoresByL2ID(0) returned %d cores, expected 2", len(cores))
	}
	if cores[0].LCore != 0 || cores[1].LCore != 4 {
		t.Errorf("GetCoresByL2ID(0) should return cores 0 and 4, got %d and %d", cores[0].LCore, cores[1].LCore)
	}
}

// TestCPUInfoL2MethodsEmptyCores tests L2 methods with empty core list
func TestCPUInfoL2MethodsEmptyCores(t *testing.T) {
	cpuInfo := &CPUInfo{
		NumCores: 0,
		Cores:    []CoreInfo{},
	}

	l2IDs := cpuInfo.GetL2IDs()
	if len(l2IDs) != 0 {
		t.Errorf("GetL2IDs() should return empty slice for no cores")
	}

	numDomains := cpuInfo.GetNumL2Domains()
	if numDomains != 0 {
		t.Errorf("GetNumL2Domains() should return 0 for no cores")
	}

	cores := cpuInfo.GetCoresByL2ID(0)
	if len(cores) != 0 {
		t.Errorf("GetCoresByL2ID(0) should return empty slice for no cores")
	}
}
