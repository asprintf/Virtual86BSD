# regress/usr.bin/sort: the tests of sort; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/sort

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_sort.sh

difftest:
	cd $(PROG_DIR) && $(MAKE)
	sh ../../lib/testlib/tl.sh tests/rsort.tl $(PROG_DIR)/sort 1 1000
	sh ../../lib/testlib/tl.sh tests/rsort.tl $(PROG_DIR)/sort 2 500 UTF8 ascii

.PHONY: test difftest
