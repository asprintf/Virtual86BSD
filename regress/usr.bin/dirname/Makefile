# regress/usr.bin/dirname: the tests of dirname; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/dirname

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_dirname.sh

.PHONY: test
