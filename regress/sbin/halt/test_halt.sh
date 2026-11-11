#!/bin/sh
# test_halt.sh - tests for halt and reboot (V86BSD).  Run: sh test_halt.sh
# The stopping itself is not tried here (it would stop the machine): it is checked
# by a boot that runs `shutdown -p now' (regress/etc/boot.sh); here the usage, the
# check that the caller is root, and that reboot is the same program.
cd "$(dirname "$0")" || exit 2
PROG_DIR=${PROG_DIR:-$PWD/../../../sbin/halt}; export PROG_DIR
pass=0 fail=0
t() {
    _n=$1 _e=$2 _s=$3; shift 3
    _o=$("$@" 2>/dev/null); _r=$?
    if [ "$_o" = "$_e" ] && [ "$_r" = "$_s" ]; then pass=$((pass+1))
    else fail=$((fail+1)); printf 'FAIL %s\n  got:  [%s] status %s\n  want: [%s] status %s\n' "$_n" "$_o" "$_r" "$_e" "$_s"; fi
}
done_tests() { echo "$1: $pass passed, $fail failed"; [ "$fail" = 0 ]; }
export LC_ALL=C
H=${H:-$PROG_DIR/halt}
T=${TMPDIR:-/tmp}/test_halt.$$
mkdir -m 755 "$T" || exit 2
trap 'rm -rf "$T"' EXIT
ln "$H" "$T/reboot" 2>/dev/null || cp "$H" "$T/reboot"
ln "$H" "$T/halt" 2>/dev/null || cp "$H" "$T/halt"
t "halt: a bad option" "usage: halt [-npq]" 0 sh -c '"$1" -x 2>&1 | grep "^usage"' sh "$T/halt"
t "halt: an operand" "usage: halt [-npq]" 1 sh -c '"$1" now 2>&1' sh "$T/halt"
t "reboot: a bad option" "usage: reboot [-nq]" 0 sh -c '"$1" -x 2>&1 | grep "^usage"' sh "$T/reboot"
t "reboot: -p is for halt" "usage: reboot [-nq]" 0 sh -c '"$1" -p 2>&1 | grep "^usage"' sh "$T/reboot"
t "reboot: an operand" "usage: reboot [-nq]" 1 sh -c '"$1" now 2>&1' sh "$T/reboot"
if [ "$(id -u)" = 0 ]; then
    cc=${CC:-}
    [ -n "$cc" ] || for c in c17 c99 cc; do command -v $c >/dev/null 2>&1 && { cc=$c; break; }; done
    if $cc -o "$T/asuser" ../../lib/testlib/asuser.c 2>/dev/null; then
        t "halt: not root" "halt: you must be root" 1 sh -c '"$1" 65534 65534 "$2" 2>&1' sh "$T/asuser" "$T/halt"
        t "reboot: not root" "reboot: you must be root" 1 sh -c '"$1" 65534 65534 "$2" 2>&1' sh "$T/asuser" "$T/reboot"
        t "halt -q: not root" "halt: you must be root" 1 sh -c '"$1" 65534 65534 "$2" -q 2>&1' sh "$T/asuser" "$T/halt"
    fi
else
    t "halt: not root" "halt: you must be root" 1 sh -c '"$1" 2>&1' sh "$T/halt"
    t "reboot: not root" "reboot: you must be root" 1 sh -c '"$1" 2>&1' sh "$T/reboot"
fi
done_tests halt
