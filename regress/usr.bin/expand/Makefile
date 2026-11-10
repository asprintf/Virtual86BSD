# regress/usr.bin/expand: the tests of expand; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/expand

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_expand.sh

.PHONY: test
