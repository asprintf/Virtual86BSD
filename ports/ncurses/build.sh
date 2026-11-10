#!/bin/sh
# build.sh - build and install ncurses 6.6 inside a V86BSD system (see
# HOWTO.md beside it).
#
# Usage: sh build.sh [-n] [-T] [dir]
#
#   dir   ncurses' sources (default /usr/src/ncurses-6.6)
#   -n    don't install the terminfo database from ncurses' terminfo.src
#         (keep the one bootstrap.sh copied from the host)
#   -T    don't build ncurses' test and demo programs (test/)
#
# Builds with c17 and V86BSD's make, sh, sed and awk through ncurses' own
# configure; nothing is changed in the sources.  Installs, under /usr/local:
#   lib/libncursesw.a, libpanelw.a, libmenuw.a, libformw.a, and the links
#       libncurses.a, libcurses.a, libpanel.a, libmenu.a, libform.a to them
#       (so that -lcurses and -lncurses work too)
#   include/curses.h, ncurses.h, term.h, panel.h, menu.h, form.h, ...
#   bin/tic, infocmp, toe, tset, clear (and captoinfo, infotocap, reset);
#       not ncurses' tput and tabs: V86BSD has those already (POSIX's)
#   bin/ncursesw6-config
#   bin/ncurses/: all of ncurses' test and demo programs (unless -T), and
#       some of them also in bin: worm, rain, xmas, blue, firework, hanoi,
#       knight, tclock, gdc, bs
# and /usr/share/terminfo: the whole terminfo database (unless -n), where
# the system's own tput, more and vi read it too.
# The libraries are static (no shared ncurses: programs link it in)
# and wide-character (UTF-8 locales work); there are no manual pages
# (they are troff, V86BSD's man shows plain text).

usage() { echo "usage: sh build.sh [-n] [-T] [dir]" >&2; exit 2; }
db=yes tests=yes
while getopts nT opt; do
	case $opt in
	n) db=no ;;
	T) tests=no ;;
	*) usage ;;
	esac
done
shift $((OPTIND - 1))
[ $# -le 1 ] || usage
dir=${1:-/usr/src/ncurses-6.6}
CC=${CC:-c17}
export CC
fail() { echo "build.sh: $*" >&2; exit 1; }
say() { echo "== $*"; }

[ -f "$dir/ncurses/tinfo/lib_setup.c" ] || fail "$dir: no ncurses sources there (sh ports/get.sh ncurses, on the host)"
cd "$dir" || exit 1
log=$(pwd)/build.log
: > "$log"
run() {   # run what... : append the output to the log, stop on failure
	"$@" >> "$log" 2>&1 || fail "failed: $* (see $log)"
}

[ -f Makefile ] && run make distclean
t=--with-tests
[ $tests = no ] && t=--without-tests
say "configure (output in $log)"
# (--build: config.guess does not know uname -s V86BSD; this is the triplet
# it found when the system still said Linux)
CFLAGS=${CFLAGS:--O 1} run ./configure --prefix=/usr/local --build=x86_64-pc-linux-gnu \
	--without-cxx --without-cxx-binding --without-ada \
	--without-shared --without-debug --without-manpages $t --enable-overwrite \
	--with-terminfo-dirs=/usr/share/terminfo \
	--with-default-terminfo-dir=/usr/share/terminfo
say "make"
run make
say "install"
run make install.libs install.includes
if [ $db = yes ]; then
	say "the terminfo database, compiled by the new tic"
	run make install.data
fi
b=/usr/local/bin
for p in tic infocmp toe tset clear; do
	cp "progs/$p" $b/ || fail "installing $p"
done
ln -f $b/tic $b/captoinfo &&
ln -f $b/tic $b/infotocap &&
ln -f $b/tset $b/reset || fail "linking captoinfo, infotocap, reset"
if [ $tests = yes ]; then
	say "the test and demo programs: $b/ncurses/"
	rm -rf $b/ncurses && mkdir $b/ncurses || exit 1
	for f in test/*; do
		# the programs: executable ELF files (not the scripts)
		[ -f "$f" ] && [ -x "$f" ] || continue
		case $(od -A n -c -N 4 "$f" | tr -d ' ') in
		*ELF) cp "$f" $b/ncurses/ || fail "installing $f" ;;
		esac
	done
	for p in worm rain xmas blue firework hanoi knight tclock gdc bs; do
		ln -f $b/ncurses/$p $b/$p || fail "linking $p"
	done
fi
(
	cd /usr/local/lib || exit 1
	for l in ncurses panel menu form; do
		rm -f "lib$l.a" && ln -s "lib${l}w.a" "lib$l.a" || exit 1
	done
	rm -f libcurses.a && ln -s libncursesw.a libcurses.a
) || fail "the library links"

# check: a small program that uses curses, linked with -lcurses
say "check"
t=/tmp/nccheck.$$
cat > $t.c <<'EOF'
#include <curses.h>
#include <term.h>
#include <stdio.h>
int main(void)
{
	int e;
	if (setupterm("vt100", 1, &e) != OK) return 1;
	printf("%s: %d columns, %d lines, clear=%s\n", curses_version(),
	       tigetnum("cols"), tigetnum("lines"),
	       tigetstr("clear") ? "yes" : "no");
	return 0;
}
EOF
$CC -o $t $t.c -lcurses >> "$log" 2>&1 && $t || fail "check program (see $log)"
rm -f $t $t.c
if [ $tests = yes ]; then
	say "done; try worm, rain, xmas, blue, or /usr/local/bin/ncurses/ncurses (a menu of tests)"
else
	say "done"
fi
