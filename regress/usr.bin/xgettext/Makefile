# regress/usr.bin/xgettext: the tests of xgettext; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/xgettext

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_xgettext.sh

difftest:
	cd $(PROG_DIR) && $(MAKE)
	sh ../../lib/testlib/tl.sh tests/xgettextdiff.tl $(PROG_DIR)/xgettext 1 300
	sh ../../lib/testlib/tl.sh tests/xgettextdiff.tl $(PROG_DIR)/xgettext 2 300

.PHONY: test difftest
