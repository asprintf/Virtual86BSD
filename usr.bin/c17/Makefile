# Copyright (c) 2026, Joost Orchidee Winter
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice, this
#    list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
#
# 3. Neither the name of the copyright holder nor the names of its
#    contributors may be used to endorse or promote products derived from
#    this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# c17 - compile standard C programs (POSIX.1-2024)
#
# Targets: all (the default), test, difftest, install, clean.
#
# c17 compiles, assembles and link-edits by itself; what it needs besides
# is a C library.  Each C library it can use has a profile in libc/ (its
# headers, its directory, the dynamic linker, the default linking); LIBC
# names the default one: glibc (dynamic linking), musl (static) or v86bsd
# (V86BSD's own C library, dynamic).  When
# c17 runs, C17_LIBC=name chooses another.

.POSIX:

CFLAGS   = -O2
LDFLAGS  =
PREFIX   = /usr/local
BINDIR   = $(PREFIX)/bin
LIBDIR   = $(PREFIX)/lib/c17
LIBC     = glibc

# STDFLAGS: more options for the compiler, none needed (the sources are
# C11 with POSIX interfaces); with gcc or clang, for example,
# STDFLAGS="-std=c11 -Wall -Wextra".  CC is make's own (POSIX: c17).
STDFLAGS =
LDLIBS   =

PROG     = c17
OBJS     = c17.o util.o cpp.o tokenize.o type.o decl.o init.o stmt.o \
	   expr.o eval.o codegen.o asm.o link.o
HDRS     = c17.h ../../lib/libxcu/cpp.h ../../lib/libxcu/port.h parse.h elf.h
# c17's own directory: libc/ and libc17rt.a
CONFIG   = -DC17_LIBDIR=\"$(LIBDIR)\" -DC17_SYSLIBDIR=\"$(PREFIX)/lib\" \
	   -DC17_LIBC_DEFAULT=\"$(LIBC)\"

all: $(PROG) libc17rt.a

$(PROG): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

$(OBJS): $(HDRS)

c17.o: c17.c Makefile
	$(CC) $(STDFLAGS) $(CFLAGS) -I../../lib/libxcu $(CONFIG) -c c17.c

.c.o:
	$(CC) $(STDFLAGS) $(CFLAGS) -I../../lib/libxcu -c $<

cpp.o: ../../lib/libxcu/cpp.c
	$(CC) $(STDFLAGS) $(CFLAGS) -I../../lib/libxcu -c ../../lib/libxcu/cpp.c

# the run-time library (complex arithmetic, and what the C libraries'
# static archives expect of the compiler) is compiled by the c17 just built
# (RTC17: another c17 that writes the same code, when this one cannot run
# here, as when it is linked with a C library not installed yet)
RTC17    = ./$(PROG)
RTOBJS   = rt_complex.o rt_float128.o rt_float128s.o rt_unwind.o rt_bits.o rt_atomic.o

libc17rt.a: rt_complex.c rt_float128.c rt_float128.s rt_unwind.c rt_bits.c rt_atomic.c $(PROG)
	C17_LIBDIR=. $(RTC17) -c -o rt_complex.o rt_complex.c
	C17_LIBDIR=. $(RTC17) -c -o rt_float128.o rt_float128.c
	C17_LIBDIR=. $(RTC17) -c -o rt_float128s.o rt_float128.s
	C17_LIBDIR=. $(RTC17) -c -o rt_unwind.o rt_unwind.c
	C17_LIBDIR=. $(RTC17) -c -o rt_bits.o rt_bits.c
	C17_LIBDIR=. $(RTC17) -c -o rt_atomic.o rt_atomic.c
	rm -f $@
	ar -rc $@ $(RTOBJS)

test: all
	cd ../../regress/usr.bin/c17 && $(MAKE) test

# compares with other implementations; needs
#   gcc (or cc) with glibc, the GNU assembler (as), and for the larger
#   suites the checkouts named in README.md
difftest: all
	cd ../../regress/usr.bin/c17 && $(MAKE) difftest

install: all
	mkdir -p $(DESTDIR)$(BINDIR) $(DESTDIR)$(LIBDIR)/libc
	cp $(PROG) $(DESTDIR)$(BINDIR)/$(PROG)
	cp libc/* $(DESTDIR)$(LIBDIR)/libc/
	cp libc17rt.a $(DESTDIR)$(LIBDIR)/libc17rt.a

clean:
	rm -f $(PROG) $(OBJS) $(RTOBJS) libc17rt.a

.PHONY: all test difftest install clean
