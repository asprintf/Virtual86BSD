#!/bin/sh
# test_talk.sh - tests for talk (POSIX.1-2024).  Run: sh test_talk.sh
cd "$(dirname "$0")" || exit 2
PROG_DIR=${PROG_DIR:-$PWD/../../../usr.bin/talk}; export PROG_DIR
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
U=${U:-$PROG_DIR/talk}
T=${TMPDIR:-/tmp}/test_talk.$$
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
# the database, the terminals (plain files), the directory of meetings, and
# a terminal type tt (../../lib/testlib/mkterminfo.tl); tests/talkdrv.tl runs talk
# on pseudo-terminals and reads their screens
TD=$OLDPWD/tests
${CC:-cc} -o mkutx "$OLDPWD/../../usr.bin/who/tests/mkutx.c" 2>/dev/null || { echo "talk: cannot compile ../../usr.bin/who/tests/mkutx.c"; exit 2; }
"$TLBIN" "$OLDPWD/../../lib/testlib/mkterminfo.tl" ti 'tt|a test terminal' 'cup=\E[%i%p1%d;%p2%dH' 'clear=\E[H\E[J' 'el=\E[K' 'cols#80' 'lines#24' || exit 2
mkdir -p dev/pts td
POSIX_UTMPX=$T/u POSIX_DEVDIR=$T/dev POSIX_TALKDIR=$T/td TERMINFO=$T/ti TALK_ME=$(id -un)
export POSIX_UTMPX POSIX_DEVDIR POSIX_TALKDIR TERMINFO TALK_ME
# another user, logged on pts/2
OTHER=nobody
[ "$TALK_ME" = nobody ] && OTHER=root
printf 'USER %s pts/0 a 1 -10\nUSER %s pts/1 b 2 -10\nUSER %s pts/2 c 3 -10\n' "$TALK_ME" "$TALK_ME" "$OTHER" | ./mkutx > u
# s SCENARIO [ARG...]: its output, the terminals emptied first
s() { : > dev/pts/0; : > dev/pts/1; : > dev/pts/2; chmod 620 dev/pts/0 dev/pts/1 dev/pts/2; "$TLBIN" "$TD/talkdrv.tl" "$U" "$@" 2>&1; }

t "the call: rings the callee's terminal" "status: [Waiting for your party to respond]
first line empty: yes
alert: yes
message: Message from talk@NODE at HH:MM ...
talk: connection requested by ME
talk: respond with: talk ME
socket: yes
exit: [0]
socket removed: yes" 0 s ring
t "the answer: both connected" "A: [Connection established]
B: [Connection established]
A alerted: yes
the socket gone: yes
rung once: 1
exit: [0, 0]" 0 s connect
t "typing: shown in the upper half, sent to the lower" "A upper: [\"hello there\", \"second\"]
B lower: [\"hello there\", \"second\"]
B upper: [\"\", \"\"]
A cursor: 1 6
A lower: [\"reply\"]
exit: [0, 0]" 0 s type
t "erase, kill, word erase: on both screens" "erase: \"abx\" \"abx\"
kill: \"kill\" \"kill\"
werase: \"kill one\" \"kill one\"
erase past the start: \"Z\" \"Z\"
exit: [0, 0]" 0 s edit
t "word erase only with iexten" "no iexten: \"one two^W.\"
exit: [0, 0]" 0 s werase_noiexten
t "other characters as ^X and M-; tab as it is; alert alerts" "sent: \"a^Ab^[c dM-^@.\"
shown: \"a^Ab^[c dM-^@.\"
B alerted: 1
no escape reached B: yes
exit: [0, 0]" 0 s chars
t "the end of a half: the next line, then the top again" "wrapped: [\"xxxxxxxxxxxxxxxxxxxx\", \"xxxxx\"]
again from the top: [\"2\", \"3\", \"1\"]
A: [\"2\", \"3\", \"1\"]
exit: [0, 0]" 0 s wrap
t "control-L redraws" "redrawn: mine theirs [Connection established]
exit: [0, 0]" 0 s redraw
t "end of file: status 0; the other told, and only able to leave" "A exit: 0
B: [Your party has left; type the end-of-f
B input ignored: yes
B still running: yes
B exit: 0" 0 s leave
t "the interrupt character: status 0" "A exit: 0
B: [Your party has left; type the end-of-f
B exit: 0" 0 s interrupt
t "an interrupt while waiting: the call taken back" "exit: 0
socket removed: yes
B calls (no stale call): yes
B exit: [0]" 0 s interrupt_waiting
t "the terminal's modes restored" "canonical again: yes echo again: yes" 0 s terminal_restored
t "a connection without the proof refused" "the challenge: \"talk 1 N\\n\"
refused: \"no\\n\"
A still waiting: yes
the real one connects: yes
exit: [0, 0]" 0 s fake_callee
t "both at once: connected" "connected: yes
exit: [0, 0]" 0 s both
t "the terminal operand" "pts/1 rung: yes
pts/0 not: yes
exit: [0]" 0 s terminal_operand
t "rung again after 10 seconds" "status: [Ringing your party again]
rung: 1 2
exit: [0]" 0 s ring_again
U8=${UTF8_LOCALE:-$(locale -a 2>/dev/null | grep -i -E '^c\.utf-?8$' | sed 1q)}
[ -n "$U8" ] || U8=$(locale -a 2>/dev/null | grep -i -E '\.utf-?8$' | sed 1q)
if [ -n "$U8" ]; then
    t "UTF-8 characters" "sent: café 中 .
exit: [0, 0]" 0 env LC_ALL="$U8" "$TLBIN" "$TD/talkdrv.tl" "$U" utf8
fi
# errors: a message on the standard output, status 1 (2 for usage)
t "the standard input not a terminal" "exit: 1
message: talk: the standard input is not a terminal" 0 s not_tty_in
t "the standard output not a terminal" "exit: 1
message: talk: the standard output is not a terminal" 0 s not_tty_out
t "no such user" "exit: 1
message: talk: no_such_xq: no such user" 0 s error no_such_xq
t "another system's user" "exit: 1
message: talk: a@b: only users of this system can be talked to" 0 s error a@b
t "not logged on" "exit: 1
message: talk: $OTHER is not logged on pts/7" 0 s error "$OTHER" pts/7
t "no operands: usage" "exit: 2
message: usage: talk address [terminal]" 0 s error
t "an option: usage" "exit: 2
message: usage: talk address [terminal]" 0 s error -x
t "too many operands: usage" "exit: 2
message: usage: talk address [terminal]" 0 s error a b c
if ! $root; then
    t "messages disabled" "exit: 1
message: talk: $OTHER has messages disabled" 0 sh -c 'chmod 600 dev/pts/2; "$TLBIN" "$1/talkdrv.tl" "$2" error "$3" 2>&1' sh "$TD" "$U" "$OTHER"
fi
# getopt() must not take an option after an operand for one (no permuting)
t "options after operands: talk USER -x: -x is the terminal" "exit: 1
message: talk: $OTHER is not logged on -x" 0 s error "$OTHER" -x

done_tests talk
