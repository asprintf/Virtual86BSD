# uucp - system-to-system copy (with uucico, its other side) (POSIX.1-2024)
#
# Targets: all (the default), test, difftest, install, clean.
#
# uucico, the other side of uucp and uux and the runner of the queued jobs,
# is installed in $(SBINDIR).

.POSIX:

CFLAGS   = -O2
LDFLAGS  =
PREFIX   = /usr/local
BINDIR   = $(PREFIX)/bin
SBINDIR  = $(PREFIX)/sbin

# STDFLAGS: more options for the compiler, none needed (the sources are
# C11 with POSIX interfaces); with gcc or clang, for example,
# STDFLAGS="-std=c11 -Wall -Wextra".  CC is make's own (POSIX: c17).
STDFLAGS =
LDLIBS   =

PROG     = uucp
OBJS     = uucp.o uucomm.o
UUCICO_OBJS = uucico.o uucomm.o

all: $(PROG) uucico

$(PROG): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

uucico: $(UUCICO_OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(UUCICO_OBJS) $(LDLIBS)

uucp.o uucico.o uucomm.o: ../lib/uucomm.h

.c.o:
	$(CC) $(STDFLAGS) $(CFLAGS) -I../lib -c $<

uucico.o: ../lib/uucico.c
	$(CC) $(STDFLAGS) $(CFLAGS) -I../lib -c ../lib/uucico.c

uucomm.o: ../lib/uucomm.c
	$(CC) $(STDFLAGS) $(CFLAGS) -I../lib -c ../lib/uucomm.c

test: all
	sh ./test_uucp.sh

difftest:
	@echo "uucp: no differential tests"

install: all
	mkdir -p $(DESTDIR)$(BINDIR)
	cp $(PROG) $(DESTDIR)$(BINDIR)/$(PROG)
	mkdir -p $(DESTDIR)$(SBINDIR)
	cp uucico $(DESTDIR)$(SBINDIR)/uucico

clean:
	rm -f $(PROG) $(OBJS) uucico uucico.o

.PHONY: all test difftest install clean
