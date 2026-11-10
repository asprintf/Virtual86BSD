# regress/usr.bin/uustat: the tests of uustat; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/uustat

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_uustat.sh

.PHONY: test
