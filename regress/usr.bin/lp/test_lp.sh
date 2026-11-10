#!/bin/sh
# test_lp.sh - tests for lp (POSIX.1-2024).  Run: sh test_lp.sh
cd "$(dirname "$0")" || exit 2
PROG_DIR=${PROG_DIR:-$PWD/../../../usr.bin/lp}; export PROG_DIR
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
U=${U:-$PROG_DIR/lp}
T=${TMPDIR:-/tmp}/test_lp.$$
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
# a configuration of printers that are files and a command (LP_CONF), and
# a mailx that keeps what it is given (on PATH)
mkdir locks bin
cat > conf <<X
# printers for the tests
printer p1 $T/out1
printer p2 nobanner $T/out2
printer pc | { echo "LP_REQUEST=\$LP_REQUEST LP_DEST=\$LP_DEST LP_TITLE=\$LP_TITLE LP_COPIES=\$LP_COPIES LP_OPTIONS=\$LP_OPTIONS"; cat; } > $T/outc
printer pf nobanner | cat > /dev/null; exit 3
class cl p1 p2
default p1
lockdir $T/locks
X
cat > bin/mailx <<'X'
#!/bin/sh
{ echo "mailx $*"; cat; } >> "$MAILOUT"
X
chmod +x bin/mailx
LP_CONF=$T/conf MAILOUT=$T/mail PATH=$T/bin:$PATH
export LP_CONF MAILOUT PATH
unset LPDEST PRINTER
me=$(id -un)
clean() { : > out1; : > out2; rm -f outc mail; }
clean
printf 'one\n' > f1; printf 'two\n' > f2
# the banner page without its date, and the rest
body() { sed '1,/^Date:/d' "$1" | tr -d '\f'; }
banner() { sed -n '/^Request:/,/^Date:/p' "$1" | sed '/^Date:/d'; }

