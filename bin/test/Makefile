# test - evaluate expression (POSIX.1-2024)
#
# Targets: all (the default), test, difftest, install, clean.
#
# [ is a link to test (it acts by its name).  As "test" is the target
# that runs the tests, the program is made with the file prog.made, which
# records when it was linked, and [ by the target lbracket.

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

PROG     = test
OBJS     = test.o

all: prog.made lbracket

prog.made: $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(PROG) $(OBJS) $(LDLIBS)
	: > prog.made

lbracket: prog.made
	rm -f '['
	ln -s $(PROG) '['

.c.o:
	$(CC) $(STDFLAGS) $(CFLAGS) -c $<

test: all
	cd ../../regress/bin/test && $(MAKE) test

difftest:
	@echo "test: no differential tests"

install: all
	mkdir -p $(DESTDIR)$(BINDIR)
	cp $(PROG) $(DESTDIR)$(BINDIR)/$(PROG)
	rm -f $(DESTDIR)$(BINDIR)/'['
	ln -s $(PROG) $(DESTDIR)$(BINDIR)/'['

clean:
	rm -f $(PROG) $(OBJS) '[' prog.made

.PHONY: all test difftest install clean lbracket
