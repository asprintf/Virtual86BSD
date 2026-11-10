# regress/bin/rm: the tests of rm; the program is built in PROG_DIR

PROG_DIR = ../../../bin/rm

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_rm.sh

.PHONY: test
