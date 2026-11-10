# regress/usr.bin/ipcrm: the tests of ipcrm; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/ipcrm

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_ipcrm.sh

.PHONY: test
