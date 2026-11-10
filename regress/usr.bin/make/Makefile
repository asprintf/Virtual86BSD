# regress/usr.bin/make: the tests of make; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/make

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_make.sh

difftest:
	cd $(PROG_DIR) && $(MAKE)
	sh ../../lib/testlib/tl.sh tests/difftest.tl $(PROG_DIR)/make

.PHONY: test difftest
