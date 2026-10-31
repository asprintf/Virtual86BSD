# cxref - generate a C-language program cross-reference table (POSIX.1-2024)
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

PROG     = cxref
OBJS     = cxref.o cpp.o cparse.o

all: $(PROG)

$(PROG): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

$(OBJS): ../lib/cpp.h ../lib/cparse.h

.c.o:
	$(CC) $(STDFLAGS) $(CFLAGS) -I../lib -c $<

cparse.o: ../lib/cparse.c
	$(CC) $(STDFLAGS) $(CFLAGS) -I../lib -c ../lib/cparse.c

cpp.o: ../lib/cpp.c ../lib/port.h
	$(CC) $(STDFLAGS) $(CFLAGS) -I../lib -c ../lib/cpp.c

test: all
	sh ./test_cxref.sh

difftest:
	@echo "cxref: no differential tests"

install: all
	mkdir -p $(DESTDIR)$(BINDIR)
	cp $(PROG) $(DESTDIR)$(BINDIR)/$(PROG)

clean:
	rm -f $(PROG) $(OBJS)

.PHONY: all test difftest install clean
