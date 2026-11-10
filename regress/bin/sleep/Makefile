# regress/bin/sleep: the tests of sleep; the program is built in PROG_DIR

PROG_DIR = ../../../bin/sleep

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_sleep.sh

.PHONY: test
