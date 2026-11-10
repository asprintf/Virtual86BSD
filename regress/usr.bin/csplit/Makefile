# regress/usr.bin/csplit: the tests of csplit; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/csplit

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_csplit.sh

.PHONY: test
