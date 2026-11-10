# regress/usr.bin/strings: the tests of strings; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/strings

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_strings.sh

.PHONY: test
