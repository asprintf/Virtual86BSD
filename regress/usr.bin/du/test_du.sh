#!/bin/sh
# test_du.sh - tests for du (POSIX.1-2024).  Run: sh test_du.sh
cd "$(dirname "$0")" || exit 2
PROG_DIR=${PROG_DIR:-$PWD/../../../usr.bin/du}; export PROG_DIR
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
U=${U:-$PROG_DIR/du}
T=${TMPDIR:-/tmp}/test_du.$$
mkdir -m 700 "$T" || exit 2
trap 'chmod -R u+rwx "$T" 2>/dev/null; rm -rf "$T"' EXIT
cd "$T" || exit 2
export LC_ALL=C
root=false; [ "$(id -u)" = 0 ] && root=true
nl='
'
# a tree: d/{f (100000 bytes), s/{g, t/}, h (a hard link to f), l -> s}
mk() {
    rm -rf d && mkdir -p d/s/t && head -c 100000 /dev/zero | tr '\0' x > d/f && echo y > d/s/g &&
    ln d/f d/h && ln -s s d/l
}
mk
size() { "$U" "$@" | sed -n '$s/ .*//p'; }

# the format: "%d %s\n"
t "the format" "ok" 0 sh -c '"$1" d | awk "\$0 !~ /^[0-9]+ [^ ]+\$/ { bad = 1 } END { if (!bad) print \"ok\" }"' sh "$U"
t "directories only, each after its contents" "d/s/t
d/s
d" 0 sh -c '"$1" d | sed "s/^[0-9]* //"' sh "$U"
t "no operand: the current directory" ". ./s ./s/t" 0 sh -c 'cd d && "$1" | sed "s/^[0-9]* //" | sort | tr "\n" " " | sed "s/ \$//"' sh "$U"
t "-s: only the total" "d" 0 sh -c '"$1" -s d | sed "s/^[0-9]* //"' sh "$U"
t "-s: the same total" "same" 0 sh -c 'a=$("$1" d | sed -n "\$s/ .*//p"); b=$("$1" -s d | sed "s/ .*//"); [ "$a" = "$b" ] && echo same' sh "$U"
t "-a: the files too" "d/f d/l d/s/g" 0 sh -c '"$1" -a d | sed "s/^[0-9]* //" | grep -v -x -e d -e d/s -e d/s/t | sort | tr "\n" " " | sed "s/ \$//"' sh "$U"
t "-a: one entry for a hard-linked file" "1" 0 sh -c '"$1" -a d | grep -c -e " d/f\$" -e " d/h\$"' sh "$U"
t "-a -s: only the total" "1" 0 sh -c '"$1" -as d | wc -l | tr -d " "' sh "$U"
t "-sa: the same" "1" 0 sh -c '"$1" -s -a d | wc -l | tr -d " "' sh "$U"
t "a file operand is written without -a" "d/f" 0 sh -c '"$1" d/f | sed "s/^[0-9]* //"' sh "$U"
t "a file operand with -s" "d/f" 0 sh -c '"$1" -s d/f | sed "s/^[0-9]* //"' sh "$U"
# sizes: 512-byte units, rounded up; -k: 1024-byte units
t "100000 bytes: at least 196 units" "ok" 0 sh -c 'n=$("$1" d/f | sed "s/ .*//"); [ "$n" -ge 196 ] && echo ok' sh "$U"
t "-k: half, rounded up" "ok" 0 sh -c 'a=$("$1" d/f | sed "s/ .*//"); b=$("$1" -k d/f | sed "s/ .*//"); [ "$b" = $(( (a + 1) / 2 )) ] && echo ok' sh "$U"
t "-k: a directory's total, rounded up" "ok" 0 sh -c 'a=$("$1" -s d/s | sed "s/ .*//"); b=$("$1" -sk d/s | sed "s/ .*//"); [ "$b" = $(( (a + 1) / 2 )) ] && echo ok' sh "$U"
t "a directory: its own size and its contents" "ok" 0 sh -c 'a=$("$1" -s d/s | sed "s/ .*//"); g=$("$1" d/s/g | sed "s/ .*//"); t=$("$1" -s d/s/t | sed "s/ .*//"); [ "$a" -ge $((g + t)) ] && echo ok' sh "$U"
t "an empty file: 0" "0 e" 0 sh -c ': > e && "$1" e' sh "$U"
rm -f e
# a file met more than once is counted and written once
t "a hard link is counted once" "same" 0 sh -c 'mkdir x && head -c 100000 /dev/zero | tr "\0" x > x/f && a=$("$1" -s x | sed "s/ .*//") && ln x/f x/h && b=$("$1" -s x | sed "s/ .*//") && rm -rf x && [ "$a" = "$b" ] && echo same' sh "$U"
t "an operand twice: written once" "1" 0 sh -c '"$1" -s d d | wc -l | tr -d " "' sh "$U"
t "a file twice: written once" "1" 0 sh -c '"$1" d/f d/f | wc -l | tr -d " "' sh "$U"
t "an operand within another one met before: nothing" "d" 0 sh -c '"$1" -s d d/s | sed "s/^[0-9]* //"' sh "$U"
t "an operand within another met after: both" "d/s d" 0 sh -c '"$1" -s d/s d | sed "s/^[0-9]* //" | tr "\n" " " | sed "s/ \$//"' sh "$U"
t "the hard link as an operand after the file: nothing" "d/f" 0 sh -c '"$1" d/f d/h | sed "s/^[0-9]* //"' sh "$U"
# symbolic links: counted as links unless -H or -L
t "a link operand: the link itself" "d/l" 0 sh -c '"$1" d/l | sed "s/^[0-9]* //"' sh "$U"
t "a link operand: not followed" "ok" 0 sh -c 'a=$("$1" -s d/l | sed "s/ .*//"); b=$("$1" -s d/s | sed "s/ .*//"); [ "$a" -lt "$b" ] && echo ok' sh "$U"
t "-H: the operand followed" "d/l/t d/l" 0 sh -c '"$1" -H d/l | sed "s/^[0-9]* //" | tr "\n" " " | sed "s/ \$//"' sh "$U"
t "-H: the size of the target" "same" 0 sh -c 'a=$("$1" -sH d/l | sed "s/ .*//"); b=$("$1" -s d/s | sed "s/ .*//"); [ "$a" = "$b" ] && echo same' sh "$U"
t "-H: links within not followed" "0${nl}d/l" 0 sh -c 'mkdir y && ln -s ../d/s y/m && "$1" -H y | sed "s/^[0-9]* //" | grep -c "y/m/" ; rm -rf y; "$1" -H d/l | sed -n "\$s/^[0-9]* //p"' sh "$U"
t "-L: links within followed" "y/m/t y/m y" 0 sh -c 'mkdir y && ln -s ../d/s y/m && "$1" -L y | sed "s/^[0-9]* //" | tr "\n" " " | sed "s/ \$//"; rm -rf y' sh "$U"
t "-L: a directory met through a link and itself: once" "1" 0 sh -c '"$1" -L d | grep -c -e " d/s\$" -e " d/l\$"' sh "$U"
t "-L: a loop is not followed for ever" "ok" 0 sh -c 'mkdir z && ln -s .. z/up && "$1" -L z >/dev/null 2>&1; rm -rf z; echo ok' sh "$U"
t "-H -L: the last wins (-L)" "y/m/t y/m y" 0 sh -c 'mkdir y && ln -s ../d/s y/m && "$1" -H -L y | sed "s/^[0-9]* //" | tr "\n" " " | sed "s/ \$//"; rm -rf y' sh "$U"
t "-L -H: the last wins (-H)" "y" 0 sh -c 'mkdir y && ln -s ../d/s y/m && "$1" -L -H y | sed "s/^[0-9]* //" | tr "\n" " " | sed "s/ \$//"; rm -rf y' sh "$U"
t "-L: a dangling link is an error" "" 1 sh -c 'ln -s nowhere dl; "$1" -L dl; r=$?; rm -f dl; exit $r' sh "$U"
t "a dangling link without -L: the link" "dl" 0 sh -c 'ln -s nowhere dl; "$1" dl | sed "s/^[0-9]* //"; r=$?; rm -f dl; exit $r' sh "$U"
# -x: the same device only (here all of it is on one)
t "-x: the same total on one device" "same" 0 sh -c 'a=$("$1" -sx d | sed "s/ .*//"); b=$("$1" -s d | sed "s/ .*//"); [ "$a" = "$b" ] && echo same' sh "$U"
t "-ax: the files too" "ok" 0 sh -c '"$1" -ax d | grep -q " d/f\$" && echo ok' sh "$U"
if [ -d /dev/shm ] && [ "$(df -P /dev 2>/dev/null | awk 'NR==2 { print $NF }')" = /dev ] && [ "$(df -P /dev/shm 2>/dev/null | awk 'NR==2 { print $NF }')" = /dev/shm ]; then
    t "-x: another file system left out" "0" 1 sh -c '"$1" -x /dev 2>/dev/null | grep -c " /dev/shm\$"' sh "$U"
    t "without -x: it is there" "1" 0 sh -c '"$1" /dev 2>/dev/null | grep -c " /dev/shm\$"' sh "$U"
