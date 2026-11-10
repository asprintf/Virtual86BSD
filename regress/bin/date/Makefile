# regress/bin/date: the tests of date; the program is built in PROG_DIR

PROG_DIR = ../../../bin/date

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_date.sh

difftest:
	cd $(PROG_DIR) && $(MAKE)
	sh ./tests/datediff.sh $(PROG_DIR)/date

.PHONY: test difftest
