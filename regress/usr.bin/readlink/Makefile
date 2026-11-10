# regress/usr.bin/readlink: the tests of readlink; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/readlink

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_readlink.sh

.PHONY: test