fi
# pathnames: as given, joined with /
t "a trailing slash" "d/s/t d/s/" 0 sh -c '"$1" d/s/ | sed "s/^[0-9]* //" | tr "\n" " " | sed "s/ \$//"' sh "$U"
t "./ in the operand" "./d/s/t ./d/s" 0 sh -c '"$1" ./d/s | sed "s/^[0-9]* //" | tr "\n" " " | sed "s/ \$//"' sh "$U"
t "a name with a space" "a b/c a b" 0 sh -c 'mkdir -p "a b/c" && "$1" "a b" | sed "s/^[0-9]* //" | tr "\n" " " | sed "s/ \$//"; rm -rf "a b"' sh "$U"
# errors
t "a missing operand: status 1" "" 1 "$U" nonexistent
t "a missing operand: a message" "1" 0 sh -c '"$1" nonexistent 2>&1 >/dev/null | grep -c nonexistent' sh "$U"
t "a missing operand among others" "d/s/t" 1 sh -c '"$1" nonexistent d/s/t | sed "s/^[0-9]* //"; "$1" nonexistent >/dev/null 2>&1' sh "$U"
if ! $root; then
    t "an unreadable directory: status 1, still written" "d/s/t d/s d" 1 sh -c 'chmod 0 d/s/t; "$1" d 2>/dev/null | sed "s/^[0-9]* //" | tr "\n" " " | sed "s/ \$//"; "$1" d >/dev/null 2>&1; r=$?; chmod 755 d/s/t; exit $r' sh "$U"
    t "an unreadable directory: a message" "1" 0 sh -c 'chmod 0 d/s/t; "$1" d 2>&1 >/dev/null | grep -c "d/s/t"; chmod 755 d/s/t' sh "$U"
fi
t "an unknown option: status 2" "" 2 "$U" -z
t "a usage message" "1" 0 sh -c '"$1" -z 2>&1 | grep -c usage' sh "$U"
t "-- ends the options" "-a" 0 sh -c 'mkdir -- -a && "$1" -s -- -a | sed "s/^[0-9]* //"; rm -rf -- -a' sh "$U"
# getopt() must not take an option after an operand for one (no permuting)
t "options after operands: du d -s: -s is a file" "" 1 sh -c '"$1" d -s >/dev/null 2>&1' sh "$U"
t "a write error: status 1" "" 1 sh -c '[ -w /dev/full ] || exit 1; "$1" d > /dev/full 2>/dev/null' sh "$U"
# deep trees: beyond the descriptors open at once
t "a deep tree" "ok" 0 sh -c 'p=deep; i=0; while [ $i -lt 300 ]; do p=$p/x; i=$((i+1)); done; mkdir -p "$p" && n=$("$1" deep | wc -l) && [ "$n" -eq 301 ] && echo ok; rm -rf deep' sh "$U"

done_tests du
