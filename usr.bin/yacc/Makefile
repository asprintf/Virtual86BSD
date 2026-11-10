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
	cd ../../regress/usr.bin/yacc && $(MAKE) test

# compares with other implementations; needs
#   bison and byacc
difftest: all
	cd ../../regress/usr.bin/yacc && $(MAKE) difftest

install: all
	mkdir -p $(DESTDIR)$(BINDIR)
	cp $(PROG) $(DESTDIR)$(BINDIR)/$(PROG)
	mkdir -p $(DESTDIR)$(LIBDIR)
	cp $(LIB) $(DESTDIR)$(LIBDIR)/$(LIB)

clean:
	rm -f $(PROG) $(OBJS) $(LIB) $(LIBOBJS)

.PHONY: all test difftest install clean
