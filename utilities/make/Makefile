# make - maintain, update, and regenerate files (POSIX.1-2024)
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

PROG     = make
OBJS     = main.o util.o var.o parse.o node.o build.o ar.o

all: $(PROG)

$(PROG): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

$(OBJS): make.h ../lib/port.h

.c.o:
	$(CC) $(STDFLAGS) $(CFLAGS) -I../lib -c $<

test: all
	sh ./test_make.sh

# compares with other implementations; needs
#   GNU make
difftest: all
	sh ../testlib/tl.sh tests/difftest.tl ./make

install: all
	mkdir -p $(DESTDIR)$(BINDIR)
	cp $(PROG) $(DESTDIR)$(BINDIR)/$(PROG)

clean:
	rm -f $(PROG) $(OBJS)

.PHONY: all test difftest install clean
