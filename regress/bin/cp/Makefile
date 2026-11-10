# regress/bin/cp: the tests of cp; the program is built in PROG_DIR

PROG_DIR = ../../../bin/cp

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_cp.sh

.PHONY: test
