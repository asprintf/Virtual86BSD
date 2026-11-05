# nm - write the name list of an object file (POSIX.1-2024)
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

PROG     = nm
OBJS     = nm.o arch.o elf.o

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
	sh ./test_nm.sh

# compares with other implementations; needs
#   GNU nm, and cc for the objects
difftest: all
	sh ./tests/nmdiff.sh ./nm

install: all
	mkdir -p $(DESTDIR)$(BINDIR)
	cp $(PROG) $(DESTDIR)$(BINDIR)/$(PROG)

clean:
	rm -f $(PROG) $(OBJS)

.PHONY: all test difftest install clean
