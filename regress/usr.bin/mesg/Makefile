# regress/usr.bin/mesg: the tests of mesg; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/mesg

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_mesg.sh

difftest:
	cd $(PROG_DIR) && $(MAKE)
	sh ./tests/mesgdiff.sh $(PROG_DIR)/mesg

.PHONY: test difftest
