#!/bin/sh
# test_write.sh - tests for write (POSIX.1-2024).  Run: sh test_write.sh
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
U=${U:-$PWD/write}
T=${TMPDIR:-/tmp}/test_write.$$
mkdir -m 700 "$T" || exit 2
trap 'chmod -R u+rwx "$T" 2>/dev/null; rm -rf "$T" "$T.tl"' EXIT
# tl: the test language, built here (../tl/tl.c; ../tl/tl.md)
TL=$(cd ../testlib && pwd) || exit 2; export TL
cc=${cc:-${CC:-}}
[ -n "$cc" ] || for c in c17 c99 cc; do command -v $c >/dev/null 2>&1 && { cc=$c; break; }; done
$cc -o "$T.tl" "$TL/../tl/tl.c" || { echo "cannot compile ../tl/tl.c"; exit 2; }
TLBIN=$T.tl; export TLBIN
tl() { "$TLBIN" "$@"; }
# ptyrun FDS COMMAND...: COMMAND with the descriptors FDS on a terminal (../testlib/ptyrun.tl)
ptyrun() { PTYRUN_INPUT=${PTYRUN_INPUT-} PTYRUN_WAIT=${PTYRUN_WAIT-} "$TLBIN" "$TL/ptyrun.tl" "$@"; }   # (assignments before a function call need not be exported)
cd "$T" || exit 2
export LC_ALL=C
root=false; [ "$(id -u)" = 0 ] && root=true
nl='
'
U8=${UTF8_LOCALE:-$(locale -a 2>/dev/null | grep -i -E '^c\.utf-?8$' | sed 1q)}
[ -n "$U8" ] || U8=$(locale -a 2>/dev/null | grep -i -E '\.utf-?8$' | sed 1q)
# the database and the terminals: tests/mkutx.c of who, and plain files
${CC:-cc} -o mkutx "$OLDPWD/../who/tests/mkutx.c" 2>/dev/null || { echo "write: cannot compile ../who/tests/mkutx.c"; exit 2; }
POSIX_UTMPX=$T/u POSIX_DEVDIR=$T/dev; export POSIX_UTMPX POSIX_DEVDIR U
setatime() { "$TLBIN" -e 'utime(argv[0], now() - num(argv[1]), stat(argv[0]).mtime);' "$@"; }
me=$(id -un)
reset() {
    rm -rf dev && mkdir -p dev/pts
    for n in 0 1 2 3; do : > dev/pts/$n; done
    chmod 620 dev/pts/0 dev/pts/2 dev/pts/3; chmod 600 dev/pts/1
    setatime dev/pts/2 100; setatime dev/pts/3 50
    ./mkutx > u <<X
USER alice pts/0 a 10 -100
USER bob pts/1 b 11 -100
USER carol pts/2 c 12 -100
USER carol pts/3 d 13 -100
LOGIN LOGIN pts/9 e 14 -100
DEAD dave pts/0 f 15 -100
X
}
# what reached a terminal, without the CRs
got() { tr -d '\r' < "dev/pts/$1"; }
reset

