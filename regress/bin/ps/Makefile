# regress/bin/ps: the tests of ps; the program is built in PROG_DIR

PROG_DIR = ../../../bin/ps

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_ps.sh

difftest:
	cd $(PROG_DIR) && $(MAKE)
	sh ../../lib/testlib/tl.sh tests/psdiff.tl $(PROG_DIR)/ps

.PHONY: test difftest
