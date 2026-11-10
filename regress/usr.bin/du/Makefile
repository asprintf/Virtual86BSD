# regress/usr.bin/du: the tests of du; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/du

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_du.sh

difftest:
	cd $(PROG_DIR) && $(MAKE)
	sh ./tests/dudiff.sh $(PROG_DIR)/du

.PHONY: test difftest
