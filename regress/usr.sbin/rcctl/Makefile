# regress/usr.sbin/rcctl: the tests of rcctl and of etc/rc.d/rc.subr (rcctl is a script, nothing is built)

PROG_DIR = ../../../usr.sbin/rcctl

test:
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_rcctl.sh

.PHONY: test
