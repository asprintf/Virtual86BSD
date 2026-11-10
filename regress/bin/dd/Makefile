# regress/bin/dd: the tests of dd; the program is built in PROG_DIR

PROG_DIR = ../../../bin/dd

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_dd.sh

difftest:
	cd $(PROG_DIR) && $(MAKE)
	sh ./tests/dddiff.sh $(PROG_DIR)/dd

.PHONY: test difftest
