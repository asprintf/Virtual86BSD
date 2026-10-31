#!/bin/sh
# test_who.sh - tests for who (POSIX.1-2024).  Run: sh test_who.sh
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
U=${U:-$PWD/who}
T=${TMPDIR:-/tmp}/test_who.$$
mkdir -m 700 "$T" || exit 2
trap 'chmod -R u+rwx "$T" 2>/dev/null; rm -rf "$T" "$T.tl"' EXIT
# tl: the test language, built here (../testlib/tl.c; ../testlib/tl.md)
TL=$(cd ../testlib && pwd) || exit 2; export TL
cc=${cc:-${CC:-}}
[ -n "$cc" ] || for c in c17 c99 cc; do command -v $c >/dev/null 2>&1 && { cc=$c; break; }; done
$cc -o "$T.tl" "$TL/tl.c" || { echo "cannot compile ../testlib/tl.c"; exit 2; }
TLBIN=$T.tl; export TLBIN
tl() { "$TLBIN" "$@"; }
# ptyrun FDS COMMAND...: COMMAND with the descriptors FDS on a terminal (../testlib/ptyrun.tl)
ptyrun() { PTYRUN_INPUT=${PTYRUN_INPUT-} PTYRUN_WAIT=${PTYRUN_WAIT-} "$TLBIN" "$TL/ptyrun.tl" "$@"; }   # (assignments before a function call need not be exported)
cd "$T" || exit 2
export LC_ALL=C TZ=UTC0
root=false; [ "$(id -u)" = 0 ] && root=true
nl='
'
# the database: a file of struct utmpx records made by tests/mkutx.c
$cc -o mkutx "$OLDPWD/tests/mkutx.c" 2>/dev/null || { echo "who: cannot compile tests/mkutx.c"; exit 2; }
mku() { ./mkutx > "$1"; }
# whether the C library keeps ut_exit (as lib/utx.c decides it)
printf '#include <utmpx.h>\n#if !defined(__GLIBC__) && !defined(__UT_HOSTSIZE)\n#error no ut_exit\n#endif\nint main(void) { return 0; }\n' > probe.c
glibc=false; $cc -o probe probe.c 2>/dev/null && glibc=true
mkdir -p dev/pts
: > dev/pts/0; chmod 620 dev/pts/0
: > dev/pts/1; chmod 600 dev/pts/1
POSIX_DEVDIR=$T/dev; export POSIX_DEVDIR
mku u <<'X'
BOOT reboot ~ - 0 1700000000
LOGIN LOGIN tty9 9 500 1700000010
INIT - - si 12 1700000001
USER alice pts/0 ts/0 1234 1700000100 example.org
USER bob pts/1 ts/1 1300 1700000200
USER carol pts/7 ts/7 1350 1700000250
DEAD - pts/2 ts/2 1400 1700000300 - 0 1
EMPTY - - - 0 0
OLD - - - 0 1700000400
NEW - - - 0 1700000460
X

t "the users" "alice    pts/0        Nov 14 22:15 (example.org)
bob      pts/1        Nov 14 22:16
carol    pts/7        Nov 14 22:17" 0 "$U" u
t "-s: the same" "alice    pts/0        Nov 14 22:15 (example.org)
bob      pts/1        Nov 14 22:16
carol    pts/7        Nov 14 22:17" 0 "$U" -s u
t "TZ" "alice    pts/0        Nov 14 17:15 (example.org)" 0 sh -c 'TZ=EST5 "$1" u | sed 1q' sh "$U"
t "-q: the names and the number" "alice bob carol
# users=3" 0 "$U" -q u
t "-q: the other options ignored" "alice bob carol
# users=3" 0 "$U" -qbTuH u
t "-H: headings" "NAME     LINE         TIME         COMMENT
alice    pts/0        Nov 14 22:15 (example.org)" 0 sh -c '"$1" -H u | sed 2q' sh "$U"
# -T: + writable by others, - not, ? unknown
t "-T: the state" "alice    + pts/0        Nov 14 22:15 (example.org)
bob      - pts/1        Nov 14 22:16
carol    ? pts/7        Nov 14 22:17" 0 "$U" -T u
t "-T: the format: name, state, line, time" "ok" 0 sh -c '"$1" -T u | awk "NR == 2 && \$1 == \"bob\" && \$2 == \"-\" && \$3 == \"pts/1\" && \$4 \" \" \$5 \" \" \$6 == \"Nov 14 22:16\" { print \"ok\" }"' sh "$U"
t "-T: others write" "+" 0 sh -c 'chmod 602 dev/pts/1; "$1" -T u | awk "\$1 == \"bob\" { print \$2 }"; chmod 600 dev/pts/1' sh "$U"
t "-TH: headings" "NAME     S LINE         TIME         COMMENT" 0 sh -c '"$1" -TH u | sed 1q' sh "$U"
t "-sT: -s wins" "bob      pts/1        Nov 14 22:16" 0 sh -c '"$1" -sT u | sed -n 2p' sh "$U"
# -u: the idle time and the pid
setatime() { "$TLBIN" -e 'utime(argv[0], now() - num(argv[1]), stat(argv[0]).mtime);' "$@"; }
setatime dev/pts/0 10
setatime dev/pts/1 7500
t "-u: . for the last minute" "alice    pts/0        Nov 14 22:15     .   1234 (example.org)" 0 sh -c '"$1" -u u | sed 1q' sh "$U"
t "-u: hours and minutes" "bob      pts/1        Nov 14 22:16  2:05   1300" 0 sh -c '"$1" -u u | sed -n 2p' sh "$U"
t "-u: ? for a line not there" "carol    pts/7        Nov 14 22:17     ?   1350" 0 sh -c '"$1" -u u | sed -n 3p' sh "$U"
setatime dev/pts/1 90000
t "-u: old after 24 hours" "bob      pts/1        Nov 14 22:16   old   1300" 0 sh -c '"$1" -u u | sed -n 2p' sh "$U"
setatime dev/pts/1 7500
t "-uH: headings" "NAME     LINE         TIME          IDLE    PID COMMENT" 0 sh -c '"$1" -uH u | sed 1q' sh "$U"
t "-Tu" "bob      - pts/1        Nov 14 22:16  2:05   1300" 0 sh -c '"$1" -Tu u | sed -n 2p' sh "$U"
# the other entries
t "-b: the boot" "         system boot  Nov 14 22:13" 0 "$U" -b u
t "-b: only the boot" "1" 0 sh -c '"$1" -b u | wc -l | tr -d " "' sh "$U"
t "-bu: the users too" "4" 0 sh -c '"$1" -bu u | wc -l | tr -d " "' sh "$U"
t "-bT: the users too" "4" 0 sh -c '"$1" -bT u | wc -l | tr -d " "' sh "$U"
t "-l: the lines waiting for a login" "LOGIN    tty9         Nov 14 22:13          500 id=9" 0 "$U" -l u
t "-p: processes spawned by init" "                      Nov 14 22:13           12 id=si" 0 "$U" -p u
if $glibc; then
    t "-d: the dead, with the exit" "         pts/2        Nov 14 22:18         1400 id=ts/2 term=0 exit=1" 0 "$U" -d u
