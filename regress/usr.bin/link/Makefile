# regress/usr.bin/link: the tests of link; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/link

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_link.sh

.PHONY: test
