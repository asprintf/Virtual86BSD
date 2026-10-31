# uux - remote command execution (POSIX.1-2024)
#
# Targets: all (the default), test, difftest, install, clean.
#
# uucico (as in ../uucp, which installs it) is made for the tests, not installed.

.POSIX:

CC       = cc
CFLAGS   = -O2
LDFLAGS  =
PREFIX   = /usr/local
BINDIR   = $(PREFIX)/bin

# required by the sources: the language standard; and the warnings
STDFLAGS = -std=c11 -Wall -Wextra
LDLIBS   =

PROG     = uux
OBJS     = uux.o uucomm.o
UUCICO_OBJS = uucico.o uucomm.o

all: $(PROG) uucico

$(PROG): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

uucico: $(UUCICO_OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(UUCICO_OBJS) $(LDLIBS)

uux.o uucico.o uucomm.o: ../lib/uucomm.h

.c.o:
	$(CC) $(STDFLAGS) $(CFLAGS) -I../lib -c $<

uucico.o: ../lib/uucico.c
	$(CC) $(STDFLAGS) $(CFLAGS) -I../lib -c ../lib/uucico.c

uucomm.o: ../lib/uucomm.c
	$(CC) $(STDFLAGS) $(CFLAGS) -I../lib -c ../lib/uucomm.c

test: all
	sh ./test_uux.sh

difftest:
	@echo "uux: no differential tests"

install: all
	mkdir -p $(DESTDIR)$(BINDIR)
	cp $(PROG) $(DESTDIR)$(BINDIR)/$(PROG)

clean:
	rm -f $(PROG) $(OBJS) uucico uucico.o

.PHONY: all test difftest install clean
