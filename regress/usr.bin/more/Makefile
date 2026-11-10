# regress/usr.bin/more: the tests of more; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/more

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_more.sh

.PHONY: test
