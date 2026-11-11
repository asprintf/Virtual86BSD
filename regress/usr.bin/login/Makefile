# regress/usr.bin/login: the tests of login; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/login

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_login.sh

.PHONY: test
