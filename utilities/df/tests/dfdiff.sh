#!/bin/sh
# dfdiff.sh DF: df -P and df -Pk compared with GNU df (with POSIXLY_CORRECT
# set) for each file system GNU df lists and some files in them; spaces
# squeezed.  The figures of a file system in use can change between the
# two runs, so a line that differs is tried again, up to five times.
U=$1
case $U in /*) ;; *) U=$PWD/$U ;; esac
fail=0 n=0
ops=$(POSIXLY_CORRECT=1 df -P 2>/dev/null | awk 'NR > 1 { print $NF }')
for f in $ops / /dev /tmp . /dev/null /proc; do
    [ -e "$f" ] || continue
    for o in -P -Pk; do
        n=$((n + 1)) k=0
        while :; do
            a=$("$U" $o "$f" 2>&1 | tr -s ' '); b=$(POSIXLY_CORRECT=1 df $o "$f" 2>&1 | tr -s ' ' | sed 's/ - / 0% /')
            [ "$a" = "$b" ] && break
            k=$((k + 1))
            if [ $k = 5 ]; then
                fail=$((fail + 1)); echo "FAIL df $o $f"; printf '%s\n---\n%s\n' "$a" "$b"; break
            fi
        done
    done
done
echo "dfdiff: $n compared, $fail failed"
[ $fail = 0 ]
