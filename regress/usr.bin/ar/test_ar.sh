#!/bin/sh
# test_ar.sh - tests for ar (POSIX.1-2024).  Run: sh test_ar.sh
cd "$(dirname "$0")" || exit 2
PROG_DIR=${PROG_DIR:-$PWD/../../../usr.bin/ar}; export PROG_DIR
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
U=${U:-$PROG_DIR/ar}
T=${TMPDIR:-/tmp}/test_ar.$$
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
export TZ=UTC0
printf 'one\n' > one; printf 'two\n' > two; printf 'three\n' > three; printf 'odd' > odd
printf 'f global func text 0 4\nlocalsym local func text 4 4\nu global notype undef 0 0\n' | $MKELF f.o
printf 'g global object data 0 4\nh weak func text 0 1\n' | $MKELF g.o
touch -t 202001020304.05 one two three odd

t "-r: a new archive, with a message" "ar: creating a.a" 0 sh -c '"$1" -r a.a one two 2>&1' sh "$U"
t "-t: the names" "one${nl}two" 0 "$U" -t a.a
t "the magic string" "!<arch>" 0 sh -c 'head -n 1 a.a'
t "-c: no message" "" 0 sh -c '"$1" -rc c.a one 2>&1' sh "$U"
t "-tv" "rw-r--r-- $(id -u)/$(id -g) 4 Jan  2 03:04 2020 one" 0 sh -c 'chmod 644 one; "$1" -rc v.a one && "$1" -tv v.a' sh "$U"
t "-t with operands: those, as given" "dir/two" 0 sh -c '"$1" -t a.a dir/two' sh "$U"
t "-t: an operand not there: status 1" "one" 1 sh -c '"$1" -t a.a one nothere 2>/dev/null' sh "$U"
t "-r -v: a for added, r for replaced" "r - one${nl}a - three" 0 "$U" -rv a.a one three
t "-r: replaced in place, added at the end" "one${nl}two${nl}three" 0 "$U" -t a.a
t "-p: the contents, in the order of the operands" "two${nl}one" 0 "$U" -p a.a two one
t "-t: in the order of the operands" "three${nl}one" 0 "$U" -t a.a three one
t "-pv: a header before each" "${nl}<two>${nl}${nl}two" 0 "$U" -pv a.a two
t "-p all" "one${nl}two${nl}three" 0 "$U" -p a.a
t "an odd size: padded" "ok" 0 sh -c '"$1" -rc o.a odd one && [ "$("$1" -p o.a odd)" = odd ] && [ "$("$1" -p o.a one)" = one ] && echo ok' sh "$U"
# -u
t "-u: an older file not replaced" "two" 0 sh -c 'cp a.a u.a; printf "TWO\n" > two2; touch -t 201901010000 two2; mkdir sub; cp two2 sub/two; touch -t 201901010000 sub/two; "$1" -ru u.a sub/two; "$1" -p u.a two' sh "$U"
t "-u: a newer one replaced" "NEW" 0 sh -c 'printf "NEW\n" > sub/two; touch -t 202101010000 sub/two; "$1" -ru u.a sub/two; "$1" -p u.a two' sh "$U"
# positions
t "-r -a posname" "one${nl}two${nl}x${nl}three" 0 sh -c 'cp a.a p.a; echo x > x; "$1" -ra two p.a x && "$1" -t p.a' sh "$U"
t "-r -b posname" "x${nl}one${nl}two${nl}three" 0 sh -c 'cp a.a p.a; "$1" -rb one p.a x && "$1" -t p.a' sh "$U"
t "-r -i posname (as -b)" "one${nl}x${nl}two${nl}three" 0 sh -c 'cp a.a p.a; "$1" -ri two p.a x && "$1" -t p.a' sh "$U"
t "-r -a: a posname not there: at the end" "one${nl}two${nl}three${nl}x" 0 sh -c 'cp a.a p.a; "$1" -ra none p.a x && "$1" -t p.a' sh "$U"
t "-m -b: a posname not there: to the end" "two${nl}three${nl}one" 0 sh -c 'cp a.a p.a; "$1" -mb none p.a one && "$1" -t p.a' sh "$U"
t "-r: a file that cannot be read: nothing changed, status 1" "one${nl}two${nl}three" 1 sh -c 'cp a.a p.a; echo new > newfile; "$1" -r p.a newfile missingfile 2>/dev/null; s=$?; "$1" -t p.a; exit $s' sh "$U"
t "-m: to the end" "two${nl}three${nl}one" 0 sh -c 'cp a.a m.a; "$1" -m m.a one && "$1" -t m.a' sh "$U"
t "-m -b" "three${nl}one${nl}two" 0 sh -c 'cp a.a m.a; "$1" -mb one m.a three && "$1" -t m.a' sh "$U"
t "-m -a" "one${nl}three${nl}two" 0 sh -c 'cp a.a m.a; "$1" -ma one m.a three && "$1" -t m.a' sh "$U"
t "-m: several, in the order of the operands" "three${nl}two${nl}one" 0 sh -c 'cp a.a m.a; "$1" -m m.a two one && "$1" -t m.a' sh "$U"
# -d, -q
t "-d -v" "d - two" 0 sh -c 'cp a.a d.a; "$1" -dv d.a two' sh "$U"
t "-d: gone" "one${nl}three" 0 "$U" -t d.a
t "-d: not there: status 1" "" 1 sh -c '"$1" -d d.a nothere 2>/dev/null' sh "$U"
t "-q: appended, also a name already there" "one${nl}two${nl}three${nl}one" 0 sh -c 'cp a.a q.a; "$1" -q q.a one && "$1" -t q.a' sh "$U"
t "-q: a new archive" "ar: creating q2.a" 0 sh -c '"$1" -q q2.a one 2>&1' sh "$U"
t "an operand matches the first of the same name" "one" 0 sh -c 'printf "ONE\n" > sub/one; "$1" -q q.a sub/one; "$1" -p q.a one' sh "$U"
# -x
mkdir xd && cd xd
t "-x: the files" "one${nl}two" 0 sh -c '"$1" -x ../a.a one two && cat one two' sh "$U"
t "-x -v" "x - three" 0 "$U" -xv ../a.a three
t "-x: the mode of the member" "-rwxr-x---" 0 sh -c 'cd ..; chmod 750 odd; "$1" -rc mode.a odd; chmod 644 odd; cd xd; rm -f odd; "$1" -x ../mode.a odd; ls -l odd | cut -c1-10' sh "$U"
t "-x: the time of extraction" "ok" 0 sh -c 'rm -f one; "$1" -x ../a.a one; [ -n "$(find one -newer ../one)" ] && echo ok' sh "$U"
t "-x -C: a file there is not replaced" "changed" 1 sh -c 'echo changed > one; "$1" -xC ../a.a one 2>/dev/null; s=$?; cat one; exit $s' sh "$U"
t "-x without -C: replaced" "one" 0 sh -c '"$1" -x ../a.a one; cat one' sh "$U"
t "-x all" "one${nl}three${nl}two" 0 sh -c 'rm -f *; "$1" -x ../a.a && ls' sh "$U"
cd ..
# long names
t "long names kept" "a_rather_long_file_name.txt${nl}one" 0 sh -c 'echo long > a_rather_long_file_name.txt; "$1" -rc l.a a_rather_long_file_name.txt one && "$1" -t l.a' sh "$U"
t "long names: the contents" "long" 0 "$U" -p l.a a_rather_long_file_name.txt
# an archive with a member whose name is longer than NAME_MAX
tl -e '
func hdr(name, size, mode) { return sprintf("%-16s%-12s%-6s%-6s%-8s%-10d`\n", name, "0", "0", "0", mode, size); }
var names = repeat("n", 300) + "/\n";
writefile("too.a", "!<arch>\n" + hdr("//", len(names), "") + names + hdr("/0", 3, "100644") + "abc\n");'
max=$(getconf NAME_MAX . 2>/dev/null || echo 255)
t "-x: a name longer than NAME_MAX: an error" "" 1 sh -c 'cd xd && "$1" -x ../too.a 2>/dev/null' sh "$U"
t "-x -T: cut to NAME_MAX" "$max" 0 sh -c 'cd xd && "$1" -xT ../too.a && ls | grep "^nnnn" | awk "{ print length }"' sh "$U"
# the symbol table
# the symbol table's names, each @ its member
cat > symtab.tl <<'X'
var d = readfile(argv[0]);
if (substr(d, 8, 16) != "/               ") {
    print("none");
    exit(0);
}
var t = substr(d, 68, int(substr(d, 56, 10)));
var n = unpack(">I", t)[0];
var names = split(substr(t, 4 + 4 * n), "\0"), out = [];
for (var i = 0; i < n; i++)
    push(out, names[i] + "@" + trim(substr(d, unpack(">I", t, 4 + 4 * i)[0], 16)));
