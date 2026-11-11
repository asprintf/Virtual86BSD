#!/bin/sh
# test_dmesg.sh - tests for dmesg (V86BSD).  Run: sh test_dmesg.sh
# The kernel's log is V86BSD's own: elsewhere only the usage is tested.  As
# root a line is written to /dev/kmsg to see it come back.
cd "$(dirname "$0")" || exit 2
PROG_DIR=${PROG_DIR:-$PWD/../../../sbin/dmesg}; export PROG_DIR
pass=0 fail=0
t() {
    _n=$1 _e=$2 _s=$3; shift 3
    _o=$("$@" 2>/dev/null); _r=$?
    if [ "$_o" = "$_e" ] && [ "$_r" = "$_s" ]; then pass=$((pass+1))
    else fail=$((fail+1)); printf 'FAIL %s\n  got:  [%s] status %s\n  want: [%s] status %s\n' "$_n" "$_o" "$_r" "$_e" "$_s"; fi
}
ok() {
    _n=$1; shift
    if "$@"; then pass=$((pass+1)); else fail=$((fail+1)); printf 'FAIL %s\n' "$_n"; fi
}
done_tests() { echo "$1: $pass passed, $fail failed"; [ "$fail" = 0 ]; }
export LC_ALL=C
D=${D:-$PROG_DIR/dmesg}
t "an operand" "usage: dmesg" 1 sh -c '"$1" x 2>&1' sh "$D"
t "an option" "usage: dmesg" 1 sh -c '"$1" -x 2>&1' sh "$D"
if [ "$(uname -s)" != V86BSD ]; then
    done_tests dmesg
    exit
fi
T=/var/tmp/test_dmesg.$$
mkdir -m 755 "$T" || exit 2
trap 'rm -rf "$T"' EXIT
"$D" > "$T/out"
t "it succeeds" "0" 0 sh -c '"$1" > /dev/null; echo $?' sh "$D"
ok "the log ends with a newline" sh -c '[ "$(tail -c 1 "$1" | od -An -c | tr -d " ")" = "\\n" ]' sh "$T/out"
ok "the root file system line is there" sh -c 'grep "^root on " "$1" > /dev/null' sh "$T/out"
ok "the console line is there" sh -c 'grep "^com0: console" "$1" > /dev/null' sh "$T/out"
ok "no line is cut at the start (the first begins a message)" sh -c 'head -n 1 "$1" | grep -E "^[A-Za-z]" > /dev/null' sh "$T/out"
ok "it is not more than the buffer" sh -c '[ "$(wc -c < "$1" | tr -d " ")" -le 65536 ]' sh "$T/out"
if [ "$(id -u)" = 0 ]; then
    echo "dmesg test marker 1" > /dev/kmsg
    t "a message written to /dev/kmsg is last" "dmesg test marker 1" 0 sh -c '"$1" | tail -n 1' sh "$D"
    t "reading does not take it away" "dmesg test marker 1" 0 sh -c '"$1" | tail -n 1' sh "$D"
    cc=${CC:-}
    [ -n "$cc" ] || for c in c17 c99 cc; do command -v $c >/dev/null 2>&1 && { cc=$c; break; }; done
    if $cc -o "$T/asuser" ../../lib/testlib/asuser.c 2>/dev/null; then
        t "dmesg as another user" "dmesg test marker 1" 0 sh -c '"$1" 65534 65534 "$2" | tail -n 1' sh "$T/asuser" "$D"
    fi
fi
done_tests dmesg
