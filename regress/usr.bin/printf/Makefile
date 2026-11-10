# regress/usr.bin/printf: the tests of printf; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/printf

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_printf.sh

.PHONY: test
