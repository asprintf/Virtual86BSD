# regress/usr.bin/cksum: the tests of cksum; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/cksum

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_cksum.sh

.PHONY: test
