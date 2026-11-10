# uustat - uucp status enquiry and job control (POSIX.1-2024)
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

PROG     = uustat
OBJS     = uustat.o uucomm.o

all: $(PROG)

$(PROG): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

$(OBJS): ../../lib/libxcu/uucomm.h

.c.o:
	$(CC) $(STDFLAGS) $(CFLAGS) -I../../lib/libxcu -c $<

uucomm.o: ../../lib/libxcu/uucomm.c
	$(CC) $(STDFLAGS) $(CFLAGS) -I../../lib/libxcu -c ../../lib/libxcu/uucomm.c

test: all
	cd ../../regress/usr.bin/uustat && $(MAKE) test

difftest:
	@echo "uustat: no differential tests"

install: all
	mkdir -p $(DESTDIR)$(BINDIR)
	cp $(PROG) $(DESTDIR)$(BINDIR)/$(PROG)

clean:
	rm -f $(PROG) $(OBJS)

.PHONY: all test difftest install clean
