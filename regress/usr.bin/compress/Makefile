# regress/usr.bin/compress: the tests of compress; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/compress

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_compress.sh

difftest:
	cd $(PROG_DIR) && $(MAKE)
	sh ../../lib/testlib/tl.sh tests/compressdiff.tl $(PROG_DIR)/compress

.PHONY: test difftest
