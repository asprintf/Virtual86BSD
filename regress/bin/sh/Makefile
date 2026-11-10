# regress/bin/sh: the tests of sh; the program is built in PROG_DIR

PROG_DIR = ../../../bin/sh

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_sh.sh

difftest:
	cd $(PROG_DIR) && $(MAKE)
	sh ../../lib/testlib/tl.sh tests/run_diff.tl

.PHONY: test difftest
