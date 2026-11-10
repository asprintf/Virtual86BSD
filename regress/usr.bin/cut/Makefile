# regress/usr.bin/cut: the tests of cut; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/cut

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_cut.sh

.PHONY: test
