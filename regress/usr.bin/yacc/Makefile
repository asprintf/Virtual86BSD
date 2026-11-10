# regress/usr.bin/yacc: the tests of yacc; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/yacc

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_yacc.sh

difftest:
	cd $(PROG_DIR) && $(MAKE)
	sh ../../lib/testlib/tl.sh tests/specdiff.tl $(PROG_DIR)/yacc 300
	sh ../../lib/testlib/tl.sh tests/randgram.tl $(PROG_DIR)/yacc 1 100

.PHONY: test difftest
