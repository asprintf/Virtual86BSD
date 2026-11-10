# regress/usr.bin/join: the tests of join; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/join

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_join.sh

.PHONY: test
