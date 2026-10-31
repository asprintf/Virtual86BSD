# strip - remove unnecessary information from strippable files (POSIX.1-2024)
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

PROG     = strip
OBJS     = strip.o arch.o elf.o

all: $(PROG)

$(PROG): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

$(OBJS): ../lib/arch.h ../lib/elf.h

.c.o:
	$(CC) $(STDFLAGS) $(CFLAGS) -I../lib -c $<

arch.o: ../lib/arch.c
	$(CC) $(STDFLAGS) $(CFLAGS) -I../lib -c ../lib/arch.c

elf.o: ../lib/elf.c
	$(CC) $(STDFLAGS) $(CFLAGS) -I../lib -c ../lib/elf.c

test: all
	sh ./test_strip.sh

# compares with other implementations; needs
#   GNU strip and nm, and cc
difftest: all
	sh ./tests/stripdiff.sh ./strip

install: all
	mkdir -p $(DESTDIR)$(BINDIR)
	cp $(PROG) $(DESTDIR)$(BINDIR)/$(PROG)

clean:
	rm -f $(PROG) $(OBJS)

.PHONY: all test difftest install clean
