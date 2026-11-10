# regress/usr.bin/tabs: the tests of tabs; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/tabs

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_tabs.sh

difftest:
	cd $(PROG_DIR) && $(MAKE)
	sh ./tests/tabsdiff.sh $(PROG_DIR)/tabs

.PHONY: test difftest
