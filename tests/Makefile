


.PHONY: setup build run run.os run.msr style

test_env: requirements.txt
	@bash -c "virtualenv -p python3 test_env"
	@bash -c "source test_env/bin/activate && \
		pip install -r requirements.txt && \
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


style: test_env
	@bash -c "source test_env/bin/activate && \
		pylint *.py plugins/*.py testcases/*.py testlib/*.py; \
		deactivate"
