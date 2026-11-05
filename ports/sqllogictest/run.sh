#!/bin/sh
# run.sh - run sqllogictest's test scripts, as its run-all.tcl does (which
# needs Tcl): every *.test file twice, with the optimizer on and off, each
# result checked against the one recorded in the script, plus SQLite's
# integrity check.
#
# Usage: sh run.sh [-j jobs] [-d dir] [file-or-directory...]
#
#   -j jobs  run that many sqllogictest processes at once (default 1)
#   -d dir   sqllogictest's sources (default /usr/src/sqllogictest)
#   operands test scripts, or directories searched for *.test (default:
#            all of dir/test: 622 scripts, some 5.9 million queries)
#
# The program is dir/src/sqllogictest (build.sh beside this script makes
# it), else sqllogictest on PATH.  The output of every run goes to
# run.log in the current directory; the last line sums them up:
#   N errors out of M tests and K invocations in S seconds
# The exit status is 0 when there were no errors and every run finished.

usage() { echo "usage: sh run.sh [-j jobs] [-d dir] [file-or-directory...]" >&2; exit 2; }
jobs=1 dir=/usr/src/sqllogictest
while getopts j:d: opt; do
	case $opt in
	j) jobs=$OPTARG ;;
	d) dir=$OPTARG ;;
	*) usage ;;
	esac
done
shift $((OPTIND - 1))
case $jobs in ''|*[!0-9]*|0) usage ;; esac
if [ -x "$dir/src/sqllogictest" ]; then BIN=$(cd "$dir/src" && pwd)/sqllogictest
elif command -v sqllogictest >/dev/null 2>&1; then BIN=$(command -v sqllogictest)
else echo "run.sh: no sqllogictest (sh build.sh)" >&2; exit 1
fi
[ $# -gt 0 ] || set -- "$dir/test"

tmp=${TMPDIR:-/tmp}/slt-run.$$
mkdir "$tmp" || exit 1
trap 'rm -rf "$tmp"' EXIT
trap 'exit 130' INT TERM
for a in "$@"; do
	if [ -d "$a" ]; then find "$a" -name '*.test' -type f
	elif [ -f "$a" ]; then echo "$a"
	else echo "run.sh: $a: no such file or directory" >&2; exit 1
	fi
done | awk -v d="$(pwd)" '{ print (substr($0, 1, 1) == "/" ? "" : d "/") $0 }' | sort > "$tmp/all"
n=$(wc -l < "$tmp/all")
[ "$n" -gt 0 ] || { echo "run.sh: no test scripts" >&2; exit 1; }
echo "run.sh: $n scripts, twice each, with $BIN, $jobs at a time"

start=$(date +%s)
# shard k gets lines k, k+jobs, k+2*jobs, ...
k=0
while [ $k -lt "$jobs" ]; do
	awk -v k=$k -v j="$jobs" '(NR - 1) % j == k' "$tmp/all" > "$tmp/list.$k"
	mkdir "$tmp/w.$k" || exit 1
	(
		# each process its own directory: sqllogictest's database is
		# ./test.db
		cd "$tmp/w.$k" || exit 1
		while IFS= read -r f; do
			for o in 0 -1; do
				r=$("$BIN" -verify -parameter "integrity_check;optimizer=$o" "$f" 2>&1)
				case $r in
				*" errors out of "*) ;;
				*) r="$r
INCOMPLETE: $BIN -verify -parameter integrity_check;optimizer=$o $f" ;;
				esac
				printf '%s\n' "$r"
			done
		done < "$tmp/list.$k" > "$tmp/out.$k"
	) &
	k=$((k + 1))
done
wait
end=$(date +%s)

cat "$tmp"/out.* > run.log
awk -v secs=$((end - start)) '
	/ errors out of / { for (i = 1; i <= NF; i++) if ($i == "errors") { e += $(i - 1); t += $(i + 3) }; runs++ }
	/^INCOMPLETE: / { bad++ }
	END {
		printf "%d errors out of %d tests and %d invocations in %d seconds\n", e, t, runs, secs
		if (bad) printf "%d invocations did not complete (see run.log)\n", bad
		exit (e > 0 || bad > 0)
	}' run.log
