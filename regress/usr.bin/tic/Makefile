# regress/usr.bin/tic: the tests of tic; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/tic

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_tic.sh

.PHONY: test
