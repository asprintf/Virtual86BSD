# regress/bin/echo: the tests of echo; the program is built in PROG_DIR

PROG_DIR = ../../../bin/echo

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_echo.sh

.PHONY: test
