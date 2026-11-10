# regress/usr.bin/logger: the tests of logger; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/logger

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_logger.sh

.PHONY: test
