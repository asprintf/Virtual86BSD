# regress/sbin/sysctl: the tests of sysctl; the program is built in PROG_DIR

PROG_DIR = ../../../sbin/sysctl

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_sysctl.sh

.PHONY: test
