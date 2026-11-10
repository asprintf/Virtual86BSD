# regress/usr.bin/awk: the tests of awk; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/awk

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_awk.sh

difftest:
	cd $(PROG_DIR) && $(MAKE)
	sh ./tests/diff.sh

.PHONY: test difftest
