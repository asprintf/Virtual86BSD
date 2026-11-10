# regress/bin/ls: the tests of ls; the program is built in PROG_DIR

PROG_DIR = ../../../bin/ls

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_ls.sh

.PHONY: test
