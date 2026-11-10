# regress/usr.bin/xargs: the tests of xargs; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/xargs

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_xargs.sh

.PHONY: test
