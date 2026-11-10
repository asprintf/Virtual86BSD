# regress/usr.bin/fuser: the tests of fuser; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/fuser

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_fuser.sh

difftest:
	cd $(PROG_DIR) && $(MAKE)
	sh ../../lib/testlib/tl.sh tests/fuserdiff.tl $(PROG_DIR)/fuser

.PHONY: test difftest
