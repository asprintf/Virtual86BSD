# regress/usr.bin/gencat: the tests of gencat; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/gencat

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_gencat.sh

difftest:
	cd $(PROG_DIR) && $(MAKE)
	sh ../../lib/testlib/tl.sh tests/gencatdiff.tl $(PROG_DIR)/gencat 1 300
	sh ../../lib/testlib/tl.sh tests/gencatdiff.tl $(PROG_DIR)/gencat 2 300

.PHONY: test difftest
