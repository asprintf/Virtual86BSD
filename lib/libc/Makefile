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
CPPFLAGS = -I$(INCLUDE) -Iinternal -D_XOPEN_SOURCE=800

CRT = $(B)/lib/crt1.o $(B)/lib/crti.o $(B)/lib/crtn.o
# libm, librt, libpthread, libxnet, libdl and libcrypt are in libc.a; the c17 utility
# accepts -l m, -l rt, -l pthread, -l xnet ..., so they exist, empty.
EMPTY = $(B)/lib/libm.a $(B)/lib/librt.a $(B)/lib/libpthread.a $(B)/lib/libxnet.a \
	$(B)/lib/libdl.a $(B)/lib/libcrypt.a

all: # (the default target; its prerequisites follow the includes)
include rules.mk
include config/$(CCNAME).mk

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
