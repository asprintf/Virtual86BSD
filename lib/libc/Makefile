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

# libc - a POSIX.1-2024 C library for x86-64 Linux.  A POSIX makefile.
#
#   make                  build with gcc: build/gcc/lib/{libc.a,crt1.o,crti.o,crtn.o},
#                         and libc.so (also the dynamic linker), Scrt1.o, rcrt1.o
#   make CCNAME=c17       build with c17 (on PATH), which also assembles, and
#                         links libc.so (c17 -G)
#   make check            namespace and header checks, then the tests
#   make sysroot          build/<cc>/sysroot (include/, lib/) for C17_LIBC=
#   make install          into $(DESTDIR)$(PREFIX)/include and $(DESTDIR)$(PREFIX)/lib
#   make clean
#
# Compiler settings are in config/$(CCNAME).mk.  After adding or removing
# a source file, run tools/genmk.sh > rules.mk.

.POSIX:

CCNAME   = gcc
B        = build/$(CCNAME)
AR       = ar
INCLUDE  = ../../include
CPPFLAGS = -I$(INCLUDE) -Iinternal -I$(ABIDIR) -D_XOPEN_SOURCE=800

# The system call ABI the library is built for (tools/mkabi.sh): linux,
# Linux's numbers (any Linux kernel, V86BSD's compat/linux too), or native,
# V86BSD's own, from the kernel's sys/syscall.h (SYSCALL_H) with the brand
# in the start files.
TARGET_ABI = linux
SYSCALL_H  = ../../sys/sys/syscall.h
ABIDIR     = $(B)/abi

CRT = $(B)/lib/crt1.o $(B)/lib/crti.o $(B)/lib/crtn.o
# libm, librt, libpthread, libxnet, libdl and libcrypt are in libc.a; the c17 utility
# accepts -l m, -l rt, -l pthread, -l xnet ..., so they exist, empty.
EMPTY = $(B)/lib/libm.a $(B)/lib/librt.a $(B)/lib/libpthread.a $(B)/lib/libxnet.a \
	$(B)/lib/libdl.a $(B)/lib/libcrypt.a

all: # (the default target; its prerequisites follow the includes)
include rules.mk
include config/$(CCNAME).mk

# written again when the ABI changes (only then do the objects)
$(ABIDIR)/syscall_nr.h: FORCE
	sh tools/mkabi.sh $(TARGET_ABI) $(SYSCALL_H) $(ABIDIR)
$(ABIDIR)/nr.sed $(ABIDIR)/note.s: $(ABIDIR)/syscall_nr.h
$(OBJS) $(STATIC_OBJS) $(DYN_OBJS): $(ABIDIR)/syscall_nr.h
FORCE:

# SHARED (config/*.mk): libc.so, Scrt1.o (PIE), rcrt1.o (static PIE)
all: $(B)/lib/libc.a $(CRT) $(EMPTY) $(SHARED)

$(B)/lib/libc.a: $(OBJS) $(STATIC_OBJS)
	mkdir -p $(B)/lib
	rm -f $@
	$(AR) -rc $@ $(OBJS) $(STATIC_OBJS)

$(EMPTY):
	mkdir -p $(B)/lib
	rm -f $@
	printf '!<arch>\n' > $@

sysroot: all
	rm -rf $(B)/sysroot
	mkdir -p $(B)/sysroot
	ln -s ../../../../../include $(B)/sysroot/include
	ln -s ../lib $(B)/sysroot/lib

check: all
	tools/genmk.sh | cmp -s - rules.mk || { echo "rules.mk is out of date: run tools/genmk.sh > rules.mk"; exit 1; }
	sh tools/check-namespace.sh $(B)/lib/libc.a
	sh ../../regress/lib/libc/headers.sh $(CCNAME)
	sh ../../regress/lib/libc/run.sh $(CCNAME)

# the headers, and libc.a, the start files and the empty libraries (and
# libc.so, Scrt1.o, rcrt1.o when built); libc.so is the program
# interpreter of programs linked with it: $(PREFIX)/lib/libc.so
PREFIX   = /usr/local
install: all
	mkdir -p $(DESTDIR)$(PREFIX)/include $(DESTDIR)$(PREFIX)/lib
	cp -R $(INCLUDE)/. $(DESTDIR)$(PREFIX)/include
	@# each file is copied beside its place and renamed into it: copying
	@# over the libc.so that running programs map would change it under
	@# them (they crash, and so does the copy: in a running V86BSD)
	@for f in $(B)/lib/libc.a $(CRT) $(EMPTY) $(SHARED); do \
		n=$${f##*/}; d=$(DESTDIR)$(PREFIX)/lib; \
		echo "cp $$f $$d/$$n"; \
		cp $$f $$d/.$$n.new && mv -f $$d/.$$n.new $$d/$$n || exit 1; \
	done

clean:
	rm -rf build ../../regress/lib/libc/out

.PHONY: all sysroot check install clean
