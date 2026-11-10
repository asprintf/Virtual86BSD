# regress/usr.bin/uudecode: the tests of uudecode; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/uudecode

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_uudecode.sh

.PHONY: test
