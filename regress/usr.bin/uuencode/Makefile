# regress/usr.bin/uuencode: the tests of uuencode; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/uuencode

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_uuencode.sh

.PHONY: test
