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

# at - execute commands at a later time (with batch, and the daemon atd) (POSIX.1-2024)
#
# Targets: all (the default), test, difftest, install, clean.
#
# batch is a link to at (it acts by its name).  atd, the daemon that runs
# the jobs, is installed in $(SBINDIR).

.POSIX:

CFLAGS   = -O2
LDFLAGS  =
PREFIX   = /usr/local
BINDIR   = $(PREFIX)/bin
SBINDIR  = $(PREFIX)/sbin

# STDFLAGS: more options for the compiler, none needed (the sources are
# C11 with POSIX interfaces); with gcc or clang, for example,
# STDFLAGS="-std=c11 -Wall -Wextra".  CC is make's own (POSIX: c17).
STDFLAGS =
LDLIBS   =

PROG     = at
OBJS     = at.o spool.o
ATD_OBJS = atd.o spool.o

all: $(PROG) batch atd

$(PROG): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

atd: $(ATD_OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(ATD_OBJS) $(LDLIBS)

batch: $(PROG)
	rm -f batch
	ln -s $(PROG) batch

at.o atd.o spool.o: spool.h

.c.o:
	$(CC) $(STDFLAGS) $(CFLAGS) -c $<

test: all
	cd ../../regress/usr.bin/at && $(MAKE) test

difftest:
	@echo "at: no differential tests"

install: all
	mkdir -p $(DESTDIR)$(BINDIR)
	cp $(PROG) $(DESTDIR)$(BINDIR)/$(PROG)
	rm -f $(DESTDIR)$(BINDIR)/batch
	ln -s $(PROG) $(DESTDIR)$(BINDIR)/batch
	mkdir -p $(DESTDIR)$(SBINDIR)
	cp atd $(DESTDIR)$(SBINDIR)/atd

clean:
	rm -f $(PROG) $(OBJS) batch atd atd.o

.PHONY: all test difftest install clean