t "the standard input to the default printer" "request id is p1-1 (1 file)" 0 sh -c 'echo hello | "$1"' sh "$U"
t "the banner page" "Request: p1-1
User:    $me
Files:   (standard input)" 0 banner out1
t "the banner: an empty line first, a form feed last" "ok" 0 sh -c 'sed -n 1p out1 | grep -q "^\$" && sed -n "/^Date:/{n;p;}" out1 | od -An -c | grep -q "\\\\f  *h" && echo ok'
t "the banner date" "ok" 0 sh -c 'grep -q "^Date:    ..*" out1 && echo ok'
t "the file after it" "hello" 0 body out1
clean
t "request numbers go up" "request id is p1-2 (1 file)" 0 sh -c 'echo x | "$1"' sh "$U"
t "the number kept in the lock file" "2" 0 cat locks/lp.p1.lock
clean
t "-s: no request id" "" 0 sh -c 'echo x | "$1" -s' sh "$U"
t "-s: printed all the same" "x" 0 body out1
clean
t "-d: the destination" "request id is p2-1 (1 file)" 0 "$U" -d p2 f1
t "a nobanner printer: the file as it is" "one" 0 cat out2
t "-c is accepted" "request id is p2-2 (1 file)" 0 "$U" -c -d p2 f1
clean
t "LPDEST" "request id is p2-3 (1 file)" 0 env LPDEST=p2 PRINTER=p1 "$U" f1
t "PRINTER" "request id is p2-4 (1 file)" 0 env PRINTER=p2 "$U" f1
t "-d before LPDEST" "request id is p1-4 (1 file)" 0 env LPDEST=p2 "$U" -d p1 f1
t "an empty LPDEST is not set" "request id is p2-5 (1 file)" 0 env LPDEST= PRINTER=p2 "$U" f1
clean
t "several files" "request id is p2-6 (2 files)" 0 "$U" -d p2 f1 f2
t "several files: one after the other" "one${nl}two" 0 cat out2
clean
t "-n: the copies, each file whole" "one${nl}two${nl}one${nl}two${nl}one${nl}two" 0 sh -c '"$1" -s -d p2 -n 3 f1 f2 && cat out2' sh "$U"
clean
t "- among files: the standard input" "one${nl}in${nl}two" 0 sh -c 'echo in | "$1" -s -d p2 f1 - f2 && cat out2' sh "$U"
clean
t "- twice: read once" "in${nl}in" 0 sh -c 'echo in | "$1" -s -d p2 -n 2 - && cat out2' sh "$U"
clean
t "the standard input with copies" "in${nl}in" 0 sh -c 'echo in | "$1" -s -d p2 -n 2 && cat out2' sh "$U"
clean
t "-t: the title on the banner page" "Request: p1-5
User:    $me
Title:   my title
Files:   f1, f2" 0 sh -c '"$1" -s -t "my title" f1 f2 && sed -n "/^Request:/,/^Date:/p" out1 | sed "/^Date:/d"' sh "$U"
clean
t "-o nobanner" "one" 0 sh -c '"$1" -s -o nobanner f1 && cat out1' sh "$U"
clean
t "-o banner on a nobanner printer" "Request: p2-11" 0 sh -c '"$1" -s -o banner -d p2 f1 && sed -n "/^Request:/p" out2' sh "$U"
clean
t "-o other, a device: an error" "" 1 "$U" -o duplex f1
t "-o other, a device: nothing printed" "0" 0 sh -c 'wc -c < out1 | tr -d " "'
# a command
t "a command: the request on its standard input" "ok" 0 sh -c '"$1" -s -d pc -t T -o a -o b=c f1 && sed -n "1p" outc | grep -q "^LP_REQUEST=pc-1 LP_DEST=pc LP_TITLE=T LP_COPIES=1 LP_OPTIONS=a b=c\$" && sed 1,/^Date:/d outc | tr -d "\f" | grep -q "^one\$" && echo ok' sh "$U"
t "a command that fails: status 1" "" 1 "$U" -d pf f1
t "a command that fails: a message" "lp: pf: the printer's command failed" 1 sh -c '"$1" -d pf f1 2>&1 >/dev/null' sh "$U"
clean
# classes and the locks
t "a class: its first free member" "request id is p1-7 (1 file)" 0 "$U" -d cl f1
"$TLBIN" -e 'lockf(open(argv[0], "rw"), "lock"); writefile(argv[1], ""); sleep(2);' locks/lp.p1.lock held &
while [ ! -f held ]; do sleep 1; done
t "a class: the next member when the first is busy" "request id is p2-12 (1 file)" 0 "$U" -d cl f1
t "a busy printer: lp waits for it" "request id is p1-8 (1 file)" 0 "$U" -d p1 f1
t "after the other holder" "ok" 0 sh -c '[ ! -f held ] || { sleep 0; echo ok; }'
wait
rm -f held
clean
# errors
t "an unknown destination: status 1" "" 1 "$U" -d nowhere f1
t "an unknown destination: a message" "lp: nowhere: no such destination" 1 sh -c '"$1" -d nowhere f1 2>&1' sh "$U"
t "a file missing: status 1" "request id is p2-13 (1 file)" 1 "$U" -d p2 f1 missing
t "a file missing: the rest printed" "one" 0 cat out2
clean
t "no file readable: status 1, nothing printed" "0" 0 sh -c '"$1" -d p2 missing 2>/dev/null; [ $? = 1 ] && wc -c < out2 | tr -d " "' sh "$U"
t "a directory: not printed" "" 1 "$U" -d p2 locks
t "no configuration, no device: status 1" "" 1 env LP_CONF=$T/none "$U" f1
t "no configuration: a message" "1" 0 sh -c 'LP_CONF=$1/none "$2" f1 2>&1 | grep -c "no destination"' sh "$T" "$U"
printf 'printer x /dev/null\nfrobnicate\n' > badconf
t "a configuration error: status 1" "" 1 env LP_CONF=$T/badconf "$U" f1
t "a configuration error: the line" "lp: $T/badconf: line 2: unknown keyword" 1 sh -c 'LP_CONF=$1/badconf "$2" f1 2>&1' sh "$T" "$U"
printf 'class c x\n' > badconf2
t "a class of no printers" "" 1 env LP_CONF=$T/badconf2 "$U" f1
printf 'printer only %s/out2\nlockdir %s/locks\n' "$T" "$T" > oneconf
t "the only printer: the default" "request id is only-1 (1 file)" 0 env LP_CONF=$T/oneconf "$U" f1
clean
t "-n 0: usage" "" 2 "$U" -n 0 f1
t "-n x: usage" "" 2 "$U" -n x f1
t "-n -1: usage" "" 2 "$U" -n -1 f1
t "an unknown option: usage" "" 2 "$U" -x f1
# mail and the terminal
t "-m: mail when printed" "mailx -s lp: request p2-14 $me
Your request p2-14 (T) has been printed on p2." 0 sh -c '"$1" -s -m -t T -d p2 f1 && cat mail' sh "$U"
clean
${CC:-cc} -o mkutx "$OLDPWD/../../usr.bin/who/tests/mkutx.c" 2>/dev/null || { echo "lp: cannot compile ../../usr.bin/who/tests/mkutx.c"; exit 2; }
mkdir -p dev/pts && : > dev/pts/4
printf 'USER %s pts/4 a 1 -10\n' "$me" | ./mkutx > u
t "-w: a message on the user's terminal" "lp: request p2-15 has been printed" 0 sh -c 'POSIX_UTMPX=u POSIX_DEVDIR=dev "$1" -s -w -d p2 f1 && tr -d "\r" < dev/pts/4 | sed /^\$/d' sh "$U"
t "-w: no mail then" "no mail" 0 sh -c '[ -f mail ] || echo no mail'
: > u2
t "-w, not logged in: mail" "mailx -s lp: request p2-16 $me
Your request p2-16 has been printed on p2." 0 sh -c 'POSIX_UTMPX=u2 POSIX_DEVDIR=dev "$1" -s -w -d p2 f1 && cat mail' sh "$U"
clean
# getopt() must not take an option after an operand for one (no permuting)
t "options after operands: lp f1 -s: -s is a file" "request id is p2-17 (1 file)" 1 "$U" -d p2 f1 -s

done_tests lp
