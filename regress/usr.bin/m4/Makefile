# regress/usr.bin/m4: the tests of m4; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/m4

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_m4.sh

difftest:
	cd $(PROG_DIR) && $(MAKE)
	sh ./tests/m4diff.sh $(PROG_DIR)/m4

.PHONY: test difftest
