#!/bin/sh
# test_newfs.sh - tests for newfs (V86BSD).  Run: sh test_newfs.sh
# The file systems are made in image files; fsck_ffs and dumpfs (built next
# to newfs) say what came out.  Needs no root.
cd "$(dirname "$0")" || exit 2
PROG_DIR=${PROG_DIR:-$PWD/../../../sbin/newfs}; export PROG_DIR
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
N=${N:-$PROG_DIR/newfs}
[ -x "$N" ] || (cd "$PROG_DIR" && make > /dev/null 2>&1)
F=$(prog fsck_ffs)
D=$(prog dumpfs)
for p in "$N" "$F" "$D"; do
    [ -x "$p" ] || { echo "newfs: cannot build $p"; exit 2; }
done
T=${TMPDIR:-/tmp}/test_newfs.$$
mkdir -m 700 "$T" || exit 2
trap 'rm -rf "$T"' EXIT
IMG=$T/img

# mkimg FILE MIB: a file of MIB MiB, zeroes
mkimg() { rm -f "$1"; dd if=/dev/zero of="$1" bs=1048576 seek=$(($2 - 1)) count=1 2>/dev/null; }
# fld NAME: the value of a field of the file system in $IMG, as dumpfs
# shows it ("bsize 16384 shift 14 ..." and "maxbpg 2048 ..." alike)
fld() {
    "$D" "$IMG" | awk -F'\t' -v n="$1" '{
        for (i = 1; i <= NF; i++) {
            if ($i == n) { v = $(i + 1); sub(/ .*/, "", v); print v; exit }
            if (index($i, n " ") == 1) { v = substr($i, length(n) + 2); print v; exit }
        }
    }'
}
# clean: fsck_ffs finds $IMG consistent
clean() { "$F" -fn "$IMG" > "$T/fsck.out" 2>&1; }
# made MIB OPTION...: newfs of a fresh image with those options is a consistent file system
made() {
    _m=$1; shift
    mkimg "$IMG" "$_m"
    "$N" -q "$@" "$IMG" > "$T/newfs.out" 2>&1 && clean
}
fails() {   # fails NAME MESSAGE COMMAND...
    _n=$1 _e=$2; shift 2
    _o=$("$@" 2>&1 > /dev/null); _r=$?
    _o=$(printf '%s\n' "$_o" | head -n 1)
    if [ "$_r" != 0 ] && [ "$_o" = "$_e" ]; then pass=$((pass+1))
    else fail=$((fail+1)); printf 'FAIL %s\n  got:  [%s] status %s\n  want: [%s] and a status not 0\n' "$_n" "$_o" "$_r" "$_e"; fi
}

# ---- usage
U1="usage: newfs [-Nq] [-b block-size] [-c fragments-per-cylinder-group] [-e maxbpg]"
usage1() { "$@" 2>&1 | grep '^usage:' | head -n 1; }
mkimg "$IMG" 8
t "no arguments" "$U1" 0 usage1 "$N"
"$N" > /dev/null 2>&1; ok "no arguments: status 1" [ $? = 1 ]
t "an unknown option" "$U1" 0 usage1 "$N" -x "$IMG"
"$N" -x "$IMG" > /dev/null 2>&1; ok "an unknown option: status 1" [ $? = 1 ]
t "two arguments" "$U1" 0 usage1 "$N" "$IMG" "$IMG"
"$N" "$IMG" "$IMG" > /dev/null 2>&1; ok "two arguments: status 1" [ $? = 1 ]
ok "an option without its value" sh -c '"$1" -b "$2" > /dev/null 2>&1; [ $? = 1 ]' sh "$N" "$IMG"
mkimg "$T/zero" 8
ok "nothing was written by the usage errors" cmp -s "$IMG" "$T/zero"

# ---- the special file
fails "a file that is not there" "newfs: $T/nosuch: No such file or directory" "$N" "$T/nosuch"
fails "a directory" "newfs: $T: Is a directory" "$N" "$T"
: > "$T/empty"
fails "an empty file" "newfs: $T/empty: no space for a file system" "$N" "$T/empty"
fails "/dev/null" "newfs: /dev/null: no space for a file system" "$N" /dev/null
ok "no file is made for a missing name" [ ! -e "$T/nosuch" ]

