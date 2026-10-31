#!/bin/sh
# test_newgrp.sh - tests for newgrp (POSIX.1-2024).  Run: sh test_newgrp.sh
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
U=${U:-$PWD/newgrp}
T=${TMPDIR:-/tmp}/test_newgrp.$$
mkdir -m 700 "$T" || exit 2
trap 'chmod -R u+rwx "$T" 2>/dev/null; rm -rf "$T" "$T.tl"' EXIT
# tl: the test language, built here (../testlib/tl.c; ../testlib/tl.md)
TL=$(cd ../testlib && pwd) || exit 2; export TL
cc=${cc:-${CC:-}}
[ -n "$cc" ] || for c in c17 c99 cc; do command -v $c >/dev/null 2>&1 && { cc=$c; break; }; done
$cc -o "$T.tl" "$TL/tl.c" || { echo "cannot compile ../testlib/tl.c"; exit 2; }
TLBIN=$T.tl; export TLBIN
tl() { "$TLBIN" "$@"; }
cd "$T" || exit 2
export LC_ALL=C
root=false; [ "$(id -u)" = 0 ] && root=true
nl='
'
# the new shell (the login shell of the user) reads its commands from the
# standard input; ids.tl writes the groups it runs with
cat > ids.tl <<'TL'
printf("%d %d [%s]\n", getgid(), getegid(), join(apply(sorted(getgroups()), str), ", "));
TL
# from SUPPLEMENTARY GROUP-ARGS...: newgrp run with the supplementary
# groups set to the list (as root), and commands on its standard input
cat > from.tl <<'TL'
setgroups(apply(filter(split(argv[0], ","), func (g) { return g != ""; }), int)) || exit(1);
exec(slice(argv, 1));
exit(1);
TL
if $root; then
    g=$(id -g nobody 2>/dev/null || echo 65534)
    gname=$(id -gn nobody 2>/dev/null)
    t "a group by name: real and effective" "$g $g" 0 sh -c 'echo "\"$TLBIN\" ids.tl" | "$1" "$2" | cut -d " " -f 1,2' sh "$U" "$gname"
    t "a group by number" "1 1" 0 sh -c 'echo "\"$TLBIN\" ids.tl" | "$1" 1 | cut -d " " -f 1,2' sh "$U"
    t "a number that is no group (root)" "54321 54321" 0 sh -c 'echo "\"$TLBIN\" ids.tl" | "$1" 54321 | cut -d " " -f 1,2' sh "$U"
    t "no operand: the user's group again" "0 0 [0]" 0 sh -c 'echo "echo \"\\\"$TLBIN\\\" ids.tl\" | \"$1\"" | "$1" 1' sh "$U"
    # the supplementary groups, as the standard says
    t "the old group in the list, the new not: added" "1 1 [0, 1, 2]" 0 sh -c 'echo "\"$TLBIN\" ids.tl" | "$TLBIN" from.tl 0,2 "$1" 1' sh "$U"
    t "both in the list: the list kept" "1 1 [0, 1, 2]" 0 sh -c 'echo "\"$TLBIN\" ids.tl" | "$TLBIN" from.tl 0,1,2 "$1" 1 | sed "s/^/0 /" | cut -d " " -f 2-' sh "$U"
    t "the old not in the list, the new in it: the new deleted, the old added" "1 1 [0, 2]" 0 sh -c 'echo "\"$TLBIN\" ids.tl" | "$TLBIN" from.tl 1,2 "$1" 1' sh "$U"
    t "neither in the list: the old added" "1 1 [0, 2]" 0 sh -c 'echo "\"$TLBIN\" ids.tl" | "$TLBIN" from.tl 2 "$1" 1' sh "$U"
    # the new shell
    t "the exit status is the shell's" "" 7 sh -c 'echo "exit 7" | "$1" 1' sh "$U"
    t "the working directory is kept" "$T" 0 sh -c 'echo pwd | "$1" 1' sh "$U"
    t "exported variables are kept" "x" 0 sh -c 'echo "echo \$FOO" | FOO=x "$1" 1' sh "$U"
    t "unexported variables are not" "" 0 sh -c 'BAR=y; echo "echo \$BAR" | "$1" 1' sh "$U"
    t "the file creation mask is kept" "0027" 0 sh -c 'umask 027; echo umask | "$1" 1' sh "$U"
    sh=$(sed -n 's/^root:[^:]*:0:[^:]*:[^:]*:[^:]*://p' /etc/passwd)
    t "the login shell of the user" "${sh##*/}" 0 sh -c 'echo "echo \$0" | "$1" 1' sh "$U"
    home=$(sed -n 's/^root:[^:]*:0:[^:]*:[^:]*:\([^:]*\):.*/\1/p' /etc/passwd)
    t "-l: the working directory is HOME" "$home" 0 sh -c 'echo pwd | "$1" -l 1' sh "$U"
    t "-l: a new environment" "|$home|root|root" 0 sh -c 'echo "echo \"\$FOO|\$HOME|\$USER|\$LOGNAME\"" | FOO=x HOME=/x "$1" -l 1' sh "$U"
    t "-l: TERM kept" "vt100" 0 sh -c 'echo "echo \$TERM" | TERM=vt100 "$1" -l 1' sh "$U"
    t "-l: a login shell" "-" 0 sh -c 'echo "echo \$0" | "$1" -l 1 | cut -c 1' sh "$U"
    t "-: as -l" "$home" 0 sh -c 'echo pwd | "$1" - 1' sh "$U"
    t "-l: the group" "1 1" 0 sh -c 'echo "\"$TLBIN\" $2" | "$1" -l 1 | cut -d " " -f 1,2' sh "$U" "$T/ids.tl"
    t "an unknown group: the shell all the same" "ran 0" 0 sh -c 'echo "echo ran \$(id -g)" | "$1" no_such_group_x9 2>/dev/null' sh "$U"
    t "an unknown group: a diagnostic" "" 0 sh -c 'echo true | "$1" no_such_group_x9 2>&1 >/dev/null | grep -q .' sh "$U"
    # a user who is not root, with newgrp set-user-ID root (as if: the real
    # user ID nobody's, the effective root's): not a member, refused
    t "set-user-ID root, not a member: refused" "yes" 0 sh -c '"$TLBIN" -e "setgroups([]) || exit(1); setresgid(65534, 65534, 65534) || exit(1); setresuid(65534, 0, 0) || exit(1); exec(argv); exit(1);" "$1" root < /dev/null 2>&1 | grep "not a member" >/dev/null && echo yes' sh "$U"
fi
# usage, for any user
t "two operands" "" 2 "$U" a b
t "an unknown option" "" 2 "$U" -x
# getopt() must not take an option after an operand for one (no permuting)
t "options after operands: newgrp g -l: -l is a second operand" "" 2 "$U" g -l

done_tests newgrp
