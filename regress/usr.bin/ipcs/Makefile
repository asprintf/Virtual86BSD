# regress/usr.bin/ipcs: the tests of ipcs; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/ipcs

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_ipcs.sh

.PHONY: test
