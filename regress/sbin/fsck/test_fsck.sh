#!/bin/sh
# test_fsck.sh - tests for fsck (V86BSD).  Run: sh test_fsck.sh
# fsck runs /sbin/fsck_TYPE, so what needs that program (and fstab, which is
# changed for a moment, on V86BSD itself only) runs only where /sbin/fsck_ffs
# is; the usage and option tests run anywhere.
cd "$(dirname "$0")" || exit 2
PROG_DIR=${PROG_DIR:-$PWD/../../../sbin/fsck}; export PROG_DIR
pass=0 fail=0
ok() {
    _n=$1; shift
    if "$@"; then pass=$((pass+1)); else fail=$((fail+1)); printf 'FAIL %s\n' "$_n"; fi
}
done_tests() { echo "fsck: $pass passed, $fail failed"; [ "$fail" = 0 ]; }
export LC_ALL=C

prog() {
    [ -x "$PROG_DIR/../$1/$1" ] || (cd "$PROG_DIR/../$1" && make > /dev/null 2>&1)
    echo "$PROG_DIR/../$1/$1"
}
F=${F:-$PROG_DIR/fsck}
[ -x "$F" ] || (cd "$PROG_DIR" && make > /dev/null 2>&1)
N=$(prog newfs)
D=$(prog dumpfs)
for p in "$N" "$F" "$D"; do
    [ -x "$p" ] || { echo "fsck: cannot build $p"; exit 2; }
done
T=${TMPDIR:-/tmp}/test_fsck.$$
mkdir -m 700 "$T" || exit 2
FSTAB_SAVED=
restore() {
    [ -z "$FSTAB_SAVED" ] || cp "$T/fstab.saved" /etc/fstab
    rm -rf "$T"
}
trap restore EXIT

# mkimg FILE: a 32 MiB file of zeroes with a file system
mkimg() {
    rm -f "$1"
    dd if=/dev/zero of="$1" bs=1048576 seek=31 count=1 2>/dev/null
    "$N" -q "$1" > /dev/null 2>&1
}
# fld IMG NAME: the value of a field of the file system, as dumpfs shows it
fld() {
    "$D" "$1" | awk -F'\t' -v n="$2" '{
        for (i = 1; i <= NF; i++) {
            if ($i == n) { v = $(i + 1); sub(/ .*/, "", v); print v; exit }
            if (index($i, n " ") == 1) { v = substr($i, length(n) + 2); print v; exit }
        }
    }'
}
# damage IMG: the magic number of the first cylinder group is gone
damage() {
    _off=$(( $(fld "$1" cblkno) * $(fld "$1" fsize) + 4 ))
    printf '\0\0\0\0' | dd of="$1" bs=1 seek="$_off" conv=notrunc 2>/dev/null
}
clean() { "$F" -t ffs -fn "$1" > /dev/null 2>&1; }
has() { grep -F -q -- "$2" "$1"; }

# ---- usage and options (no file system is looked at)
"$F" -x > "$T/o" 2>&1; _r=$?
ok "an unknown option: status 1" [ "$_r" = 1 ]
ok "  usage: fsck [-dfNnpvy] ..." sh -c 'grep -q "^usage: fsck \[-dfNnpvy\] \[-b block#\] \[-l maxparallel\]" "$1"' sh "$T/o"
"$F" -t ffs -t ffs /nonexistent > "$T/o" 2>&1; _r=$?
ok "two -t: status 1" [ "$_r" = 1 ]
ok "  one -t only" has "$T/o" "only one -t option may be specified."
"$F" -l abc /nonexistent > "$T/o" 2>&1; _r=$?
ok "-l abc: status 1" [ "$_r" = 1 ]
ok "  -l abc: invalid" has "$T/o" "-l abc: invalid"
"$F" -T ffs > "$T/o" 2>&1; _r=$?
ok "-T without a colon: status 1" [ "$_r" = 1 ]
ok "  Invalid option string" has "$T/o" "Invalid option string"

