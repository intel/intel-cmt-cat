.PHONY: install uninstall test coverage style clean

install:
	@bash -c "python -m pip wheel --wheel-dir=/tmp . && \
		python -m pip install --no-index --find-links=/tmp pqos && \
		rm -rf /tmp/pqos*.whl"

uninstall:
	@bash -c "python -m pip uninstall pqos"

test_env:
	@bash -c "virtualenv -p python3 test_env"
	@bash -c "source test_env/bin/activate && \
		python -m pip install -r requirements.txt && \
		deactivate"

test: test_env
	@bash -c "source test_env/bin/activate && \
		python -m coverage run --source pqos \
			-m unittest discover pqos/test -v; \
		deactivate"

coverage: test_env
	@bash -c "source test_env/bin/activate && \
		python -m coverage report -m \
			--omit test_env/*.py,pqos/test/*.py,setup.py; \
		deactivate"

pylint: test_env
	@bash -c "source test_env/bin/activate && \
		python -m pylint pqos/*.py pqos/test/*.py && \
		deactivate"

bandit: test_env
	@bash -c "source test_env/bin/activate && \
		python -m bandit *.py pqos/*.py && \
		deactivate"

CODESPELL?=codespell
.PHONY: codespell
codespell:
	$(CODESPELL) . --skip ./test_env -q 2

style:
	${MAKE} pylint
	${MAKE} bandit
	${MAKE} codespell

clean:
	@rm -rf test_env
	@rm -rf pqos/__pycache__ pqos/*.pyc
	@rm -rf pqos/test/__pycache__ pqos/test/*.pyc
	@rm -f .coverage
