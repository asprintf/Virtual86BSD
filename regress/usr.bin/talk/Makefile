# regress/usr.bin/talk: the tests of talk; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/talk

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_talk.sh

.PHONY: test