t "the message" "hello${nl}world${nl}EOT" 0 sh -c 'printf "hello\nworld\n" | "$U" alice && tr -d "\r" < dev/pts/0 | sed 1,2d'
t "the first message" "ok" 0 sh -c ': > dev/pts/0; echo x | "$U" alice && tr -d "\r" < dev/pts/0 | sed -n 2p | grep -q "^.Message from [^ ]* (???) \[[A-Z][a-z][a-z] [A-Z][a-z][a-z] [ 1-3][0-9] [0-9][0-9]:[0-9][0-9]:[0-9][0-9] [0-9]*\]\.\.\.\$" && echo ok'
t "the first message: an empty line, an alert" "ok" 0 sh -c ': > dev/pts/0; echo x | "$U" alice && od -An -c dev/pts/0 | sed 1q | grep -q "^ *\\\\r  *\\\\n  *\\\\a  *M  *e" && echo ok'
t "the sender's login name" "ok" 0 sh -c ': > dev/pts/0; echo x | "$U" alice && n=$(tr -d "\r" < dev/pts/0 | sed -n "2s/^.Message from \([^ ]*\) .*/\1/p") && { [ "$n" = "$(id -un)" ] || [ "$n" = "$(logname 2>/dev/null)" ]; } && echo ok'
t "newline is CR LF" "61 0d 0a 62 0d 0a 45 4f 54 0d 0a" 0 sh -c ': > dev/pts/0; printf "a\nb\n" | "$U" alice && sed 1,2d dev/pts/0 | od -An -tx1 | tr -s " \n" " " | sed "s/^ //; s/ \$//"'
t "EOT at end of file" "EOT" 0 sh -c ': > dev/pts/0; "$U" alice < /dev/null && tr -d "\r" < dev/pts/0 | sed -n 3p'
t "a last line without a newline: EOT on a line of its own" "abc${nl}EOT" 0 sh -c ': > dev/pts/0; printf abc | "$U" alice && tr -d "\r" < dev/pts/0 | sed 1,2d'
t "the standard output: nothing for one terminal" "" 0 sh -c ': > dev/pts/0; echo x | "$U" alice'
# characters
t "control characters as ^X" "a^Ab^[c^?d${nl}EOT" 0 sh -c ': > dev/pts/0; printf "a\001b\033c\177d\n" | "$U" alice && tr -d "\r" < dev/pts/0 | sed 1,2d'
t "tab and alert as they are" "ok" 0 sh -c ': > dev/pts/0; printf "a\tb\007c\n" | "$U" alice && tr -d "\r" < dev/pts/0 | sed -n 3p | od -An -c | tr -s " " | grep -q "a \\\\t b \\\\a c" && echo ok'
t "a CR as it is (space class)" "ok" 0 sh -c ': > dev/pts/0; printf "a\rb\n" | "$U" alice && od -An -c dev/pts/0 | tr -d "\n" | tr -s " " | grep -q "a \\\\r b \\\\r \\\\n" && echo ok'
t "bytes not characters: M-" "M-^@M-iM-^?${nl}EOT" 0 sh -c ': > dev/pts/0; printf "\200\351\377\n" | "$U" alice && tr -d "\r" < dev/pts/0 | sed 1,2d'
if [ -n "$U8" ]; then
    t "a UTF-8 character as it is" "é${nl}EOT" 0 sh -c ': > dev/pts/0; printf "\303\251\n" | LC_ALL=$1 "$U" alice && tr -d "\r" < dev/pts/0 | sed 1,2d' sh "$U8"
    t "a UTF-8 character across two reads" "é${nl}EOT" 0 sh -c ': > dev/pts/0; { printf "\303"; sleep 1; printf "\251\n"; } | LC_ALL=$1 "$U" alice && tr -d "\r" < dev/pts/0 | sed 1,2d' sh "$U8"
    t "a broken UTF-8 sequence: M-" "M-CA${nl}EOT" 0 sh -c ': > dev/pts/0; printf "\303A\n" | LC_ALL=$1 "$U" alice && tr -d "\r" < dev/pts/0 | sed 1,2d' sh "$U8"
fi
# the recipient
t "not logged on: status 1" "" 1 sh -c 'echo x | "$U" nobody_here'
t "not logged on: a message" "1" 0 sh -c 'echo x | "$U" nobody_here 2>&1 | grep -c "not logged on"'
t "a login process is not a user" "" 1 sh -c 'echo x | "$U" LOGIN'
t "a dead process is not a user" "" 1 sh -c 'echo x | "$U" dave'
if ! $root; then
    t "messages disabled: status 1" "" 1 sh -c 'echo x | "$U" bob'
    t "messages disabled: a message" "1" 0 sh -c 'echo x | "$U" bob 2>&1 | grep -c "disabled"'
    t "messages disabled: nothing written" "0" 0 sh -c 'echo x | "$U" bob 2>/dev/null; wc -c < dev/pts/1 | tr -d " "'
