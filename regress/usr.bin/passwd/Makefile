# regress/usr.bin/passwd: the tests of passwd; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/passwd

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_passwd.sh

.PHONY: test
