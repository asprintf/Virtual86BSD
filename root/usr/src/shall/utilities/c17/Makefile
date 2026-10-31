# c17 - compile standard C programs (POSIX.1-2024)
#
# Targets: all (the default), test, difftest, install, clean.
#
# c17 compiles, assembles and link-edits by itself; what it needs besides
# is a C library.  Each C library it can use has a profile in libc/ (its
# headers, its directory, the dynamic linker, the default linking); LIBC
# names the default one: glibc (dynamic linking), musl (static) or shall
# (Shall's own C library, static).  When
# c17 runs, C17_LIBC=name chooses another.

.POSIX:

CC       = cc
CFLAGS   = -O2
LDFLAGS  =
PREFIX   = /usr/local
BINDIR   = $(PREFIX)/bin
LIBDIR   = $(PREFIX)/lib/c17
LIBC     = glibc

# required by the sources: the language standard; and the warnings
STDFLAGS = -std=c11 -Wall -Wno-switch
LDLIBS   =

PROG     = c17
OBJS     = c17.o util.o cpp.o tokenize.o type.o decl.o init.o stmt.o \
	   expr.o eval.o codegen.o asm.o link.o
HDRS     = c17.h ../lib/cpp.h ../lib/port.h parse.h elf.h
# c17's own directory: libc/ and libc17rt.a
CONFIG   = -DC17_LIBDIR=\"$(LIBDIR)\" -DC17_SYSLIBDIR=\"$(PREFIX)/lib\" \
	   -DC17_LIBC_DEFAULT=\"$(LIBC)\"

all: $(PROG) libc17rt.a

$(PROG): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

$(OBJS): $(HDRS)

c17.o: c17.c Makefile
	$(CC) $(STDFLAGS) $(CFLAGS) -I../lib $(CONFIG) -c c17.c

.c.o:
	$(CC) $(STDFLAGS) $(CFLAGS) -I../lib -c $<

cpp.o: ../lib/cpp.c
	$(CC) $(STDFLAGS) $(CFLAGS) -I../lib -c ../lib/cpp.c

# the run-time library (complex arithmetic, and what the C libraries'
# static archives expect of the compiler) is compiled by the c17 just built
RTOBJS   = rt_complex.o rt_float128.o rt_float128s.o rt_unwind.o rt_bits.o

libc17rt.a: rt_complex.c rt_float128.c rt_float128.s rt_unwind.c rt_bits.c $(PROG)
	C17_LIBDIR=. ./$(PROG) -c -o rt_complex.o rt_complex.c
	C17_LIBDIR=. ./$(PROG) -c -o rt_float128.o rt_float128.c
	C17_LIBDIR=. ./$(PROG) -c -o rt_float128s.o rt_float128.s
	C17_LIBDIR=. ./$(PROG) -c -o rt_unwind.o rt_unwind.c
	C17_LIBDIR=. ./$(PROG) -c -o rt_bits.o rt_bits.c
	rm -f $@
	ar -rc $@ $(RTOBJS)

test: all
	sh ./test_c17.sh

# compares with other implementations; needs
#   gcc (or cc) with glibc, the GNU assembler (as), and for the larger
#   suites the checkouts named in README.md
difftest: all
	sh ./tests/difftest.sh

install: all
	mkdir -p $(DESTDIR)$(BINDIR) $(DESTDIR)$(LIBDIR)/libc
	cp $(PROG) $(DESTDIR)$(BINDIR)/$(PROG)
	cp libc/* $(DESTDIR)$(LIBDIR)/libc/
	cp libc17rt.a $(DESTDIR)$(LIBDIR)/libc17rt.a

clean:
	rm -f $(PROG) $(OBJS) $(RTOBJS) libc17rt.a

.PHONY: all test difftest install clean
