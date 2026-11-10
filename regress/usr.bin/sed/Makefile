# regress/usr.bin/sed: the tests of sed; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/sed

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_sed.sh

.PHONY: test
