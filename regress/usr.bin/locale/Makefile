# regress/usr.bin/locale: the tests of locale; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/locale

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_locale.sh

difftest:
	cd $(PROG_DIR) && $(MAKE)
	sh ./tests/localediff.sh $(PROG_DIR)/locale

.PHONY: test difftest
