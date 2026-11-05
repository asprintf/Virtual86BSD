# tl - the test language: a small interpreted language in the style of C,
# with the interfaces of POSIX and pseudo-terminals as its library
# (not a utility of POSIX: the language the tests of this collection
# are written in, installed as a part of the system)
#
# Targets: all (the default), test, difftest, install, clean.

.POSIX:

CFLAGS   = -O2
LDFLAGS  =
PREFIX   = /usr/local
BINDIR   = $(PREFIX)/bin
MANDIR   = $(PREFIX)/share/posixutilities/man

# STDFLAGS: more options for the compiler, none needed (the sources are
# C99 with POSIX interfaces); with gcc or clang, for example,
# STDFLAGS="-std=c11 -Wall -Wextra".  CC is make's own (POSIX: c17).
STDFLAGS =
LDLIBS   =

PROG     = tl
OBJS     = tl.o

all: $(PROG)

$(PROG): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

.c.o:
	$(CC) $(STDFLAGS) $(CFLAGS) -c $<

test: all
	sh ./test_tl.sh

difftest:
	@echo "tl: no differential tests (there is no other implementation)"

# the page of man is tl.md, as text; whatis gets its line
install: all
	mkdir -p $(DESTDIR)$(BINDIR) $(DESTDIR)$(MANDIR)
	cp $(PROG) $(DESTDIR)$(BINDIR)/$(PROG)
	cp tl.md $(DESTDIR)$(MANDIR)/tl
	w=$(DESTDIR)$(MANDIR)/whatis; \
	{ if test -f $$w; then grep -v '^tl - ' $$w; fi; \
	  echo 'tl - the test language (a scripting language in the style of C)'; } | sort > $$w.new && \
	mv $$w.new $$w

clean:
	rm -f $(PROG) $(OBJS)

.PHONY: all test difftest install clean
