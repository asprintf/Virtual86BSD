#!/bin/sh
# test_fsck_ffs.sh - tests for fsck_ffs (V86BSD).  Run: sh test_fsck_ffs.sh
# The file systems are made by newfs in image files and damaged by writing
# into them where dumpfs says the inodes, the summary and the cylinder groups
# are; fsck_ffs must call each one damaged, repair it, and then call it
# clean.  Needs no root.
cd "$(dirname "$0")" || exit 2
PROG_DIR=${PROG_DIR:-$PWD/../../../sbin/fsck_ffs}; export PROG_DIR
pass=0 fail=0
# t NAME EXPECTED-STDOUT EXPECTED-STATUS COMMAND...  (stdout compared without trailing newlines)
t() {
    _n=$1 _e=$2 _s=$3; shift 3
    _o=$("$@" 2>/dev/null); _r=$?
    if [ "$_o" = "$_e" ] && [ "$_r" = "$_s" ]; then pass=$((pass+1))
    else fail=$((fail+1)); printf 'FAIL %s\n  got:  [%s] status %s\n  want: [%s] status %s\n' "$_n" "$_o" "$_r" "$_e" "$_s"; fi
}
# te NAME EXPECTED-OUTPUT EXPECTED-STATUS COMMAND...  (stdout and stderr together)
te() {
    _n=$1 _e=$2 _s=$3; shift 3
    _o=$("$@" 2>&1); _r=$?
    if [ "$_o" = "$_e" ] && [ "$_r" = "$_s" ]; then pass=$((pass+1))
    else fail=$((fail+1)); printf 'FAIL %s\n  got:  [%s] status %s\n  want: [%s] status %s\n' "$_n" "$_o" "$_r" "$_e" "$_s"; fi
}
# ok NAME CONDITION-COMMAND...
ok() {
    _n=$1; shift
    if "$@"; then pass=$((pass+1)); else fail=$((fail+1)); printf 'FAIL %s\n' "$_n"; fi
}
done_tests() { echo "$1: $pass passed, $fail failed"; [ "$fail" = 0 ]; }
export LC_ALL=C

# the program of a neighbouring directory, built if it is not there
prog() {
    [ -x "$PROG_DIR/../$1/$1" ] || (cd "$PROG_DIR/../$1" && make > /dev/null 2>&1)
    echo "$PROG_DIR/../$1/$1"
}
F=${F:-$PROG_DIR/fsck_ffs}
[ -x "$F" ] || (cd "$PROG_DIR" && make > /dev/null 2>&1)
N=$(prog newfs)
D=$(prog dumpfs)
for p in "$N" "$F" "$D"; do
    [ -x "$p" ] || { echo "fsck_ffs: cannot build $p"; exit 2; }
done
T=${TMPDIR:-/tmp}/test_fsck_ffs.$$
mkdir -m 700 "$T" || exit 2
trap 'rm -rf "$T"' EXIT
IMG=$T/img

