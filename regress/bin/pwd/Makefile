# regress/bin/pwd: the tests of pwd; the program is built in PROG_DIR

PROG_DIR = ../../../bin/pwd

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_pwd.sh

.PHONY: test
