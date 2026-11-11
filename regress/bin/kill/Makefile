# regress/bin/kill: the tests of kill; the program is built in PROG_DIR

PROG_DIR = ../../../bin/kill

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_kill.sh

.PHONY: test
