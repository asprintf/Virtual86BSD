# sbin - system administration commands (POSIX.1-2024 XCU), each in its own directory
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

SUBDIR = chown

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
