#!/bin/sh
# test_strip.sh - tests for strip (POSIX.1-2024).  Run: sh test_strip.sh
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
U=${U:-$PWD/strip}
T=${TMPDIR:-/tmp}/test_strip.$$
mkdir -m 700 "$T" || exit 2
trap 'chmod -R u+rwx "$T" 2>/dev/null; rm -rf "$T" "$T.tl"' EXIT
# tl: the test language, built here (../testlib/tl.c; ../testlib/tl.md)
TL=$(cd ../testlib && pwd) || exit 2; export TL
cc=${cc:-${CC:-}}
[ -n "$cc" ] || for c in c17 c99 cc; do command -v $c >/dev/null 2>&1 && { cc=$c; break; }; done
$cc -o "$T.tl" "$TL/tl.c" || { echo "cannot compile ../testlib/tl.c"; exit 2; }
TLBIN=$T.tl; export TLBIN
tl() { "$TLBIN" "$@"; }
cd "$T" || exit 2
export LC_ALL=C
root=false; [ "$(id -u)" = 0 ] && root=true
nl='
'
MKELF="$TLBIN $TL/mkelf.tl"
INFO="$TLBIN $OLDPWD/tests/elfinfo.tl"
cat > spec <<'X'
f global func text 0x10 8
d global object data 4 4
b local object bss 0 32
u global notype undef 0 0
w weak func text 0 1
r_used local func text 32 4
unused local func text 40 4
n local object debug 0 1
file.c local file abs 0 0
X
for o in "" -32 -be "-32 -be"; do
    $MKELF x.o $o < spec
    t "an object $o: before" "sections: .text .data .bss .rodata .debug_info .rela.text .symtab .strtab .shstrtab
symbols: b r_used unused n file.c f d u w
ok" 0 $INFO x.o
    t "an object $o: the debugging section and the unused locals gone" "sections: .text .data .bss .rodata .rela.text .symtab .strtab .shstrtab
symbols: r_used f d w
ok" 0 sh -c '"$1" x.o && $2 x.o' sh "$U" "$INFO"
    t "an object $o: stripped again, the same" "same" 0 sh -c 'cp x.o x2.o; "$1" x2.o && cmp -s x.o x2.o && echo same' sh "$U"
done
t "the permissions kept" "-rwx--x---" 0 sh -c '$2 y.o < spec; chmod 710 y.o; "$1" y.o; ls -l y.o | cut -c1-10' sh "$U" "$MKELF"
CC=${CC:-cc}
cat > m.c <<'X'
#include <stdio.h>
static int counter = 3;
static int helper(int x) { return x * counter; }
int api(int x) { return helper(x) + 1; }
int main(void) { printf("%d\n", api(4)); return 0; }
X
cat > lib.c <<'X'
int twice(int x) { return 2 * x; }
X
cat > use.c <<'X'
#include <stdio.h>
int twice(int);
int main(void) { printf("%d\n", twice(21)); return 0; }
X
if $CC -g -o m m.c 2>/dev/null && ./m > /dev/null; then
    t "an executable: it runs as before" "13" 0 sh -c '"$1" m && ./m' sh "$U"
    t "an executable: no symbol table, no debugging" "symbols: none${nl}0" 1 sh -c '$1 m | sed -n 2p; $1 m | sed -n 1p | grep -c debug' sh "$INFO"
    t "an executable: smaller" "ok" 0 sh -c '$2 -g -o m0 m.c && "$1" m && [ $(wc -c < m) -lt $(wc -c < m0) ] && echo ok' sh "$U" "$CC"
    t "an object: it links and runs" "13" 0 sh -c '$2 -g -c m.c && "$1" m.o && $2 -o m2 m.o && ./m2' sh "$U" "$CC"
    t "an object: the global symbols kept" "symbols: api main printf" 0 sh -c '$1 m.o | sed -n 2p' sh "$INFO"
    if $CC -g -c lib.c && $CC -g -c use.c; then
        t "an archive: each member stripped, it links" "42" 0 sh -c 'rm -f l.a; ar -rc l.a lib.o && "$1" l.a && $2 -o u use.o l.a && ./u' sh "$U" "$CC"
        t "an archive: its members stripped" "0" 1 sh -c 'mkdir ax && cd ax && ar -x ../l.a && $1 lib.o | grep -c debug' sh "$INFO"
        if $CC -shared -fPIC -o libt.so lib.c 2>/dev/null && $CC -o us use.c -L. -lt 2>/dev/null; then
            t "a shared object: still usable" "42" 0 sh -c '"$1" libt.so && LD_LIBRARY_PATH=. ./us' sh "$U"
            t "a shared object: no symbol table" "symbols: none" 0 sh -c '$1 libt.so | sed -n 2p' sh "$INFO"
        fi
    fi
fi
# errors
printf 'text\n' > text.txt
t "not an object: status 1" "" 1 sh -c '"$1" text.txt 2>/dev/null' sh "$U"
t "not an object: left as it is" "text" 0 cat text.txt
t "not an object among others: they are stripped" "symbols: r_used f d w" 1 sh -c '$3 z.o < spec; "$1" text.txt z.o 2>/dev/null; s=$?; $2 z.o | sed -n 2p; exit $s' sh "$U" "$INFO" "$MKELF"
t "a missing file: status 1" "" 1 sh -c '"$1" missing.o 2>/dev/null' sh "$U"
t "no operands: usage" "" 2 "$U"
t "an option: usage" "" 2 "$U" -s x.o
# getopt() must not take an option after an operand for one (no permuting)
t "options after operands: strip x.o -s: -s is a file" "" 1 sh -c '"$1" x.o -s 2>/dev/null' sh "$U"

done_tests strip
