# regress/usr.bin/tty: the tests of tty; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/tty

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_tty.sh

.PHONY: test
