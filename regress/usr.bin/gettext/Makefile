# regress/usr.bin/gettext: the tests of gettext; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/gettext

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_gettext.sh

difftest:
	cd $(PROG_DIR) && $(MAKE)
	sh ../../lib/testlib/tl.sh tests/gettextdiff.tl $(PROG_DIR)/gettext 1 500
	sh ../../lib/testlib/tl.sh tests/gettextdiff.tl $(PROG_DIR)/gettext 2 500

.PHONY: test difftest
