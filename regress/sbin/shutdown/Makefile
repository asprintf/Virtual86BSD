# regress/sbin/shutdown: the tests of shutdown; the program is built in PROG_DIR

PROG_DIR = ../../../sbin/shutdown

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_shutdown.sh

.PHONY: test