fi
t "others may write: allowed" "x${nl}EOT" 0 sh -c 'chmod 602 dev/pts/1; echo x | "$U" bob; r=$?; chmod 600 dev/pts/1; tr -d "\r" < dev/pts/1 | sed 1,2d; : > dev/pts/1; exit $r'
t "the terminal not there: not allowed" "" 1 sh -c 'printf "USER eve pts/8 x 1 -10\n" | ./mkutx > u2 && echo x | POSIX_UTMPX=u2 "$U" eve'
# several terminals
t "several: the least idle" "x${nl}EOT" 0 sh -c ': > dev/pts/2; : > dev/pts/3; setatime() { "$TLBIN" -e "utime(argv[0], now() - num(argv[1]), stat(argv[0]).mtime);" "$@"; }; setatime dev/pts/2 100; setatime dev/pts/3 50; echo x | "$U" carol >/dev/null && tr -d "\r" < dev/pts/3 | sed 1,2d'
t "several: the other untouched" "0" 0 sh -c ': > dev/pts/2; : > dev/pts/3; echo x | "$U" carol >/dev/null; wc -c < dev/pts/2 | tr -d " "'
t "several: a message on the standard output" "write: carol is logged on more than one place.
You are connected to \"pts/3\"." 0 sh -c ': > dev/pts/3; setatime() { "$TLBIN" -e "utime(argv[0], now() - num(argv[1]), stat(argv[0]).mtime);" "$@"; }; setatime dev/pts/2 100; setatime dev/pts/3 50; echo x | "$U" carol'
t "several: the terminal operand" "x${nl}EOT" 0 sh -c ': > dev/pts/2; echo x | "$U" carol pts/2 && tr -d "\r" < dev/pts/2 | sed 1,2d'
t "the terminal operand: no message" "" 0 sh -c 'echo x | "$U" carol pts/2'
t "the terminal operand as a pathname in /dev" "x${nl}EOT" 0 sh -c ': > dev/pts/2; echo x | "$U" carol /dev/pts/2 && tr -d "\r" < dev/pts/2 | sed 1,2d'
t "the terminal operand as a pathname in the directory" "x${nl}EOT" 0 sh -c ': > dev/pts/2; echo x | "$U" carol "$POSIX_DEVDIR/pts/2" && tr -d "\r" < dev/pts/2 | sed 1,2d'
t "not on that terminal: status 1" "" 1 sh -c 'echo x | "$U" carol pts/0'
t "not on that terminal: a message" "1" 0 sh -c 'echo x | "$U" carol pts/0 2>&1 | grep -c "not logged on pts/0"'
# on a terminal: the sender's line, the alerts, the interrupt and EOF characters
cat > tw.sh <<'X'
l=$(tty) || exit 3; l=${l#/dev/}
printf 'USER rcpt pts/0 a 1 -10\nUSER %s %s b 2 -10\n' "$(id -un)" "$l" | ./mkutx > u3
echo "$l" > line
: > dev/pts/0
POSIX_UTMPX=u3 exec "$U" "$@"
X
EOT=$(printf '\004') INTR=$(printf '\003')
export nl
PTYRUN_INPUT="x${nl}$EOT" ptyrun 0 sh tw.sh rcpt > /dev/null
t "the sender's terminal in the first message" "ok" 0 sh -c 'l=$(cat line); tr -d "\r" < dev/pts/0 | sed -n 2p | grep -q "Message from [^ ]* ($l)" && echo ok'
t "the sender's terminal alerted twice" "2" 0 sh -c 'PTYRUN_INPUT="$1" "$TLBIN" "$TL/ptyrun.tl" 01 sh tw.sh rcpt | tr -cd "\007" | wc -c | tr -d " "' sh "$EOT"
t "a line, then the EOF character: EOT" "hi${nl}EOT" 0 sh -c 'PTYRUN_INPUT="hi${nl}$1" "$TLBIN" "$TL/ptyrun.tl" 0 sh tw.sh rcpt >/dev/null; tr -d "\r" < dev/pts/0 | sed 1,2d' sh "$EOT"
t "the EOF character in a line: that much, then EOT" "hi${nl}EOT" 0 sh -c 'PTYRUN_INPUT="hi$1$1" "$TLBIN" "$TL/ptyrun.tl" 0 sh tw.sh rcpt >/dev/null; tr -d "\r" < dev/pts/0 | sed 1,2d' sh "$EOT"
t "the erase character: the line as edited" "hx${nl}EOT" 0 sh -c 'PTYRUN_INPUT="hi$(printf "\177")x${nl}$1" "$TLBIN" "$TL/ptyrun.tl" 0 sh tw.sh rcpt >/dev/null; tr -d "\r" < dev/pts/0 | sed 1,2d' sh "$EOT"
t "the interrupt character: EOT, status 0" "EOT" 0 sh -c 'PTYRUN_WAIT=1 PTYRUN_INPUT="$1" "$TLBIN" "$TL/ptyrun.tl" 0 sh tw.sh rcpt >/dev/null || exit $?; tr -d "\r" < dev/pts/0 | sed 1,2d' sh "$INTR"
t "the interrupt character in a line: the line discarded, EOT" "EOT" 0 sh -c 'PTYRUN_WAIT=1 PTYRUN_INPUT="abc$1" "$TLBIN" "$TL/ptyrun.tl" 0 sh tw.sh rcpt >/dev/null || exit $?; tr -d "\r" < dev/pts/0 | sed 1,2d' sh "$INTR"
cat > ts.sh <<'X'
l=$(tty) || exit 3; l=${l#/dev/}
printf 'USER %s %s b 2 -10\n' "$(id -un)" "$l" | ./mkutx > u4
chmod g+w "$(tty)"
POSIX_UTMPX=u4 POSIX_DEVDIR=/dev exec "$U" "$(id -un)"
X
t "to oneself on one's only terminal" "ok" 0 sh -c 'PTYRUN_INPUT="x${nl}$1" "$TLBIN" "$TL/ptyrun.tl" 01 sh ts.sh | grep -q "Message from" && echo ok' sh "$EOT"
# usage
t "no operands: status 2" "" 2 "$U"
t "three operands: status 2" "" 2 "$U" a b c
t "an option: status 2" "" 2 "$U" -x
t "a usage message" "1" 0 sh -c '"$U" 2>&1 | grep -c usage'
t "-- ends the options" "x${nl}EOT" 0 sh -c ': > dev/pts/0; echo x | "$U" -- alice && tr -d "\r" < dev/pts/0 | sed 1,2d'
# getopt() must not take an option after an operand for one (no permuting)
t "options after operands: write alice -x: -x is a terminal" "" 1 sh -c 'echo x | "$U" alice -x'
t "an interrupt ignored stays ignored" "y${nl}EOT${nl}0" 0 sh -c ': > dev/pts/0; mkfifo ff; { sh -c "exec \"\$0\" alice < ff > /dev/null 2>&1" "$U"; echo $? > st2; } & exec 3> ff; sleep 1; kill -INT $! 2>/dev/null; sleep 1; echo y >&3; exec 3>&-; wait; tr -d "\r" < dev/pts/0 | sed 1,2d; cat st2'

done_tests write