# mkimg FILE MIB: a file of MIB MiB, zeroes
mkimg() { rm -f "$1"; dd if=/dev/zero of="$1" bs=1048576 seek=$(($2 - 1)) count=1 2>/dev/null; }
# fld NAME: the value of a field of the file system in $IMG, as dumpfs shows it
fld() {
    "$D" "$IMG" | awk -F'\t' -v n="$1" '{
        for (i = 1; i <= NF; i++) {
            if ($i == n) { v = $(i + 1); sub(/ .*/, "", v); print v; exit }
            if (index($i, n " ") == 1) { v = substr($i, length(n) + 2); print v; exit }
        }
    }'
}
# le VALUE BYTES: printf escapes for a little endian number
le() {
    _n=$1 _w=$2 _s=
    while [ "$_w" -gt 0 ]; do
        _s="$_s\\$(printf '%03o' $((_n & 255)))"
        _n=$((_n >> 8)); _w=$((_w - 1))
    done
    printf '%s' "$_s"
}
# poke OFFSET ESCAPES: write bytes into $IMG
poke() { printf "$2" | dd of="$IMG" bs=1 seek="$1" conv=notrunc 2>/dev/null; }
# peek8 OFFSET: the 8 byte little endian number at OFFSET of $IMG
peek8() {
    set -- $(od -A n -v -t u1 -j "$1" -N 8 "$IMG")
    _v=0 _i=8
    while [ "$_i" -ge 1 ]; do
        eval "_b=\${$_i}"
        _v=$((_v * 256 + _b)); _i=$((_i - 1))
    done
    echo "$_v"
}
# layout: where things are in $IMG (the variables are those of the super-block)
layout() {
    fsz=$(fld fsize) cblkno=$(fld cblkno) iblkno=$(fld iblkno) dblkno=$(fld dblkno)
    fpg=$(fld fpg) csaddr=$(fld csaddr) frag=$(fld frag) ncg=$(fld ncg)
    if [ "$(fld magic)" = 19540119 ]; then isz=256 sbloc=65536; else isz=128 sbloc=8192; fi
}
# the offset of inode 2 (3 ...) and the first byte of the group's cylinder group block
ino() { echo $((iblkno * fsz + $1 * isz)); }
cgoff() { echo $(((fpg * $1 + cblkno) * fsz)); }

# make OPTION...: a fresh 32 MiB file system, and its layout
make() {
    mkimg "$IMG" 32
    "$N" -q "$@" "$IMG" > /dev/null 2>&1 && layout
}
has() { grep -F -q -- "$2" "$1"; }

# ---- usage
U="usage: fsck_ffs [-fnpy] [-b block#] [-c level] [-m mode] filesystem"
usage1() { "$@" 2>&1 | grep '^usage:' | head -n 1; }
# the output of a command without the spaces at the ends of its lines
nots() {
    "$@" > "$T/nots.out" 2>&1; _s=$?
    sed 's/ *$//' "$T/nots.out"
    return $_s
}
make
t "no arguments" "$U" 0 usage1 "$F"
t "an unknown option" "$U" 0 usage1 "$F" -x "$IMG"
t "no file system" "$U" 0 usage1 "$F" -n
t "two file systems" "$U" 0 usage1 "$F" -fn "$IMG" "$IMG"
for a in "" "-x $IMG" "-n" "-fn $IMG $IMG"; do
    ok "usage error is status 1 ($a)" sh -c '"$1" $2 > /dev/null 2>&1; [ $? = 1 ]' sh "$F" "$a"
done
ok "a bad mode for -m" sh -c '"$1" -m 9 -fn "$2" > /dev/null 2>&1; [ $? != 0 ]' sh "$F" "$IMG"
te "a conversion level that is not there" "cannot do level 2 conversion" 8 "$F" -c 2 -fn "$IMG"

# ---- what is not there, and what is not a file system
te "a file that is not there" "Can't open $T/nosuch: No such file or directory" 8 "$F" -fn "$T/nosuch"
mkimg "$IMG" 8
te "zeroes" "** $IMG (NO WRITE)
BAD SUPER BLOCK: MAGIC NUMBER WRONG
$IMG: NO DISKLABEL TO FIND THE BACKUP SUPERBLOCKS; USE -b BLOCK (newfs prints where they are)" 8 "$F" -fn "$IMG"
ok "zeroes with -p" sh -c '"$1" -p "$2" > /dev/null 2>&1; [ $? = 8 ]' sh "$F" "$IMG"
mkimg "$T/zero" 8
"$F" -fy "$IMG" > /dev/null 2>&1
ok "zeroes with -y: not touched" cmp -s "$IMG" "$T/zero"

