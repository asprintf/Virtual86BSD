# uux - remote command execution (POSIX.1-2024)
#
# Targets: all (the default), test, difftest, install, clean.
#
# uucico (as in ../uucp, which installs it) is made for the tests, not installed.

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

PROG     = uux
OBJS     = uux.o uucomm.o
UUCICO_OBJS = uucico.o uucomm.o

all: $(PROG) uucico

$(PROG): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

uucico: $(UUCICO_OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(UUCICO_OBJS) $(LDLIBS)

uux.o uucico.o uucomm.o: ../../lib/libxcu/uucomm.h

.c.o:
	$(CC) $(STDFLAGS) $(CFLAGS) -I../../lib/libxcu -c $<

uucico.o: ../../lib/libxcu/uucico.c
	$(CC) $(STDFLAGS) $(CFLAGS) -I../../lib/libxcu -c ../../lib/libxcu/uucico.c

uucomm.o: ../../lib/libxcu/uucomm.c
	$(CC) $(STDFLAGS) $(CFLAGS) -I../../lib/libxcu -c ../../lib/libxcu/uucomm.c

test: all
	cd ../../regress/usr.bin/uux && $(MAKE) test

difftest:
	@echo "uux: no differential tests"

install: all
	mkdir -p $(DESTDIR)$(BINDIR)
	cp $(PROG) $(DESTDIR)$(BINDIR)/$(PROG)

clean:
	rm -f $(PROG) $(OBJS) uucico uucico.o

.PHONY: all test difftest install clean
