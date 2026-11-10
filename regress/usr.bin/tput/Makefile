# regress/usr.bin/tput: the tests of tput; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/tput

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_tput.sh

difftest:
	cd $(PROG_DIR) && $(MAKE)
	sh ../../lib/testlib/tl.sh tests/tputdiff.tl $(PROG_DIR)/tput

.PHONY: test difftest
