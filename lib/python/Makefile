.PHONY: install uninstall test coverage style clean

install:
	@bash -c "python -m pip install ."

uninstall:
	@bash -c "python -m pip uninstall pqos"

test_env:
	@bash -c "virtualenv -p python3 test_env"
	@bash -c "source test_env/bin/activate && \
		python -m pip install -r requirements.txt && \
		deactivate"

test: test_env
	@bash -c "source test_env/bin/activate && \
		python -m coverage run --source . \
			-m unittest discover pqos/test -v; \
		deactivate"

coverage: test_env
	@bash -c "source test_env/bin/activate && \
		python -m coverage report -m \
			--omit test_env/*.py,pqos/test/*.py,setup.py; \
		deactivate"

style: test_env
	@bash -c "source test_env/bin/activate && \
		python -m pylint pqos/*.py pqos/test/*.py; \
		deactivate"

clean:
	@rm -rf test_env
	@rm -rf pqos/__pycache__ pqos/*.pyc
	@rm -rf pqos/test/__pycache__ pqos/test/*.pyc
	@rm -f .coverage
