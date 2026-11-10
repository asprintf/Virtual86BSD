# regress/usr.bin/nohup: the tests of nohup; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/nohup

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_nohup.sh

.PHONY: test
