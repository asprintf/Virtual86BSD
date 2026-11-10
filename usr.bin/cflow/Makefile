# cflow - generate a C-language flowgraph (POSIX.1-2024)
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

PROG     = cflow
OBJS     = cflow.o cpp.o cparse.o elf.o

all: $(PROG)

$(PROG): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

$(OBJS): ../../lib/libxcu/cpp.h ../../lib/libxcu/cparse.h ../../lib/libxcu/elf.h

.c.o:
	$(CC) $(STDFLAGS) $(CFLAGS) -I../../lib/libxcu -c $<

cparse.o: ../../lib/libxcu/cparse.c
	$(CC) $(STDFLAGS) $(CFLAGS) -I../../lib/libxcu -c ../../lib/libxcu/cparse.c

cpp.o: ../../lib/libxcu/cpp.c ../../lib/libxcu/port.h
	$(CC) $(STDFLAGS) $(CFLAGS) -I../../lib/libxcu -c ../../lib/libxcu/cpp.c

elf.o: ../../lib/libxcu/elf.c
	$(CC) $(STDFLAGS) $(CFLAGS) -I../../lib/libxcu -c ../../lib/libxcu/elf.c

test: all
	cd ../../regress/usr.bin/cflow && $(MAKE) test

# compares with other implementations; needs
#   GNU cflow (on the programs awk, bc, dd, ed, find, lex, m4, make, sed, vi, yacc)
difftest: all
	cd ../../regress/usr.bin/cflow && $(MAKE) difftest

install: all
	mkdir -p $(DESTDIR)$(BINDIR)
	cp $(PROG) $(DESTDIR)$(BINDIR)/$(PROG)

clean:
	rm -f $(PROG) $(OBJS)

.PHONY: all test difftest install clean
