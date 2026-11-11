# regress/libexec/getty: the tests of getty; the program is built in PROG_DIR

PROG_DIR = ../../../libexec/getty

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_getty.sh

.PHONY: test
