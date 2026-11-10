# regress/bin/mkdir: the tests of mkdir; the program is built in PROG_DIR

PROG_DIR = ../../../bin/mkdir

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_mkdir.sh

.PHONY: test
