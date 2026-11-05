# xgettext - extract gettext call strings from C-language source files (POSIX.1-2024)
#
# Targets: all (the default), test, difftest, install, clean.

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

PROG     = xgettext
OBJS     = xgettext.o po.o cesc.o

all: $(PROG)

$(PROG): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

$(OBJS): ../lib/po.h ../lib/cesc.h

.c.o:
	$(CC) $(STDFLAGS) $(CFLAGS) -I../lib -c $<

cesc.o: ../lib/cesc.c
	$(CC) $(STDFLAGS) $(CFLAGS) -I../lib -c ../lib/cesc.c

po.o: ../lib/po.c
	$(CC) $(STDFLAGS) $(CFLAGS) -I../lib -c ../lib/po.c

test: all
	sh ./test_xgettext.sh

# compares with other implementations; needs
#   GNU xgettext
difftest: all
	sh ../testlib/tl.sh tests/xgettextdiff.tl ./xgettext 1 300
	sh ../testlib/tl.sh tests/xgettextdiff.tl ./xgettext 2 300

install: all
	mkdir -p $(DESTDIR)$(BINDIR)
	cp $(PROG) $(DESTDIR)$(BINDIR)/$(PROG)

clean:
	rm -f $(PROG) $(OBJS)

.PHONY: all test difftest install clean
