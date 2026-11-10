# regress/bin/cat: the tests of cat; the program is built in PROG_DIR

PROG_DIR = ../../../bin/cat

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_cat.sh

.PHONY: test
