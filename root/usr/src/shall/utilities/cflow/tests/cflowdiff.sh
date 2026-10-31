#!/bin/sh
# cflowdiff.sh CFLOW FILE...: cflow compared with GNU cflow (--format=posix
# --all, reading the files not preprocessed) on a program: the places of
# the definitions of the functions both find defined (name, file, line),
# and the calls between these functions (caller, callee), as sets: every
# call GNU cflow finds must be found (more may be: GNU cflow does not
# parse some C99, and misses calls after it; their number is written).
# GNU cflow lists callees in the order of the source and writes types its
# own way, so the trees themselves are not compared.
U=$1
shift
case $U in /*) ;; *) U=$PWD/$U ;; esac
command -v cflow >/dev/null && cflow --version 2>/dev/null | grep -q GNU || { echo "cflowdiff: no GNU cflow"; exit 2; }
G=$(command -v cflow)
T=${TMPDIR:-/tmp}/cflowdiff.$$
mkdir "$T" || exit 2
trap 'rm -rf "$T"' EXIT
export LC_ALL=C
# a tree: definitions (name file line) to $2, calls (caller callee) to $3;
# $1: g for GNU's indentation (one space and four per level), u for ours
# (one space at the top, four per level below)
split() {
    awk -v mode="$1" -v defs="$2" -v calls="$3" '{
        line = $0
        sub(/^ *[0-9]+/, "", line)
        match(line, /^ */); ind = RLENGTH
        rest = substr(line, ind + 1)
        if (mode == "g") d = int((ind - 1) / 4); else d = ind <= 1 ? 0 : int(ind / 4)
        name = rest; sub(/:.*/, "", name)
        stack[d] = name
        if (d > 0) print stack[d - 1], name > calls
        if (match(rest, /<[^ >]+ [0-9]+>/)) {
            s = substr(rest, RSTART + 1, RLENGTH - 2); split(s, a, " ")
            print name, a[1], a[2] > defs
        }
    }'
}
: > "$T/ud"; : > "$T/ue"; : > "$T/gd"; : > "$T/ge"
"$U" "$@" 2>/dev/null | split u "$T/ud" "$T/ue"
"$G" --format=posix --all "$@" 2>/dev/null | split g "$T/gd" "$T/ge"
awk '{ print $1 }' "$T/ud" | sort -u > "$T/un"
awk '{ print $1 }' "$T/gd" | sort -u > "$T/gn"
comm -12 "$T/un" "$T/gn" > "$T/both"
# the lines whose names (all fields but file and line) are in both
keep() { awk -v n="$2" 'NR == FNR { k[$1] = 1; next } ($1 in k) && (n == 1 || ($2 in k))' "$T/both" "$1" | sort -u; }
fail=0
keep "$T/ud" 1 > "$T/a"; keep "$T/gd" 1 > "$T/b"
cmp -s "$T/a" "$T/b" || { fail=1; echo "FAIL definitions"; diff "$T/a" "$T/b" | head -10; }
keep "$T/ue" 2 > "$T/a"; keep "$T/ge" 2 > "$T/b"
# calls GNU cflow finds and this one does not
comm -13 "$T/a" "$T/b" > "$T/c"
[ -s "$T/c" ] && { fail=1; echo "FAIL calls only GNU cflow finds:"; head -10 "$T/c"; }
extra=$(comm -23 "$T/a" "$T/b" | wc -l | tr -d ' ')
echo "cflowdiff: $(wc -l < "$T/both" | tr -d ' ') functions, $(wc -l < "$T/b" | tr -d ' ') calls: $([ $fail = 0 ] && echo the same || echo different) ($extra more found here)"
[ $fail = 0 ]
