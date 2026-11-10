# regress/usr.bin/at: the tests of at; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/at

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_at.sh

.PHONY: test
