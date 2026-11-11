# regress/sbin/mount: the tests of mount; the program is built in PROG_DIR

PROG_DIR = ../../../sbin/mount

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_mount.sh

.PHONY: test
