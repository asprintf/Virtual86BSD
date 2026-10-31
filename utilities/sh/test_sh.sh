#!/bin/sh
# test_sh.sh - tests for sh (POSIX.1-2024).  Run: sh test_sh.sh
#
# The suites, with their data in tests/ (see README.md):
#   regression  tests/cases*.txt, expected results in tests/expected.json
#   terminal    interactive use on a pseudo-terminal
# Each suite ends with a line "N passed, M failed"; this script adds them up.

cd "$(dirname "$0")" || exit 2
pass=0 fail=0

# suite NAME COMMAND: run one suite, show what failed, count the results
suite() {
    _n=$1
    _o=$(eval "$2" 2>&1)
    _r=$?
    _l=$(printf '%s\n' "$_o" | tail -n 1)
    _p=$(printf '%s\n' "$_l" | sed -n 's/^\([0-9]*\) passed, \([0-9]*\) failed$/\1/p')
    _f=$(printf '%s\n' "$_l" | sed -n 's/^\([0-9]*\) passed, \([0-9]*\) failed$/\2/p')
    if [ -z "$_p" ]; then
        _p=0 _f=1
        _l="did not finish"
    elif [ "$_r" != 0 ] && [ "$_f" = 0 ]; then
        _f=1
    fi
    if [ "$_f" != 0 ]; then
        printf '%s\n' "$_o" | grep -v '^ok ' | sed '$d' | sed "s/^/  [$_n] /"
    fi
    echo "  $_n: $_l"
    pass=$((pass + _p))
    fail=$((fail + _f))
}

[ -x ./sh ] || { echo "sh: not built (run make first)"; exit 2; }

# tl: the test language, built here (../testlib/tl.c; ../testlib/tl.md)
cc=${CC:-}
[ -n "$cc" ] || for c in c17 c99 cc; do command -v $c >/dev/null 2>&1 && { cc=$c; break; }; done
TLBIN=${TMPDIR:-/tmp}/tl.$$; export TLBIN
trap 'rm -f "$TLBIN"' EXIT
$cc -o "$TLBIN" ../testlib/tl.c || { echo "cannot compile ../testlib/tl.c"; exit 2; }

suite regression '"$TLBIN" tests/regress.tl'
suite terminal '"$TLBIN" tests/pty_test.tl'

# kill as a utility of its own (kill.sh, installed as kill: XCU 1.6)
sleep 30 & kp=$!
if ./sh kill.sh -s TERM "$kp" && ! wait "$kp" && [ "$(./sh kill.sh -l 15)" = TERM ]; then
    pass=$((pass + 1)); echo "  kill.sh: ok"
else
    fail=$((fail + 1)); echo "  kill.sh: FAILED"; kill "$kp" 2>/dev/null
fi

echo "sh: $pass passed, $fail failed"
[ "$fail" = 0 ]
