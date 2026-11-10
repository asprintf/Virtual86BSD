# usr.bin - the other user commands (POSIX.1-2024 XCU), each in its own directory
#
#   make              build all of them
#   make test         build, then run every test suite (../regress/run_tests.sh)
#   make difftest     compare with other implementations (see each README.md)
#   make install      install into $(DESTDIR)$(BINDIR)
#   make clean        remove everything that was built
#
# Macros given on the command line (CC=, CFLAGS=, PREFIX=, BINDIR=, DESTDIR=)
# reach every directory.  One utility: cd sed && make.

.POSIX:

PREFIX = /usr/local
BINDIR = $(PREFIX)/bin

SUBDIR = ar asa at awk basename bc c17 cal cflow cksum cmp comm \
	compress crontab csplit ctags cut cxref diff dirname du env \
	expand false file find fold fuser gencat getconf gettext grep \
	head iconv id ipcrm ipcs join lex link locale localedef logger \
	logname lp m4 mailx make man mesg mkfifo more msgfmt newgrp nice \
	nl nm nohup od paste patch pathchk pr printf readlink realpath \
	renice sccs sed sort split strings strip tabs tail talk tee time \
	timeout tl touch tput tr true tsort tty uname unexpand uniq \
	unlink uucp uudecode uuencode uustat uux vi wc who write xargs \
	xgettext yacc

all:
	@for u in $(SUBDIR); do \
	    (cd $$u && $(MAKE)) || exit 1; \
	done

test: all
	sh ../regress/run_tests.sh $(SUBDIR)

difftest: all
	@for u in $(SUBDIR); do \
	    (cd $$u && $(MAKE) difftest) || exit 1; \
	done

install: all
	@for u in $(SUBDIR); do \
	    (cd $$u && $(MAKE) BINDIR=$(BINDIR) install) || exit 1; \
	done

clean:
	@for u in $(SUBDIR); do \
	    (cd $$u && $(MAKE) clean) || exit 1; \
	done

.PHONY: all test difftest install clean
