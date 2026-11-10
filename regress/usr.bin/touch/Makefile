# regress/usr.bin/touch: the tests of touch; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/touch

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_touch.sh

.PHONY: test
