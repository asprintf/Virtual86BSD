# regress/usr.bin/tl: the tests of tl; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/tl

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_tl.sh

.PHONY: test
