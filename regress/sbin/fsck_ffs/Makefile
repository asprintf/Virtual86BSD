# regress/sbin/fsck_ffs: the tests of fsck_ffs; the program is built in
# PROG_DIR, and so are newfs and dumpfs, which make and look at the images

PROG_DIR = ../../../sbin/fsck_ffs

test:
	cd $(PROG_DIR) && $(MAKE)
	cd $(PROG_DIR)/../newfs && $(MAKE)
	cd $(PROG_DIR)/../dumpfs && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_fsck_ffs.sh

.PHONY: test