# ---- values that are refused
mkimg "$IMG" 64
fails "-b not a power of two" "newfs: block size is too small: 3000" "$N" -b 3000 "$IMG"
fails "-b too small" "newfs: block size is too small: 1024" "$N" -b 1024 "$IMG"
fails "-f not a power of two" "newfs: fragment size must be a power of 2, not 3000" "$N" -f 3000 "$IMG"
fails "-f too small for -b" "newfs: fragment size 512 is too small, minimum with block size 8192 is 1024" "$N" -b 8192 -f 512 "$IMG"
fails "-m over 99" "newfs: free space % is too large: 100" "$N" -m 100 "$IMG"
fails "-m negative" "newfs: free space % is too small: -1" "$N" -m -1 "$IMG"
fails "-O 3" "newfs: 3: invalid ffs version" "$N" -O 3 "$IMG"
fails "-o neither space nor time" "newfs: fast: unknown optimization preference: use \`space' or \`time'." "$N" -o fast "$IMG"
fails "-S 100" "newfs: sector size invalid: 100" "$N" -S 100 "$IMG"
fails "-i 0" "newfs: bytes per inode is too small: 0" "$N" -i 0 "$IMG"
fails "-g 0" "newfs: average file size is too small: 0" "$N" -g 0 "$IMG"
fails "-h 0" "newfs: average files per dir is too small: 0" "$N" -h 0 "$IMG"
fails "-s tiny" "newfs: file system size 25 < minimum size of 64 fragments" "$N" -s 100 "$IMG"
fails "-s over the file" "newfs: $IMG: maximum file system size is 131072 sectors" "$N" -s 99999999 "$IMG"
mkimg "$T/zero" 64
ok "none of them wrote anything" cmp -s "$IMG" "$T/zero"

# ---- -N and -q
mkimg "$IMG" 64; mkimg "$T/zero" 64
"$N" -N "$IMG" > "$T/n.out" 2>&1; ok "-N: status 0" [ $? = 0 ]
ok "-N writes nothing" cmp -s "$IMG" "$T/zero"
ok "-N tells what newfs would make" sh -c 'grep -q "^4 cylinder groups of 16.00MB, 1024 blocks, 2048 inodes each\$" "$1" && grep -q "^super-block backups (for fsck -b #) at:\$" "$1"' sh "$T/n.out"
"$N" "$IMG" > "$T/n1.out" 2>&1
ok "the same text without -N" cmp -s "$T/n.out" "$T/n1.out"
ok "the file system is made" clean
t "the first line names the file, its size and sectors" "$IMG: 64.0MB in 131072 sectors of 512 bytes" 0 sh -c 'sed -n 1p "$1"' sh "$T/n1.out"
t "the backup super-blocks, 4 groups" " 160, 32928, 65696, 98464," 0 sh -c 'sed -n 4p "$1"' sh "$T/n1.out"
mkimg "$IMG" 64
"$N" -q "$IMG" > "$T/q.out" 2>&1; ok "-q: status 0" [ $? = 0 ]
ok "-q leaves out the backup list" sh -c '! grep -q "super-block backups" "$1"' sh "$T/q.out"
t "-q still names the file system" "$IMG: 64.0MB in 131072 sectors of 512 bytes" 0 sh -c 'sed -n 1p "$1"' sh "$T/q.out"
ok "-q made a file system" clean

