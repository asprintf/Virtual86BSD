# uustat - uucp status enquiry and job control (POSIX.1-2024)
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

PROG     = uustat
OBJS     = uustat.o uucomm.o

all: $(PROG)

$(PROG): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

$(OBJS): ../lib/uucomm.h

.c.o:
	$(CC) $(STDFLAGS) $(CFLAGS) -I../lib -c $<

uucomm.o: ../lib/uucomm.c
	$(CC) $(STDFLAGS) $(CFLAGS) -I../lib -c ../lib/uucomm.c

test: all
	sh ./test_uustat.sh

difftest:
	@echo "uustat: no differential tests"

install: all
	mkdir -p $(DESTDIR)$(BINDIR)
	cp $(PROG) $(DESTDIR)$(BINDIR)/$(PROG)

clean:
	rm -f $(PROG) $(OBJS)

.PHONY: all test difftest install clean
