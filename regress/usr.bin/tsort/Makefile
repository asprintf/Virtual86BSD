# regress/usr.bin/tsort: the tests of tsort; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/tsort

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_tsort.sh

.PHONY: test
