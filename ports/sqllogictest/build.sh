#!/bin/sh
# build.sh - build sqllogictest inside a V86BSD system (see HOWTO.md beside
# it), against the SQLite of ports/sqlite: the point is to test that one.
# (sqllogictest's src/ also carries an SQLite amalgamation, which isn't
# used: it would need posix-autoconf.diff of ports/sqlite as well.)
#
# Usage: sh build.sh [-I] [dir]
#
#   dir   sqllogictest's sources and tests (default /usr/src/sqllogictest)
#   -I    build only: don't install /usr/local/bin/sqllogictest
#
# Applies posix.diff (beside this script) once: strcasecmp's header.
# Then run the tests with run.sh beside this script.

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
dir=${1:-/usr/src/sqllogictest}
CC=${CC:-c17}
CFLAGS=${CFLAGS:--O 1}
fail() { echo "build.sh: $*" >&2; exit 1; }
say() { echo "== $*"; }

[ -f "$dir/src/sqllogictest.c" ] ||
	fail "$dir: no sqllogictest there (sh ports/get.sh sqllogictest, on the host)"
cd "$dir/src" || exit 1
log=$(pwd)/build.log
: > "$log"
run() {   # run what... : append the output to the log, stop on failure
	echo "$*" >> "$log"
	"$@" >> "$log" 2>&1 || fail "failed: $* (see $log)"
}
if grep 'POSIX has it here' sqllogictest.c >/dev/null; then
	say "posix.diff: already applied"
else
	say "applying posix.diff"
	(cd .. && patch -p1 -i "$here/posix.diff") >> "$log" 2>&1 || fail "patch failed (see $log)"
fi
rm -f ./*.o sqllogictest
run $CC $CFLAGS -c md5.c
[ -f /usr/local/include/sqlite3.h ] && [ -f /usr/local/lib/libsqlite3.a ] ||
	fail "SQLite isn't installed: sh $here/../sqlite/build.sh first"
say "sqllogictest with the installed SQLite ($(sed -n 's/^#define SQLITE_VERSION  *"\(.*\)"/\1/p' /usr/local/include/sqlite3.h))"
# its own sqlite3.h must not be found first: compile in a copy without it
t=/tmp/slt.$$
mkdir $t && cp sqllogictest.c sqllogictest.h slt_sqlite.c slt_odbc3.c $t/ || exit 1
run $CC $CFLAGS -D OMIT_ODBC=1 -o sqllogictest $t/sqllogictest.c md5.o -lsqlite3
rm -rf $t
./sqllogictest -verify -parameter 'integrity_check;optimizer=0' ../test/select1.test ||
	fail "sqllogictest doesn't run"
if [ $inst = yes ]; then
	mkdir -p /usr/local/bin && cp sqllogictest /usr/local/bin/ || fail "installing"
	say "installed /usr/local/bin/sqllogictest"
fi
say "done; the whole suite: sh $here/run.sh"
