# regress/usr.bin/mkfifo: the tests of mkfifo; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/mkfifo

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_mkfifo.sh

.PHONY: test
