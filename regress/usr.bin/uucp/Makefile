# regress/usr.bin/uucp: the tests of uucp; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/uucp

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_uucp.sh

.PHONY: test
