# regress/sbin/chown: the tests of chown; the program is built in PROG_DIR

PROG_DIR = ../../../sbin/chown

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_chown.sh

.PHONY: test
