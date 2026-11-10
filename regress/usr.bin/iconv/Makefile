# regress/usr.bin/iconv: the tests of iconv; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/iconv

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_iconv.sh

difftest:
	cd $(PROG_DIR) && $(MAKE)
	sh ./tests/iconvdiff.sh $(PROG_DIR)/iconv

.PHONY: test difftest
