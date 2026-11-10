# regress/usr.bin/comm: the tests of comm; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/comm

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_comm.sh

.PHONY: test
