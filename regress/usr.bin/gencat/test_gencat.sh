#!/bin/sh
# test_gencat.sh - tests for gencat (POSIX.1-2024).  Run: sh test_gencat.sh
cd "$(dirname "$0")" || exit 2
PROG_DIR=${PROG_DIR:-$PWD/../../../usr.bin/gencat}; export PROG_DIR
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
U=${U:-$PROG_DIR/gencat}
T=${TMPDIR:-/tmp}/test_gencat.$$
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
RC="$TLBIN $OLDPWD/tests/readcat.tl"
TD=$OLDPWD/tests
cat > a.msg <<'X'
$ a comment
$set 1 the first set
1 one
2 two words
$set 3
5 five
X
t "a catalog" "1 1 one${nl}1 2 two words${nl}3 5 five${nl}size 1 depth 3${nl}ok" 0 sh -c '"$1" a.cat a.msg && $2 a.cat' sh "$U" "$RC"
t "nothing on the standard output" "" 0 sh -c 'rm -f a.cat; "$1" a.cat a.msg' sh "$U"
t "the magic number" "ok" 0 sh -c 'od -An -tx1 a.cat | sed 1q | grep -q -e "^ de 08 04 96" -e "^ 96 04 08 de" && echo ok'
t "without \$set: set NL_SETD (1)" "1 1 x${nl}size 1 depth 1${nl}ok" 0 sh -c 'printf "1 x\n" > b.msg; rm -f b.cat; "$1" b.cat b.msg && $2 b.cat' sh "$U" "$RC"
t "one blank separates: the rest is the text" "1 1  two spaces  ${nl}size 1 depth 1${nl}ok" 0 sh -c 'printf "1  two spaces  \n" > b.msg; rm -f b.cat; "$1" b.cat b.msg && $2 b.cat' sh "$U" "$RC"
t "a tab separates too" "1 1 x${nl}size 1 depth 1${nl}ok" 0 sh -c 'printf "1\tx\n" > b.msg; rm -f b.cat; "$1" b.cat b.msg && $2 b.cat' sh "$U" "$RC"
t "an empty text with its blank: an empty message" "1 1 ${nl}1 2 b${nl}size 1 depth 2${nl}ok" 0 sh -c 'printf "1 \n2 b\n" > b.msg; rm -f b.cat; "$1" b.cat b.msg && $2 b.cat' sh "$U" "$RC"
t "escapes" '1 1 \n\011\013\010\015\014\AB\001q"' 0 sh -c 'printf "1 \\\\n\\\\t\\\\v\\\\b\\\\r\\\\f\\\\\\\\\\\\101\\\\102\\\\1\\\\q\\\\\"\n" > b.msg; rm -f b.cat; "$1" b.cat b.msg && $2 b.cat | sed 1q' sh "$U" "$RC"
t "a continued line" "1 1 This line continues to the next line" 0 sh -c 'printf "1 This line continues \\\\\nto the next line\n" > b.msg; rm -f b.cat; "$1" b.cat b.msg && $2 b.cat | sed 1q' sh "$U" "$RC"
cat > q.msg <<'X'
$quote "
1 "  quoted  "
2 "with \" inside"
3 ""
4 not quoted
$quote
5 "no quoting now"
X
t "\$quote" '1 1   quoted  
1 2 with " inside
1 3 
1 4 not quoted
1 5 "no quoting now"' 0 sh -c 'rm -f q.cat; "$1" q.cat q.msg && $2 q.cat | sed "\$d" | sed "\$d"' sh "$U" "$RC"
t "\$quote: no closing quote: status 1" "" 1 sh -c 'printf "\$quote \"\n1 \"open\n" > b.msg; rm -f b.cat; "$1" b.cat b.msg 2>/dev/null' sh "$U"
# merging into a catalog
cp a.cat m.cat
cat > m.msg <<'X'
$set 1
2 TWO
4 four
$set 2
1 new set
X
t "merged: kept, replaced, added" "1 1 one${nl}1 2 TWO${nl}1 4 four${nl}2 1 new set${nl}3 5 five" 0 sh -c '"$1" m.cat m.msg && $2 m.cat | sed -n "/^[0-9]/p"' sh "$U" "$RC"
cp a.cat d.cat
printf '$set 1\n1\n' > d.msg
t "a number alone deletes the message" "1 2 two words${nl}3 5 five" 0 sh -c '"$1" d.cat d.msg && $2 d.cat | sed -n "/^[0-9]/p"' sh "$U" "$RC"
t "a number alone for a message not there: nothing" "1 1 x" 0 sh -c 'printf "1 x\n2\n" > e.msg; rm -f e.cat; "$1" e.cat e.msg && $2 e.cat | sed -n "/^[0-9]/p"' sh "$U" "$RC"
cp a.cat ds.cat
printf '$delset 1 bye\n' > ds.msg
t "\$delset" "3 5 five" 0 sh -c '"$1" ds.cat ds.msg && $2 ds.cat | sed -n "/^[0-9]/p"' sh "$U" "$RC"
t "\$delset of what a msgfile before defined" "3 5 five" 0 sh -c 'rm -f ds2.cat; "$1" ds2.cat a.msg ds.msg && $2 ds2.cat | sed -n "/^[0-9]/p"' sh "$U" "$RC"
t "several msgfiles: the later wins" "1 1 b" 0 sh -c 'printf "1 a\n" > x1.msg; printf "1 b\n" > x2.msg; rm -f x.cat; "$1" x.cat x1.msg x2.msg && $2 x.cat | sed -n "/^[0-9]/p"' sh "$U" "$RC"
t "out of order: taken" "1 1 a${nl}1 3 c" 0 sh -c 'printf "3 c\n1 a\n" > o.msg; rm -f o.cat; "$1" o.cat o.msg && $2 o.cat | sed -n "/^[0-9]/p"' sh "$U" "$RC"
t "empty lines and comments" "1 1 x" 0 sh -c 'printf "\n\$ comment\n\$\n   \n1 x\n" > c.msg; rm -f c.cat; "$1" c.cat c.msg && $2 c.cat | sed -n "/^[0-9]/p"' sh "$U" "$RC"
t "an unknown \$ directive: a warning, left out" "1 1 x" 0 sh -c 'printf "\$frob 3\n1 x\n" > c.msg; rm -f c.cat; "$1" c.cat c.msg 2>/dev/null && $2 c.cat | sed -n "/^[0-9]/p"' sh "$U" "$RC"
t "an empty msgfile: an empty catalog" "size 1 depth 1${nl}ok" 0 sh -c ': > n.msg; rm -f n.cat; "$1" n.cat n.msg && $2 n.cat' sh "$U" "$RC"
# - for catfile and msgfile
t "catfile -: the standard output" "1 1 one${nl}1 2 two words${nl}3 5 five" 0 sh -c '"$1" - a.msg > out.cat && $2 out.cat | sed -n "/^[0-9]/p"' sh "$U" "$RC"
t "msgfile -: the standard input" "1 1 x" 0 sh -c 'rm -f i.cat; printf "1 x\n" | "$1" i.cat - && $2 i.cat | sed -n "/^[0-9]/p"' sh "$U" "$RC"
# the table: as catgets() looks
{ echo '$set 1'; i=1; while [ $i -le 60 ]; do echo "$i m$i"; i=$((i + 1)); done; echo '$set 9'; echo '7 seven'; } > big.msg
t "a larger table: each message where it is looked for" "ok" 0 sh -c 'rm -f big.cat; "$1" big.cat big.msg && $2 big.cat | sed -n "\$p"' sh "$U" "$RC"
t "the size and depth GNU gencat would choose" "size 5 depth 1" 0 sh -c 'printf "1 a\n2 b\n3 c\n4 d\n5 e\n" > s.msg; rm -f s.cat; "$1" s.cat s.msg && $2 s.cat | grep size' sh "$U" "$RC"
if command -v getconf >/dev/null && getconf GNU_LIBC_VERSION >/dev/null 2>&1 && ${CC:-cc} -o catread "$TD/catread.c" 2>/dev/null; then
    t "the C library's catgets() reads it" "" 0 ./catread "$T/big.cat"
    t "the C library's catgets() after a merge" "" 0 ./catread "$T/m.cat"
