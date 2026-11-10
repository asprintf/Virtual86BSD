# regress/usr.bin/crontab: the tests of crontab; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/crontab

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_crontab.sh

.PHONY: test
