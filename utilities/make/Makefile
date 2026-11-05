# make - maintain, update, and regenerate files (POSIX.1-2024)
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
