# regress/sbin/fsck: the tests of fsck; the program is built in PROG_DIR, and
# so are fsck_ffs's neighbours newfs and dumpfs, which make the images

PROG_DIR = ../../../sbin/fsck

test:
	cd $(PROG_DIR) && $(MAKE)
	cd $(PROG_DIR)/../newfs && $(MAKE)
	cd $(PROG_DIR)/../dumpfs && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_fsck.sh

.PHONY: test
