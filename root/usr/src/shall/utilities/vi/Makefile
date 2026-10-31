# vi - screen-oriented (visual) display editor, with ex (POSIX.1-2024)
#
# Targets: all (the default), test, difftest, install, clean.
#
# One binary, ex; vi and view are links to it (it acts by its name).

.POSIX:

CC       = cc
CFLAGS   = -O2
LDFLAGS  =
PREFIX   = /usr/local
BINDIR   = $(PREFIX)/bin

# required by the sources: the language standard; and the warnings
STDFLAGS = -std=c11 -Wall -Wextra
LDLIBS   =

PROG     = ex
OBJS     = main.o util.o buf.o regex.o options.o excmd.o cmds.o exfile.o \
	   term.o screen.o vi.o vinput.o

all: $(PROG) vi view

$(PROG): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

vi: $(PROG)
	rm -f vi
	ln -s $(PROG) vi

view: $(PROG)
	rm -f view
	ln -s $(PROG) view

$(OBJS): ex.h cmd.h vi.h ../lib/port.h

.c.o:
	$(CC) $(STDFLAGS) $(CFLAGS) -I../lib -c $<

test: all
	sh ./test_vi.sh

# compares with other implementations; needs
#   nvi and vim
difftest: all
	sh ../testlib/tl.sh tests/exdiff.tl
	sh ../testlib/tl.sh tests/vidiff.tl
	sh ../testlib/tl.sh tests/rediff.tl

install: all
	mkdir -p $(DESTDIR)$(BINDIR)
	cp $(PROG) $(DESTDIR)$(BINDIR)/$(PROG)
	rm -f $(DESTDIR)$(BINDIR)/vi
	ln -s $(PROG) $(DESTDIR)$(BINDIR)/vi
	rm -f $(DESTDIR)$(BINDIR)/view
	ln -s $(PROG) $(DESTDIR)$(BINDIR)/view

clean:
	rm -f $(PROG) $(OBJS) vi view

.PHONY: all test difftest install clean
