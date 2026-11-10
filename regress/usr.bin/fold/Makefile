# regress/usr.bin/fold: the tests of fold; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/fold

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_fold.sh

.PHONY: test
