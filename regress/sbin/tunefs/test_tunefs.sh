#!/bin/sh
# test_tunefs.sh - tests for tunefs (V86BSD).  Run: sh test_tunefs.sh
# The file systems are made by newfs in image files; tunefs changes their
# parameters, dumpfs shows them and fsck_ffs must still call them clean.
# Needs no root.
cd "$(dirname "$0")" || exit 2
PROG_DIR=${PROG_DIR:-$PWD/../../../sbin/tunefs}; export PROG_DIR
pass=0 fail=0
ok() {
    _n=$1; shift
    if "$@"; then pass=$((pass+1)); else fail=$((fail+1)); printf 'FAIL %s\n' "$_n"; fi
}
done_tests() { echo "tunefs: $pass passed, $fail failed"; [ "$fail" = 0 ]; }
export LC_ALL=C

prog() {
    [ -x "$PROG_DIR/../$1/$1" ] || (cd "$PROG_DIR/../$1" && make > /dev/null 2>&1)
    echo "$PROG_DIR/../$1/$1"
}
U=${U:-$PROG_DIR/tunefs}
[ -x "$U" ] || (cd "$PROG_DIR" && make > /dev/null 2>&1)
N=$(prog newfs)
D=$(prog dumpfs)
F=$(prog fsck_ffs)
for p in "$N" "$U" "$D" "$F"; do
    [ -x "$p" ] || { echo "tunefs: cannot build $p"; exit 2; }
done
T=${TMPDIR:-/tmp}/test_tunefs.$$
mkdir -m 700 "$T" || exit 2
trap 'rm -rf "$T"' EXIT
IMG=$T/img

mkimg() { rm -f "$IMG"; dd if=/dev/zero of="$IMG" bs=1048576 seek=31 count=1 2>/dev/null; "$N" -q "$@" "$IMG" > /dev/null 2>&1; }
fld() {
    "$D" "$IMG" | awk -F'\t' -v n="$1" '{
        for (i = 1; i <= NF; i++) {
            if ($i == n) { v = $(i + 1); sub(/ .*/, "", v); print v; exit }
            if (index($i, n " ") == 1) { v = substr($i, length(n) + 2); print v; exit }
        }
    }'
}
clean() { "$F" -fn "$IMG" > /dev/null 2>&1; }
status() { "$@" > "$T/out" 2>&1; echo $?; }
has() { grep -F -q -- "$1" "$T/out"; }

# ---- usage and errors
ok "no arguments: usage, status 2" [ "$(status "$U")" = 2 ]
ok "  the usage line" has "usage: tunefs [-AFN]"
ok "an unknown option: status 2" [ "$(status "$U" -x "$IMG")" = 2 ]
mkimg
ok "-m 100: status 1" [ "$(status "$U" -F -m 100 "$IMG")" = 1 ]
ok "  says why" has "Invalid number \`100' for minimum percentage of free space"
ok "-e 0: status 1" [ "$(status "$U" -F -e 0 "$IMG")" = 1 ]
ok "-o fast: status 10" [ "$(status "$U" -F -o fast "$IMG")" = 10 ]
ok "  space or time" has "options are \`space' or \`time'"
ok "a file that is not there: status 1" [ "$(status "$U" -F "$T/nonesuch")" = 1 ]
: > "$T/empty"
ok "a file that is no file system: status 5" [ "$(status "$U" -F "$T/empty")" = 5 ]
dd if=/dev/zero of="$T/zero" bs=1048576 count=2 2>/dev/null
ok "zeroes: no super block, status 5" [ "$(status "$U" -F "$T/zero")" = 5 ]
ok "  cannot find the super block" has "cannot find filesystem superblock"

