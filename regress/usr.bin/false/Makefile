# regress/usr.bin/false: the tests of false; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/false

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_false.sh

.PHONY: test
