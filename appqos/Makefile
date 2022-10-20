###############################################################################
# Makefile script for AppQoS POC
#
# @par
# BSD LICENSE
#
# Copyright(c) 2019-2022 Intel Corporation. All rights reserved.
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

VENV_DIR?=../venv/appqos_$(shell hostname)

.PHONY: test cov style run setup setup-dev

$(VENV_DIR): Pipfile
	WORKON_HOME=$(VENV_DIR) pipenv install --skip-lock

setup:
	$(MAKE) $(VENV_DIR)

setup-dev: Pipfile
	WORKON_HOME=$(VENV_DIR) pipenv install --dev --skip-lock

test: $(VENV_DIR)
	WORKON_HOME=$(VENV_DIR) PYTHONPATH=. pipenv run python3 -m pytest tests

cov: $(VENV_DIR)
	WORKON_HOME=$(VENV_DIR) PYTHONPATH=. pipenv run python3 -m pytest --cov-report term-missing --cov-config .coveragerc --cov . tests

pylint:
	WORKON_HOME=$(VENV_DIR) pipenv run python3 -m pylint --rcfile=$(CURDIR)/.pylintrc ./appqos ./tests

bandit:
	WORKON_HOME=$(VENV_DIR) pipenv run python3 -m bandit -r ./appqos

.PHONY: codespell
codespell:
	WORKON_HOME=$(VENV_DIR) pipenv run codespell -L unit --skip ca -q 3

.PHONY: build
build: $(VENV_DIR)
	WORKON_HOME=$(VENV_DIR) pipenv run python3 -m build

style:
	${MAKE} pylint
	${MAKE} bandit
	${MAKE} codespell

run:
	WORKON_HOME=$(VENV_DIR) pipenv run python3 -m appqos

clean:
	@rm -rf __pycache__
	@rm -rf .pytest_cache
	@rm -rf dist
