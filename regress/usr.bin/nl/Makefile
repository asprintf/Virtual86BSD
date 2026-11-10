# regress/usr.bin/nl: the tests of nl; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/nl

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_nl.sh

.PHONY: test
