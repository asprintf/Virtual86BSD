# regress/bin/hostname: the tests of hostname; the program is built in PROG_DIR

PROG_DIR = ../../../bin/hostname

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_hostname.sh

.PHONY: test
