# regress/bin/ed: the tests of ed; the program is built in PROG_DIR

PROG_DIR = ../../../bin/ed

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_ed.sh

difftest:
	cd $(PROG_DIR) && $(MAKE)
	sh ../../lib/testlib/tl.sh tests/difftest.tl $(PROG_DIR)/ed
	sh ../../lib/testlib/tl.sh tests/randdiff.tl $(PROG_DIR)/ed 1 300

.PHONY: test difftest
