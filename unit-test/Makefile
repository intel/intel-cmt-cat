
LIB_DIR=$(CURDIR)/../lib

.PHONY: run style clean

all:
	#$(MAKE) -C $(LIB_DIR)
	$(MAKE) -C mock
	$(MAKE) -C lib

run:
	$(MAKE) -C lib run

style:
	$(MAKE) -C lib style
	$(MAKE) -C mock style

clean:
	$(MAKE) -C mock clean
	$(MAKE) -C lib clean
