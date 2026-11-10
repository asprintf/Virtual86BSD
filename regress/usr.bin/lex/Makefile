# regress/usr.bin/lex: the tests of lex; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/lex

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_lex.sh

difftest:
	cd $(PROG_DIR) && $(MAKE)
	sh ../../lib/testlib/tl.sh tests/difftest.tl $(PROG_DIR)/lex
	sh ../../lib/testlib/tl.sh tests/randlex.tl $(PROG_DIR)/lex 1 200

.PHONY: test difftest
