# msgfmt - create messages objects from portable messages object source files (POSIX.1-2024)
#
# Targets: all (the default), test, difftest, install, clean.

.POSIX:

CC       = cc
CFLAGS   = -O2
LDFLAGS  =
PREFIX   = /usr/local
BINDIR   = $(PREFIX)/bin

# required by the sources: the language standard; and the warnings
STDFLAGS = -std=c11 -Wall -Wextra
LDLIBS   =

PROG     = msgfmt
OBJS     = msgfmt.o po.o plural.o cesc.o

all: $(PROG)

$(PROG): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

$(OBJS): ../lib/po.h ../lib/plural.h ../lib/cesc.h

.c.o:
	$(CC) $(STDFLAGS) $(CFLAGS) -I../lib -c $<

cesc.o: ../lib/cesc.c
	$(CC) $(STDFLAGS) $(CFLAGS) -I../lib -c ../lib/cesc.c

plural.o: ../lib/plural.c
	$(CC) $(STDFLAGS) $(CFLAGS) -I../lib -c ../lib/plural.c

po.o: ../lib/po.c
	$(CC) $(STDFLAGS) $(CFLAGS) -I../lib -c ../lib/po.c

test: all
	sh ./test_msgfmt.sh

# compares with other implementations; needs
#   GNU msgfmt
difftest: all
	sh ../testlib/tl.sh tests/msgfmtdiff.tl ./msgfmt 1 300
	sh ../testlib/tl.sh tests/msgfmtdiff.tl ./msgfmt 2 300

install: all
	mkdir -p $(DESTDIR)$(BINDIR)
	cp $(PROG) $(DESTDIR)$(BINDIR)/$(PROG)

clean:
	rm -f $(PROG) $(OBJS)

.PHONY: all test difftest install clean
