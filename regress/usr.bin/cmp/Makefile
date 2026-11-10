# regress/usr.bin/cmp: the tests of cmp; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/cmp

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_cmp.sh

.PHONY: test