print(join(out, " "));
X
# addmember.tl ARCHIVE FILE: FILE added as a member, without a symbol table
cat > addmember.tl <<'X'
var d = readfile(argv[1]);
appendfile(argv[0], sprintf("%-16s%-12s%-6s%-6s%-8s%-10d`\n", argv[1] + "/", "0", "0", "0", "100644", len(d)) +
           d + (len(d) % 2 ? "\n" : ""));
X
t "objects: a symbol table of their global symbols" "f@f.o/ g@g.o/ h@g.o/" 0 sh -c '"$1" -rc s.a one f.o g.o && "$TLBIN" symtab.tl s.a' sh "$U"
t "no objects: no symbol table" "none" 0 "$TLBIN" symtab.tl a.a
t "-d: the symbol table made again" "g@g.o/ h@g.o/" 0 sh -c '"$1" -d s.a f.o && "$TLBIN" symtab.tl s.a' sh "$U"
t "-s alone: a symbol table made" "none${nl}f@f.o/" 0 sh -c 'printf "!<arch>\n" > n.a; "$TLBIN" addmember.tl n.a f.o; "$TLBIN" symtab.tl n.a; "$1" -s n.a && "$TLBIN" symtab.tl n.a' sh "$U"
t "the permissions of an archive kept" "-rw-------" 0 sh -c 'chmod 600 a.a; "$1" -r a.a one; ls -l a.a | cut -c1-10' sh "$U"
t "the historical form: ar rv" "r - one" 0 "$U" rv a.a one
t "a missing archive: status 1" "" 1 sh -c '"$1" -t missing.a 2>/dev/null' sh "$U"
t "not an archive: status 1" "" 1 sh -c '"$1" -t one 2>/dev/null' sh "$U"
t "no operation: usage" "" 2 "$U" a.a
t "two operations: usage" "" 2 "$U" -t -x a.a
t "-a without -r or -m: usage" "" 2 "$U" -ta one a.a
t "-d without files: usage" "" 2 "$U" -d a.a
# getopt() must not take an option after an operand for one (no permuting)
t "options after operands: ar -t a.a -v: -v is a name" "" 1 sh -c '"$1" -t a.a -v 2>/dev/null' sh "$U"

done_tests ar
