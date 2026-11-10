# regress/usr.bin/tee: the tests of tee; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/tee

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_tee.sh

.PHONY: test
