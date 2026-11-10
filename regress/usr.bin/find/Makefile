# regress/usr.bin/find: the tests of find; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/find

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_find.sh

difftest:
	cd $(PROG_DIR) && $(MAKE)
	sh ../../lib/testlib/tl.sh tests/rfind.tl $(PROG_DIR)/find 1 1000

.PHONY: test difftest
