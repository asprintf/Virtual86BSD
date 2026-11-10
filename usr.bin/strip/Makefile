# strip - remove unnecessary information from strippable files (POSIX.1-2024)
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

PROG     = strip
OBJS     = strip.o arch.o elf.o

all: $(PROG)

$(PROG): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

$(OBJS): ../../lib/libxcu/arch.h ../../lib/libxcu/elf.h

.c.o:
	$(CC) $(STDFLAGS) $(CFLAGS) -I../../lib/libxcu -c $<

arch.o: ../../lib/libxcu/arch.c
	$(CC) $(STDFLAGS) $(CFLAGS) -I../../lib/libxcu -c ../../lib/libxcu/arch.c

elf.o: ../../lib/libxcu/elf.c
	$(CC) $(STDFLAGS) $(CFLAGS) -I../../lib/libxcu -c ../../lib/libxcu/elf.c

test: all
	cd ../../regress/usr.bin/strip && $(MAKE) test

# compares with other implementations; needs
#   GNU strip and nm, and cc
difftest: all
	cd ../../regress/usr.bin/strip && $(MAKE) difftest

install: all
	mkdir -p $(DESTDIR)$(BINDIR)
	cp $(PROG) $(DESTDIR)$(BINDIR)/$(PROG)

clean:
	rm -f $(PROG) $(OBJS)

.PHONY: all test difftest install clean
