// Copyright(c) 2025 Zededa, Inc.
// SPDX-License-Identifier: BSD-3-Clause

// Package pqos provides Go bindings for Intel(R) Resource Director Technology (RDT)
// Platform QoS (PQoS) library.
//
// This package wraps the libpqos C library and provides idiomatic Go interfaces
// for monitoring and allocation of platform resources including:
//   - Cache Monitoring Technology (CMT)
//   - Memory Bandwidth Monitoring (MBM)
//   - Cache Allocation Technology (CAT)
//   - Code and Data Prioritization (CDP)
//   - Memory Bandwidth Allocation (MBA)
package pqos

/*
#cgo LDFLAGS: -L${SRCDIR}/../../../lib -lpqos
#cgo CFLAGS: -I${SRCDIR}/../../../lib
#include <stdlib.h>
#include <pqos.h>

// Helper function to set the interface field (since 'interface' is a Go keyword)
static inline void set_pqos_config_interface(struct pqos_config *cfg, enum pqos_interface iface) {
	cfg->interface = iface;
}
*/
import "C"
import (
	"fmt"
	"os"
	"sync"
	"unsafe"
)

// Interface types define how the library interacts with the hardware
const (
	// InterMSR uses Model Specific Registers interface
	InterMSR = C.PQOS_INTER_MSR
	// InterOS uses Operating System interface (resctrl on Linux)
	InterOS = C.PQOS_INTER_OS
	// InterOSResctrlMon uses OS interface with resctrl monitoring
	InterOSResctrlMon = C.PQOS_INTER_OS_RESCTRL_MON
	// InterAuto automatically selects the best available interface
	InterAuto = C.PQOS_INTER_AUTO
)

// Log verbosity levels
const (
	// LogVerbositySilent suppresses all log messages
	LogVerbositySilent = -1
	// LogVerbosityDefault shows warning and error messages
	LogVerbosityDefault = 0
	// LogVerbosityVerbose shows warning, error and info messages
	LogVerbosityVerbose = 1
	// LogVerbositySuperVerbose shows warning, error, info and debug messages
	LogVerbositySuperVerbose = 2
)

// Return values
const (
	RetvalOK        = C.PQOS_RETVAL_OK        // Success
	RetvalError     = C.PQOS_RETVAL_ERROR     // Generic error
	RetvalParam     = C.PQOS_RETVAL_PARAM     // Parameter error
	RetvalResource  = C.PQOS_RETVAL_RESOURCE  // Resource error
	RetvalInit      = C.PQOS_RETVAL_INIT      // Initialization error
	RetvalTransport = C.PQOS_RETVAL_TRANSPORT // Transport error
	RetvalPerfCtr   = C.PQOS_RETVAL_PERF_CTR  // Performance counter error
	RetvalBusy      = C.PQOS_RETVAL_BUSY      // Resource busy error
	RetvalInter     = C.PQOS_RETVAL_INTER     // Interface not supported
	RetvalOverflow  = C.PQOS_RETVAL_OVERFLOW  // Data overflow
)

// Config represents PQoS library configuration
type Config struct {
	// Interface specifies which interface to use (MSR, OS, etc.)
	Interface int
	// Verbose sets the logging verbosity level
	Verbose int
	// LogFile is an optional file for logging (defaults to stderr)
	LogFile *os.File
}

// PQoS represents the main library handle
type PQoS struct {
	mu          sync.Mutex
	initialized bool
}

var (
	instanceOnce sync.Once
	instance     *PQoS

	initOnce sync.Once
	initErr  error
)

var (
	// ErrNotInitialized is returned when an operation is attempted before Init()
	ErrNotInitialized = fmt.Errorf("pqos: not initialized")
)

// GetInstance returns the singleton instance of PQoS.
// The instance is created lazily on first call.
func GetInstance() *PQoS {
	instanceOnce.Do(func() {
		instance = &PQoS{}
	})
	return instance
}

