# regress/bin/expr: the tests of expr; the program is built in PROG_DIR

PROG_DIR = ../../../bin/expr

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_expr.sh

.PHONY: test
