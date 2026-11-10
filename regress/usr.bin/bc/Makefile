# regress/usr.bin/bc: the tests of bc; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/bc

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_bc.sh

difftest:
	cd $(PROG_DIR) && $(MAKE)
	sh ../../lib/testlib/tl.sh tests/difftest.tl $(PROG_DIR)/bc
	sh ../../lib/testlib/tl.sh tests/randbc.tl $(PROG_DIR)/bc 1 300

.PHONY: test difftest
