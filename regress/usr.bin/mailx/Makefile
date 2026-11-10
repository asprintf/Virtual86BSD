# regress/usr.bin/mailx: the tests of mailx; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/mailx

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_mailx.sh

.PHONY: test
