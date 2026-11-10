# regress/bin/test: the tests of test; the program is built in PROG_DIR

PROG_DIR = ../../../bin/test

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_test.sh

.PHONY: test
