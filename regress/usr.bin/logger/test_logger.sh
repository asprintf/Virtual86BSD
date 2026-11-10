#!/bin/sh
# test_logger.sh - tests for logger (POSIX.1-2024).  Run: sh test_logger.sh
cd "$(dirname "$0")" || exit 2
PROG_DIR=${PROG_DIR:-$PWD/../../../usr.bin/logger}; export PROG_DIR
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
U=${U:-$PROG_DIR/logger}
T=${TMPDIR:-/tmp}/test_logger.$$
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
# a log daemon for the tests (tl), on a socket here (logger -u); each
# message it receives is a line of the file log, without the time stamp
cat > logd.tl <<'X'
var path = argv[0], out = argv[1], kind = argv[2];
var stamp = "^(<[0-9]+>)[A-Z][a-z][a-z] [ 123][0-9] [0-9][0-9]:[0-9][0-9]:[0-9][0-9] ";
func put(m) {
    appendfile(out, (matchat(m, stamp) != nil ? sub(m, stamp, "\\1") : "BADSTAMP " + m) + "\n");
}
if (kind == "dgram") {
    var s = unixdgram(path);
    writefile(path + ".ready", "");
    for (;;)
        put(fdread(s, 1 << 20));
}
var s = unixlisten(path);
writefile(path + ".ready", "");
for (;;) {
    var c = accept(s), buf = "";
    for (;;) {
        var b = fdread(c, 65536);
        if (b == nil || b == "")
            break;
        buf += b;
        for (var z = index(buf, "\0"); z >= 0; z = index(buf, "\0")) {
            put(substr(buf, 0, z));
            buf = substr(buf, z + 1);
        }
    }
    close(c);
}
X
"$TLBIN" logd.tl "$T/d.sock" "$T/log" dgram & d1=$!
"$TLBIN" logd.tl "$T/s.sock" "$T/slog" stream & d2=$!
trap 'kill $d1 $d2 2>/dev/null; chmod -R u+rwx "$T" 2>/dev/null; rm -rf "$T" "$T.tl"' EXIT
i=0; while [ ! -e d.sock.ready ] || [ ! -e s.sock.ready ]; do sleep 1; i=$((i+1)); [ $i -gt 20 ] && { echo "no log daemon"; exit 2; }; done
# lg EXPECTED-LOG LOGGER-ARGS...: log, then the lines the daemon got
lg() {
    _n=$1 _e=$2; shift 2
    t "$_n" "$_e" 0 sh -c ': > log; "$0" "$@" || exit; sleep 1; cat log' "$U" -u "$T/d.sock" "$@"
}
lg "a string: user.notice, the tag" "<13>me: hello" -t me hello
lg "strings: joined by spaces" "<13>me: a b  c" -t me a "b " c
lg "-p local0.err" "<131>me: x" -t me -p local0.err x
lg "-p user.debug" "<15>me: x" -t me -p user.debug x
lg "-p local7.emerg" "<184>me: x" -t me -p local7.emerg x
lg "-p of every local facility" "<$((16*8+6))>me: x" -t me -p local0.info x
lg "-p LOCAL3.WARNING: case does not matter" "<156>me: x" -t me -p LOCAL3.WARNING x
lg "-p a level alone: user" "<11>me: x" -t me -p err x
lg "-p daemon.info" "<30>me: x" -t me -p daemon.info x
t "-i: the process ID" "yes" 0 sh -c ': > log; "$1" -u "$2" -i -t me x; sleep 1; grep -E "^<13>me\[[0-9]+\]: x\$" log >/dev/null && echo yes' sh "$U" "$T/d.sock"
t "the standard input: lines" "<13>me: one${nl}<13>me: two" 0 sh -c ': > log; printf "one\ntwo\n" | "$1" -u "$2" -t me; sleep 1; cat log' sh "$U" "$T/d.sock"
t "the standard input: empty lines are not logged" "<13>me: a${nl}<13>me: b" 0 sh -c ': > log; printf "a\n\n\nb" | "$1" -u "$2" -t me; sleep 1; cat log' sh "$U" "$T/d.sock"
t "-f file" "<13>me: f1${nl}<13>me: f2" 0 sh -c ': > log; printf "f1\nf2\n" > in; "$1" -u "$2" -t me -f in; sleep 1; cat log' sh "$U" "$T/d.sock"
t "-f, and strings: the strings" "<13>me: s" 0 sh -c ': > log; printf "f1\n" > in; "$1" -u "$2" -t me -f in s; sleep 1; cat log' sh "$U" "$T/d.sock"
t "a long message" "70000" 0 sh -c ': > log; m=$(awk "BEGIN { while (i++ < 70000) printf \"x\" }"); "$1" -u "$2" -t me "$m"; sleep 1; sed "s/^<13>me: //" log | tr -d "\n" | wc -c | tr -d " "' sh "$U" "$T/d.sock"
t "the default tag: the login name, or the user's" "yes" 0 sh -c ': > log; "$1" -u "$2" x; sleep 1; n=$(logname 2>/dev/null || id -un); grep -E "^<13>$n: x\$" log >/dev/null && echo yes' sh "$U" "$T/d.sock"
t "a stream socket: NUL-ended messages" "<13>me: s1${nl}<13>me: s2" 0 sh -c ': > slog; printf "s1\ns2\n" | "$1" -u "$2" -t me; sleep 1; cat slog' sh "$U" "$T/s.sock"
t "the time stamp" "" 1 grep BADSTAMP log slog
# errors
t "no log daemon: an error" "" 1 "$U" -u "$T/none.sock" x
t "no log daemon: a diagnostic" "" 0 sh -c '"$1" -u "$2" x 2>&1 | grep -q .' sh "$U" "$T/none.sock"
t "-p not a priority" "" 1 "$U" -u "$T/d.sock" -p user.nothing x
t "-p not a facility" "" 1 "$U" -u "$T/d.sock" -p nofac.err x
t "-f a missing file" "" 1 "$U" -u "$T/d.sock" -f no_such_file
t "an unknown option" "" 2 "$U" -x y
t "-t without a tag" "" 2 "$U" -t
t "--" "<13>me: -i" 0 sh -c ': > log; "$1" -u "$2" -t me -- -i; sleep 1; cat log' sh "$U" "$T/d.sock"
# getopt() must not take an option after an operand for one (no permuting)
t "options after operands: logger x -i: -i is a string" "<13>me: x -i" 0 sh -c ': > log; "$1" -u "$2" -t me x -i; sleep 1; cat log' sh "$U" "$T/d.sock"

done_tests logger
