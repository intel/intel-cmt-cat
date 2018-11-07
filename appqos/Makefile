###############################################################################
# Makefile script for AppQoS POC
#
# @par
# BSD LICENSE
#
# Copyright(c) 2019 Intel Corporation. All rights reserved.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
#   * Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
#   * Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in
#     the documentation and/or other materials provided with the
#     distribution.
#   * Neither the name of Intel Corporation nor the names of its
#     contributors may be used to endorse or promote products derived
#     from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
###############################################################################

PYTHON_INCLUDES?=$(shell python2.7-config --includes)
INCLUDES=$(PYTHON_INCLUDES)
LIBS=-lpython2.7
CCFLAGS=-shared -fPIC

KERNEL_VER = $(shell uname -r | cut -f1,2 -d'.')
KERNEL_MIN_VER_REQ = $(shell echo $(KERNEL_VER)\>=3.9 | bc )

ifeq ($(KERNEL_MIN_VER_REQ),0)
all:
	@echo "Kernel 3.9 or newer required, skipping"
else
all: perf_api kernel_module
endif

perf_api:
	$(CC) $(CCFLAGS) $(INCLUDES) $(LIBS) -o perfapi.so perfapi.c

kernel_module:
	$(MAKE) -C pid_cache_flush

clean:
	-rm -f perfapi.so
	$(MAKE) -C pid_cache_flush clean

.PHONY: test cov style pylint_style kernel_module_style perf_api_style virtual_env
.PHONY: cppcheck kernel_module_cppcheck perf_api_cppcheck

test: perf_api
	PYTHONPATH=. pytest tests/

cov: perf_api
	PYTHONPATH=. pytest --cov-report term-missing --cov-config .coveragerc --cov . tests/

style: kernel_module_style perf_api_style pylint_style

virtual_env:
	@bash -c "virtualenv $(CURDIR)/virtual_env"
	@bash -c "source $(CURDIR)/virtual_env/bin/activate && \
		python -m pip install -r $(CURDIR)/requirements.txt && \
		deactivate"

pylint_style: virtual_env
	@bash -c "source $(CURDIR)/virtual_env/bin/activate && \
		pylint --rcfile=$(CURDIR)/.pylintrc *.py && \
		deactivate"

kernel_module_style:
	$(MAKE) -C pid_cache_flush style

CHECKPATCH?=checkpatch.pl
perf_api_style:
	$(CHECKPATCH) --no-tree --no-signoff --emacs \
	--ignore CODE_INDENT,LEADING_SPACE -f perfapi.c

CPPCHECK?=cppcheck
cppcheck: kernel_module_cppcheck perf_api_cppcheck

kernel_module_cppcheck:
	$(MAKE) -C pid_cache_flush cppcheck

perf_api_cppcheck:
	$(CPPCHECK) --enable=all --std=c99 --template=gcc --suppress=unusedFunction perfapi.c
