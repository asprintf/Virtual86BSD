# usr.sbin - system administration programs that are not needed to boot or
# repair the system (so not in sbin/), each in its own directory
#
#   make              build all of them
#   make test         build, then run every test suite (../regress/run_tests.sh)
#   make install      install into $(DESTDIR)$(BINDIR)
#   make clean        remove everything that was built
#
# Macros given on the command line (CC=, CFLAGS=, PREFIX=, BINDIR=, DESTDIR=)
# reach every directory.  BINDIR is $(PREFIX)/sbin (/usr/sbin on the system);
# the programs of usr.sbin/ are linked dynamically (see ../distrib/build.sh).
# One program: cd config && make.

.POSIX:

PREFIX = /usr/local
BINDIR = $(PREFIX)/sbin

SUBDIR = config

all:
	@for u in $(SUBDIR); do \
	    (cd $$u && $(MAKE)) || exit 1; \
	done

test: all
	sh ../regress/run_tests.sh $(SUBDIR)

install: all
	@for u in $(SUBDIR); do \
	    (cd $$u && $(MAKE) BINDIR=$(BINDIR) install) || exit 1; \
	done

clean:
	@for u in $(SUBDIR); do \
	    (cd $$u && $(MAKE) clean) || exit 1; \
	done

.PHONY: all test install clean
