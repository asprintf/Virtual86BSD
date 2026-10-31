# cflow - generate a C-language flowgraph (POSIX.1-2024)
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

PROG     = cflow
OBJS     = cflow.o cpp.o cparse.o elf.o

all: $(PROG)

$(PROG): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

$(OBJS): ../lib/cpp.h ../lib/cparse.h ../lib/elf.h

.c.o:
	$(CC) $(STDFLAGS) $(CFLAGS) -I../lib -c $<

cparse.o: ../lib/cparse.c
	$(CC) $(STDFLAGS) $(CFLAGS) -I../lib -c ../lib/cparse.c

cpp.o: ../lib/cpp.c ../lib/port.h
	$(CC) $(STDFLAGS) $(CFLAGS) -I../lib -c ../lib/cpp.c

elf.o: ../lib/elf.c
	$(CC) $(STDFLAGS) $(CFLAGS) -I../lib -c ../lib/elf.c

test: all
	sh ./test_cflow.sh

# compares with other implementations; needs
#   GNU cflow (on the programs awk, bc, dd, ed, find, lex, m4, make, sed, vi, yacc)
difftest: all
	for p in awk bc dd ed find lex m4 make sed vi yacc; do \
	  echo "cflowdiff: $$p"; sh ./tests/cflowdiff.sh ./cflow ../$$p/*.c || exit 1; done

install: all
	mkdir -p $(DESTDIR)$(BINDIR)
	cp $(PROG) $(DESTDIR)$(BINDIR)/$(PROG)

clean:
	rm -f $(PROG) $(OBJS)

.PHONY: all test difftest install clean
