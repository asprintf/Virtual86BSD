# regress/usr.bin/unexpand: the tests of unexpand; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/unexpand

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_unexpand.sh

.PHONY: test
