# who - display who is on the system (POSIX.1-2024)
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

PROG     = who
OBJS     = who.o utx.o

all: $(PROG)

$(PROG): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

$(OBJS): ../lib/utx.h

.c.o:
	$(CC) $(STDFLAGS) $(CFLAGS) -I../lib -c $<

utx.o: ../lib/utx.c
	$(CC) $(STDFLAGS) $(CFLAGS) -I../lib -c ../lib/utx.c

test: all
	sh ./test_who.sh

# compares with other implementations; needs
#   GNU who
difftest: all
	sh ./tests/whodiff.sh ./who

install: all
	mkdir -p $(DESTDIR)$(BINDIR)
	cp $(PROG) $(DESTDIR)$(BINDIR)/$(PROG)

clean:
	rm -f $(PROG) $(OBJS)

.PHONY: all test difftest install clean
