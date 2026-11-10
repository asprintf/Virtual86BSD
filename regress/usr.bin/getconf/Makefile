# regress/usr.bin/getconf: the tests of getconf; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/getconf

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_getconf.sh

difftest:
	cd $(PROG_DIR) && $(MAKE)
	sh ./tests/gcdiff.sh $(PROG_DIR)/getconf

.PHONY: test difftest
