# regress/usr.bin/timeout: the tests of timeout; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/timeout

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_timeout.sh

.PHONY: test