# ---- consistent file systems
mkimg "$IMG" 64; "$N" -q "$IMG" > /dev/null 2>&1
S="1 files, 1 used, 31638 free (14 frags, 3953 blocks, 0.0% fragmentation)"
te "clean, without -f" "** $IMG (NO WRITE)
** File system is clean; not checking" 0 "$F" -n "$IMG"
te "clean, -p" "$IMG: file system is clean; not checking" 0 "$F" -p "$IMG"
te "-fp" "$IMG: $S" 0 "$F" -fp "$IMG"
te "-fn" "** $IMG (NO WRITE)
** File system is already clean
** Last Mounted on
** Phase 1 - Check Blocks and Sizes
** Phase 2 - Check Pathnames
** Phase 3 - Check Connectivity
** Phase 4 - Check Reference Counts
** Phase 5 - Check Cyl groups
$S" 0 nots "$F" -fn "$IMG"
te "-fy" "** $IMG
** File system is already clean
** Last Mounted on
** Phase 1 - Check Blocks and Sizes
** Phase 2 - Check Pathnames
** Phase 3 - Check Connectivity
** Phase 4 - Check Reference Counts
** Phase 5 - Check Cyl groups
$S" 0 nots "$F" -fy "$IMG"
ok "-y alone on a clean file system" sh -c '"$1" -y "$2" > /dev/null 2>&1' sh "$F" "$IMG"
cp "$IMG" "$T/before"
"$F" -fn "$IMG" > /dev/null 2>&1
ok "-n writes nothing" cmp -s "$IMG" "$T/before"
"$F" -fp "$IMG" > /dev/null 2>&1
ok "-p on a clean file system changes nothing" cmp -s "$IMG" "$T/before" || [ "$(fld clean)" = 1 ]
t "the file system is marked clean" "1" 0 fld clean

for g in "" "-b 4096 -f 512" "-b 8192 -f 1024" "-b 16384 -f 4096" "-b 32768 -f 4096" "-b 4096 -f 4096" \
    "-O 1" "-O 1 -b 8192 -f 1024" "-m 0" "-o space" "-i 2048" "-s 20000"; do
    make $g
    ok "clean: -fn with $g" sh -c '"$1" -fn "$2" > /dev/null 2>&1' sh "$F" "$IMG"
    ok "clean: -fp with $g" sh -c '"$1" -fp "$2" > /dev/null 2>&1' sh "$F" "$IMG"
    ok "clean: -fy with $g" sh -c '"$1" -fy "$2" > /dev/null 2>&1' sh "$F" "$IMG"
done

