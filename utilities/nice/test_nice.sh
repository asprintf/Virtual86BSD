#!/bin/sh
# test_nice.sh - tests for nice (POSIX.1-2024).  Run: sh test_nice.sh
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
U=${U:-$PWD/nice}
T=${TMPDIR:-/tmp}/test_nice.$$
mkdir -m 700 "$T" || exit 2
trap 'chmod -R u+rwx "$T" 2>/dev/null; rm -rf "$T" "$T.tl"' EXIT
# tl: the test language, built here (../tl/tl.c; ../tl/tl.md)
TL=$(cd ../testlib && pwd) || exit 2; export TL
cc=${cc:-${CC:-}}
[ -n "$cc" ] || for c in c17 c99 cc; do command -v $c >/dev/null 2>&1 && { cc=$c; break; }; done
$cc -o "$T.tl" "$TL/../tl/tl.c" || { echo "cannot compile ../tl/tl.c"; exit 2; }
TLBIN=$T.tl; export TLBIN
tl() { "$TLBIN" "$@"; }
cd "$T" || exit 2
export LC_ALL=C
root=false; [ "$(id -u)" = 0 ] && root=true
nl='
'
# nv: the nice value it runs with
nv='print(nice(0));'
b=$(tl -e "$nv")
lim() { _v=$(($1)); [ $_v -gt 19 ] && _v=19; [ $_v -lt -20 ] && _v=-20; echo $_v; }
t "no -n: 10" "$(lim "$b + 10")" 0 "$U" "$TLBIN" -e "$nv"
t "-n 5" "$(lim "$b + 5")" 0 "$U" -n 5 "$TLBIN" -e "$nv"
t "-n5" "$(lim "$b + 5")" 0 "$U" -n5 "$TLBIN" -e "$nv"
t "-n +3" "$(lim "$b + 3")" 0 "$U" -n +3 "$TLBIN" -e "$nv"
t "-n 0" "$b" 0 "$U" -n 0 "$TLBIN" -e "$nv"
t "a large increment: the highest value" "19" 0 "$U" -n 100 "$TLBIN" -e "$nv"
t "nested" "$(lim "$b + 3 + 4")" 0 "$U" -n 3 "$U" -n 4 "$TLBIN" -e "$nv"
if $root; then
    t "-n -5 (as root)" "$(lim "$b - 5")" 0 "$U" -n -5 "$TLBIN" -e "$nv"
    t "a large negative increment (as root): the lowest value" "-20" 0 "$U" -n -100 "$TLBIN" -e "$nv"
else
    t "-n -5 without privileges: the utility is run anyway" "$b" 0 "$U" -n -5 "$TLBIN" -e "$nv"
    t "-n -5 without privileges: a warning" "" 0 sh -c '"$1" -n -5 true 2>&1 | grep -q .' sh "$U"
fi
t "the utility's status" "" 7 "$U" sh -c 'exit 7'
t "the arguments" "a b${nl}c" 0 "$U" printf '%s\n' "a b" c
t "--" "$(lim "$b + 10")" 0 "$U" -- "$TLBIN" -e "$nv"
t "-n 2 --" "$(lim "$b + 2")" 0 "$U" -n 2 -- "$TLBIN" -e "$nv"
t "PATH is searched" "ran" 0 sh -c 'mkdir -p bin && printf "#!/bin/sh\necho ran\n" > bin/prog && chmod +x bin/prog && PATH=$PWD/bin:$PATH "$1" prog' sh "$U"
t "a script without #!: run by sh" "plain" 0 sh -c 'printf "echo plain\n" > plain && chmod +x plain && "$1" ./plain' sh "$U"
t "not found: 127" "" 127 "$U" no_such_utility_x9
t "not found by pathname: 127" "" 127 "$U" ./no_such_file
# a directory in PATH that cannot be searched does not make it 126
$root || t "not found past an unsearchable directory: 127" "" 127 sh -c 'mkdir -p nd && chmod 0 nd && PATH=$PWD/nd:$PATH "$1" no_such_utility_here' sh "$U"
t "not executable: 126" "" 126 sh -c ': > nx && chmod 644 nx && "$1" ./nx' sh "$U"
t "a directory: 126" "" 126 sh -c 'mkdir -p dir && "$1" ./dir' sh "$U"
t "no utility" "" 125 "$U"
t "-n without a utility" "" 125 "$U" -n 5
t "-n x" "" 125 "$U" -n x true
t "-n 3x" "" 125 "$U" -n 3x true
t "-n empty" "" 125 "$U" -n "" true
t "-n out of range" "" 125 "$U" -n 99999999999999999999 true
t "an unknown option" "" 125 "$U" -x true
# getopt() must not take an option after an operand for one (no permuting)
t "options after operands: nice printf %s -n: -n is printf's" "-n" 0 "$U" printf '%s' -n

done_tests nice
