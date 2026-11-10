# regress/usr.bin/split: the tests of split; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/split

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_split.sh

.PHONY: test
