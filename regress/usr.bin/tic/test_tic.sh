#!/bin/sh
# test_tic.sh - tests for tic (NetBSD's, of lib/libcurses).  Run: sh test_tic.sh
cd "$(dirname "$0")" || exit 2
PROG_DIR=${PROG_DIR:-$PWD/../../../usr.bin/tic}; export PROG_DIR
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
U=${U:-$PROG_DIR/tic}
T=${TMPDIR:-/tmp}/test_tic.$$
mkdir -m 700 "$T" || exit 2
trap 'rm -rf "$T"' EXIT
cd "$T" || exit 2
export LC_ALL=C

# two terminals, one using the other, and an alias
printf 'aa|alias one|first terminal,\n\tcols#80, am, cup=\\E[%%i%%p1%%d;%%p2%%dH,\nbb|second,\n\tlines#24, use=aa,\n' > s.ti
t "no output by default" "" 0 "$U" s.ti
ok "the database is source.cdb" test -s s.ti.cdb
t "-s: the counts, on the standard error" "2 entries and 1 aliases written to x.cdb" 0 sh -c '"$1" -s -o x.cdb s.ti 2>&1' sh "$U"
ok "-o: the database named" test -s x.cdb
ok "the same database twice" cmp -s s.ti.cdb x.cdb
t "-c: only checked, nothing written" "" 0 "$U" -c -o c.cdb s.ti
ok "-c: no database" test ! -f c.cdb
# -S: the C source of the named entries; bb has aa's cup through use=
out=$("$U" -S s.ti bb)
ok "-S: a compiled_terms array" eval 'case $out in *"compiled_terms[]"*) true ;; *) false ;; esac'
ok "-S: the entry named" eval 'case $out in *'"'"'"bb"'"'"'*) true ;; *) false ;; esac'
ok "-S: use= brings aa's cup" eval 'case $out in *'"'"'\033\133\045\151'"'"'*) true ;; *) false ;; esac'
ok "-S: no database" test ! -f s.ti.cdb.tmp
# diagnostics
printf 'dd|x,\n\tuse=missing,\n' > m.ti
ok "a use= of nothing: a diagnostic" sh -c '"$1" -o m.cdb m.ti 2>&1 >/dev/null | grep -q missing' sh "$U"
printf 'cc|bad,\n\tcols#80, nosuchcap=1,\n' > bad.ti
ok "an unknown capability: a diagnostic" sh -c '"$1" -o b.cdb bad.ti 2>&1 >/dev/null | grep -q nosuchcap' sh "$U"
t "no source" "" 1 "$U"
t "a source that is not there" "" 1 "$U" /nonexistent/s.ti
t "an unknown option" "" 1 "$U" -z s.ti
ok "usage on the standard error" sh -c '"$1" -z s.ti 2>&1 | grep -q "^usage: tic"' sh "$U"

done_tests tic
