# regress/sbin/halt: the tests of halt; the program is built in PROG_DIR

PROG_DIR = ../../../sbin/halt

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_halt.sh

.PHONY: test
