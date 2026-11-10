#!/bin/sh
# all.sh - build and install, inside a V86BSD system, every port whose
# sources are in /usr/src (sh ports/get.sh all, on the host), in the order
# their dependencies need: ncurses, rogue, lua, sqlite, sqllogictest,
# mbedtls, curl.
#
# Usage: sh all.sh [-t]
#
#   -t   each port's own tests too (build.sh -t: some minutes more; not
#        sqllogictest's millions of queries: run.sh beside its build.sh)
#
# A port that fails stops nothing but the ports that need it; the end
# lists what was built.

usage() { echo "usage: sh all.sh [-t]" >&2; exit 2; }
t=
while getopts t opt; do
	case $opt in
	t) t=-t ;;
	*) usage ;;
	esac
done
shift $((OPTIND - 1))
[ $# = 0 ] || usage
here=$(cd "$(dirname "$0")" && pwd)

ok= failed= missing=
# port DIR NEEDS OPTS: build port $1 from /usr/src/$2 if there, when the
# ports in $3 (blank-separated) were built
port() {
	p=$1 src=/usr/src/$2 needs=$3 opts=$4
	if [ ! -d "$src" ]; then missing="$missing $p"; return; fi
	for n in $needs; do
		case " $ok " in *" $n "*) ;; *)
			echo "== all.sh: $p skipped: it needs $n"; failed="$failed $p"; return ;;
		esac
	done
	echo "== all.sh: $p"
	# shellcheck disable=SC2086
	if sh "$here/$p/build.sh" $opts; then ok="$ok $p"; else failed="$failed $p"; fi
}
port ncurses ncurses-6.6 "" ""
port rogue rogue-5.4.4 "ncurses" ""
port lua lua-5.4.7 "" "$t"
port sqlite sqlite-3.54.1 "" "$t"
port sqllogictest sqllogictest "sqlite" ""
port mbedtls mbedtls-3.6.7 "" "$t"
port curl curl-8.23.0 "mbedtls" "$t"

echo "== all.sh: built:${ok:- none}"
[ -z "$missing" ] || echo "== all.sh: no sources (sh ports/get.sh NAME on the host):$missing"
[ -z "$failed" ] || { echo "== all.sh: failed:$failed"; exit 1; }
