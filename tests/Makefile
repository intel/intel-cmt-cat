


.PHONY: setup build run run-os run-msr style setup setup-dev

TEST_ENV?=$(shell hostname).json
VENV_DIR?=../venv/test_$(shell hostname)

setup:
	WORKON_HOME=$(VENV_DIR) pipenv install --skip-lock

setup-dev:
	WORKON_HOME=$(VENV_DIR) pipenv install --skip-lock --dev

build: test_env

clean:
	@rm -rf __pycache__
	@rm -rf test/__pycache__
	@rm -rf test/*.pyc
	@rm -rf testlib/__pycache__
	@rm -rf testlib/*.pyc
	@rm -rf plugins/__pycache__
	@rm -rf plugins/*.pyc

pylint:
	WORKON_HOME=$(VENV_DIR) pipenv run python3 -m pylint *.py plugins/*.py testcases/*.py testlib/*.py

bandit:
	WORKON_HOME=$(VENV_DIR) pipenv run python3 -m bandit -s B101,B404,B603 *.py plugins/*.py testcases/*.py testlib/*.py;

.PHONY: codespell
codespell:
	WORKON_HOME=$(VENV_DIR) pipenv run codespell . -L unit,astroid,CLOS -q 2

style:
	${MAKE} pylint
	${MAKE} bandit
	${MAKE} codespell

run:
	WORKON_HOME=$(VENV_DIR) python3 -m pipenv run pytest --env=$(TEST_ENV) testcases

run-os:
	WORKON_HOME=$(VENV_DIR) pipenv run pytest --env=$(TEST_ENV) --iface-os

run-msr:
	WORKON_HOME=$(VENV_DIR) pipenv run pytest --env=$(TEST_ENV) --iface-msr


