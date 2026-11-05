# man - display system documentation (POSIX.1-2024)
#
# Targets: all (the default), test, difftest, install, clean.
#
# The pages are made from the HTML edition of the standard by mkpages.tl
# (make pages SPEC=its directory) and installed with man; they are the
# standard's text, so they are not distributed with these utilities.

.POSIX:

CFLAGS   = -O2
LDFLAGS  =
PREFIX   = /usr/local
BINDIR   = $(PREFIX)/bin
MANDIR   = $(PREFIX)/share/posixutilities/man
SPEC     =

# STDFLAGS: more options for the compiler, none needed (the sources are
# C11 with POSIX interfaces); with gcc or clang, for example,
# STDFLAGS="-std=c11 -Wall -Wextra".  CC is make's own (POSIX: c17).
STDFLAGS =
DEFS     = -DMAN_DIR='"$(MANDIR)"'
LDLIBS   =

PROG     = man
OBJS     = man.o

all: $(PROG)

$(PROG): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

.c.o:
	$(CC) $(STDFLAGS) $(DEFS) $(CFLAGS) -c $<

pages:
	@test -n "$(SPEC)" || { echo "usage: make pages SPEC=the directory of the standard's HTML edition"; exit 2; }
	sh ../testlib/tl.sh mkpages.tl $(SPEC) pages

test: all
	sh ./test_man.sh

difftest:
	@echo "man: no differential tests"

install: all
	mkdir -p $(DESTDIR)$(BINDIR)
	cp $(PROG) $(DESTDIR)$(BINDIR)/$(PROG)
	if test -d pages; then mkdir -p $(DESTDIR)$(MANDIR) && cp pages/* $(DESTDIR)$(MANDIR)/; fi

clean:
	rm -f $(PROG) $(OBJS)
	rm -rf pages

.PHONY: all test difftest install clean pages
