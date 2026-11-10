# regress/bin/df: the tests of df; the program is built in PROG_DIR

PROG_DIR = ../../../bin/df

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_df.sh

difftest:
	cd $(PROG_DIR) && $(MAKE)
	sh ./tests/dfdiff.sh $(PROG_DIR)/df

.PHONY: test difftest
