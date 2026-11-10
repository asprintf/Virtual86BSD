# lex - generate programs for lexical tasks (POSIX.1-2024)
#
# Targets: all (the default), test, difftest, install, clean.

.POSIX:

CFLAGS   = -O2
LDFLAGS  =
PREFIX   = /usr/local
BINDIR   = $(PREFIX)/bin
LIBDIR   = $(PREFIX)/lib

# STDFLAGS: more options for the compiler, none needed (the sources are
# C11 with POSIX interfaces); with gcc or clang, for example,
# STDFLAGS="-std=c11 -Wall -Wextra".  CC is make's own (POSIX: c17).
STDFLAGS =
LDLIBS   =

PROG     = lex
OBJS     = main.o scan.o re.o dfa.o gen.o util.o
LIB      = libl.a
LIBOBJS  = libl/main.o libl/yywrap.o

all: $(PROG) $(LIB)

$(PROG): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

$(LIB): $(LIBOBJS)
	rm -f $@
	ar -rc $@ $(LIBOBJS)

libl/main.o: libl/main.c
	$(CC) $(CFLAGS) -c -o libl/main.o libl/main.c

libl/yywrap.o: libl/yywrap.c
	$(CC) $(CFLAGS) -c -o libl/yywrap.o libl/yywrap.c

$(OBJS): lex.h

.c.o:
	$(CC) $(STDFLAGS) $(CFLAGS) -c $<

test: all
	cd ../../regress/usr.bin/lex && $(MAKE) test

# compares with other implementations; needs
#   flex
difftest: all
	cd ../../regress/usr.bin/lex && $(MAKE) difftest

install: all
	mkdir -p $(DESTDIR)$(BINDIR)
	cp $(PROG) $(DESTDIR)$(BINDIR)/$(PROG)
	mkdir -p $(DESTDIR)$(LIBDIR)
	cp $(LIB) $(DESTDIR)$(LIBDIR)/$(LIB)

clean:
	rm -f $(PROG) $(OBJS) $(LIB) $(LIBOBJS)

.PHONY: all test difftest install clean
