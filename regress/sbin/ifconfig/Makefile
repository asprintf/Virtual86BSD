# regress/sbin/ifconfig: the tests of ifconfig; the program is built in PROG_DIR

PROG_DIR = ../../../sbin/ifconfig

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_ifconfig.sh

.PHONY: test
