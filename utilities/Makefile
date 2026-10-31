# posixutilities - utilities of POSIX.1-2024 (XCU), each in its own directory
#
#   make              build all of them
#   make test         build, then run every test suite (run_tests.sh)
#   make difftest     compare with other implementations (see each README.md)
#   make install      install into $(PREFIX)/bin (and libl.a, liby.a into $(PREFIX)/lib)
#   make clean        remove everything that was built
#
# Macros given on the command line (CC=, CFLAGS=, PREFIX=, DESTDIR=) reach
# every directory.  One utility: cd sed && make.

.POSIX:

UTILS = ar asa at awk basename bc c17 cal cat cflow chgrp chmod chown cksum \
	cmp comm compress cp crontab csplit ctags cut cxref date dd df diff \
	dirname du echo ed env expand expr false file find fold fuser gencat \
	getconf gettext grep head iconv id ipcrm ipcs join lex link ln locale \
	localedef logger logname lp ls m4 mailx make man mesg mkdir mkfifo more \
	msgfmt mv newgrp nice nl nm nohup od paste patch pathchk pax pr printf \
	ps pwd readlink realpath renice rm rmdir sccs sed sh sleep sort split \
	strings strip stty tabs tail talk tee test time timeout touch tput tr \
	true tsort tty uname unexpand uniq unlink uucp uudecode uuencode uustat \
	uux vi wc who write xargs xgettext yacc

all:
	@for u in $(UTILS); do \
	    (cd $$u && $(MAKE)) || exit 1; \
	done

test: all
	sh ./run_tests.sh

difftest: all
	@for u in $(UTILS); do \
	    (cd $$u && $(MAKE) difftest) || exit 1; \
	done

install: all
	@for u in $(UTILS); do \
	    (cd $$u && $(MAKE) install) || exit 1; \
	done

clean:
	@for u in $(UTILS); do \
	    (cd $$u && $(MAKE) clean) || exit 1; \
	done

.PHONY: all test difftest install clean
