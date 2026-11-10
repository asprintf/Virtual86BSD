# regress/usr.bin/ctags: the tests of ctags; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/ctags

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_ctags.sh

difftest:
	cd $(PROG_DIR) && $(MAKE)
	sh ./tests/ctagsdiff.sh $(PROG_DIR)/ctags ../../../usr.bin/*/*.c

.PHONY: test difftest
