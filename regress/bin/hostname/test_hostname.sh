#!/bin/sh
# test_hostname.sh - tests for hostname (V86BSD).  Run: sh test_hostname.sh
cd "$(dirname "$0")" || exit 2
PROG_DIR=${PROG_DIR:-$PWD/../../../bin/hostname}; export PROG_DIR
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
U=${U:-$PROG_DIR/hostname}
t "the name is uname's" "$(uname -n)" 0 "$U"
t "-s: up to the first dot" "$(uname -n | sed 's/\..*//')" 0 "$U" -s
te "too many operands" "usage: hostname [-s] [name]" 1 "$U" a b
te "-s and a name" "usage: hostname [-s] [name]" 1 "$U" -s name
t "a bad option" "" 1 "$U" -x
# setting changes the host: only on V86BSD, as root, and put back
if [ "$(uname -s)" = V86BSD ] && [ "$(id -u)" = 0 ]; then
    old=$(uname -n)
    t "set a name" "hostname-test" 0 sh -c '"$1" hostname-test && "$1"' sh "$U"
    t "uname sees it" "hostname-test" 0 uname -n
    t "-s of a name with a domain" "short" 0 sh -c '"$1" short.example.org && "$1" -s' sh "$U"
    t "the whole name is kept" "short.example.org" 0 "$U"
    te "a name of 65 bytes" "hostname: Invalid argument" 1 "$U" aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa
    "$U" "$old"
fi
done_tests hostname
