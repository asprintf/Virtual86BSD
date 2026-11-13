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
# libcurses - X/Open Curses (Issue 4 Version 2), with terminfo and termcap:
# NetBSD's libcurses and libterminfo (see share/doc/credits.md).  A POSIX
# makefile; CC is make's own (c17), or e.g. CC="../libc/tools/libc-cc gcc"
# on a host.
#
#   make                  libcurses.a
#   make install          curses.h, unctrl.h, term.h, termcap.h into
#                         $(DESTDIR)$(PREFIX)/include, libcurses.a into
#                         $(DESTDIR)$(PREFIX)/lib with libterminfo.a and
#                         libtermcap.a as links to it
#   make terms            terminfo/compiled_terms.c again, the terminals built
#                         into the library, with TIC (a tic of this tree)
#   make check            the tests (../../regress/lib/libcurses/run.sh)
#   make clean

.POSIX:

CFLAGS   = -O2
PREFIX   = /usr/local
AR       = ar
TIC      = tic

CPPFLAGS = -D_DEFAULT_SOURCE -DHAVE_WCHAR -DTERMINFO_COMPILE -DTERMINFO_DB \
	-DTERMINFO_COMPAT -I. -Iterminfo -Icdb -Icompat

OBJS = acs.o addbytes.o addch.o addchnstr.o addnstr.o add_wch.o \
	add_wchstr.o addwstr.o attributes.o background.o bell.o \
	border.o box.o cchar.o chgat.o clear.o clearok.o clrtobot.o \
	clrtoeol.o color.o copywin.o cr_put.o ctrace.o cur_hash.o \
	curses.o curs_set.o delch.o deleteln.o delwin.o echochar.o \
	echo_wchar.o erase.o fileio.o flushok.o fullname.o getch.o \
	getstr.o get_wch.o get_wstr.o getyx.o idcok.o idlok.o \
	id_subwins.o immedok.o inch.o inchstr.o initscr.o insch.o \
	insdelln.o insertln.o insstr.o instr.o ins_wch.o ins_wstr.o \
	in_wch.o in_wchstr.o inwstr.o keyname.o keypad.o leaveok.o \
	line.o meta.o mouse.o move.o mvwin.o newwin.o nodelay.o \
	notimeout.o overlay.o overwrite.o pause.o printw.o putchar.o \
	refresh.o resize.o ripoffline.o scanw.o scr_dump.o screen.o scroll.o \
	scrollok.o setterm.o slk.o standout.o syncok.o timeout.o \
	toucholap.o touchwin.o tstp.o tty.o unctrl.o underscore.o \
	version.o vidattr.o \
	terminfo/compile.o terminfo/curterm.o terminfo/hash.o \
	terminfo/setupterm.o terminfo/term.o terminfo/termcap.o \
	terminfo/ti.o terminfo/tparm.o terminfo/tputs.o \
	cdb/cdbr.o cdb/cdbw.o cdb/mi_vector_hash.o

HDRS = curses.h curses_private.h fileio.h keymap.h unctrl.h \
	terminfo/term.h terminfo/term_private.h terminfo/termcap.h \
	cdb/cdbr.h cdb/cdbw.h compat/sys/endian.h compat/nbtool_config.h

all: libcurses.a

libcurses.a: $(OBJS)
	rm -f $@
	$(AR) -rc $@ $(OBJS)

$(OBJS): $(HDRS)
terminfo/term.o: terminfo/compiled_terms.c
terminfo/termcap.o: terminfo/termcap_map.c terminfo/termcap_hash.c

.c.o:
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

# the cdb sources as NetBSD builds them for its tools
cdb/cdbr.o: cdb/cdbr.c
	$(CC) $(CPPFLAGS) -DHAVE_NBTOOL_CONFIG_H=1 $(CFLAGS) -c -o $@ cdb/cdbr.c
cdb/cdbw.o: cdb/cdbw.c
	$(CC) $(CPPFLAGS) -DHAVE_NBTOOL_CONFIG_H=1 $(CFLAGS) -c -o $@ cdb/cdbw.c
cdb/mi_vector_hash.o: cdb/mi_vector_hash.c
	$(CC) $(CPPFLAGS) -DHAVE_NBTOOL_CONFIG_H=1 $(CFLAGS) -c -o $@ cdb/mi_vector_hash.c

fileio.h: shlib_version genfileioh.awk
	awk -f genfileioh.awk < shlib_version > $@

terminfo/hash.c: terminfo/genhash terminfo/term.h
	sh terminfo/genhash terminfo/term.h > $@

terms:
	TOOL_TIC="$(TIC)" TERMINFO=../../share/terminfo/terminfo sh terminfo/genterms > terminfo/compiled_terms.c.new
	mv terminfo/compiled_terms.c.new terminfo/compiled_terms.c

install: all
	mkdir -p $(DESTDIR)$(PREFIX)/include $(DESTDIR)$(PREFIX)/lib
	cp curses.h unctrl.h terminfo/term.h terminfo/termcap.h $(DESTDIR)$(PREFIX)/include/
	cp libcurses.a $(DESTDIR)$(PREFIX)/lib/.libcurses.a.new
	mv -f $(DESTDIR)$(PREFIX)/lib/.libcurses.a.new $(DESTDIR)$(PREFIX)/lib/libcurses.a
	cd $(DESTDIR)$(PREFIX)/lib && rm -f libterminfo.a libtermcap.a && \
		ln -s libcurses.a libterminfo.a && ln -s libcurses.a libtermcap.a

check:
	sh ../../regress/lib/libcurses/run.sh

clean:
	rm -f libcurses.a $(OBJS) fileio.h terminfo/hash.c
	rm -rf ../../regress/lib/libcurses/out

.PHONY: all terms install check clean
