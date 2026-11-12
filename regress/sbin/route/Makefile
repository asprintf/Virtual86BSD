# regress/sbin/route: the tests of route; the program is built in PROG_DIR

PROG_DIR = ../../../sbin/route

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_route.sh

.PHONY: test
