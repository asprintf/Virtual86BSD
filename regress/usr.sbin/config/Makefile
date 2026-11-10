# regress/usr.sbin/config: the tests of config; the program is built in PROG_DIR

PROG_DIR = ../../../usr.sbin/config

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_config.sh

.PHONY: test
