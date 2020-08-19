###############################################################################
# Makefile script for AppQoS POC
#
# @par
# BSD LICENSE
#
# Copyright(c) 2019-2020 Intel Corporation. All rights reserved.
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

VENV_DIR=env_$(shell hostname)

.PHONY: test cov style run

test: $(VENV_DIR)
	@bash -c "source $(CURDIR)/$(VENV_DIR)/bin/activate && \
		python -m pytest tests/ && \
		deactivate"

cov: $(VENV_DIR)
	@bash -c "source $(CURDIR)/$(VENV_DIR)/bin/activate && \
		python -m pytest --cov-report term-missing --cov-config .coveragerc --cov . tests/ && \
		deactivate"

$(VENV_DIR): requirements.txt
	@bash -c "virtualenv -p python3 $(CURDIR)/$(VENV_DIR)"
	@bash -c "source $(CURDIR)/$(VENV_DIR)/bin/activate && \
		python -m pip install -r $(CURDIR)/requirements.txt && \
		python -m pip wheel --wheel-dir=$(CURDIR) $(CURDIR)/../lib/python && \
		python -m pip install --no-index --find-links=$(CURDIR) pqos && \
		rm -rf $(CURDIR)/pqos*.whl && \
		deactivate"

pylint: $(VENV_DIR)
	@bash -c "source $(CURDIR)/$(VENV_DIR)/bin/activate && \
		python -m pylint --rcfile=$(CURDIR)/.pylintrc *.py && \
		deactivate"

bandit: $(VENV_DIR)
	@bash -c "source $(CURDIR)/$(VENV_DIR)/bin/activate && \
		python -m bandit *.py &&\
		deactivate"

CODESPELL?=codespell
.PHONY: codespell
codespell:
	$(CODESPELL) . --skip 'env_*' -L unning -q 2

style:
	${MAKE} pylint
	${MAKE} bandit
	${MAKE} codespell

run: $(VENV_DIR)
	@bash -c "source $(CURDIR)/$(VENV_DIR)/bin/activate && \
		./appqos.py && \
		deactivate"

clean:
	rm -rf $(VENV_DIR)
