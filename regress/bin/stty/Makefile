# regress/bin/stty: the tests of stty; the program is built in PROG_DIR

PROG_DIR = ../../../bin/stty

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_stty.sh

.PHONY: test
