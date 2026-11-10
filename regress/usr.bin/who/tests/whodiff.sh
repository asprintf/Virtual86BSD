#!/bin/sh
# whodiff.sh WHO: who compared with GNU who on databases made by mkutx.c,
# with the blanks squeezed (the columns differ) and TZ=UTC0.  The state
# and idle time (-T, -u, -a) come from the terminals, which GNU who looks
# for in /dev only, and GNU who writes the clock changes (-t)
# differently: these are not compared.
U=$1
case $U in /*) ;; *) U=$PWD/$U ;; esac
S=$(cd "$(dirname "$0")" && pwd)
T=${TMPDIR:-/tmp}/whodiff.$$
mkdir "$T" || exit 2
trap 'rm -rf "$T"' EXIT
cd "$T" || exit 2
${CC:-cc} -o mkutx "$S/mkutx.c" || exit 2
export TZ=UTC0 LC_ALL=C
fail=0 n=0
mkdir -p dev/pts
: > dev/pts/0; chmod 620 dev/pts/0
: > dev/pts/1; chmod 600 dev/pts/1
printf '%s\n' 'BOOT reboot ~ - 0 1700000000' 'LOGIN LOGIN tty9 9 500 1700000010' \
    'INIT - - si 12 1700000001' 'USER alice pts/0 ts/0 1234 1700000100 example.org' \
    'USER bob pts/1 ts/1 1300 1700000200' 'USER carol pts/7 ts/7 1350 1700000250' \
    'DEAD - pts/2 ts/2 1400 1700000300 - 0 1' 'EMPTY - - - 0 0' 'RUNLVL runlevel ~ - 13107 1700000005' \
    'USER dave pts/3 ts/3 1500 1700500000' 'DEAD x pts/4 ts/4 1600 1700600000 - 15 0' | ./mkutx > u
: > e
for f in u e; do
    for o in "" -s -q -H -b -d -l -p -r -bd -lp -sH -qH -bdlpr; do
        n=$((n + 1))
        a=$(POSIX_DEVDIR=$T/dev "$U" $o $f 2>&1 | tr -s ' ')
        b=$(cd dev && who $o ../$f 2>&1 | tr -s ' ' | sed 's/ *$//')
        if [ "$a" != "$b" ]; then
            fail=$((fail + 1)); echo "FAIL who $o $f"; printf '%s\n---\n%s\n' "$a" "$b"
        fi
    done
done
echo "whodiff: $n compared, $fail failed"
[ $fail = 0 ]
