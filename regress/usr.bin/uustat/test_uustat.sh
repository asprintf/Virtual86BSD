#!/bin/sh
# test_uustat.sh - tests for uustat (POSIX.1-2024).  Run: sh test_uustat.sh
cd "$(dirname "$0")" || exit 2
PROG_DIR=${PROG_DIR:-$PWD/../../../usr.bin/uustat}; export PROG_DIR
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
U=${U:-$PROG_DIR/uustat}
T=${TMPDIR:-/tmp}/test_uustat.$$
mkdir -m 700 "$T" || exit 2
trap 'chmod -R u+rwx "$T" 2>/dev/null; rm -rf "$T"' EXIT
cd "$T" || exit 2
export LC_ALL=C
root=false; [ "$(id -u)" = 0 ] && root=true
nl='
'
# a spool with jobs of two users, written as uucp and uux write them
mkdir -p s/jobs s/data s/status
cat > u.conf <<X
nodename here
spool $T/s
pubdir $T/pub
system there exit 1
system far exit 1
X
UUCP_CONF=$T/u.conf; export UUCP_CONF
TZ=UTC0; export TZ
me=$(id -un 2>/dev/null || id -u)
job() {     # id user system attempts error, then lines
    { printf 'id %s\nuser %s\nsystem %s\ntime 1700000000\nflags d\nattempts %s\n' "$1" "$2" "$3" "$4"
      [ -n "$5" ] && printf 'error %s\n' "$5"
      shift 5; printf '%s\n' "$@"; } > "s/jobs/$1"
}
job there.0001 "$me" there 0 '' 'item - /home/a.txt there ~/a.txt -'
job far.0002 other far 2 'cannot%20reach%20far' 'item - /home/b.txt far ~/b.txt s/data/far.0002.0'
job there.0003 other there 0 '' 'x there' 'w lit cat - -' 'line there!cat%20x'
job here.0004 "$me" here 0 '' 'item - /x - /y -' 'item - /p - /q -'
echo data > s/data/far.0002.0
printf '1700000100 cannot reach far: the connection was lost\n' > s/status/far
t "the user's jobs" "there.0001 $me there 2023-11-14 22:13 copy /home/a.txt to there!~/a.txt
here.0004 $me here 2023-11-14 22:13 copy /x to /y; copy /p to /q" 0 "$U"
t "-u user" 'far.0002 other far 2023-11-14 22:13 copy /home/b.txt to far!~/b.txt (2 attempts: cannot reach far)
there.0003 other there 2023-11-14 22:13 execute there!cat x' 0 "$U" -u other
t "-s system: any user's" "there.0001 $me there 2023-11-14 22:13 copy /home/a.txt to there!~/a.txt
there.0003 other there 2023-11-14 22:13 execute there!cat x" 0 "$U" -s there
t "-s and -u" 'there.0003 other there 2023-11-14 22:13 execute there!cat x' 0 "$U" -s there -u other
t "-s: no jobs" '' 0 "$U" -s nowhere
t "-q: the systems with jobs" 'there 2 -
far 1 2023-11-14 22:15 cannot reach far: the connection was lost
here 1 -' 0 "$U" -q
t "-r: the job's files touched" "Job there.0001 rejuvenated
yes" 0 sh -c 'touch -t 200001010000 s/jobs/there.0001; touch -t 200101010000 ref; "$1" -r there.0001 && test s/jobs/there.0001 -nt ref && echo yes' sh "$U"
if [ "$(id -u)" != 0 ]; then
    t "-k: not another user's job" 'uustat: far.0002: not your job' 1 sh -c '"$1" -k far.0002 2>&1' sh "$U"
    t "-r: not another user's job" 'uustat: far.0002: not your job' 1 sh -c '"$1" -r far.0002 2>&1' sh "$U"
else
    t "-k: any job with appropriate privileges" 'Job far.0002 killed
gone' 0 sh -c '"$1" -k far.0002 && test ! -f s/data/far.0002.0 && echo gone' sh "$U"
fi
t "-k: the job removed" 'Job there.0001 killed
here.0004' 0 sh -c '"$1" -k there.0001 && "$1" | cut -d " " -f 1' sh "$U"
t "-k: a job that is not there" 'uustat: nosuch.1: no such job' 1 sh -c '"$1" -k nosuch.1 2>&1' sh "$U"
t "-r: a job that is not there" 'uustat: nosuch.1: no such job' 1 sh -c '"$1" -r nosuch.1 2>&1' sh "$U"
t "-k: a name with / is no job" 'uustat: ../u.conf: no such job' 1 sh -c '"$1" -k ../u.conf 2>&1' sh "$U"
t "no spool: no jobs" '' 0 sh -c 'printf "spool %s/none\n" "$PWD" > n.conf; UUCP_CONF=$PWD/n.conf "$1"' sh "$U"
t "-q with -s: a usage error" '' 2 "$U" -q -s there
t "-k with -u: a usage error" '' 2 "$U" -k x -u me
t "-q with -k: a usage error" '' 2 "$U" -q -k x
t "an operand: a usage error" '' 2 "$U" there
t "an unknown option: a usage error" '' 2 "$U" -x
# getopt() must not take an option after an operand for one (no permuting)
t "an option after an operand is an operand" '' 2 "$U" there -q
done_tests uustat
