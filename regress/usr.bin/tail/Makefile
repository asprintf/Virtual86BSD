# regress/usr.bin/tail: the tests of tail; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/tail

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_tail.sh

.PHONY: test
