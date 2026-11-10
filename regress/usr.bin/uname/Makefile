# regress/usr.bin/uname: the tests of uname; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/uname

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_uname.sh

.PHONY: test
