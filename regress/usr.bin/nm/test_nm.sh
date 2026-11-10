#!/bin/sh
# test_nm.sh - tests for nm (POSIX.1-2024).  Run: sh test_nm.sh
cd "$(dirname "$0")" || exit 2
PROG_DIR=${PROG_DIR:-$PWD/../../../usr.bin/nm}; export PROG_DIR
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
U=${U:-$PROG_DIR/nm}
T=${TMPDIR:-/tmp}/test_nm.$$
mkdir -m 700 "$T" || exit 2
trap 'chmod -R u+rwx "$T" 2>/dev/null; rm -rf "$T" "$T.tl"' EXIT
# tl: the test language, built here (../../../usr.bin/tl/tl.c; ../../../usr.bin/tl/tl.md)
TL=$(cd ../../lib/testlib && pwd) || exit 2; export TL
cc=${cc:-${CC:-}}
[ -n "$cc" ] || for c in c17 c99 cc; do command -v $c >/dev/null 2>&1 && { cc=$c; break; }; done
$cc -o "$T.tl" "$TL/../../../usr.bin/tl/tl.c" || { echo "cannot compile ../../../usr.bin/tl/tl.c"; exit 2; }
TLBIN=$T.tl; export TLBIN
tl() { "$TLBIN" "$@"; }
cd "$T" || exit 2
export LC_ALL=C
root=false; [ "$(id -u)" = 0 ] && root=true
nl='
'
MKELF="$TLBIN $TL/mkelf.tl"
cat > spec <<'X'
f global func text 0x10 8
d global object data 4 4
b local object bss 0 32
r local object rodata 1 2
u global notype undef 0 0
a global notype abs 99 0
c global object common 8 16
w weak func text 0 1
wu weak notype undef 0 0
vo weak object data 8 4
r_rel local func text 32 4
n local object debug 0 1
file.c local file abs 0 0
X
$MKELF x.o < spec
ALL='a A 99 0
b b 0 32
c C 16 16
d D 4 4
f T 16 8
n N 0 1
r r 1 2
r_rel t 32 4
u U 0 0
vo V 8 4
w W 0 1
wu w 0 0'
t "the symbols, their types, decimal by default" "$ALL" 0 "$U" x.o
t "-P: hexadecimal" "a A 63 0
b b 0 20
c C 10 10
d D 4 4
f T 10 8" 0 sh -c '"$1" -P x.o | sed 5q' sh "$U"
t "-t d with -P" "a A 99 0" 0 sh -c '"$1" -P -t d x.o | sed 1q' sh "$U"
t "-t o" "f T 20 10" 0 sh -c '"$1" -t o x.o | grep "^f "' sh "$U"
t "-o" "f T 20 10" 0 sh -c '"$1" -o x.o | grep "^f "' sh "$U"
t "-x" "f T 10 8" 0 sh -c '"$1" -x x.o | grep "^f "' sh "$U"
t "-t x" "f T 10 8" 0 sh -c '"$1" -t x x.o | grep "^f "' sh "$U"
t "-g: the global (and weak) ones" "a${nl}c${nl}d${nl}f${nl}u${nl}vo${nl}w${nl}wu" 0 sh -c '"$1" -g x.o | cut -d" " -f1' sh "$U"
t "-u: the undefined ones" "u U 0 0${nl}wu w 0 0" 0 "$U" -u x.o
t "-v: by value" "b${nl}n${nl}u${nl}w${nl}wu${nl}r${nl}d${nl}vo${nl}c${nl}f${nl}r_rel${nl}a" 0 sh -c '"$1" -v x.o | cut -d" " -f1' sh "$U"
t "-f: the redundant symbols too" "file.c f 0 0" 0 sh -c '"$1" -f x.o | grep "^file.c"' sh "$U"
t "-e: the external and static ones" "$ALL" 0 "$U" -e x.o
t "-A: the file on each line" "x.o: a A 99 0" 0 sh -c '"$1" -A x.o | sed 1q' sh "$U"
# classes and byte orders
for o in -32 -be "-32 -be"; do
    $MKELF y.o $o < spec
    t "the same symbols: $o" "$ALL" 0 "$U" y.o
done
# several files and archives
printf 'g global func text 0 4\n' | $MKELF g.o
t "several files: a line naming each" "x.o:${nl}a A 99 0${nl}g.o:${nl}g T 0 4" 0 sh -c '"$1" x.o g.o | sed -n "1,2p;/^g.o:/,\$p"' sh "$U"
t "several files with -A: no such lines" "x.o: a A 99 0${nl}g.o: g T 0 4" 0 sh -c '"$1" -A x.o g.o | sed -n "1p;\$p"' sh "$U"
"${AR:-ar}" -rc lib.a x.o g.o 2>/dev/null || { printf '!<arch>\n' > lib.a; }
if "$U" lib.a > /dev/null 2>&1; then
    t "an archive: a line for each member" "lib.a[x.o]:${nl}lib.a[g.o]:" 0 sh -c '"$1" lib.a | grep ":\$"' sh "$U"
    t "an archive with -A" "lib.a[g.o]: g T 0 4" 0 sh -c '"$1" -A lib.a | tail -n 1' sh "$U"
fi
# no symbols, errors
printf 'not an object\n' > text.txt
t "not an object file: status 1" "" 1 "$U" text.txt
t "not an object file: a message" "1" 0 sh -c '"$1" text.txt 2>&1 | grep -c "not an object file"' sh "$U"
# no section headers: a file without a symbol table
tl -e 'var d = readfile(argv[0]);
writefile(argv[1], substr(d, 0, 40) + repeat("\0", 8) + substr(d, 48, 12) + repeat("\0", 4) + substr(d, 64));' x.o nosym.o
t "no symbols: reported, status 0" "nm: nosym.o: no symbols" 0 sh -c '"$1" nosym.o 2>&1' sh "$U"
t "a missing file: status 1" "" 1 "$U" missing.o
t "a missing file among others" "g T 0 4" 0 sh -c '"$1" missing.o g.o 2>/dev/null | grep -v ":\$"' sh "$U"
t "-g with -u: usage" "" 2 "$U" -g -u x.o
t "-t z: usage" "" 2 "$U" -t z x.o
t "no operands: usage" "" 2 "$U"
# getopt() must not take an option after an operand for one (no permuting)
t "options after operands: nm x.o -u: -u is a file" "" 1 sh -c '"$1" x.o -u > /dev/null 2>&1' sh "$U"

done_tests nm
