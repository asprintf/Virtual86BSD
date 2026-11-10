#!/bin/sh
# run_tests.sh - build every utility and run its tests
# (regress/{bin,sbin,usr.bin,usr.sbin}/<utility>/test_<utility>.sh).
#
# Usage: sh run_tests.sh [utility...]
#
# Each test script ends with "<utility>: N passed, M failed"; that line is
# shown for each utility, with the details of whatever failed, and then
# the totals.  The exit status is 0 if everything passed.
#
#   CC=...   rebuild with another compiler command first ("make clean"
#            and "make CC=..."), for example with sanitizers:
#            ASAN_OPTIONS=detect_leaks=0 \
#            CC="cc -fsanitize=address,undefined -g" sh run_tests.sh
#   STDFLAGS=... more options for the compiler (with CC), e.g.
#            STDFLAGS="-std=c11 -Wall -Wextra" for gcc or clang
#   MAKE=... the make to build with (default: make)
cd "$(dirname "$0")/.." || exit 2
TOP=$(pwd)
MAKE=${MAKE:-make}

# the directory (bin, sbin, usr.bin or usr.sbin) a utility is in
where() {
    for d in bin sbin usr.bin usr.sbin; do
        if [ -d "$TOP/$d/$1" ]; then echo "$d"; return; fi
    done
    return 1
}

# with CC: the STDFLAGS to give make ("keep": the Makefiles' own, none)
stdflags=keep
if [ -n "$CC" ] && [ "${STDFLAGS+set}" = set ]; then
    stdflags=$STDFLAGS
fi

if [ $# -gt 0 ]; then
    list=$*
else
    list=$(for d in bin sbin usr.bin usr.sbin; do ls "regress/$d"; done | sed '/^lib$/d')
fi

log=${TMPDIR:-/tmp}/run_tests.$$
: > "$log" || exit 2
trap 'rm -f "$log"' EXIT
failed= total_pass=0 total_fail=0
for u in $list; do
    d=$(where "$u") || { echo "$u: no such utility"; failed="$failed $u"; continue; }
    if [ ! -f "regress/$d/$u/test_$u.sh" ]; then
        echo "$u: no test_$u.sh"
        failed="$failed $u"
        continue
    fi
    if [ -n "$CC" ] && [ "$stdflags" = keep ]; then
        (cd "$d/$u" && $MAKE -s clean && $MAKE -s CC="$CC") > "$log" 2>&1
    elif [ -n "$CC" ]; then
        (cd "$d/$u" && $MAKE -s clean && $MAKE -s CC="$CC" STDFLAGS="$stdflags") > "$log" 2>&1
    else
        (cd "$d/$u" && $MAKE -s) > "$log" 2>&1
    fi
    if [ $? != 0 ]; then
        echo "$u: BUILD FAILED"
        sed 's/^/    /' "$log"
        failed="$failed $u"
        continue
    fi
    out=$(cd "regress/$d/$u" && PROG_DIR="$TOP/$d/$u" sh "./test_$u.sh" 2>&1)
    rc=$?
    summary=$(printf '%s\n' "$out" | tail -n 1)
    echo "$summary"
    if [ $rc != 0 ]; then
        printf '%s\n' "$out" | sed '$d' | sed 's/^/    /'
        failed="$failed $u"
    fi
    p=$(printf '%s\n' "$summary" | sed -n 's/.*: \([0-9]*\) passed, \([0-9]*\) failed$/\1/p')
    f=$(printf '%s\n' "$summary" | sed -n 's/.*: \([0-9]*\) passed, \([0-9]*\) failed$/\2/p')
    total_pass=$((total_pass + ${p:-0}))
    total_fail=$((total_fail + ${f:-1}))
done

echo "total: $total_pass passed, $total_fail failed"
if [ -n "$failed" ]; then
    echo "failing:$failed"
    exit 1
fi
exit 0