# ---- what runs fsck_ffs
if [ -x /sbin/fsck_ffs ]; then
    IMG=$T/img
    mkimg "$IMG"
    "$F" -t ffs -fn "$IMG" > "$T/o" 2>&1; _r=$?
    ok "-t ffs -fn on a clean file system: status 0" [ "$_r" = 0 ]
    ok "  fsck_ffs ran: Phase 5" has "$T/o" "Phase 5 - Check Cyl groups"
    "$F" -fn "$IMG" > /dev/null 2>&1; _r=$?
    ok "no -t: the type is ffs: status 0" [ "$_r" = 0 ]
    "$F" -p -t ffs "$IMG" > "$T/o" 2>&1; _r=$?
    ok "-p on a clean file system: status 0" [ "$_r" = 0 ]
    ok "  and it is not checked" has "$T/o" "file system is clean; not checking"
    damage "$IMG"
    cp "$IMG" "$T/damaged"
    "$F" -t ffs -fn "$IMG" > "$T/o" 2>&1; _r=$?
    ok "a damaged file system, -fn: status not 0" [ "$_r" != 0 ]
    ok "  BAD MAGIC NUMBER" has "$T/o" "BAD MAGIC NUMBER"
    ok "  and it is as it was" cmp -s "$IMG" "$T/damaged"
    "$F" -t ffs -fy "$IMG" > /dev/null 2>&1; _r=$?
    ok "-fy repairs it: status 0" [ "$_r" = 0 ]
    ok "  and it is clean" clean "$IMG"
    "$F" -t ffs -fn "$T/nonexistent" > /dev/null 2>&1; _r=$?
    ok "a file that is not there: status not 0" [ "$_r" != 0 ]
    "$F" -t nonesuch -fn "$IMG" > "$T/o" 2>&1; _r=$?
    ok "no fsck_nonesuch: status not 0" [ "$_r" != 0 ]

    # ---- fstab: only on V86BSD, and as root (it is /etc/fstab that changes)
    if [ "$(uname -s)" = V86BSD ] && [ "$(id -u)" = 0 ]; then
        cp /etc/fstab "$T/fstab.saved" && FSTAB_SAVED=1
        A=$T/a B=$T/b C=$T/c
        mkimg "$A"; mkimg "$B"; mkimg "$C"
        damage "$A"; damage "$B"; damage "$C"
        cp "$C" "$T/c.damaged"
        cat > /etc/fstab <<EOT
$A /a ffs rw 1 1
$B /b ffs rw 2 2
$C /c ffs rw 0 0
EOT
        "$F" -fn > "$T/o" 2>&1; _r=$?
        ok "fsck -fn: the first bad file system stops it: status not 0" [ "$_r" != 0 ]
        "$F" -fy > "$T/o" 2>&1; _r=$?
        ok "fsck -fy (no -p: one after the other): status 0" [ "$_r" = 0 ]
        ok "  the pass 1 file system is repaired" clean "$A"
        ok "  the pass 2 file system is repaired" clean "$B"
        ok "  pass 0 is not checked" cmp -s "$C" "$T/c.damaged"
        "$F" -p > "$T/o" 2>&1; _r=$?
        ok "fsck -p on clean file systems: status 0" [ "$_r" = 0 ]
        ok "  both were skipped as clean" sh -c 'test "$(grep -c "file system is clean; not checking" "$1")" = 2' sh "$T/o"
        "$F" -t ufs -p > /dev/null 2>&1; _r=$?
        ok "-t ufs: no file system of that type: status 0" [ "$_r" = 0 ]
        "$F" -p "$B" > "$T/o" 2>&1; _r=$?
        ok "fsck -p of one file system named by its device: status 0" [ "$_r" = 0 ]
        "$F" -p /b > "$T/o" 2>&1; _r=$?
        ok "fsck -p of one named by its mount point: status 0" [ "$_r" = 0 ]
        cp "$T/fstab.saved" /etc/fstab; FSTAB_SAVED=
    fi
else
    echo "fsck: no /sbin/fsck_ffs here: the tests that run it are skipped"
fi
done_tests
