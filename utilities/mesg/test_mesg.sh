#!/bin/sh
# test_mesg.sh - tests for mesg (POSIX.1-2024).  Run: sh test_mesg.sh
cd "$(dirname "$0")" || exit 2
pass=0 fail=0
# t NAME EXPECTED-STDOUT EXPECTED-STATUS COMMAND...  (stdout compared without trailing newlines)
t() {
    _n=$1 _e=$2 _s=$3; shift 3
    _o=$("$@" 2>/dev/null); _r=$?
    if [ "$_o" = "$_e" ] && [ "$_r" = "$_s" ]; then pass=$((pass+1))
    else fail=$((fail+1)); printf 'FAIL %s\n  got:  [%s] status %s\n  want: [%s] status %s\n' "$_n" "$_o" "$_r" "$_e" "$_s"; fi
}
# ok NAME CONDITION-COMMAND...
ok() {
    _n=$1; shift
    if "$@"; then pass=$((pass+1)); else fail=$((fail+1)); printf 'FAIL %s\n' "$_n"; fi
}
done_tests() { echo "$1: $pass passed, $fail failed"; [ "$fail" = 0 ]; }
U=${U:-$PWD/mesg}
T=${TMPDIR:-/tmp}/test_mesg.$$
mkdir -m 700 "$T" || exit 2
trap 'chmod -R u+rwx "$T" 2>/dev/null; rm -rf "$T" "$T.tl"' EXIT
# tl: the test language, built here (../testlib/tl.c; ../testlib/tl.md)
TL=$(cd ../testlib && pwd) || exit 2; export TL
cc=${cc:-${CC:-}}
[ -n "$cc" ] || for c in c17 c99 cc; do command -v $c >/dev/null 2>&1 && { cc=$c; break; }; done
$cc -o "$T.tl" "$TL/tl.c" || { echo "cannot compile ../testlib/tl.c"; exit 2; }
TLBIN=$T.tl; export TLBIN
tl() { "$TLBIN" "$@"; }
# ptyrun FDS COMMAND...: COMMAND with the descriptors FDS on a terminal (../testlib/ptyrun.tl)
ptyrun() { PTYRUN_INPUT=${PTYRUN_INPUT-} PTYRUN_WAIT=${PTYRUN_WAIT-} "$TLBIN" "$TL/ptyrun.tl" "$@"; }   # (assignments before a function call need not be exported)
cd "$T" || exit 2
export LC_ALL=C
root=false; [ "$(id -u)" = 0 ] && root=true
nl='
'
export U

t "n: status 1, the group's permission gone" "1 --" 0 ptyrun 012 sh -c '"$U" n; r=$?; s=$(ls -lL "$(tty)" | cut -c6,9); echo "$r $s"'
t "y: status 0, the group may write" "0 w-" 0 ptyrun 012 sh -c '"$U" n; "$U" y; r=$?; s=$(ls -lL "$(tty)" | cut -c6,9); echo "$r $s"'
t "no operand after y: is y, status 0" "is y
0" 0 ptyrun 012 sh -c '"$U" y; "$U"; echo $?'
t "no operand after n: is n, status 1" "is n
1" 0 ptyrun 012 sh -c '"$U" n; "$U"; echo $?'
t "n takes away the permission of others too" "1 --" 0 ptyrun 012 sh -c 'chmod go+w "$(tty)" 2>/dev/null; "$U" n; r=$?; s=$(ls -lL "$(tty)" | cut -c6,9); echo "$r $s"'
t "others may write: allowed" "is y
0" 0 ptyrun 012 sh -c '"$U" n; chmod o+w "$(tty)"; "$U"; echo $?; "$U" n; exit 0'
t "y twice" "0" 0 ptyrun 012 sh -c '"$U" y; "$U" y; echo $?'
t "n twice" "1" 0 ptyrun 012 sh -c '"$U" n; "$U" n; echo $?'
t "y leaves the other bits" "same" 0 ptyrun 012 sh -c 'f=$(tty); "$U" n; a=$(ls -lL "$f" | cut -c1-5,7-8,10); "$U" y; b=$(ls -lL "$f" | cut -c1-5,7-8,10); [ "$a" = "$b" ] && echo same'
# which terminal: the standard input, output, error in turn
t "the terminal on the standard input only" "1 --" 0 ptyrun 0 sh -c '"$U" n > /dev/null 2>&1; r=$?; s=$(ls -lL "$(tty)" | cut -c6,9); echo "$r $s"'
t "the terminal on the standard output only" "is n" 0 ptyrun 1 sh -c '"$U" n < /dev/null 2>/dev/null; "$U" < /dev/null 2>/dev/null; exit 0'
t "the terminal on the standard error only" "1" 0 ptyrun 2 sh -c '"$U" n < /dev/null > /dev/null; echo $? > /dev/tty'
t "no terminal: status 2" "" 2 sh -c '"$U" < /dev/null > /dev/null 2>/dev/null' 
t "no terminal: a message" "1" 0 sh -c '"$U" < /dev/null 2>&1 >/dev/null | grep -c terminal'
t "a bad operand: status 2" "" 2 ptyrun 01 "$U" x
t "two operands: status 2" "" 2 ptyrun 01 "$U" y n
t "a usage message" "1" 0 sh -c '"$U" x < /dev/null 2>&1 | grep -c usage'
t "-- before the operand" "is n" 0 ptyrun 012 sh -c '"$U" -- n; "$U" --; exit 0'
t "an option: an error" "" 2 sh -c '"$U" -y < /dev/null 2>/dev/null'
# getopt() must not take an option after an operand for one (no permuting)
t "options after operands: mesg y -n: two operands" "" 2 ptyrun 01 "$U" y -n

done_tests mesg
