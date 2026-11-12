# regress/sbin/newfs: the tests of newfs; the program is built in PROG_DIR,
# and so are fsck_ffs and dumpfs, with which the tests look at the result

PROG_DIR = ../../../sbin/newfs

test:
	cd $(PROG_DIR) && $(MAKE)
	cd $(PROG_DIR)/../fsck_ffs && $(MAKE)
	cd $(PROG_DIR)/../dumpfs && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_newfs.sh

.PHONY: test
