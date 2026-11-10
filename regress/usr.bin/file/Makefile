# regress/usr.bin/file: the tests of file; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/file

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_file.sh

difftest:
	cd $(PROG_DIR) && $(MAKE)
	sh ./tests/filediff.sh $(PROG_DIR)/file

.PHONY: test difftest
