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

LIBDIR ?= ../lib
PYTHON_INCLUDES?=$(shell python2.7-config --includes)
INCLUDES=$(PYTHON_INCLUDES)
LIBS=-lpython2.7
CCFLAGS=-shared -fPIC -I$(LIBDIR)
LDFLAGS=-L$(LIBDIR)

all: pqos_api

pqos_api:
	$(CC) $(CCFLAGS) $(LDFLAGS) $(INCLUDES) $(LIBS) -lpqos -o pqosapi.so pqosapi.c

clean:
	-$(RM) -f pqosapi.so

.PHONY: test cov style pylint_style pqos_api_style virtual_env
.PHONY: cppcheck pqos_api_cppcheck

test: pqos_api
	PYTHONPATH=. pytest tests/

cov: pqos_api
	PYTHONPATH=. pytest --cov-report term-missing --cov-config .coveragerc --cov . tests/

style: pqos_api_style pylint_style

virtual_env:
	@bash -c "virtualenv $(CURDIR)/virtual_env"
	@bash -c "source $(CURDIR)/virtual_env/bin/activate && \
		python -m pip install -r $(CURDIR)/requirements.txt && \
		deactivate"

pylint_style: virtual_env
	@bash -c "source $(CURDIR)/virtual_env/bin/activate && \
		pylint --rcfile=$(CURDIR)/.pylintrc *.py && \
		deactivate"

CHECKPATCH?=checkpatch.pl

pqos_api_style:
	$(CHECKPATCH) --no-tree --no-signoff --emacs \
	--ignore CODE_INDENT,LEADING_SPACE -f pqosapi.c

CPPCHECK?=cppcheck
cppcheck: pqos_api_cppcheck

pqos_api_cppcheck:
	$(CPPCHECK) --enable=all --std=c99 --template=gcc --suppress=unusedFunction pqosapi.c
