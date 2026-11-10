# regress/usr.bin/man: the tests of man; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/man

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_man.sh

.PHONY: test
