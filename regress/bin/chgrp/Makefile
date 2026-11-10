# regress/bin/chgrp: the tests of chgrp; the program is built in PROG_DIR

PROG_DIR = ../../../bin/chgrp

test:
	cd $(PROG_DIR) && $(MAKE)
	PROG_DIR=`cd $(PROG_DIR) && pwd` sh ./test_chgrp.sh

.PHONY: test
