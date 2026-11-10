# sccs - the SCCS utilities: admin, delta, get, prs, rmdel, sact, sccs, unget, val, what (POSIX.1-2024)
#
# Targets: all (the default), test, difftest, install, clean.
#
# One program, sccs; the others are links to it (it acts by its name).

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

PROG     = sccs
OBJS     = main.o sfile.o ldiff.o admin.o delta.o get.o prs.o rmdel.o sact.o \
	   unget.o val.o what.o front.o

all: $(PROG) admin delta get prs rmdel sact unget val what

$(PROG): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

admin: $(PROG)
	rm -f admin
	ln -s $(PROG) admin

delta: $(PROG)
	rm -f delta
	ln -s $(PROG) delta

get: $(PROG)
	rm -f get
	ln -s $(PROG) get

prs: $(PROG)
	rm -f prs
	ln -s $(PROG) prs

rmdel: $(PROG)
	rm -f rmdel
	ln -s $(PROG) rmdel

sact: $(PROG)
	rm -f sact
	ln -s $(PROG) sact

unget: $(PROG)
	rm -f unget
	ln -s $(PROG) unget

val: $(PROG)
	rm -f val
	ln -s $(PROG) val

what: $(PROG)
	rm -f what
	ln -s $(PROG) what

$(OBJS): sccs.h

.c.o:
	$(CC) $(STDFLAGS) $(CFLAGS) -c $<

test: all
	cd ../../regress/usr.bin/sccs && $(MAKE) test

# compares with other implementations; needs
#   CSSC (its programs and its sccs)
difftest: all
	cd ../../regress/usr.bin/sccs && $(MAKE) difftest

install: all
	mkdir -p $(DESTDIR)$(BINDIR)
	cp $(PROG) $(DESTDIR)$(BINDIR)/$(PROG)
	rm -f $(DESTDIR)$(BINDIR)/admin
	ln -s $(PROG) $(DESTDIR)$(BINDIR)/admin
	rm -f $(DESTDIR)$(BINDIR)/delta
	ln -s $(PROG) $(DESTDIR)$(BINDIR)/delta
	rm -f $(DESTDIR)$(BINDIR)/get
	ln -s $(PROG) $(DESTDIR)$(BINDIR)/get
	rm -f $(DESTDIR)$(BINDIR)/prs
	ln -s $(PROG) $(DESTDIR)$(BINDIR)/prs
	rm -f $(DESTDIR)$(BINDIR)/rmdel
	ln -s $(PROG) $(DESTDIR)$(BINDIR)/rmdel
	rm -f $(DESTDIR)$(BINDIR)/sact
	ln -s $(PROG) $(DESTDIR)$(BINDIR)/sact
	rm -f $(DESTDIR)$(BINDIR)/unget
	ln -s $(PROG) $(DESTDIR)$(BINDIR)/unget
	rm -f $(DESTDIR)$(BINDIR)/val
	ln -s $(PROG) $(DESTDIR)$(BINDIR)/val
	rm -f $(DESTDIR)$(BINDIR)/what
	ln -s $(PROG) $(DESTDIR)$(BINDIR)/what

clean:
	rm -f $(PROG) $(OBJS) admin delta get prs rmdel sact unget val what

.PHONY: all test difftest install clean
