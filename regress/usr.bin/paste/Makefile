# regress/usr.bin/paste: the tests of paste; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/paste

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_paste.sh

.PHONY: test
