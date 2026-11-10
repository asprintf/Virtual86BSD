# regress/usr.bin/c17: the tests of c17; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/c17

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_c17.sh

difftest:
	cd $(PROG_DIR) && $(MAKE)
	sh ./tests/difftest.sh

.PHONY: test difftest
