# Makefile for Intel(R) RDT PQoS Go bindings
#
# Copyright(c) 2025 Zededa, Inc.
# SPDX-License-Identifier: BSD-3-Clause

.PHONY: all build test clean examples help install-lib check-deps

# Go parameters
GOCMD=go
GOBUILD=$(GOCMD) build
GOCLEAN=$(GOCMD) clean
GOTEST=$(GOCMD) test
GOGET=$(GOCMD) get
GOMOD=$(GOCMD) mod
GOFMT=$(GOCMD) fmt

# Directories
PKG_DIR=./pqos
EXAMPLES_DIR=./examples
LIB_DIR=../

# Library paths
LIBPQOS_PATH=/usr/local/lib
LIBPQOS_INCLUDE=/usr/local/include

# CGO flags
export CGO_LDFLAGS=-L$(LIBPQOS_PATH) -lpqos
export CGO_CFLAGS=-I$(LIBPQOS_INCLUDE)
export LD_LIBRARY_PATH=$(LIBPQOS_PATH):$$LD_LIBRARY_PATH

all: check-deps build examples

help:
	@echo "Intel(R) RDT PQoS Go Bindings Makefile"
	@echo ""
	@echo "Targets:"
	@echo "  all          - Check dependencies, build package and examples"
	@echo "  build        - Build the Go package"
	@echo "  examples     - Build all examples"
	@echo "  test         - Run tests"
	@echo "  clean        - Remove built binaries"
	@echo "  install-lib  - Build and install libpqos C library"
	@echo "  check-deps   - Check if libpqos is installed"
	@echo "  fmt          - Format Go code"
	@echo "  tidy         - Tidy go.mod"
	@echo "  help         - Show this help message"
	@echo ""
	@echo "Examples:"
	@echo "  make build              - Build the package"
	@echo "  make examples           - Build all examples"
	@echo "  make test               - Run tests"
	@echo "  sudo make install-lib   - Install libpqos (requires root)"

check-deps:
	@echo "Checking dependencies..."
	@if [ ! -f "$(LIBPQOS_PATH)/libpqos.so.6" ]; then \
		echo "ERROR: libpqos not found at $(LIBPQOS_PATH)"; \
		echo "Please run 'sudo make install-lib' to install it"; \
		exit 1; \
	fi
	@if [ ! -f "$(LIBPQOS_INCLUDE)/pqos.h" ]; then \
		echo "ERROR: pqos.h not found at $(LIBPQOS_INCLUDE)"; \
		echo "Please run 'sudo make install-lib' to install it"; \
		exit 1; \
	fi
	@echo "Dependencies OK"

build: check-deps
	@echo "Building pqos package..."
	cd $(PKG_DIR) && $(GOBUILD) -v

examples: check-deps
	@echo "Building examples..."
	@for dir in $(EXAMPLES_DIR)/*/ ; do \
		if [ -f "$$dir/main.go" ]; then \
			echo "Building $$dir..."; \
			cd "$$dir" && $(GOBUILD) -o $$(basename $$dir) || exit 1; \
			cd - > /dev/null; \
		fi \
	done
	@echo "Examples built successfully"

test: check-deps
	@echo "Running tests..."
	cd $(PKG_DIR) && $(GOTEST) -v

fmt:
	@echo "Formatting Go code..."
	$(GOFMT) ./...

tidy:
	@echo "Tidying go.mod..."
	$(GOMOD) tidy

clean:
	@echo "Cleaning..."
	$(GOCLEAN)
	@for dir in $(EXAMPLES_DIR)/*/ ; do \
		if [ -f "$$dir/main.go" ]; then \
			rm -f "$$dir/$$(basename $$dir)"; \
		fi \
	done
	@echo "Clean complete"

install-lib:
	@echo "Building and installing libpqos..."
	@if [ "$$(id -u)" != "0" ]; then \
		echo "ERROR: This target must be run as root (use sudo)"; \
		exit 1; \
	fi
	cd $(LIB_DIR) && $(MAKE) clean && $(MAKE) && $(MAKE) install
	@echo "libpqos installed successfully"
	@echo "You may need to run: sudo ldconfig"

# Development targets
dev-setup: install-lib tidy
	@echo "Development environment ready"

run-capability: examples
	@echo "Running capability example..."
	@if [ "$$(id -u)" != "0" ]; then \
		echo "Note: This may require root privileges"; \
		echo "Try: sudo make run-capability"; \
		echo ""; \
	fi
	$(EXAMPLES_DIR)/capability/capability

# Check if running with sufficient privileges
check-priv:
	@if [ "$$(id -u)" != "0" ]; then \
		echo "WARNING: Not running as root. Some operations may fail."; \
		echo "Consider running with: sudo make <target>"; \
	fi
