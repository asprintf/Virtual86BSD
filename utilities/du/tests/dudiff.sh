#!/bin/sh
# dudiff.sh DU [SEED [RUNS]]: random trees, du compared with GNU du (run
# with POSIXLY_CORRECT set, its tabs made spaces).  Hard links and links
# to directories make the entry written depend on the order of the walk,
# so with them only the totals of -s are compared.
U=$1; seed=${2:-1}; runs=${3:-50}
case $U in /*) ;; *) U=$PWD/$U ;; esac
T=${TMPDIR:-/tmp}/dudiff.$$
mkdir "$T" || exit 2
trap 'rm -rf "$T"' EXIT
cd "$T" || exit 2
fail=0 n=0
r=0
while [ $r -lt "$runs" ]; do
    rm -rf t && mkdir t
    # the tree, made by awk from the seed
    awk -v seed=$((seed * 1000 + r)) 'BEGIN {
        srand(seed); nd = 1; dirs[0] = "t"
        for (i = 0; i < 40; i++) {
            d = dirs[int(rand() * nd)]
            k = rand()
            if (k < 0.3) { dirs[nd++] = d "/d" i; print "mkdir " d "/d" i }
            else if (k < 0.8) { print "head -c " int(rand() * 20000) " /dev/zero > " d "/f" i }
            else if (k < 0.9) { print "ln -s f" i " " d "/s" i }
            else print "ln -s nowhere " d "/n" i
        }
    }' > mk.sh
    sh mk.sh 2>/dev/null
    for o in "" -a -s -k -ak -sk -x -H -L -aL; do
        a=$("$U" $o t 2>/dev/null | sort); b=$(POSIXLY_CORRECT=1 du $o t 2>/dev/null | tr '\t' ' ' | sort)
        n=$((n + 1))
        if [ "$a" != "$b" ]; then
            fail=$((fail + 1)); echo "FAIL seed $seed run $r: du $o t"
            printf '%s\n---\n%s\n' "$a" "$b" | head -20
        fi
    done
    # hard links: the totals
    f=$(find t -type f | sed 1q)
    if [ -n "$f" ]; then
        ln "$f" t/hard
        for o in -s -sk -sL; do
            a=$("$U" $o t 2>/dev/null); b=$(POSIXLY_CORRECT=1 du $o t 2>/dev/null | tr '\t' ' ')
            n=$((n + 1))
            [ "$a" = "$b" ] || { fail=$((fail + 1)); echo "FAIL seed $seed run $r: du $o t (hard link)"; echo "$a / $b"; }
        done
    fi
    r=$((r + 1))
done
echo "dudiff: $n compared, $fail failed"
[ $fail = 0 ]
