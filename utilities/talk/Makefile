# talk - talk to another user (POSIX.1-2024)
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
