# regress/usr.bin/pathchk: the tests of pathchk; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/pathchk

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_pathchk.sh

.PHONY: test
