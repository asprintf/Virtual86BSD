# regress/usr.bin/tr: the tests of tr; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/tr

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_tr.sh

.PHONY: test
