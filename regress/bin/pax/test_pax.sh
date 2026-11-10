#!/bin/sh
# test_pax.sh - tests for pax (POSIX.1-2024).  Run: sh test_pax.sh
#
# The suites, with their data in tests/ (see README.md):
#   regression  tests/cases/, expected output in tests/expected/
# Each suite ends with a line "N passed, M failed"; this script adds them up.

cd "$(dirname "$0")" || exit 2
PROG_DIR=${PROG_DIR:-$PWD/../../../bin/pax}; export PROG_DIR
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

[ -x "$PROG_DIR/pax" ] || { echo "pax: not built (run make first)"; exit 2; }

# tl: the test language, built here (../../../usr.bin/tl/tl.c; ../../../usr.bin/tl/tl.md)
cc=${CC:-}
[ -n "$cc" ] || for c in c17 c99 cc; do command -v $c >/dev/null 2>&1 && { cc=$c; break; }; done
TLBIN=${TMPDIR:-/tmp}/tl.$$; export TLBIN
trap 'rm -f "$TLBIN"' EXIT
$cc -o "$TLBIN" ../../../usr.bin/tl/tl.c || { echo "cannot compile ../../../usr.bin/tl/tl.c"; exit 2; }

suite regression '"$TLBIN" tests/run.tl $PROG_DIR/pax'

echo "pax: $pass passed, $fail failed"
[ "$fail" = 0 ]
