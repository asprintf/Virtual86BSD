#!/bin/sh
# mesgdiff.sh MESG: mesg compared with util-linux mesg on a terminal: the
# exit status after each sequence of operands, and whether the group or
# others may write to the terminal (util-linux mesg y may give others the
# permission too).  Only the y or n of the reports is compared.
U=$1
case $U in /*) ;; *) U=$PWD/$U ;; esac
TL=$(cd "$(dirname "$0")/../../testlib" && pwd) || exit 2
command -v mesg >/dev/null || { echo "mesgdiff: no mesg"; exit 2; }
# tl, the test language (../../testlib/tl.c), for ptyrun.tl
cc=${CC:-}
[ -n "$cc" ] || for c in c17 c99 cc; do command -v $c >/dev/null 2>&1 && { cc=$c; break; }; done
TLBIN=${TMPDIR:-/tmp}/tl.$$
trap 'rm -f "$TLBIN"' EXIT
$cc -o "$TLBIN" "$TL/tl.c" || exit 2
fail=0 n=0
for seq in "y" "n" "y y" "n n" "y n" "n y" "n -" "y -" "go+w n" "o+w -" "g+w -" "go-w -"; do
    for m in "$U" mesg; do
        "$TLBIN" "$TL/ptyrun.tl" 012 sh -c '
            m=$1; shift; chmod g-w,o-w "$(tty)"
            for a; do case $a in
                -) r=$("$m" 2>/dev/null); s=$?; echo "report ${r##* } $s";;
                *[+-]w) chmod "$a" "$(tty)";;
                *) "$m" "$a" > /dev/null 2>&1; echo "status $?";;
            esac; done
            case $(ls -lL "$(tty)" | cut -c6,9) in *w*) echo allowed;; *) echo denied;; esac' sh "$m" $seq > out.$$.$( [ "$m" = mesg ] && echo b || echo a)
    done
    n=$((n + 1))
    if ! cmp -s out.$$.a out.$$.b; then
        fail=$((fail + 1)); echo "FAIL mesg $seq"; cat out.$$.a; echo ---; cat out.$$.b
    fi
    rm -f out.$$.a out.$$.b
done
echo "mesgdiff: $n compared, $fail failed"
[ $fail = 0 ]
