#!/bin/sh
# test_more.sh - tests for more (POSIX.1-2024).  Run: sh test_more.sh
# The terminal tests drive more on a pseudo-terminal (tests/term.tl).
cd "$(dirname "$0")" || exit 2
PROG_DIR=${PROG_DIR:-$PWD/../../../usr.bin/more}; export PROG_DIR
pass=0 fail=0
# t NAME EXPECTED-STDOUT EXPECTED-STATUS COMMAND...  (stdout compared without trailing newlines)
t() {
    _n=$1 _e=$2 _s=$3; shift 3
    _o=$("$@" 2>/dev/null); _r=$?
    if [ "$_o" = "$_e" ] && [ "$_r" = "$_s" ]; then pass=$((pass+1))
    else fail=$((fail+1)); printf 'FAIL %s\n  got:  [%s] status %s\n  want: [%s] status %s\n' "$_n" "$_o" "$_r" "$_e" "$_s"; fi
}
# tb NAME EXPECTED-OD-C COMMAND...  (exact bytes of stdout, as od -c)
tb() {
    _n=$1 _e=$2; shift 2
    _o=$("$@" 2>/dev/null | od -An -c | tr -s ' \n' ' ' | sed 's/^ //;s/ $//')
    if [ "$_o" = "$_e" ]; then pass=$((pass+1))
    else fail=$((fail+1)); printf 'FAIL %s\n  got:  %s\n  want: %s\n' "$_n" "$_o" "$_e"; fi
}
# ok NAME CONDITION-COMMAND...
ok() {
    _n=$1; shift
    if "$@"; then pass=$((pass+1)); else fail=$((fail+1)); printf 'FAIL %s\n' "$_n"; fi
}
done_tests() { echo "$1: $pass passed, $fail failed"; [ "$fail" = 0 ]; }
U=${U:-$PROG_DIR/more}
# a UTF-8 locale for the multibyte tests, which are skipped without one (POSIX
# names none): $UTF8_LOCALE, or C.UTF-8, or any UTF-8 locale that locale -a lists
U8=${UTF8_LOCALE:-$(locale -a 2>/dev/null | grep -i -E '^c\.utf-?8$' | sed 1q)}
[ -n "$U8" ] || U8=$(locale -a 2>/dev/null | grep -i -E '\.utf-?8$' | sed 1q)
export U8
T=${TMPDIR:-/tmp}/test_more.$$
mkdir -m 700 "$T" || exit 2
trap 'rm -rf "$T" "$T.tl"' EXIT
# tl: the test language, built here (../../../usr.bin/tl/tl.c; ../../../usr.bin/tl/tl.md)
TL=$(cd ../../lib/testlib && pwd) || exit 2; export TL
cc=${cc:-${CC:-}}
[ -n "$cc" ] || for c in c17 c99 cc; do command -v $c >/dev/null 2>&1 && { cc=$c; break; }; done
$cc -o "$T.tl" "$TL/../../../usr.bin/tl/tl.c" || { echo "cannot compile ../../../usr.bin/tl/tl.c"; exit 2; }
TLBIN=$T.tl; export TLBIN
tl() { "$TLBIN" "$@"; }
cd "$T" || exit 2
i=1; while [ $i -le 30 ]; do printf 'l%02d\n' $i; i=$((i+1)); done > f30
printf 'a\n\n\n\nb\n' > blanks

# ---- standard output not a terminal: a copy ----
t "filter copy" "$(cat f30)" 0 "$U" f30
t "filter two files" "$(cat f30 blanks)" 0 "$U" f30 blanks
t "filter -s" "a

b" 0 "$U" -s blanks
t "filter other options ignored" "$(cat f30)" 0 "$U" -n 3 -e -c f30
t "filter stdin" "x" 0 sh -c 'echo x | "$1"' sh "$U"
t "filter -" "a
x
a" 0 sh -c 'echo x | "$1" blanks - blanks | grep -v "^$" | grep -v b' sh "$U"
tb "filter keeps bytes" 'a \b b \r \n 001' sh -c 'printf "a\bb\r\n\001" > raw; "$1" raw' sh "$U"
t "filter -s: a 1-byte last line is not blank" "x" 0 sh -c 'printf "\n\nx" > lb; "$1" -s lb | tail -n 1' sh "$U"
t "filter missing file" "x" 1 sh -c 'echo x > ok; "$1" nofile ok' sh "$U"
[ -c /dev/full ] && t "filter write error" "" 1 sh -c '"$1" f30 > /dev/full' sh "$U"
t "bad -n" "" 1 "$U" -n 0 f30
t "bad option" "" 1 "$U" -z f30

# ---- on a terminal ----
# tests/term.tl: more on a pseudo-terminal, the screen as a terminal shows it
out=$("$TLBIN" "$OLDPWD/tests/term.tl" "$U" 2>&1); rc=$?
np=$(printf '%s\n' "$out" | sed -n 's/^PASSES \([0-9]*\) FAILS \([0-9]*\)$/\1/p')
nf=$(printf '%s\n' "$out" | sed -n 's/^PASSES \([0-9]*\) FAILS \([0-9]*\)$/\2/p')
printf '%s\n' "$out" | grep -v '^PASSES '
pass=$((pass + ${np:-0})); fail=$((fail + ${nf:-1}))
[ "$rc" = 0 ] || [ -n "$nf" ] || fail=$((fail + 1))
# getopt() must not take an option after an operand for one (no permuting)
t "options after operands: more f -c: -c is a file" "x" 1 sh -c 'd=${TMPDIR:-/tmp}/opts.$$ && mkdir "$d" && cd "$d" && echo x > f && { "$1" f -c; }; r=$?; cd / && rm -rf "$d"; exit $r' sh "$U"

done_tests more