fi
# errors
t "a line of something else: status 1" "" 1 sh -c 'printf "x y\n" > e.msg; rm -f e.cat; "$1" e.cat e.msg 2>/dev/null' sh "$U"
t "an error: no catalog written" "no" 0 sh -c '[ -f e.cat ] && echo yes || echo no'
t "an error: the line" "gencat: e.msg:1: a line that is neither a directive nor a message" 1 sh -c '"$1" e.cat e.msg 2>&1' sh "$U"
t "message number 0: status 1" "" 1 sh -c 'printf "0 x\n" > e.msg; "$1" e.cat e.msg 2>/dev/null' sh "$U"
t "\$set 0: status 1" "" 1 sh -c 'printf "\$set 0\n" > e.msg; "$1" e.cat e.msg 2>/dev/null' sh "$U"
t "\$set x: status 1" "" 1 sh -c 'printf "\$set x\n" > e.msg; "$1" e.cat e.msg 2>/dev/null' sh "$U"
t "a number with letters: status 1" "" 1 sh -c 'printf "12a x\n" > e.msg; "$1" e.cat e.msg 2>/dev/null' sh "$U"
t "catfile not a catalog: status 1" "" 1 sh -c 'echo junk > j.cat; "$1" j.cat a.msg 2>/dev/null' sh "$U"
t "catfile not a catalog: left alone" "junk" 0 cat j.cat
t "a msgfile missing: status 1" "" 1 "$U" z.cat missing.msg
t "no operands: usage" "" 2 "$U"
t "one operand: usage" "" 2 "$U" a.cat
t "an option: usage" "" 2 "$U" -x a.cat a.msg
t "-- before the operands" "1 1 one" 0 sh -c 'rm -f dd.cat; "$1" -- dd.cat a.msg && $2 dd.cat | sed 1q' sh "$U" "$RC"
# getopt() must not take an option after an operand for one (no permuting)
t "options after operands: gencat x.cat a.msg -x: -x is a msgfile" "" 1 sh -c 'rm -f oa.cat; "$1" oa.cat a.msg -x 2>/dev/null' sh "$U"

done_tests gencat
