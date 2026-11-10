# regress/bin/chmod: the tests of chmod; the program is built in PROG_DIR

PROG_DIR = ../../../bin/chmod

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_chmod.sh

.PHONY: test
