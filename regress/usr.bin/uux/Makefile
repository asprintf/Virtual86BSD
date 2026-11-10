# regress/usr.bin/uux: the tests of uux; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/uux

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_uux.sh

.PHONY: test
