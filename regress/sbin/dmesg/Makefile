# regress/sbin/dmesg: the tests of dmesg; the program is built in PROG_DIR

PROG_DIR = ../../../sbin/dmesg

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_dmesg.sh

.PHONY: test
