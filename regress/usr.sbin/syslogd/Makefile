# regress/usr.sbin/syslogd: the tests of syslogd; the program is built in PROG_DIR

PROG_DIR = ../../../usr.sbin/syslogd

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_syslogd.sh

.PHONY: test
