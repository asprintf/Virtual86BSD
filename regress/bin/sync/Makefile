# regress/bin/sync: the tests of sync; the program is built in PROG_DIR

PROG_DIR = ../../../bin/sync

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_sync.sh

.PHONY: test