# ---- every combination makes a consistent file system
for g in "64|" "32|-b 4096 -f 512" "32|-b 8192 -f 1024" "32|-b 16384 -f 4096" "64|-b 32768 -f 4096" \
    "64|-b 65536 -f 8192" "32|-b 4096 -f 4096" "32|-b 8192 -f 8192" "32|-b 16384 -f 16384" \
    "32|-m 0" "32|-m 10" "32|-m 50" "32|-m 99" "32|-o space" "32|-o time" \
    "32|-O 1" "32|-O 2" "32|-O 1 -b 8192 -f 1024" "32|-O 1 -b 4096 -f 512" "32|-O 1 -m 1 -o space" \
    "32|-s 20000" "32|-s 65536" "32|-g 65536 -h 16" "32|-g 4096 -h 256" "32|-g 1000000 -h 1" \
    "32|-i 1024 -b 4096 -f 4096" "32|-i 4096" "32|-i 65536" "32|-i 1000000" "32|-e 100" "32|-e 20000" \
    "32|-c 4096" "32|-c 2048" "32|-S 512" "32|-S 4096" \
    "32|-b 8192 -f 2048 -m 3 -o space -g 8192 -h 32 -i 2048 -e 1000" \
    "32|-O 1 -b 32768 -f 4096 -m 7 -i 8192" \
    "1|" "2|" "3|" "5|" "10|" "100|" "200|"; do
    mib=${g%%|*} o=${g#*|}
    ok "newfs $o on $mib MiB" made "$mib" $o
done
ok "newfs a second time on the same image" sh -c 'cp "$1" "$2" && "$3" -q "$2" >/dev/null 2>&1 && "$4" -fn "$2" >/dev/null 2>&1' sh "$IMG" "$T/two" "$N" "$F"
mkimg "$IMG" 32
"$N" -q -b 4096 -f 512 "$IMG" > /dev/null 2>&1
"$N" -q -b 16384 -f 2048 "$IMG" > /dev/null 2>&1
ok "a new geometry over an old one" clean
t "the new geometry is the one found" "16384" 0 fld bsize

# ---- what ends up in the super-block
mkimg "$IMG" 64; "$N" -q "$IMG" > /dev/null 2>&1
t "the default block size" "16384" 0 fld bsize
t "the default fragment size" "2048" 0 fld fsize
t "8 fragments to a block" "8" 0 fld frag
t "the default free space" "5%" 0 fld minfree
t "the default optimisation" "time" 0 fld optim
t "FFS2 by default" "19540119" 0 fld magic
t "4 cylinder groups" "4" 0 fld ncg
t "the size in fragments" "32768" 0 fld size
t "one directory (/)" "1" 0 fld ndir
t "the average file size" "16384" 0 fld avgfilesize
t "the average files per directory" "64" 0 fld avgfpdir
t "maxbpg" "2048" 0 fld maxbpg
t "clean flag" "1" 0 fld clean
t "not read-only" "0" 0 fld ronly
# one at a time
sbfld() {   # sbfld NAME VALUE OPTION...: with the options the field has the value
    _f=$1 _v=$2; shift 2
    mkimg "$IMG" 64; "$N" -q "$@" "$IMG" > /dev/null 2>&1
    _g=$(fld "$_f")
    if [ "$_g" = "$_v" ]; then pass=$((pass+1))
    else fail=$((fail+1)); printf 'FAIL %s with %s\n  got:  [%s]\n  want: [%s]\n' "$_f" "$*" "$_g" "$_v"; fi
}
sbfld bsize 4096 -b 4096 -f 512
sbfld fsize 512 -b 4096 -f 512
sbfld bsize 65536 -b 65536 -f 8192
sbfld fsize 8192 -b 65536 -f 8192
sbfld frag 8 -b 65536 -f 8192
sbfld frag 1 -b 4096 -f 4096
sbfld frag 2 -b 8192 -f 4096
sbfld minfree 0% -m 0
sbfld minfree 10% -m 10
sbfld minfree 99% -m 99
sbfld optim space -o space
sbfld optim time -o time
sbfld optim space -m 2 -o space
sbfld magic 11954 -O 1
sbfld magic 19540119 -O 2
sbfld maxbpg 100 -e 100
sbfld maxbpg 20000 -e 20000
sbfld avgfilesize 65536 -g 65536
sbfld avgfpdir 16 -h 16
sbfld avgfilesize 4096 -g 4096 -h 256
sbfld avgfpdir 256 -g 4096 -h 256
sbfld size 5000 -b 16384 -f 2048 -s 20000
sbfld size 16384 -s 65536
sbfld size 32768 -s 131072
sbfld ipg 4096 -i 4096
sbfld ipg 1024 -i 16384
sbfld ipg 256 -i 65536
# the groups cover the file system, and not one more
groups() {   # groups OPTION...
    mkimg "$IMG" 64; "$N" -q "$@" "$IMG" > /dev/null 2>&1
    _c=$(fld ncg) _p=$(fld fpg) _s=$(fld size)
    [ $((_c * _p)) -ge "$_s" ] && [ $(((_c - 1) * _p)) -lt "$_s" ]
}
for o in "" "-b 4096 -f 512" "-s 4000" "-s 40000" "-b 65536 -f 8192" "-O 1" "-O 1 -s 50000"; do
    ok "the groups cover the file system with $o" groups $o
done

# ---- the file is only written inside the file system
# a marker beyond the end of a smaller file system stays
mkimg "$IMG" 32
printf 'MARKER-AFTER-THE-END' | dd of="$IMG" bs=1 seek=$((20 * 1048576)) conv=notrunc 2>/dev/null
"$N" -q -s 32768 "$IMG" > /dev/null 2>&1       # 16 MiB
t "the marker beyond a smaller file system" "MARKER-AFTER-THE-END" 0 sh -c 'dd if="$1" bs=1 skip=$((20 * 1048576)) count=20 2>/dev/null' sh "$IMG"
ok "the smaller file system is fine" clean
ok "the file keeps its size" sh -c '[ "$(wc -c < "$1" | tr -d " ")" = 33554432 ]' sh "$IMG"
# the size of the file is the size of the file system
mkimg "$IMG" 7
t "a 7 MiB file" "14336" 0 sh -c '"$1" -N "$2" | sed -n "1s/.* in \([0-9]*\) sectors.*/\1/p"' sh "$N" "$IMG"
t "-s with MiB of sectors" "7.0MB in 14336 sectors of 512 bytes" 0 sh -c '"$1" -N "$2" | sed -n "1s/^[^:]*: //p"' sh "$N" "$IMG"
done_tests newfs
