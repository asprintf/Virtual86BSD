# regress/usr.bin/true: the tests of true; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/true

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_true.sh

.PHONY: test