# ---- damage and repair
# scenario NAME MESSAGE: the damage is done (by the caller); -n says MESSAGE
# and status 1 and writes nothing, -y repairs it (status 0), and then -n and
# -p say it is clean; the file system is marked clean
repair() {
    _name=$1 _msg=$2
    cp "$IMG" "$T/damaged"
    "$F" -fn "$IMG" > "$T/o1" 2>&1; _r=$?
    if [ "$_r" = 1 ] && has "$T/o1" "$_msg" && cmp -s "$IMG" "$T/damaged"; then pass=$((pass+1))
    else fail=$((fail+1)); printf 'FAIL %s: -fn\n  status %s (want 1), %s\n' "$_name" "$_r" "$(has "$T/o1" "$_msg" && echo "found [$_msg]" || echo "[$_msg] missing")"; sed 's/^/    /' "$T/o1"; fi
    "$F" -fy "$IMG" > "$T/o2" 2>&1; _r=$?
    if [ "$_r" = 0 ] && has "$T/o2" "FILE SYSTEM WAS MODIFIED"; then pass=$((pass+1))
    else fail=$((fail+1)); printf 'FAIL %s: -fy\n  status %s (want 0)\n' "$_name" "$_r"; sed 's/^/    /' "$T/o2"; fi
    "$F" -fn "$IMG" > "$T/o3" 2>&1; _r=$?
    if [ "$_r" = 0 ] && ! has "$T/o3" "? no"; then pass=$((pass+1))
    else fail=$((fail+1)); printf 'FAIL %s: -fn after the repair\n  status %s (want 0)\n' "$_name" "$_r"; sed 's/^/    /' "$T/o3"; fi
    "$F" -n "$IMG" > "$T/o4" 2>&1; _r=$?
    if [ "$_r" = 0 ] && has "$T/o4" "File system is clean; not checking"; then pass=$((pass+1))
    else fail=$((fail+1)); printf 'FAIL %s: -n after the repair\n  status %s (want 0, clean)\n' "$_name" "$_r"; sed 's/^/    /' "$T/o4"; fi
}
# preen NAME MESSAGE: the same repaired by -p (status 0, MESSAGE with its
# fix in the output)
preen() {
    _name=$1 _msg=$2
    "$F" -fp "$IMG" > "$T/o2" 2>&1; _r=$?
    if [ "$_r" = 0 ] && has "$T/o2" "$_msg"; then pass=$((pass+1))
    else fail=$((fail+1)); printf 'FAIL %s: -fp\n  status %s (want 0)\n' "$_name" "$_r"; sed 's/^/    /' "$T/o2"; fi
    "$F" -fn "$IMG" > "$T/o3" 2>&1; _r=$?
    if [ "$_r" = 0 ] && ! has "$T/o3" "? no"; then pass=$((pass+1))
    else fail=$((fail+1)); printf 'FAIL %s: -fn after -p\n  status %s (want 0)\n' "$_name" "$_r"; sed 's/^/    /' "$T/o3"; fi
}
# an unreferenced regular file in inode 3, 100 bytes long, in the block N blocks into the data area
orphan() {
    _i=$(ino 3)
    poke "$_i" "$(le 33188 2)$(le 1 2)"
    poke $((_i + 16)) "$(le 100 8)"
    poke $((_i + 112)) "$(le $((dblkno + $1 * frag)) 8)"
}

for g in "" "-b 4096 -f 512" "-b 8192 -f 1024" "-b 32768 -f 4096" "-b 4096 -f 4096"; do
    make $g
    L="[$g]"
    [ "$ncg" -gt 1 ] || { echo "newfs $g: one cylinder group, the group tests are skipped"; continue; }
    # the last cylinder group's magic number
    make $g; poke $(($(cgoff $((ncg - 1))) + 4)) "$(le 0 4)"
    repair "a cylinder group without its magic number $L" "CG $((ncg - 1)): BAD MAGIC NUMBER"
    # the summary of the free space
    make $g; poke $((csaddr * fsz)) "$(le 4294967295 4)"
    repair "a wrong summary of the free blocks $L" "FREE BLK COUNT(S) WRONG IN SUPERBLK"
    make $g; poke $((csaddr * fsz)) "$(le 4294967295 4)"
    preen "a wrong summary of the free blocks, preen $L" "FREE BLK COUNT(S) WRONG IN SUPERBLK (SALVAGED)"
    # the link count of the root
    make $g; poke $(($(ino 2) + 2)) "$(le 9 2)"
    repair "a wrong link count of / $L" "COUNT 9 SHOULD BE 2"
    make $g; poke $(($(ino 2) + 2)) "$(le 9 2)"
    preen "a wrong link count of /, preen $L" "(ADJUSTED)"
    # the directory block of the root
    make $g; rootblk=$(( $(peek8 $(($(ino 2) + 112))) * fsz ))
    poke "$rootblk" "$(le 3 4)"
    repair "a wrong inode number of . $L" "BAD INODE NUMBER FOR '.'"
    make $g; rootblk=$(( $(peek8 $(($(ino 2) + 112))) * fsz ))
    poke $((rootblk + 12)) "$(le 5 4)"
    repair "a wrong inode number of .. $L" "BAD INODE NUMBER FOR '..'"
    make $g; rootblk=$(( $(peek8 $(($(ino 2) + 112))) * fsz ))
    poke $((rootblk + 4)) "$(le 1 2)"
    repair "a bad record length in / $L" "DIRECTORY CORRUPTED"
    make $g; rootblk=$(( $(peek8 $(($(ino 2) + 112))) * fsz ))
    poke "$rootblk" "$(le 3 4)"
    "$F" -fp "$IMG" > "$T/o2" 2>&1; _r=$?
    ok "a wrong . is more than -p repairs: status 8 $L" [ "$_r" = 8 ]
    ok "  and says so $L" has "$T/o2" "UNEXPECTED INCONSISTENCY; RUN fsck_ffs MANUALLY."
    # a file nobody refers to
    make $g; orphan 3
    repair "an unreferenced file $L" "UNREF FILE  I=3  OWNER=root MODE=100644"
    t "  it is in lost+found now (a directory more) $L" "2" 0 fld ndir
    make $g; orphan 3
    preen "an unreferenced file, preen $L" "(RECONNECTED)"
    ok "  lost+found is created by -p $L" has "$T/o2" "NO lost+found DIRECTORY (CREATED)"
    # blocks that are twice in use
    make $g
    for n in 3 4; do
        _i=$(ino $n)
        poke "$_i" "$(le 33188 2)$(le 1 2)"
        poke $((_i + 16)) "$(le 100 8)"
        poke $((_i + 112)) "$(le $((dblkno + 3 * frag)) 8)"
    done
    repair "two files with the same block $L" "DUP I=4"
    # a block that is outside the file system
    make $g
    _i=$(ino 3)
    poke "$_i" "$(le 33188 2)$(le 1 2)"
    poke $((_i + 16)) "$(le 100 8)"
    poke $((_i + 112)) "$(le 4294967295 8)"
    repair "a block number outside the file system $L" "4294967295 BAD I=3"
