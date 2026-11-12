# regress/usr.sbin/makefs: the tests of makefs; the program is built in
# PROG_DIR, and so are fsck_ffs and dumpfs, which look at the images

PROG_DIR = ../../../usr.sbin/makefs

test:
	cd $(PROG_DIR) && $(MAKE)
	cd $(PROG_DIR)/../../sbin/fsck_ffs && $(MAKE)
	cd $(PROG_DIR)/../../sbin/dumpfs && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_makefs.sh

.PHONY: test
