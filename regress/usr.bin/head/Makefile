# regress/usr.bin/head: the tests of head; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/head

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_head.sh

.PHONY: test
