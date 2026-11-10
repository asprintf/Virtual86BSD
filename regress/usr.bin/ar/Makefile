# regress/usr.bin/ar: the tests of ar; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/ar

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_ar.sh

difftest:
	cd $(PROG_DIR) && $(MAKE)
	sh ../../lib/testlib/tl.sh tests/ardiff.tl $(PROG_DIR)/ar

.PHONY: test difftest
