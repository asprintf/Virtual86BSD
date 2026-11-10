# regress/usr.bin/write: the tests of write; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/write

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_write.sh

.PHONY: test
