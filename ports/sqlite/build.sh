#!/bin/sh
# build.sh - build and install SQLite 3.54 inside a Shall system (see
# HOWTO.md beside it).
#
# Usage: sh build.sh [-I] [-t] [dir]
#
#   dir   SQLite's sources (default /usr/src/sqlite-3.54.1): either the
#         source tree (git tag version-3.54.1, or sqlite-src-3540100.zip)
#         or the amalgamation (sqlite-autoconf-3540100.tar.gz)
#   -t    run SQLite's C test programs afterwards (source tree only):
#         fuzzcheck, mptest, threadtest3, speedtest1
#   -I    build only, don't install
#
# Applies posix-src.diff or posix-autoconf.diff (beside this script) once,
# then SQLite's own configure (autosetup, which first builds its jimsh
# with c17) and make.  From the source tree the amalgamation, sqlite3.c, is
# generated on the way (by jimsh, lemon and mkkeywordhash, all built here).
# Installs, under /usr/local: bin/sqlite3, lib/libsqlite3.a, lib/libsqlite3.so
# (made with c17 -G), include/sqlite3.h and sqlite3ext.h.

usage() { echo "usage: sh build.sh [-I] [-t] [dir]" >&2; exit 2; }
inst=yes test=no
while getopts It opt; do
	case $opt in
	I) inst=no ;;
	t) test=yes ;;
	*) usage ;;
	esac
done
shift $((OPTIND - 1))
[ $# -le 1 ] || usage
here=$(cd "$(dirname "$0")" && pwd)
dir=${1:-/usr/src/sqlite-3.54.1}
CC=${CC:-c17}
export CC
fail() { echo "build.sh: $*" >&2; exit 1; }
say() { echo "== $*"; }

if [ -f "$dir/src/sqlite.h.in" ] && [ -f "$dir/main.mk" ]; then
	kind=src
elif [ -f "$dir/sqlite3.c" ] && [ -f "$dir/shell.c" ]; then
	kind=autoconf
else
	fail "$dir: no SQLite sources there (sh ports/get.sh sqlite, on the host)"
fi
[ $test = yes ] && [ $kind = autoconf ] &&
	fail "-t needs the source tree (the amalgamation has no tests)"
cd "$dir" || exit 1
log=$(pwd)/build.log
: > "$log"
run() {   # run what... : append the output to the log, stop on failure
	"$@" >> "$log" 2>&1 || fail "failed: $* (see $log)"
}

if grep 'gone from POSIX.1-2024' autosetup/jimsh0.c >/dev/null; then
	say "posix-$kind.diff: already applied"
else
	say "applying posix-$kind.diff"
	run patch -p1 -i "$here/posix-$kind.diff"
fi
[ -f Makefile ] && run make distclean

# CC_FOR_BUILD: autosetup builds its Tcl interpreter (jimsh0) with "cc"
# (not a POSIX utility) unless told otherwise.  LD: it insists on finding
# one, though only the compiler links.  --disable-shared and CFLAGS.core=:
# its shared-library options are gcc's (-fPIC, -shared); libsqlite3.so is
# made below with c17's -G instead.
say "configure (output in $log)"
CC_FOR_BUILD=$CC LD=$CC CFLAGS=${CFLAGS:--O 1} run ./configure --prefix=/usr/local --disable-shared
say "make"
run make CFLAGS.core=
[ -x sqlite3 ] && [ -f libsqlite3.a ] || fail "no sqlite3 made (see $log)"
say "libsqlite3.so, with c17 -G"
feat=$(sed -n 's/^OPT_FEATURE_FLAGS = //p' Makefile | sed 's/\$(OPTIONS)//')
# shellcheck disable=SC2086
run $CC -G ${CFLAGS:--O 1} $feat -o libsqlite3.so sqlite3.c

# a check that needs no test suite: SQL with a known answer
say "check"
out=$(./sqlite3 :memory: <<'EOF'
CREATE TABLE t(a INTEGER PRIMARY KEY, b TEXT);
WITH RECURSIVE c(x) AS (VALUES(1) UNION ALL SELECT x+1 FROM c WHERE x<1000)
  INSERT INTO t(b) SELECT printf('row %04d', x) FROM c;
CREATE INDEX tb ON t(b);
SELECT count(*), sum(a), max(b), round(sqrt(2), 6), json_array(1, 'two') FROM t;
SELECT b FROM t WHERE b LIKE 'row 05%' ORDER BY b DESC LIMIT 1;
EOF
) || fail "sqlite3 failed"
want='1000|500500|row 1000|1.414214|[1,"two"]
row 0599'
[ "$out" = "$want" ] || fail "check: wrong answer:
$out"
echo "$out"

if [ $test = yes ]; then
	say "the test programs"
	run make CFLAGS.core= fuzzcheck mptester threadtest3 speedtest1
	for t in fuzzcheck mptest threadtest3 speedtest1; do
		say "test: $t (output in $t.log)"
		case $t in
		fuzzcheck)   ./fuzzcheck test/fuzzdata*.db ;;
		mptest)      make CFLAGS.core= mptest ;;
		threadtest3) ./threadtest3 ;;
		speedtest1)  rm -f speedtest1.db &&
			     ./speedtest1 --verify --size 25 speedtest1.db && rm -f speedtest1.db ;;
		esac > $t.log 2>&1 || fail "$t failed (see $(pwd)/$t.log)"
		case $t in
		fuzzcheck)   grep 'errors out of' $t.log ;;
		mptest)      grep '^Summary' $t.log | sort | uniq -c ;;
		threadtest3) tail -n 1 $t.log ;;
		speedtest1)  tail -n 1 $t.log ;;
		esac
	done
fi

[ $inst = yes ] || { say "done: $(pwd)/sqlite3"; exit 0; }
say "install"
p=/usr/local
mkdir -p $p/bin $p/lib $p/include &&
cp sqlite3 $p/bin/ &&
cp libsqlite3.a libsqlite3.so $p/lib/ &&
cp sqlite3.h sqlite3ext.h $p/include/ || fail "installing"
# and a program that uses the library: linked with libsqlite3.so
t=/tmp/sqcheck.$$
cat > $t.c <<'EOF'
#include <sqlite3.h>
#include <stdio.h>
static int row(void *u, int n, char **v, char **c)
{
	(void)u; (void)c;
	for (int i = 0; i < n; i++)
		printf("%s%s", i ? " " : "", v[i] ? v[i] : "NULL");
	printf("\n");
	return 0;
}
int main(void)
{
	sqlite3 *db;
	char *err = 0;
	if (sqlite3_open(":memory:", &db) != SQLITE_OK) return 1;
	if (sqlite3_exec(db, "SELECT sqlite_version(), 6*7", row, 0, &err) != SQLITE_OK) {
		fprintf(stderr, "%s\n", err);
		return 1;
	}
	return sqlite3_close(db) != SQLITE_OK;
}
EOF
$CC -o $t $t.c -lsqlite3 >> "$log" 2>&1 && $t || fail "a program with -lsqlite3 (see $log)"
rm -f $t $t.c
say "done: sqlite3 (try: sqlite3 /tmp/test.db)"
