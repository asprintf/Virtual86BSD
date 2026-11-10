#!/bin/sh
# build.sh - build Lua 5.4 inside a V86BSD system (see HOWTO.md beside it).
#
# Usage: sh build.sh [-I] [-t] [dir]
#
#   dir   Lua's sources: the release tarball's directory (lua-5.4.7, with
#         src/ in it) or a git clone (the sources at the top);
#         default /usr/src/lua-5.4.7
#   -t    run Lua's test suite afterwards (testes/ of a git clone, or the
#         separate test tarball unpacked beside the sources)
#   -I    build only; without it lua (and luac) go to /usr/local/bin,
#         liblua.a to /usr/local/lib and its headers to /usr/local/include
#
# Only V86BSD's own tools are used: c17 compiles and links, ar makes
# liblua.a.  Lua's makefile isn't used: it passes gcc's options, runs
# ranlib (not POSIX) and makes ldo.c use _setjmp (gone from POSIX.1-2024).

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
dir=${1:-/usr/src/lua-5.4.7}
CC=${CC:-c17}
fail() { echo "build.sh: $*" >&2; exit 1; }

[ -d "$dir" ] || fail "$dir: no such directory (sh ports/get.sh lua, on the host)"
cd "$dir" || exit 1
top=$(pwd)
if [ -f src/lua.c ]; then cd src; elif [ ! -f lua.c ]; then fail "$dir: no lua.c in it or in src/"; fi
echo "== building Lua in $(pwd) with $CC"

rm -f ./*.o liblua.a lua luac
for f in *.c; do
	case $f in
	lua.c|luac.c|onelua.c|ltests.c) ;;     # programs, not the library
	ldo.c) $CC -O 1 -c ldo.c || exit 1 ;;   # ISO setjmp, not _setjmp
	*)     $CC -O 1 -D LUA_USE_POSIX -c "$f" || exit 1 ;;
	esac
done
ar -rc liblua.a ./*.o || exit 1
$CC -O 1 -D LUA_USE_POSIX -c lua.c && $CC -o lua lua.o liblua.a -lm || exit 1
if [ -f luac.c ]; then
	$CC -O 1 -D LUA_USE_POSIX -c luac.c && $CC -o luac luac.o liblua.a -lm || exit 1
fi
./lua -v || fail "the new lua doesn't run"
here=$(pwd)

if [ $test = yes ]; then
	t=
	for d in "$top/testes" "$top"/lua-5.4.*-tests "$top"/../lua-5.4.*-tests; do
		[ -f "$d/all.lua" ] && { t=$d; break; }
	done
	[ -n "$t" ] || fail "no test suite found (testes/all.lua)"
	echo "== test suite in $t"
	(cd "$t" && "$here/lua" -e "_U=true" all.lua) > "$here/test.log" 2>&1
	st=$?
	tail -n 3 "$here/test.log"
	grep 'final OK' "$here/test.log" >/dev/null || fail "test suite failed (status $st): $here/test.log"
fi

if [ $inst = yes ]; then
	p=/usr/local
	mkdir -p $p/bin $p/lib $p/include || exit 1
	cp lua $p/bin/lua || exit 1
	[ -f luac ] && { cp luac $p/bin/luac || exit 1; }
	cp liblua.a $p/lib/ && cp lua.h luaconf.h lualib.h lauxlib.h $p/include/ || exit 1
	[ -f lua.hpp ] && cp lua.hpp $p/include/
	echo "== installed: $p/bin/lua$([ -f luac ] && echo ' and luac'), $p/lib/liblua.a, the headers in $p/include"
fi
echo "== done"
