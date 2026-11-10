# localedef - define locale environment (POSIX.1-2024)
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

PROG     = localedef
OBJS     = localedef.o lex.o charmap.o simple.o ctype.o collate.o

all: $(PROG)

$(PROG): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

$(OBJS): localedef.h

.c.o:
	$(CC) $(STDFLAGS) $(CFLAGS) -c $<

test: all
	cd ../../regress/usr.bin/localedef && $(MAKE) test

# compares with other implementations; needs
#   the GNU C library's localedef, its locale sources and charmaps
difftest: all
	cd ../../regress/usr.bin/localedef && $(MAKE) difftest

install: all
	mkdir -p $(DESTDIR)$(BINDIR)
	cp $(PROG) $(DESTDIR)$(BINDIR)/$(PROG)

clean:
	rm -f $(PROG) $(OBJS)

.PHONY: all test difftest install clean
