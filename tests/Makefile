.PHONY: \
	clean clean-venv \
	setup setup-dev \
	pylint bandit codespell style

TEST_ENV ?= $(shell hostname).json

# Local venv directory used for style tooling (created under tests/).
STYLE_VENV_DIR ?= .style-venv

# If set, use this python interpreter and skip venv create/install/cleanup logic.
# This is the "external path provided" escape hatch.
# Example:
#   make -C tests VENV_PYTHON=/path/to/python style
VENV_PYTHON ?=

# If set to 1, remove STYLE_VENV_DIR after running a command.
# Default is 0 to allow CI to reuse the env created by setup-dev.
STYLE_CLEAN ?= 0

PYLINT_FILES = *.py plugins/*.py testcases/*.py testlib/*.py
BANDIT_FILES = *.py plugins/*.py testcases/*.py testlib/*.py

STYLE_VENV = ./tools/style_venv.py --venv-dir "$(STYLE_VENV_DIR)" --pipfile Pipfile
ifneq ($(strip $(VENV_PYTHON)),)
STYLE_VENV += --python "$(VENV_PYTHON)"
else
# In venv mode, reuse existing venv by default (CI-friendly).
STYLE_VENV += --reuse
endif

ifeq ($(strip $(STYLE_CLEAN)),1)
STYLE_VENV += --cleanup
endif

clean:
	@rm -rf __pycache__ test/__pycache__ test/*.pyc testlib/__pycache__ testlib/*.pyc plugins/__pycache__ plugins/*.pyc

clean-venv:
	@rm -rf $(STYLE_VENV_DIR)

# CI contract: Jenkins calls `make setup-dev` in tests/.
# We create/install and KEEP the venv so subsequent `make style` (from repo root)
# can reuse it without reinstalling.
setup:
	$(STYLE_VENV) --keep --install-only

setup-dev:
	$(STYLE_VENV) --keep --install-only

# Single-tool targets (reuse venv if present)
pylint:
	$(STYLE_VENV) -- sh -lc 'pylint $(PYLINT_FILES)'

bandit:
	$(STYLE_VENV) -- sh -lc 'bandit -s B101,B404,B603 $(BANDIT_FILES)'

codespell:
	$(STYLE_VENV) -- sh -lc 'codespell . --skip=Makefile -L unit,astroid,CLOS,clos -q 2'

# Full style run: create/install once (if needed), then run all tools.
# codespell is intentionally quiet when there are no issues.
style:
	$(STYLE_VENV) -- sh -lc '\
		pylint $(PYLINT_FILES) && \
		bandit -s B101,B404,B603 $(BANDIT_FILES) && \
		codespell . --skip=Makefile -L unit,astroid,CLOS,clos -q 2 \
	'
