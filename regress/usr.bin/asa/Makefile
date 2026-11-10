# regress/usr.bin/asa: the tests of asa; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/asa

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_asa.sh

.PHONY: test
