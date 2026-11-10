# regress/bin/ln: the tests of ln; the program is built in PROG_DIR

PROG_DIR = ../../../bin/ln

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_ln.sh

.PHONY: test
