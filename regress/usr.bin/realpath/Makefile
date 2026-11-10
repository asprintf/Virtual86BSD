# regress/usr.bin/realpath: the tests of realpath; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/realpath

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_realpath.sh

.PHONY: test
