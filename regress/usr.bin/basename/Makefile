# regress/usr.bin/basename: the tests of basename; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/basename

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_basename.sh

.PHONY: test
