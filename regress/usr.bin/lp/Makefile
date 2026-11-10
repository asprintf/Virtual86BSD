# regress/usr.bin/lp: the tests of lp; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/lp

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_lp.sh

.PHONY: test
