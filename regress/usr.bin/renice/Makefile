# regress/usr.bin/renice: the tests of renice; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/renice

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_renice.sh

.PHONY: test
