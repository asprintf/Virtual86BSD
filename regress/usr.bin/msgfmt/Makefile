# regress/usr.bin/msgfmt: the tests of msgfmt; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/msgfmt

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_msgfmt.sh

difftest:
	cd $(PROG_DIR) && $(MAKE)
	sh ../../lib/testlib/tl.sh tests/msgfmtdiff.tl $(PROG_DIR)/msgfmt 1 300
	sh ../../lib/testlib/tl.sh tests/msgfmtdiff.tl $(PROG_DIR)/msgfmt 2 300

.PHONY: test difftest
