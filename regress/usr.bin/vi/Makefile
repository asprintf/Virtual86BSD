# regress/usr.bin/vi: the tests of vi; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/vi

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_vi.sh

difftest:
	cd $(PROG_DIR) && $(MAKE)
	sh ../../lib/testlib/tl.sh tests/exdiff.tl
	sh ../../lib/testlib/tl.sh tests/vidiff.tl
	sh ../../lib/testlib/tl.sh tests/rediff.tl

.PHONY: test difftest
