# regress/usr.bin/pr: the tests of pr; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/pr

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_pr.sh

difftest:
	cd $(PROG_DIR) && $(MAKE)
	sh ./tests/prdiff.sh $(PROG_DIR)/pr

.PHONY: test difftest
