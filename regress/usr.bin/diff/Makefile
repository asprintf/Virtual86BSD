# regress/usr.bin/diff: the tests of diff; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/diff

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_diff.sh

difftest:
	cd $(PROG_DIR) && $(MAKE)
	sh ../../lib/testlib/tl.sh tests/rdiff.tl $(PROG_DIR)/diff 1 1000

.PHONY: test difftest
