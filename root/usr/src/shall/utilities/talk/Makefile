# talk - talk to another user (POSIX.1-2024)
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

PROG     = talk
OBJS     = talk.o utx.o terminfo.o

all: $(PROG)

$(PROG): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

$(OBJS): ../lib/utx.h ../lib/port.h
talk.o terminfo.o: ../lib/terminfo.h

.c.o:
	$(CC) $(STDFLAGS) $(CFLAGS) -I../lib -c $<

terminfo.o: ../lib/terminfo.c
	$(CC) $(STDFLAGS) $(CFLAGS) -I../lib -c ../lib/terminfo.c

utx.o: ../lib/utx.c
	$(CC) $(STDFLAGS) $(CFLAGS) -I../lib -c ../lib/utx.c

test: all
	sh ./test_talk.sh

difftest:
	@echo "talk: no differential tests"

install: all
	mkdir -p $(DESTDIR)$(BINDIR)
	cp $(PROG) $(DESTDIR)$(BINDIR)/$(PROG)

clean:
	rm -f $(PROG) $(OBJS)

.PHONY: all test difftest install clean
