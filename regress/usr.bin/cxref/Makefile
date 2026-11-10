# regress/usr.bin/cxref: the tests of cxref; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/cxref

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_cxref.sh

.PHONY: test
