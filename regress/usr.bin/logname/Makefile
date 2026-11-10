# regress/usr.bin/logname: the tests of logname; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/logname

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_logname.sh

.PHONY: test
