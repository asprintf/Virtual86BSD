# regress/usr.bin/unlink: the tests of unlink; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/unlink

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_unlink.sh

.PHONY: test
