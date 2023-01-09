
LIB_DIR=$(CURDIR)/../lib

.PHONY: run style clean

all:
	$(MAKE) -C $(LIB_DIR)
	$(MAKE) -C mock
	$(MAKE) -C lib
	$(MAKE) -C output
	$(MAKE) -C pqos

run:
	$(MAKE) -C lib run
	$(MAKE) -C pqos run

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
