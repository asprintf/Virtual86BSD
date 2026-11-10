# tabs - set terminal tabs (POSIX.1-2024)
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

PROG     = tabs
OBJS     = tabs.o terminfo.o

all: $(PROG)

$(PROG): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

$(OBJS): ../../lib/libxcu/terminfo.h

.c.o:
	$(CC) $(STDFLAGS) $(CFLAGS) -I../../lib/libxcu -c $<

terminfo.o: ../../lib/libxcu/terminfo.c
	$(CC) $(STDFLAGS) $(CFLAGS) -I../../lib/libxcu -c ../../lib/libxcu/terminfo.c

test: all
	cd ../../regress/usr.bin/tabs && $(MAKE) test

# compares with other implementations; needs
#   ncurses tabs
difftest: all
	cd ../../regress/usr.bin/tabs && $(MAKE) difftest

install: all
	mkdir -p $(DESTDIR)$(BINDIR)
	cp $(PROG) $(DESTDIR)$(BINDIR)/$(PROG)

clean:
	rm -f $(PROG) $(OBJS)

.PHONY: all test difftest install clean
