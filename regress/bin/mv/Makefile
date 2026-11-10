# regress/bin/mv: the tests of mv; the program is built in PROG_DIR

PROG_DIR = ../../../bin/mv

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_mv.sh

.PHONY: test
