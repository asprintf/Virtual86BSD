# regress/sbin/tunefs: the tests of tunefs; the program is built in PROG_DIR,
# and so are newfs, dumpfs and fsck_ffs, which make, show and check the images

PROG_DIR = ../../../sbin/tunefs

test:
	cd $(PROG_DIR) && $(MAKE)
	cd $(PROG_DIR)/../newfs && $(MAKE)
	cd $(PROG_DIR)/../dumpfs && $(MAKE)
	cd $(PROG_DIR)/../fsck_ffs && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_tunefs.sh

.PHONY: test
