#!/bin/sh
# test_fuser.sh - tests for fuser (POSIX.1-2024).  Run: sh test_fuser.sh
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
U=${U:-$PWD/fuser}
T=${TMPDIR:-/tmp}/test_fuser.$$
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
# processes that use files here: started in the background, killed at the
# end
pids=
bg() { "$@" >/dev/null 2>&1 & pids="$pids $!"; last=$!; }
trap 'kill $pids 2>/dev/null; chmod -R u+rwx "$T" 2>/dev/null; rm -rf "$T" "$T.tl"' EXIT
mkdir d d2
echo data > f
echo other > g
: > unused
sleep 60 < f >/dev/null 2>&1 & pids="$pids $!"; p1=$!
(cd d && exec sleep 60 >/dev/null 2>&1) & pids="$pids $!"; p2=$!
cp "$(command -v sleep)" ./mysleep
bg ./mysleep 60; p3=$last
printf 'some bytes to map\n' > mapped
bg "$TLBIN" -e 'mmap("mapped"); sleep(60);'; p4=$last
sleep 1
me=$(id -un)
t "an open file: the process ID on the standard output" " $p1" 0 sh -c '"$1" f 2>/dev/null' sh "$U"
t "an open file: the pathname and a colon on the standard error" "f:" 0 sh -c '"$1" f 2>&1 >/dev/null' sh "$U"
t "both to one file: one line" "f: $p1" 0 sh -c '"$1" f 2>&1' sh "$U"
t "-u: the user name" "f: $p1($me)" 0 sh -c '"$1" -u f 2>&1' sh "$U"
t "-u: on the standard error" "f: ($me)" 0 sh -c '"$1" -u f 2>&1 >/dev/null | sed "s/:/: /"' sh "$U"
t "a file no one uses" "unused:" 0 sh -c '"$1" unused 2>&1' sh "$U"
t "the working directory: c" "d: ${p2}c" 0 sh -c '"$1" d 2>&1' sh "$U"
t "the program: e" "mysleep: ${p3}e" 0 sh -c '"$1" mysleep 2>&1' sh "$U"
t "a mapped file: m" "mapped: ${p4}m" 0 sh -c '"$1" mapped 2>&1' sh "$U"
t "the root directory: r" "yes" 0 sh -c '"$1" / 2>&1 | tr " " "\n" | grep -x "${2}r" >/dev/null && echo yes' sh "$U" $p1
t "the working directory and the root: rc" "yes" 0 sh -c 'cd / && "$1" / 2>&1 | tr " " "\n" | grep -E -x "[0-9]+rc" >/dev/null && echo yes' sh "$U"
t "several files: a line each, in order" "f: $p1${nl}unused:${nl}d: ${p2}c" 0 sh -c '"$1" f unused d 2>&1' sh "$U"
t "the process IDs only on the standard output" " $p1 ${p2}" 0 sh -c '"$1" f d 2>/dev/null' sh "$U"
t "-f: only the named file" "f: $p1" 0 sh -c '"$1" -f f 2>&1' sh "$U"
t "a pathname as given" "./d/../f: $p1" 0 sh -c '"$1" ./d/../f 2>&1' sh "$U"
t "fuser itself is not listed" "g:" 0 "$TLBIN" -e 'open("g", "r"); dup2(1, 2); exec(argv);' "$U" g
t "-c: every file of the file system" "yes" 0 sh -c '"$1" -c d2 2>/dev/null | tr " " "\n" | grep -x $2 >/dev/null && echo yes' sh "$U" $p1
t "without -c: only the directory" "d2:" 0 sh -c '"$1" d2 2>&1' sh "$U"
# a block device that holds a mounted file system: the one here, if its
# device file is in /dev
blk=$(tl -e '
func walk(dir, dev) {
    var names = listdir(dir), dirs = [];
    if (names == nil)
        return nil;
    for (var f : names) {
        var p = dir + "/" + f, l = lstat(p);
        if (l == nil)
            continue;
        if (l.type == "d") {
            push(dirs, p);
            continue;
        }
        var s = stat(p);
        if (s != nil && s.type == "b" && s.rdev == dev)
            return p;
    }
    for (var d : dirs) {
        var r = walk(d, dev);
        if (r != nil)
            return r;
    }
    return nil;
}
var r = walk("/dev", stat(".").dev);
if (r != nil)
    print(r);' 2>/dev/null)
if [ -n "$blk" ]; then
    t "a mounted block device: the processes of its file system" "yes" 0 sh -c '"$1" "$3" 2>/dev/null | tr " " "\n" | grep -x $2 >/dev/null && echo yes' sh "$U" $p1 "$blk"
    t "a mounted block device, -f: only the device file" "" 1 sh -c '"$1" -f "$3" 2>/dev/null | tr " " "\n" | grep -x $2' sh "$U" $p1 "$blk"
fi
# errors
t "a file that does not exist: status 1" "" 1 "$U" no_such_file
t "a file that does not exist: a diagnostic on its line" "yes" 0 sh -c '"$1" no_such_file 2>&1 | grep "^no_such_file: ." >/dev/null && echo yes' sh "$U"
t "a file that does not exist: the others done" "no_such_file: X${nl}f: $p1" 0 sh -c '"$1" no_such_file f 2>&1 | sed "1s/: .*/: X/"' sh "$U"
t "no operand" "" 2 "$U"
t "an unknown option" "" 2 "$U" -x f
t "--" "f: $p1" 0 sh -c '"$1" -- f 2>&1' sh "$U"
# getopt() must not take an option after an operand for one (no permuting)
t "options after operands: fuser f -u: -u is a file" " $p1" 1 "$U" f -u
[ -c /dev/full ] && t "a write error" "" 1 sh -c '"$1" f > /dev/full 2>/dev/null' sh "$U"

done_tests fuser
