#!/bin/sh
# build.sh - build and install Rogue 5.4.4 inside a V86BSD system (see
# HOWTO.md beside it).  Needs ncurses first (ports/ncurses/build.sh).
#
# Usage: sh build.sh [-I] [dir]
#
#   dir   Rogue's sources: github.com/Davidslv/rogue, branch master
#         (default /usr/src/rogue-5.4.4)
#   -I    build only, don't install
#
# Applies posix.diff (beside this script) once, then Rogue's own configure
# and make, with c17.  Installs /usr/local/bin/rogue, the score file
# /var/games/rogue.scr, the manual page for V86BSD's man (man rogue:
# /usr/local/share/man/rogue), and
# the guide, /usr/local/share/doc/rogue/rogue.doc.

usage() { echo "usage: sh build.sh [-I] [dir]" >&2; exit 2; }
inst=yes
while getopts I opt; do
	case $opt in
	I) inst=no ;;
	*) usage ;;
	esac
done
shift $((OPTIND - 1))
[ $# -le 1 ] || usage
here=$(cd "$(dirname "$0")" && pwd)
dir=${1:-/usr/src/rogue-5.4.4}
CC=${CC:-c17}
export CC
fail() { echo "build.sh: $*" >&2; exit 1; }
say() { echo "== $*"; }

[ -f "$dir/rogue.h" ] || fail "$dir: no Rogue sources there (sh ports/get.sh rogue, on the host)"
[ -f /usr/local/include/curses.h ] && [ -f /usr/local/lib/libcurses.a ] ||
	fail "ncurses isn't installed: sh $here/../ncurses/build.sh first"
cd "$dir" || exit 1
log=$(pwd)/build.log
: > "$log"
run() {   # run what... : append the output to the log, stop on failure
	"$@" >> "$log" 2>&1 || fail "failed: $* (see $log)"
}

if grep 'toascii() is gone' fight.c >/dev/null; then
	say "posix.diff: already applied"
else
	say "applying posix.diff"
	run patch -p1 -i "$here/posix.diff"
fi
[ -f Makefile ] && run make clean
say "configure (output in $log)"
# (--build: config.guess does not know uname -s V86BSD; this is the triplet
# it found when the system still said Linux)
CFLAGS=${CFLAGS:--O 1} run ./configure --prefix=/usr/local --build=x86_64-pc-linux-gnu \
	--enable-scorefile=/var/games/rogue.scr \
	--enable-lockfile=/var/games/rogue.lck
say "make"
run make
[ -x rogue ] || fail "no rogue made (see $log)"
[ $inst = yes ] || { say "done: $(pwd)/rogue"; exit 0; }

say "install"
cp rogue /usr/local/bin/rogue || fail "installing rogue"
mkdir -p /var/games /usr/local/share/doc/rogue || exit 1
[ -f /var/games/rogue.scr ] || { : > /var/games/rogue.scr && chmod 666 /var/games/rogue.scr; } ||
	fail "making /var/games/rogue.scr"
chmod 1777 /var/games    # for the lock file, made and removed by every player
cp rogue.doc LICENSE.TXT /usr/local/share/doc/rogue/ || exit 1
m=/usr/local/share/man     # in MANPATH (/etc/profile)
mkdir -p $m && cp rogue.cat $m/rogue || exit 1
if ! grep '^rogue ' $m/whatis >/dev/null 2>&1; then
	echo "rogue - Exploring The Dungeons of Doom" >> $m/whatis
fi
say "done: rogue (man rogue; the guide: more /usr/local/share/doc/rogue/rogue.doc)"
