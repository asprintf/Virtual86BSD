# regress/usr.bin/time: the tests of time; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/time

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_time.sh

.PHONY: test
