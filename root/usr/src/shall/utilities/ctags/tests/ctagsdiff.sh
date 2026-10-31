#!/bin/sh
# ctagsdiff.sh CTAGS FILE...: ctags -x compared with universal-ctags
# (ctags-universal, or a ctags that is it) on the C files: the
# identifiers, line numbers and files of the function definitions,
# typedefs, macros and structure, union and enumeration tags (the kinds
# f t d s u g of universal-ctags; its anonymous tags left out, main
# written as M<file>), as sets.
U=$1
shift
case $U in /*) ;; *) U=$PWD/$U ;; esac
G=
for c in ctags-universal universal-ctags ctags; do
    if command -v $c >/dev/null 2>&1 && $c --version 2>/dev/null | grep -q 'Universal Ctags'; then
        G=$c
        break
    fi
done
[ -n "$G" ] || { echo "ctagsdiff: no universal-ctags"; exit 2; }
T=${TMPDIR:-/tmp}/ctagsdiff.$$
mkdir "$T" || exit 2
trap 'rm -rf "$T"' EXIT
export LC_ALL=C
fail=0 n=0
for f in "$@"; do
    n=$((n + 1))
    "$U" -x "$f" | awk '{ print $1, $2, $3 }' | sort -u > "$T/a"
    m=$(basename "$f" .c)
    "$G" -x --language-force=C --kinds-C=dfgstu --_xformat='%N %n %F' "$f" 2>/dev/null |
        awk -v m="M$m" '$1 !~ /^__anon/ { if ($1 == "main") $1 = m; print $1, $2, $3 }' | sort -u > "$T/b"
    if ! cmp -s "$T/a" "$T/b"; then
        fail=$((fail + 1))
        echo "FAIL $f"
        diff "$T/a" "$T/b" | head -6
    fi
done
echo "ctagsdiff: $n compared, $fail failed"
[ $fail = 0 ]
