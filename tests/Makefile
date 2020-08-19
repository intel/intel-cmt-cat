


.PHONY: setup build run run.os run.msr style

test_env: requirements.txt
	@bash -c "virtualenv -p python3 $(CURDIR)/test_env"
	@bash -c "source $(CURDIR)/test_env/bin/activate && \
		python -m pip install -r $(CURDIR)/requirements.txt && \
		deactivate"

build: test_env

clean:
	@rm -rf test_env
	@rm -rf __pycache__
	@rm -rf test/__pycache__
	@rm -rf test/*.pyc
	@rm -rf testlib/__pycache__
	@rm -rf testlib/*.pyc
	@rm -rf plugins/__pycache__
	@rm -rf plugins/*.pyc

pylint: test_env
	@bash -c "source $(CURDIR)/test_env/bin/activate && \
		python -m pylint *.py plugins/*.py testcases/*.py testlib/*.py && \
		deactivate"

bandit: test_env
	@bash -c "source $(CURDIR)/test_env/bin/activate && \
		python -m bandit -s B101,B404,B603 *.py plugins/*.py testcases/*.py testlib/*.py; \
		deactivate"

CODESPELL?=codespell
.PHONY: codespell
codespell:
	$(CODESPELL) . --skip ./test_env -q 2

style:
	${MAKE} pylint
	${MAKE} bandit
	${MAKE} codespell
