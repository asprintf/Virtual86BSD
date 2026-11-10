# regress/usr.bin/nice: the tests of nice; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/nice

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_nice.sh

.PHONY: test
