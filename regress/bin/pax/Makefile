# regress/bin/pax: the tests of pax; the program is built in PROG_DIR

PROG_DIR = ../../../bin/pax

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_pax.sh

difftest:
	cd $(PROG_DIR) && $(MAKE)
	sh ./tests/interop.sh $(PROG_DIR)/pax

.PHONY: test difftest
