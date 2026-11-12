# Copyright (c) 2026, asprintf
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

# makefs - make an FFS image from a directory tree, without root (V86BSD)
#
# NetBSD's usr.sbin/makefs, FFS only; see README.md.
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

# the sources are NetBSD's: nbtool_config.h is what they expect of NetBSD's
# own libc, -I. puts NetBSD's ufs headers before the system's
DEFS     = -DHAVE_NBTOOL_CONFIG_H=1 -I.

PROG     = makefs
OBJS     = makefs.o walk.o image.o misc.o buf.o ffs_alloc.o ffs_balloc.o mkfs.o ufs_bmap.o ffs_subr.o ffs_tables.o ffs_bswap.o

all: $(PROG)

$(PROG): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

makefs.o: makefs.c
	$(CC) $(STDFLAGS) $(CFLAGS) $(DEFS) -c makefs.c

walk.o: walk.c
	$(CC) $(STDFLAGS) $(CFLAGS) $(DEFS) -c walk.c

image.o: image.c
	$(CC) $(STDFLAGS) $(CFLAGS) $(DEFS) -c image.c

misc.o: misc.c
	$(CC) $(STDFLAGS) $(CFLAGS) $(DEFS) -c misc.c

buf.o: ffs/buf.c
	$(CC) $(STDFLAGS) $(CFLAGS) $(DEFS) -c ffs/buf.c

ffs_alloc.o: ffs/ffs_alloc.c
	$(CC) $(STDFLAGS) $(CFLAGS) $(DEFS) -c ffs/ffs_alloc.c

ffs_balloc.o: ffs/ffs_balloc.c
	$(CC) $(STDFLAGS) $(CFLAGS) $(DEFS) -c ffs/ffs_balloc.c

mkfs.o: ffs/mkfs.c
	$(CC) $(STDFLAGS) $(CFLAGS) $(DEFS) -c ffs/mkfs.c

ufs_bmap.o: ffs/ufs_bmap.c
	$(CC) $(STDFLAGS) $(CFLAGS) $(DEFS) -c ffs/ufs_bmap.c

ffs_subr.o: ufs/ffs/ffs_subr.c
	$(CC) $(STDFLAGS) $(CFLAGS) $(DEFS) -c ufs/ffs/ffs_subr.c

ffs_tables.o: ufs/ffs/ffs_tables.c
	$(CC) $(STDFLAGS) $(CFLAGS) $(DEFS) -c ufs/ffs/ffs_tables.c

ffs_bswap.o: ufs/ffs/ffs_bswap.c
	$(CC) $(STDFLAGS) $(CFLAGS) $(DEFS) -c ufs/ffs/ffs_bswap.c

$(OBJS): nbtool_config.h makefs.h ffs.h ffs/buf.h

test: all
	cd ../../regress/usr.sbin/makefs && $(MAKE) test

difftest:
	@echo "makefs: no differential tests"

install: all
	mkdir -p $(DESTDIR)$(BINDIR)
	cp $(PROG) $(DESTDIR)$(BINDIR)/$(PROG)

clean:
	rm -f $(PROG) $(OBJS)

.PHONY: all test difftest install clean