else
    t "-d: the dead" "         pts/2        Nov 14 22:18         1400 id=ts/2" 0 "$U" -d u
fi
t "-t: the changes of the clock" "         old time     Nov 14 22:20
         new time     Nov 14 22:21" 0 "$U" -t u
t "-lp: both" "LOGIN    tty9         Nov 14 22:13          500 id=9
                      Nov 14 22:13           12 id=si" 0 "$U" -lp u
t "-sb" "         system boot  Nov 14 22:13" 0 "$U" -sb u
if $glibc; then
    mku r <<'X'
RUNLVL runlevel ~ - 13107 1700000005
X
    t "-r: the run level" "         run-level 3  Nov 14 22:13 last=3" 0 "$U" -r r
fi
t "-r: none" "" 0 "$U" -r u
t "-a: all" "           system boot  Nov 14 22:13
LOGIN      tty9         Nov 14 22:13     ?    500 id=9
                        Nov 14 22:13           12 id=si
alice    + pts/0        Nov 14 22:15     .   1234 (example.org)
bob      - pts/1        Nov 14 22:16  2:05   1300
carol    ? pts/7        Nov 14 22:17     ?   1350" 0 sh -c '"$1" -a u | sed 6q' sh "$U"
t "-a: 9 entries" "9" 0 sh -c '"$1" -a u | wc -l | tr -d " "' sh "$U"
t "-aH: headings" "NAME     S LINE         TIME          IDLE    PID COMMENT EXIT" 0 sh -c '"$1" -aH u | sed 1q' sh "$U"
t "no trailing blanks" "0" 1 sh -c '"$1" -a u | grep -c " \$"' sh "$U"
# -m and am i: the invoker's terminal
cat > mt.sh <<'X'
l=$(tty) || exit 3
l=${l#/dev/}
printf 'USER me %s x 1 1700000100\nUSER other pts/99 y 2 1700000100\n' "$l" | ./mkutx > m
unset POSIX_DEVDIR
POSIX_UTMPX=m "$U" "$@" | awk '{ print $1 }'
X
export U
t "-m: the invoker's line" "me" 0 ptyrun 0 sh mt.sh -m
t "am i" "me" 0 ptyrun 0 sh mt.sh am i
t "am I" "me" 0 ptyrun 0 sh mt.sh am I
t "-m: no terminal: nothing" "" 0 sh -c '"$1" -m u < /dev/null > out 2>/dev/null 3>&2; r=$?; cat out; exit $r' sh "$U"
t "-m without a terminal, -q still works" "alice bob carol
# users=3" 0 sh -c '"$1" -mq u < /dev/null 2>/dev/null' sh "$U"
# the database
t "an empty database" "" 0 sh -c ': > e && "$1" e' sh "$U"
t "-q: none" "${nl}# users=0" 0 sh -c ': > e && "$1" -q e' sh "$U"
t "the default database (the hook for tests)" "alice bob carol
# users=3" 0 sh -c 'POSIX_UTMPX=u "$1" -q' sh "$U"
t "the system's database: no error" "0" 0 sh -c 'unset POSIX_UTMPX; "$1" >/dev/null 2>&1; echo $?' sh "$U"
t "a missing file: status 1" "" 1 "$U" nonexistent
t "a missing file: a message" "1" 0 sh -c '"$1" nonexistent 2>&1 | grep -c nonexistent' sh "$U"
t "am alone: a file" "" 1 "$U" am
t "am x: not am i" "" 2 "$U" am x
t "three operands: usage" "" 2 "$U" am i x
t "an unknown option: status 2" "" 2 "$U" -z
t "a usage message" "1" 0 sh -c '"$1" -z 2>&1 | grep -c usage' sh "$U"
t "-- ends the options" "# users=3" 0 sh -c '"$1" -q -- u | sed -n 2p' sh "$U"
# getopt() must not take an option after an operand for one (no permuting)
t "options after operands: who u -q: two operands" "" 2 "$U" u -q
t "a write error: status 1" "" 1 sh -c '[ -w /dev/full ] || exit 1; "$1" u > /dev/full 2>/dev/null' sh "$U"

done_tests who
