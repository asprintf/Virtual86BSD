# ctags - create a tags file (POSIX.1-2024)
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

PROG     = ctags
OBJS     = ctags.o cpp.o cparse.o

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
	sh ./test_ctags.sh

# compares with other implementations; needs
#   Universal Ctags (on the C files of these utilities)
difftest: all
	sh ./tests/ctagsdiff.sh ./ctags ../*/*.c

install: all
	mkdir -p $(DESTDIR)$(BINDIR)
	cp $(PROG) $(DESTDIR)$(BINDIR)/$(PROG)

clean:
	rm -f $(PROG) $(OBJS)

.PHONY: all test difftest install clean
