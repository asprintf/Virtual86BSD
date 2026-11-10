# regress/usr.bin/nm: the tests of nm; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/nm

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_nm.sh

difftest:
	cd $(PROG_DIR) && $(MAKE)
	sh ./tests/nmdiff.sh $(PROG_DIR)/nm

.PHONY: test difftest
