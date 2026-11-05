#!/bin/sh
# test_asa.sh - tests for asa (POSIX.1-2024).  Run: sh test_asa.sh
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
U=${U:-$PWD/asa}
T=${TMPDIR:-/tmp}/test_asa.$$
mkdir -m 700 "$T" || exit 2
trap 'chmod -R u+rwx "$T" 2>/dev/null; rm -rf "$T"' EXIT
cd "$T" || exit 2
export LC_ALL=C
root=false; [ "$(id -u)" = 0 ] && root=true
nl='
'
# od -c of the output, blanks squeezed
printf '%s\n' "od -An -c | tr -s ' \\n' '  ' | sed 's/^ //; s/ \$//'" > oc.sh
t "space: the line as it is" 'a b' 0 sh -c 'printf " a b\n" | "$1"' sh "$U"
t "space: a newline after the last line" 'a \n' 0 sh -c 'printf " a\n" | "$1" | sh ./oc.sh' sh "$U"
t "space: lines one after another" 'a \n b \n' 0 sh -c 'printf " a\n b\n" | "$1" | sh ./oc.sh' sh "$U"
t "0: a newline before" 'a \n \n b \n' 0 sh -c 'printf " a\n0b\n" | "$1" | sh ./oc.sh' sh "$U"
t "0 on the first line" '\n a \n' 0 sh -c 'printf "0a\n" | "$1" | sh ./oc.sh' sh "$U"
t "1: a form feed" 'a \n \f b \n' 0 sh -c 'printf " a\n1b\n" | "$1" | sh ./oc.sh' sh "$U"
t "1 on the first line" '\f a \n' 0 sh -c 'printf "1a\n" | "$1" | sh ./oc.sh' sh "$U"
t "+: a carriage return instead of the newline" 'a \r b \n' 0 sh -c 'printf " a\n+b\n" | "$1" | sh ./oc.sh' sh "$U"
t "+ on the first line: as space" 'a \n' 0 sh -c 'printf "+a\n" | "$1" | sh ./oc.sh' sh "$U"
t "+ twice: overprinting" 'a \r b \r c \n' 0 sh -c 'printf " a\n+b\n+c\n" | "$1" | sh ./oc.sh' sh "$U"
t "another character: as space" 'a \n b \n' 0 sh -c 'printf "xa\n-b\n" | "$1" | sh ./oc.sh' sh "$U"
t "an empty line: an empty line" 'a \n \n b \n' 0 sh -c 'printf " a\n\n b\n" | "$1" | sh ./oc.sh' sh "$U"
t "a last line without a newline" 'a \n b \n' 0 sh -c 'printf " a\n b" | "$1" | sh ./oc.sh' sh "$U"
t "the first character only" ' 0 a' 0 sh -c 'printf "  0 a\n" | "$1"' sh "$U"
t "an empty input: nothing" '' 0 sh -c ': | "$1" | sh ./oc.sh' sh "$U"
printf ' one\n0two\n' > f1; printf '1three\n+four\n' > f2
t "files: each from its start" 'o n e \n \n t w o \n \f t h r e e \r f o u r \n' 0 sh -c '"$1" f1 f2 | sh ./oc.sh' sh "$U"
t "-: the standard input" 'o n e \n \n t w o \n x \n' 0 sh -c 'printf " x\n" | "$1" f1 - | sh ./oc.sh' sh "$U"
t "a missing file: the others written" 'o n e \n \n t w o \n' 0 sh -c '"$1" missing f1 | sh ./oc.sh' sh "$U"
t "a missing file: status 1" '' 1 sh -c '"$1" missing f1 > /dev/null' sh "$U"
t "a missing file: nothing on the standard error" '' 0 sh -c '"$1" missing 2>&1 >/dev/null; true' sh "$U"
# getopt() must not take an option after an operand for one (no permuting)
t "options after operands: asa f1 -x: -x is a file" '' 1 sh -c '"$1" f1 -x > /dev/null' sh "$U"

done_tests asa
