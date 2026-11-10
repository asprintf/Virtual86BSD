# regress/usr.bin/localedef: the tests of localedef; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/localedef

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_localedef.sh

difftest:
	cd $(PROG_DIR) && $(MAKE)
	sh ./tests/localedefdiff.sh $(PROG_DIR)/localedef

.PHONY: test difftest
