# regress/usr.bin/env: the tests of env; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/env

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_env.sh

.PHONY: test
