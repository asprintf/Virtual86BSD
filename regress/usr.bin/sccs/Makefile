# regress/usr.bin/sccs: the tests of sccs; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/sccs

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_sccs.sh

difftest:
	cd $(PROG_DIR) && $(MAKE)
	sh ../../lib/testlib/tl.sh tests/csscdiff.tl . 1 100
	sh ../../lib/testlib/tl.sh tests/csscdiff.tl . 2 100

.PHONY: test difftest
