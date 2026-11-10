# regress/usr.bin/cflow: the tests of cflow; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/cflow

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_cflow.sh

difftest:
	cd $(PROG_DIR) && $(MAKE)
	for p in awk bc dd ed find lex m4 make sed vi yacc; do \
	  echo "cflowdiff: $$p"; sh ./tests/cflowdiff.sh $(PROG_DIR)/cflow ../$$p/*.c || exit 1; done

.PHONY: test difftest
