# regress/usr.bin/who: the tests of who; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/who

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_who.sh

difftest:
	cd $(PROG_DIR) && $(MAKE)
	sh ./tests/whodiff.sh $(PROG_DIR)/who

.PHONY: test difftest
