# regress/usr.bin/grep: the tests of grep; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/grep

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_grep.sh

.PHONY: test
