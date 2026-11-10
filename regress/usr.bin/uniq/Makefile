# regress/usr.bin/uniq: the tests of uniq; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/uniq

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_uniq.sh

.PHONY: test
