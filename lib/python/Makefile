###############################################################################
# Makefile script for python wrapper
#
# @par
# BSD LICENSE
#
# Copyright(c) 2020-2022 Intel Corporation. All rights reserved.
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

VENV_DIR?=../../venv/lib_$(shell hostname)

.PHONY: test coverage style clean setup setup-dev

$(VENV_DIR): Pipfile
	WORKON_HOME=$(VENV_DIR) pipenv install --skip-lock

setup:
	$(MAKE) $(VENV_DIR)

setup-dev: Pipfile
	WORKON_HOME=$(VENV_DIR) pipenv install --skip-lock --dev

test: $(VENV_DIR)
	WORKON_HOME=$(VENV_DIR) pipenv run python -m unittest discover pqos/test -v

coverage: $(VENV_DIR)
	WORKON_HOME=$(VENV_DIR) pipenv run python -m coverage run --source pqos -m unittest discover pqos/test
	WORKON_HOME=$(VENV_DIR) pipenv run python -m coverage report -m --omit pqos/test/*.py,setup.py

pylint: $(VENV_DIR)
	WORKON_HOME=$(VENV_DIR) pipenv run python -m pylint pqos/*.py pqos/test/*.py

bandit: $(VENV_DIR)
	WORKON_HOME=$(VENV_DIR) pipenv run python -m bandit *.py pqos/*.py

CODESPELL?=codespell
.PHONY: codespell
codespell:
	$(CODESPELL) . -q 2

style:
	${MAKE} setup-dev
	${MAKE} pylint
	${MAKE} bandit
	${MAKE} codespell

clean:
	@rm -rf pqos/__pycache__ pqos/*.pyc
	@rm -rf pqos/test/__pycache__ pqos/test/*.pyc
	@rm -f .coverage
