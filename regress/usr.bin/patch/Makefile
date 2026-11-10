# regress/usr.bin/patch: the tests of patch; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/patch

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_patch.sh

difftest:
	cd $(PROG_DIR) && $(MAKE)
	sh ../../lib/testlib/tl.sh tests/patchdiff.tl $(PROG_DIR)/patch

.PHONY: test difftest