// Init initializes the PQoS library with the given configuration.
// This must be called before any other PQoS operations.
//
// The first call to Init() performs the actual initialization.
// Subsequent calls are no-ops and return nil (or the cached error).
// This is safe to call from multiple goroutines.
//
// Note: Only the first Init() call's config is used. Later calls
// ignore their config parameter and just mark the instance as initialized.
//
// Example:
//
//	cfg := &pqos.Config{
//	    Interface: pqos.InterOS,
//	    Verbose:   pqos.LogVerbosityDefault,
//	}
//	pqos := pqos.GetInstance()
//	if err := pqos.Init(cfg); err != nil {
//	    log.Fatal(err)
//	}
//	defer pqos.Fini()
func (p *PQoS) Init(cfg *Config) error {
	p.mu.Lock()
	defer p.mu.Unlock()

	// Fast path: this instance already initialized
	if p.initialized {
		return nil
	}

	// Do the process-wide pqos_init() only once
	initOnce.Do(func() {
		if cfg == nil {
			cfg = &Config{
				Interface: InterAuto,
				Verbose:   LogVerbosityDefault,
			}
		}

		var cConfig C.struct_pqos_config
		// Note: 'interface' is a Go keyword, so we use a C helper function to set it
		C.set_pqos_config_interface(&cConfig, C.enum_pqos_interface(cfg.Interface))
		cConfig.verbose = C.int(cfg.Verbose)

		// Set log file descriptor
		if cfg.LogFile != nil {
			cConfig.fd_log = C.int(cfg.LogFile.Fd())
		} else {
			cConfig.fd_log = C.int(os.Stderr.Fd())
		}

		ret := C.pqos_init(&cConfig)
		if ret != C.PQOS_RETVAL_OK {
			initErr = retvalToError(ret, "pqos_init")
			return
		}
	})

	// Everyone sees the same init result
	if initErr != nil {
		return initErr
	}

	p.initialized = true
	return nil
}

// Fini shuts down the PQoS library and releases resources.
// This should be called when the library is no longer needed.
//
// Note: This affects the entire process. After Fini() is called,
// Init() cannot be called again in the same process (sync.Once limitation).
func (p *PQoS) Fini() error {
	p.mu.Lock()
	defer p.mu.Unlock()

	if !p.initialized {
		return ErrNotInitialized
	}

	ret := C.pqos_fini()
	if ret != C.PQOS_RETVAL_OK {
		return retvalToError(ret, "pqos_fini")
	}

	p.initialized = false
	return nil
}

// IsInitialized returns whether the library has been initialized
func (p *PQoS) IsInitialized() bool {
	p.mu.Lock()
	defer p.mu.Unlock()
	return p.initialized
}

// GetInterface returns the currently active PQoS interface
func (p *PQoS) GetInterface() (int, error) {
	p.mu.Lock()
	defer p.mu.Unlock()

	if !p.initialized {
		return 0, ErrNotInitialized
	}

	var iface C.enum_pqos_interface
	ret := C.pqos_inter_get(&iface)
	if ret != C.PQOS_RETVAL_OK {
		return 0, retvalToError(ret, "pqos_inter_get")
	}

	return int(iface), nil
}

// retvalToError converts a C return value to a Go error with context
func retvalToError(retval C.int, function string) error {
	var msg string
	switch retval {
	case C.PQOS_RETVAL_OK:
		return nil
	case C.PQOS_RETVAL_ERROR:
		msg = "generic error"
	case C.PQOS_RETVAL_PARAM:
		msg = "parameter error"
	case C.PQOS_RETVAL_RESOURCE:
		msg = "resource error"
	case C.PQOS_RETVAL_INIT:
		msg = "initialization error"
	case C.PQOS_RETVAL_TRANSPORT:
		msg = "transport error"
	case C.PQOS_RETVAL_PERF_CTR:
		msg = "performance counter error"
	case C.PQOS_RETVAL_BUSY:
		msg = "resource busy"
	case C.PQOS_RETVAL_INTER:
		msg = "interface not supported"
	case C.PQOS_RETVAL_OVERFLOW:
		msg = "data overflow"
	default:
		msg = fmt.Sprintf("unknown error code %d", retval)
	}
	return fmt.Errorf("%s failed: %s", function, msg)
}

// InterfaceToString converts an interface constant to a string name
func InterfaceToString(iface int) string {
	switch iface {
	case InterMSR:
		return "MSR"
	case InterOS:
		return "OS"
	case InterOSResctrlMon:
		return "OS_RESCTRL_MON"
	case InterAuto:
		return "AUTO"
	default:
		return fmt.Sprintf("UNKNOWN(%d)", iface)
	}
}

// VerbosityToString converts a verbosity level to a string name
func VerbosityToString(verbose int) string {
	switch verbose {
	case LogVerbositySilent:
		return "SILENT"
	case LogVerbosityDefault:
		return "DEFAULT"
	case LogVerbosityVerbose:
		return "VERBOSE"
	case LogVerbositySuperVerbose:
		return "SUPER_VERBOSE"
	default:
		return fmt.Sprintf("UNKNOWN(%d)", verbose)
	}
}

// safeCString converts a Go string to a C string, ensuring proper cleanup
func safeCString(s string) *C.char {
	return C.CString(s)
}

// freeCString frees a C string
func freeCString(s *C.char) {
	C.free(unsafe.Pointer(s))
}
