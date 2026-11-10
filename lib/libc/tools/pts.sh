#!/bin/sh
# pts.sh - builds and runs the Open POSIX Test Suite, one test at a time
#
#   sh pts.sh SRC OUT CC [TIMEOUT [K/M]]
#
# SRC is LTP's testcases/open_posix_testsuite, OUT a directory for the
# programs, logs and OUT/results (one line per test: "NAME STATUS"),
# CC the compiler command.  Tests are conformance/**/N-M.c (those named
# *-buildonly.c only compiled) and functional/**/*.c, as LTP's own
# locate-test picks them.  A test already in OUT/results is not run again,
# so a run that was cut short continues where it stopped.
# Status: PASS FAIL UNRESOLVED UNSUPPORTED UNTESTED (exit 0 1 2 4 5),
# BUILD (does not compile), HUNG (TIMEOUT seconds, default 300, as LTP),
# SIGNAL-n, EXIT-n.  K/M: only the tests whose number (from 1) is K modulo M,
# so that M machines can share the work.  PTS_LIST=file: only the tests it
# names; PTS_VERBOSE=1: each test's name on the standard error as it starts.
SRC=$1 OUT=$2 CC=$3 TMO=${4:-300} PART=${5:-0/1}
K=${PART%/*} M=${PART#*/}
[ -n "$CC" ] || { echo "usage: sh pts.sh SRC OUT CC [TIMEOUT]" >&2; exit 2; }
case $SRC in /*) ;; *) SRC=$(pwd)/$SRC ;; esac
case $OUT in /*) ;; *) OUT=$(pwd)/$OUT ;; esac
CFLAGS="-D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -I$SRC/include"
mkdir -p "$OUT" || exit 1
: >> "$OUT/results"
# a run that was cut short may have left a last line half written
awk 'NF == 2 && $2 ~ /^(PASS|FAIL|UNRESOLVED|UNSUPPORTED|UNTESTED|BUILD|HUNG|SIGNAL-[0-9]+|EXIT-[0-9]+)$/' "$OUT/results" > "$OUT/results.ok" &&
	mv "$OUT/results.ok" "$OUT/results"
if [ ! -f "$OUT/common.o" ]; then
	$CC $CFLAGS -c -o "$OUT/common.o" "$SRC/lib/common.c" || exit 1
fi
if [ -n "${PTS_LIST:-}" ]; then
	sort "$PTS_LIST" > "$OUT/list"	# only these (names as in OUT/results)
else
	(cd "$SRC" && { find conformance -type f -name '[0-9]*-[0-9]*.c'; find functional -type f -name '*.c'; } | sort) > "$OUT/list"
fi
total=$(wc -l < "$OUT/list")
n=0
while IFS= read -r t; do
	n=$((n + 1))
	[ $((n % M)) -eq "$K" ] || continue
	grep -q "^$t " "$OUT/results" && continue
	dir=$(dirname "$t") name=$(basename "$t" .c)
	[ -z "${PTS_VERBOSE:-}" ] || echo "pts: $t" >&2
	d=$OUT/$dir
	mkdir -p "$d"
	case $name in
	*buildonly*)
		if $CC $CFLAGS -I"$SRC/$dir" -c -o "$d/$name.o" "$SRC/$t" > "$d/$name.blog" 2>&1
		then st=PASS; else st=BUILD; fi ;;
	*)
		if $CC $CFLAGS -I"$SRC/$dir" -o "$d/$name" "$SRC/$t" "$OUT/common.o" -lpthread -lrt -lm > "$d/$name.blog" 2>&1
		then
			(cd "$d" && exec timeout -k 10 "$TMO" "./$name" > "$name.log" 2>&1 < /dev/null)
			r=$?
			case $r in
			0) st=PASS ;; 1) st=FAIL ;; 2) st=UNRESOLVED ;; 4) st=UNSUPPORTED ;; 5) st=UNTESTED ;;
			124) st=HUNG ;;
			*) if [ $r -gt 128 ]; then st=SIGNAL-$((r - 128)); else st=EXIT-$r; fi ;;
			esac
		else
			st=BUILD
		fi ;;
	esac
	echo "$t $st" >> "$OUT/results"
	[ $((n % 50)) -eq 0 ] && echo "pts: $n of $total ($(date +%H:%M:%S))"
done < "$OUT/list"
echo "pts: done, $total tests"
awk '{ c[$2]++ } END { for (s in c) printf "%-12s %5d\n", s, c[s] }' "$OUT/results" | sort
