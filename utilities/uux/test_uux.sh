#!/bin/sh
# test_uux.sh - tests for uux (POSIX.1-2024).  Run: sh test_uux.sh
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
U=${U:-$PWD/uux}
T=${TMPDIR:-/tmp}/test_uux.$$
mkdir -m 700 "$T" || exit 2
trap 'chmod -R u+rwx "$T" 2>/dev/null; rm -rf "$T"' EXIT
cd "$T" || exit 2
export LC_ALL=C
root=false; [ "$(id -u)" = 0 ] && root=true
nl='
'
# two systems: here (this one) and there; "down" cannot be reached
UUCICO=${UUCICO:-$(cd "$OLDPWD" && pwd)/uucico}
mkdir h t h/pub t/pub
cat > h.conf <<X
nodename here
spool $T/h/spool
pubdir $T/h/pub
mail sh $T/mail.sh
system there env UUCP_CONF=$T/t.conf $UUCICO -S
system down exit 1
X
cat > t.conf <<X
nodename there
spool $T/t/spool
pubdir $T/t/pub
mail sh $T/mail.sh
commands cat sort wc sh
X
printf 'printf "%%s %%s\\n" "$3" "$2" >> %s/mailbox; cat >> %s/mailbox\n' "$T" "$T" > mail.sh
UUCP_CONF=$T/h.conf; export UUCP_CONF
printf 'b\na\nc\n' > list.txt
printf 'x\n' > t/pub/x.txt
me=$(id -un 2>/dev/null || id -u)
t "a command there, a file from here, the output here" 'a
b
c' 0 sh -c '"$1" "there!sort !list.txt > !out1" && cat out1' sh "$U"
t "a file there: given by its path there" 'x' 0 sh -c '"$1" "there!cat there!~/x.txt > !out2" && cat out2' sh "$U"
t "the output to a file there" 'a
b
c' 0 sh -c '"$1" "there!sort !list.txt > there!~/sorted" && cat t/pub/sorted' sh "$U"
t "a command here, a file from there" 'x' 0 sh -c '"$1" "!cat there!~/x.txt > out3" && cat out3' sh "$U"
t "no system: here" '3' 0 sh -c '"$1" "wc -l < list.txt > out4" && tr -d " " < out4' sh "$U"
t "a pipeline: all of it there" '3' 0 sh -c '"$1" "there!cat !list.txt | wc -l > !out5" && tr -d " " < out5' sh "$U"
t "-p: the standard input is the command's" 'piped' 0 sh -c 'echo piped | "$1" -p "there!cat > !out6" && cat out6' sh "$U"
t "- as the first operand: as -p" 'dash' 0 sh -c 'echo dash | "$1" - "there!cat > !out7" && cat out7' sh "$U"
t "the operands are joined" 'a
b
c' 0 sh -c '"$1" there!sort !list.txt ">" !out8 && cat out8' sh "$U"
t "quoted words are not files" 'there!x' 0 sh -c '"$1" "there!sh -c \"echo there!x\" > !out9" && cat out9' sh "$U"
t "an unknown system before !: a word as it is (a mail address)" 'nosuchsys!bob' 0 sh -c '"$1" "!echo nosuchsys!bob > out13" && cat out13' sh "$U"
t "an output file in parentheses, sent where it goes" 'made' 0 sh -c '"$1" "there!sh -c \"echo made > o\" (!o)" 2>&1; "$1" "there!sh -c \"echo made > out10\" (!$PWD/out10)" && cat out10' sh "$U"
t "the files of other systems go to where it runs, by their names" 'a
b
c
x' 0 sh -c '"$1" "!sort there!~/x.txt !list.txt > out11" && cat out11' sh "$U"
t "two files of the same name: an error" 'uux: x.txt: two files of the same name' 1 sh -c 'cp t/pub/x.txt h/pub/x.txt; "$1" "there!cat !~/x.txt !$PWD/h/pub/x.txt" 2>&1' sh "$U"
t "the standard error of a command there comes here" 'oops' 0 sh -c '"$1" "there!sh -c \"echo oops >&2\"" 2>&1' sh "$U"
t "a command not permitted there" 'uux: there: rm: command not permitted' 1 sh -c 'rm -f mailbox; "$1" "there!rm -f x" 2>&1' sh "$U"
t "... the user gets mail" "$me uux job there.N failed" 0 sh -c 'sed "1s/[0-9][0-9]* failed/N failed/; 1q" mailbox' sh "$U"
t "-n: no mail" '' 1 sh -c 'rm -f mailbox; "$1" -n "there!rm -f x" 2>/dev/null; s=$?; test -f mailbox && echo mail; exit $s' sh "$U"
t "a file there outside the public directory" "uux: there: $T/list.txt: not permitted" 1 sh -c '"$1" -n "there!cat there!$PWD/list.txt" 2>&1' sh "$U"
t "a command that fails: status 0, mail" "0
$me uux job here.N failed" 0 sh -c 'rm -f mailbox; "$1" "!false"; echo $?; sed "1s/[0-9][0-9]* failed/N failed/; 1q" mailbox' sh "$U"
t "a command that fails, -n: no mail" '' 0 sh -c 'rm -f mailbox; "$1" -n "!false" && ! test -f mailbox' sh "$U"
t "-j: the job's id" 'there.' 0 sh -c '"$1" -j "there!cat !list.txt" | sed "s/[0-9]*\$//"' sh "$U"
t "a job done is not in the spool" '' 0 sh -c 'ls h/spool/jobs' sh "$U"
t "a system not reached: status 1, the job queued" "uux: cannot reach down: the connection was lost; the job is queued
1" 1 sh -c '"$1" "down!rmail me" 2>&1; s=$?; ls h/spool/jobs | wc -l | tr -d " "; exit $s' sh "$U"
rm -f h/spool/jobs/*
t ">> is not accepted" 'uux: >>: not accepted' 1 sh -c '"$1" "!cat x >> y" 2>&1' sh "$U"
t "<< is not accepted" 'uux: <<: not accepted' 1 sh -c '"$1" "!cat << y" 2>&1' sh "$U"
t ">| is not accepted" 'uux: >|: not accepted' 1 sh -c '"$1" "!cat x >| y" 2>&1' sh "$U"
t ">& is not accepted" 'uux: >&: not accepted' 1 sh -c '"$1" "!cat x >& y" 2>&1' sh "$U"
t "; is not accepted" 'uux: ;: only a pipeline can be run' 1 sh -c '"$1" "!cat x ; ls" 2>&1' sh "$U"
t "a system name on a command after the first" 'uux: there!wc: only the first command can have a system name' 1 sh -c '"$1" "!cat x | there!wc" 2>&1' sh "$U"
t "an unknown system" 'uux: nowhere: unknown system' 1 sh -c '"$1" "nowhere!cat x" 2>&1' sh "$U"
t "no operand: a usage error" '' 2 "$U"
t "an unknown option: a usage error" '' 2 "$U" -x "!true"
# getopt() must not take an option after an operand for one (no permuting)
t "an option after an operand is an operand" '-j' 0 sh -c '"$1" "!echo" -j "> out12" && cat out12' sh "$U"
done_tests uux
