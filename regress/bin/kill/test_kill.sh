#!/bin/sh
# test_kill.sh - tests for kill (V86BSD).  Run: sh test_kill.sh
cd "$(dirname "$0")" || exit 2
PROG_DIR=${PROG_DIR:-$PWD/../../../bin/kill}; export PROG_DIR
pass=0 fail=0
# t NAME EXPECTED-STDOUT EXPECTED-STATUS COMMAND...  (stdout compared without trailing newlines)
t() {
    _n=$1 _e=$2 _s=$3; shift 3
    _o=$("$@" 2>/dev/null); _r=$?
    if [ "$_o" = "$_e" ] && [ "$_r" = "$_s" ]; then pass=$((pass+1))
    else fail=$((fail+1)); printf 'FAIL %s\n  got:  [%s] status %s\n  want: [%s] status %s\n' "$_n" "$_o" "$_r" "$_e" "$_s"; fi
}
# te NAME EXPECTED-OUTPUT EXPECTED-STATUS COMMAND...  (stdout and stderr together)
te() {
    _n=$1 _e=$2 _s=$3; shift 3
    _o=$("$@" 2>&1); _r=$?
    if [ "$_o" = "$_e" ] && [ "$_r" = "$_s" ]; then pass=$((pass+1))
    else fail=$((fail+1)); printf 'FAIL %s\n  got:  [%s] status %s\n  want: [%s] status %s\n' "$_n" "$_o" "$_r" "$_e" "$_s"; fi
}
# ok NAME CONDITION-COMMAND...
ok() {
    _n=$1; shift
    if "$@"; then pass=$((pass+1)); else fail=$((fail+1)); printf 'FAIL %s\n' "$_n"; fi
}
done_tests() { echo "$1: $pass passed, $fail failed"; [ "$fail" = 0 ]; }
export LC_ALL=C
U=${U:-$PROG_DIR/kill}
bg() { sleep 60 & echo $!; }   # a process to signal
gone() { ! kill -s 0 "$1" 2>/dev/null; }

ok "-l lists the names" sh -c 'l=" $("$1" -l) "; for n in HUP INT QUIT KILL TERM USR1 CHLD; do case $l in *" $n "*) ;; *) exit 1;; esac; done' sh "$U"
ok "-l: one line" sh -c '[ "$("$1" -l | wc -l)" -eq 1 ]' sh "$U"
t "-l 9" "KILL" 0 "$U" -l 9
t "-l 15" "TERM" 0 "$U" -l 15
t "-l of a status: 128 + n" "TERM" 0 "$U" -l 143
t "-l 129 is HUP" "HUP" 0 "$U" -l 129
t "-l of a bad status" "" 1 "$U" -l abc
t "-l of a number that is no signal" "" 1 "$U" -l 200
t "-l 0: no signal" "" 1 "$U" -l 0
# a target is started in the same shell (so that wait can say how it ended: 128 + signal)
t "-s TERM" "143" 0 sh -c 'sleep 60 & p=$!; "$1" -s TERM $p; wait $p; echo $?' sh "$U"
t "-TERM" "143" 0 sh -c 'sleep 60 & p=$!; "$1" -TERM $p; wait $p; echo $?' sh "$U"
t "-s KILL" "137" 0 sh -c 'sleep 60 & p=$!; "$1" -s KILL $p; wait $p; echo $?' sh "$U"
t "-KILL" "137" 0 sh -c 'sleep 60 & p=$!; "$1" -KILL $p; wait $p; echo $?' sh "$U"
t "-9" "137" 0 sh -c 'sleep 60 & p=$!; "$1" -9 $p; wait $p; echo $?' sh "$U"
t "-s 9" "137" 0 sh -c 'sleep 60 & p=$!; "$1" -s 9 $p; wait $p; echo $?' sh "$U"
t "-s HUP" "129" 0 sh -c 'sleep 60 & p=$!; "$1" -s HUP $p; wait $p; echo $?' sh "$U"
t "the default is TERM" "143" 0 sh -c 'sleep 60 & p=$!; "$1" $p; wait $p; echo $?' sh "$U"
t "-- ends the options" "143" 0 sh -c 'sleep 60 & p=$!; "$1" -- $p; wait $p; echo $?' sh "$U"
t "-s 0: only checks, the process lives" "0 alive" 0 sh -c 'sleep 60 & p=$!; "$1" -s 0 $p; r=$?; sleep 1; kill -s 0 $p && echo "$r alive"; kill -9 $p; wait $p 2>/dev/null; true' sh "$U"
t "-0 too" "0 alive" 0 sh -c 'sleep 60 & p=$!; "$1" -0 $p; r=$?; sleep 1; kill -s 0 $p && echo "$r alive"; kill -9 $p; wait $p 2>/dev/null; true' sh "$U"
t "the signal arrives (a trap prints it)" "got USR1" 0 sh -c 'sh -c '"'"'trap "echo got USR1; exit 0" USR1; while :; do sleep 1; done'"'"' & p=$!; sleep 1; "$1" -USR1 $p; wait $p' sh "$U"
t "several processes" "143 143" 0 sh -c 'sleep 60 & a=$!; sleep 60 & b=$!; "$1" $a $b; wait $a; x=$?; wait $b; echo $x $?' sh "$U"
t "a process that is not there: status 1" "" 1 "$U" -s 0 999999
te "...with a message" "kill: 999999: No such process" 1 "$U" -s 0 999999
t "one failure, the others still get it" "143" 0 sh -c 'sleep 60 & a=$!; "$1" 999999 $a 2>/dev/null; wait $a; echo $?' sh "$U"
te "a negative pid is a process group" "kill: -999999: No such process" 1 "$U" -- -999999
te "an unknown signal name" "kill: NOPE: unknown signal" 2 "$U" -s NOPE 1
te "an unknown -name" "kill: NOPE: unknown signal" 2 "$U" -NOPE 1
te "not a pid" "kill: abc: not a process id" 1 "$U" abc
te "no operand" "usage: kill [-s signal_name] pid ...
       kill -l [exit_status]
       kill [-signal_name] pid ...
       kill [-signal_number] pid ..." 2 "$U" -s TERM
te "no arguments" "usage: kill [-s signal_name] pid ...
       kill -l [exit_status]
       kill [-signal_name] pid ...
       kill [-signal_number] pid ..." 2 "$U"
done_tests kill
