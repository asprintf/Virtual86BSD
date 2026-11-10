# regress/usr.bin/cal: the tests of cal; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/cal

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_cal.sh

difftest:
	cd $(PROG_DIR) && $(MAKE)
	sh ./tests/caldiff.sh $(PROG_DIR)/cal

.PHONY: test difftest