# ---- what is there (-N changes nothing)
mkimg
cp "$IMG" "$T/before"
"$U" -F -N "$IMG" > "$T/out" 2>&1; st=$?
ok "-N: status 0" [ "$st" = 0 ]
ok "  maximum contiguous block count" has "maximum contiguous block count 1"
ok "  maximum blocks per file in a cylinder group" has "maximum blocks per file in a cylinder group $(fld maxbpg)"
ok "  minimum percentage of free space 5%" has "minimum percentage of free space 5%"
ok "  optimization preference: time" has "optimization preference: time"
ok "  average file size" has "average file size: $(fld avgfilesize)"
ok "  files per directory" has "expected number of files per directory: $(fld avgfpdir)"
ok "  no changes made" has "tunefs: no changes made"
ok "  and the image is as it was" cmp -s "$IMG" "$T/before"
"$U" -F -N -m 20 -o space "$IMG" > "$T/out" 2>&1
ok "-N with changes: they are told, not made" sh -c 'grep -q "minimum percentage of free space changes from 5% to 20%" "$1" && cmp -s "$2" "$3"' sh "$T/out" "$IMG" "$T/before"

# ---- changes
mkimg
"$U" -F -e 500 -g 8192 -h 32 -m 10 -o space "$IMG" > "$T/out" 2>&1; st=$?
ok "-e 500 -g 8192 -h 32 -m 10 -o space: status 0" [ "$st" = 0 ]
ok "  maxbpg is 500" [ "$(fld maxbpg)" = 500 ]
ok "  avgfilesize is 8192" [ "$(fld avgfilesize)" = 8192 ]
ok "  avgfpdir is 32" [ "$(fld avgfpdir)" = 32 ]
ok "  minfree is 10%" [ "$(fld minfree)" = "10%" ]
ok "  optim is space" [ "$(fld optim)" = space ]
ok "  it says what changed" has "maximum blocks per file in a cylinder group changes from"
ok "  and the file system is still clean" clean
ok "  minfree 10 with space: should optimize for time (a warning only)" has "should optimize for time with minfree >= 5%"
"$U" -F -m 10 "$IMG" > "$T/out" 2>&1
ok "the same value again: remains unchanged" has "minimum percentage of free space remains unchanged at 10%"
"$U" -F -o space "$IMG" > "$T/out" 2>&1
ok "the same optimization again: remains unchanged" has "optimization preference remains unchanged as space"
"$U" -F -o time "$IMG" > "$T/out" 2>&1
ok "-o time with minfree 10: nothing to warn of" sh -c 'grep -q "changes from space to time" "$1" && ! grep -q "should optimize" "$1"' sh "$T/out"
"$U" -F -m 2 "$IMG" > "$T/out" 2>&1
ok "-m 2 with time: should optimize for space" has "should optimize for space with minfree < 5%"
"$U" -F -m 8 -o space "$IMG" > "$T/out" 2>&1
ok "-o space with minfree 8: should optimize for time" has "should optimize for time with minfree >= 5%"
ok "  the file system is clean after all of it" clean

# ---- -A: the copies of the super block too
mkimg
"$N" -N "$IMG" > "$T/nf" 2>&1
first=$(sed -n '/^super-block backups/{n;s/^ *\([0-9]*\),.*/\1/p;}' "$T/nf")
"$U" -F -A -m 12 "$IMG" > /dev/null 2>&1
printf '\0\0\0\0' | dd of="$IMG" bs=1 seek=$((65536 + 1372)) conv=notrunc 2>/dev/null     # the magic of the first copy
"$F" -fn "$IMG" > /dev/null 2>&1; st=$?
ok "the primary super block is gone: status 8" [ "$st" = 8 ]
"$F" -b "$first" -fy "$IMG" > /dev/null 2>&1
ok "fsck_ffs -b the copy repairs it" clean
ok "  and the copy had the -A change (minfree 12%)" [ "$(fld minfree)" = "12%" ]
mkimg
"$U" -F -m 12 "$IMG" > /dev/null 2>&1
printf '\0\0\0\0' | dd of="$IMG" bs=1 seek=$((65536 + 1372)) conv=notrunc 2>/dev/null
"$F" -b "$first" -fy "$IMG" > /dev/null 2>&1
ok "without -A the copy has the old value (5%)" [ "$(fld minfree)" = "5%" ]
done_tests
