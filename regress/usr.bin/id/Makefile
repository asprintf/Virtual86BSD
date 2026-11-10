# regress/usr.bin/id: the tests of id; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/id

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_id.sh

difftest:
	cd $(PROG_DIR) && $(MAKE)
	sh ./tests/iddiff.sh $(PROG_DIR)/id

.PHONY: test difftest
