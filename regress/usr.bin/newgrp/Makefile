# regress/usr.bin/newgrp: the tests of newgrp; the program is built in PROG_DIR

PROG_DIR = ../../../usr.bin/newgrp

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_newgrp.sh

.PHONY: test
