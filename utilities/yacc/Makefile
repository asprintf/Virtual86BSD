# yacc - yet another compiler compiler (POSIX.1-2024)
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

PROG     = yacc
OBJS     = main.o reader.o lalr.o tables.o output.o verbose.o util.o
LIB      = liby.a
LIBOBJS  = liby/main.o liby/yyerror.o

all: $(PROG) $(LIB)

$(PROG): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

$(LIB): $(LIBOBJS)
	rm -f $@
	ar -rc $@ $(LIBOBJS)

liby/main.o: liby/main.c
	$(CC) $(CFLAGS) -c -o liby/main.o liby/main.c

liby/yyerror.o: liby/yyerror.c
	$(CC) $(CFLAGS) -c -o liby/yyerror.o liby/yyerror.c

$(OBJS): yacc.h

.c.o:
	$(CC) $(STDFLAGS) $(CFLAGS) -c $<

test: all
	sh ./test_yacc.sh

# compares with other implementations; needs
#   bison and byacc
difftest: all
	sh ../testlib/tl.sh tests/specdiff.tl ./yacc 300
	sh ../testlib/tl.sh tests/randgram.tl ./yacc 1 100

install: all
	mkdir -p $(DESTDIR)$(BINDIR)
	cp $(PROG) $(DESTDIR)$(BINDIR)/$(PROG)
	mkdir -p $(DESTDIR)$(LIBDIR)
	cp $(LIB) $(DESTDIR)$(LIBDIR)/$(LIB)

clean:
	rm -f $(PROG) $(OBJS) $(LIB) $(LIBOBJS)

.PHONY: all test difftest install clean
