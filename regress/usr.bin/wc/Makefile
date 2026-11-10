# regress/usr.bin/wc: the tests of wc; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/wc

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_wc.sh

.PHONY: test
