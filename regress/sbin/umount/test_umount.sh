#!/bin/sh
# test_umount.sh - tests for umount (V86BSD).  Run: sh test_umount.sh
cd "$(dirname "$0")" || exit 2
PROG_DIR=${PROG_DIR:-$PWD/../../../sbin/umount}; export PROG_DIR
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
U=${U:-$PROG_DIR/umount}
M=${M:-$PROG_DIR/../mount/mount}
T=${TMPDIR:-/tmp}/test_umount.$$
if [ "$(uname -s)" != V86BSD ] || [ "$(id -u)" != 0 ]; then
    echo "umount: 0 passed, 0 failed (skipped: needs root on V86BSD)"
    exit 0
fi
[ -x "$M" ] || { cd ../../../sbin/mount && make > /dev/null && M=$PWD/mount; cd - > /dev/null; }
mkdir -m 700 "$T" || exit 2
mkdir "$T/a" "$T/b"
trap '"$U" "$T/a" "$T/b" 2>/dev/null; rm -rf "$T"' EXIT
usage="usage: umount [-v] special | node ...
       umount -a [-v] [-t type[,type...]]"
mounted() { "$M" | grep -c " on $1 "; }

"$M" -t tmpfs tmpfs "$T/a"
t "it is mounted" "1" 0 mounted "$T/a"
t "umount by directory" "0" 0 sh -c '"$1" "$2" && "$3" | grep -c " on $2 "; true' sh "$U" "$T/a" "$M"
te "-v says what it did" "$T/a: unmounted" 0 sh -c '"$1" -t tmpfs tmpfs "$2/a" && "$3" -v "$2/a"' sh "$M" "$T" "$U"
te "not mounted" "umount: $T/a: not currently mounted" 1 "$U" "$T/a"
te "a name that is not there at all" "umount: /nowhere/x: not currently mounted" 1 "$U" /nowhere/x
t "a path with .. is made absolute" "0" 0 sh -c '"$1" -t tmpfs tmpfs "$2/a" && "$3" "$2/b/../a" && "$1" | grep -c " on $2/a "; true' sh "$M" "$T" "$U"
t "two at once" "00" 0 sh -c '"$1" -t tmpfs tmpfs "$2/a"; "$1" -t tmpfs tmpfs "$2/b"; "$3" "$2/a" "$2/b"; echo $("$1" | grep -c " on $2/a ")$("$1" | grep -c " on $2/b ")' sh "$M" "$T" "$U"
t "one that fails, the other is done" "1 0" 0 sh -c '"$2" -t tmpfs tmpfs "$3/a"; "$1" /nowhere "$3/a" 2>/dev/null; echo "$?" $("$2" | grep -c " on $3/a ")' sh "$U" "$M" "$T"
te "/ is busy" "umount: /: Device or resource busy" 1 "$U" /
te "something mounted below is busy" "umount: $T/a: Device or resource busy" 1 sh -c '"$1" -t tmpfs tmpfs "$2/a"; mkdir "$2/a/m"; "$1" -t tmpfs tmpfs "$2/a/m"; "$3" "$2/a"' sh "$M" "$T" "$U"
t "...and the inner one goes first" "00" 0 sh -c '"$1" "$2/a/m" && "$1" "$2/a"; echo $("$3" | grep -c " on $2/a/m ")$("$3" | grep -c " on $2/a ")' sh "$U" "$T" "$M"
te "a file open on it is busy" "umount: $T/a: Device or resource busy" 1 sh -c '"$1" -t tmpfs tmpfs "$2/a"; echo x > "$2/a/f"; cd "$2/a"; "$3" "$2/a"' sh "$M" "$T" "$U"
"$U" "$T/a" 2>/dev/null
te "no arguments" "$usage" 1 "$U"
te "-a with an operand" "$usage" 1 "$U" -a x
te "-t without -a" "$usage" 1 "$U" -t tmpfs "$T/a"
t "-a -t of a type nothing has: nothing happens" "" 0 "$U" -a -t nosuchtype
# -a: only if the only tmpfs mounts are ours (it unmounts every one)
if [ "$("$M" -t tmpfs | wc -l | tr -d ' ')" = 0 ]; then
    t "-a -t tmpfs: all of them, the last first" "00" 0 sh -c '"$1" -t tmpfs tmpfs "$2/a"; "$1" -t tmpfs tmpfs "$2/b"; "$3" -a -t tmpfs; echo $("$1" -t tmpfs | wc -l | tr -d " ")$("$1" | grep -c "$2")' sh "$M" "$T" "$U"
fi
t "-a keeps /, /dev and /proc" "ok" 0 sh -c '"$1" -a -t ext2,proc,devtmpfs; "$2" | grep " on / " > /dev/null && "$2" | grep " on /proc " > /dev/null && echo ok' sh "$U" "$M"
done_tests umount
