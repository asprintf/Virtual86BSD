# regress/sbin/dumpfs: the tests of dumpfs; the program is built in PROG_DIR,
# and so are newfs and fsck_ffs, which make and check the images

PROG_DIR = ../../../sbin/dumpfs

test:
	cd $(PROG_DIR) && $(MAKE)
	cd $(PROG_DIR)/../newfs && $(MAKE)
	cd $(PROG_DIR)/../fsck_ffs && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_dumpfs.sh

.PHONY: test
