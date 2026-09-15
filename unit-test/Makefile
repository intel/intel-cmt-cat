include ../pre-build.mk

LIB_DIR=$(CURDIR)/../lib

.PHONY: run style clean

all:
	$(MAKE) -C $(LIB_DIR)
	$(MAKE) -C mock
	$(MAKE) -C lib
	$(MAKE) -C output
	$(MAKE) -C pqos

# both directories are run before the result is reported, for the reason each
# of them runs all of its own binaries: a failure in one is not a reason to
# stop learning what the other would have said. The target still fails if
# either did
run:
	@failed=""; \
	$(MAKE) -C lib run || failed="$$failed lib"; \
	$(MAKE) -C pqos run || failed="$$failed pqos"; \
	if [ -n "$$failed" ]; then \
		printf 'FAILED in:%s\n' "$$failed"; \
		exit 1; \
	fi

run.lib:
	$(MAKE) -C lib run

style:
	$(MAKE) -C lib style
	$(MAKE) -C mock style
	$(MAKE) -C output style
	$(MAKE) -C pqos style

clean:
	$(MAKE) -C mock clean
	$(MAKE) -C lib clean
	$(MAKE) -C output clean
	$(MAKE) -C pqos clean
