# gettext - retrieve text string from messages object (with ngettext) (POSIX.1-2024)
#
# Targets: all (the default), test, difftest, install, clean.
#
# ngettext is a link to gettext (it acts by its name).

.POSIX:

CFLAGS   = -O2
LDFLAGS  =
PREFIX   = /usr/local
BINDIR   = $(PREFIX)/bin

# STDFLAGS: more options for the compiler, none needed (the sources are
# C11 with POSIX interfaces); with gcc or clang, for example,
# STDFLAGS="-std=c11 -Wall -Wextra".  CC is make's own (POSIX: c17).
STDFLAGS =
LDLIBS   =

PROG     = gettext
OBJS     = gettext.o mo.o plural.o cesc.o

all: $(PROG) ngettext

$(PROG): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

ngettext: $(PROG)
	rm -f ngettext
	ln -s $(PROG) ngettext

$(OBJS): mo.h ../lib/plural.h ../lib/cesc.h

.c.o:
	$(CC) $(STDFLAGS) $(CFLAGS) -I../lib -c $<

cesc.o: ../lib/cesc.c
	$(CC) $(STDFLAGS) $(CFLAGS) -I../lib -c ../lib/cesc.c

plural.o: ../lib/plural.c
	$(CC) $(STDFLAGS) $(CFLAGS) -I../lib -c ../lib/plural.c

test: all
	sh ./test_gettext.sh

# compares with other implementations; needs
#   GNU gettext, ngettext and msgfmt, and localedef
difftest: all
	sh ../testlib/tl.sh tests/gettextdiff.tl ./gettext 1 500
	sh ../testlib/tl.sh tests/gettextdiff.tl ./gettext 2 500

install: all
	mkdir -p $(DESTDIR)$(BINDIR)
	cp $(PROG) $(DESTDIR)$(BINDIR)/$(PROG)
	rm -f $(DESTDIR)$(BINDIR)/ngettext
	ln -s $(PROG) $(DESTDIR)$(BINDIR)/ngettext

clean:
	rm -f $(PROG) $(OBJS) ngettext

.PHONY: all test difftest install clean