done

# ---- FFS1
for g in "-O 1" "-O 1 -b 8192 -f 1024"; do
    make $g
    L="[$g]"
    poke $((csaddr * fsz)) "$(le 4294967295 4)"
    repair "FFS1: a wrong summary $L" "FREE BLK COUNT(S) WRONG IN SUPERBLK"
    make $g; poke $(($(ino 2) + 2)) "$(le 9 2)"
    repair "FFS1: a wrong link count of / $L" "COUNT 9 SHOULD BE 2"
done

# ---- an unreadable super-block, and the copies of it
for g in "" "-b 4096 -f 512" "-O 1"; do
    make $g
    L="[$g]"
    "$N" -N $g "$IMG" > "$T/backups" 2>&1
    first=$(sed -n '/^super-block backups/{n;s/^ *\([0-9]*\),.*/\1/p;}' "$T/backups")
    second=$(sed -n '/^super-block backups/{n;s/^ *[0-9]*, *\([0-9]*\),.*/\1/p;}' "$T/backups")
    ok "newfs says where the backups are $L" [ -n "$first" ] && [ -n "$second" ]
    poke $((sbloc + 1372)) "$(le 0 4)"     # fs_magic
    "$F" -fn "$IMG" > "$T/o1" 2>&1; _r=$?
    ok "the super-block is gone: status 8 $L" [ "$_r" = 8 ]
    ok "  BAD SUPER BLOCK $L" has "$T/o1" "BAD SUPER BLOCK: MAGIC NUMBER WRONG"
    "$F" -b "$first" -fy "$IMG" > "$T/o2" 2>&1; _r=$?
    ok "  -b $first repairs it: status 0 $L" [ "$_r" = 0 ]
    ok "  from the copy $L" has "$T/o2" "UPDATE STANDARD SUPERBLOCK? yes"
    ok "  and the file system is whole again $L" sh -c '"$1" -fn "$2" > /dev/null 2>&1' sh "$F" "$IMG"
    poke $((sbloc + 1372)) "$(le 0 4)"
    "$F" -b "$second" -fy "$IMG" > /dev/null 2>&1
    ok "  the second copy works too $L" sh -c '"$1" -fn "$2" > /dev/null 2>&1' sh "$F" "$IMG"
done
done_tests fsck_ffs
