# regress/usr.bin/od: the tests of od; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/od

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_od.sh

difftest:
	cd $(PROG_DIR) && $(MAKE)
	sh ./tests/oddiff.sh $(PROG_DIR)/od

.PHONY: test difftest
