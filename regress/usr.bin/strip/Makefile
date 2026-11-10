# regress/usr.bin/strip: the tests of strip; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/strip

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_strip.sh

difftest:
	cd $(PROG_DIR) && $(MAKE)
	sh ./tests/stripdiff.sh $(PROG_DIR)/strip

.PHONY: test difftest
