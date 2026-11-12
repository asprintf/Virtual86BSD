#!/bin/sh
# test_makefs.sh - tests for makefs (V86BSD).  Run: sh test_makefs.sh
# makefs makes images of trees; fsck_ffs must call each one clean and
# count the files of the tree, dumpfs shows the parameters.  (What the files
# hold is checked by mounting an image in V86BSD.)  Needs no root.
cd "$(dirname "$0")" || exit 2
PROG_DIR=${PROG_DIR:-$PWD/../../../usr.sbin/makefs}; export PROG_DIR
pass=0 fail=0
ok() {
    _n=$1; shift
    if "$@"; then pass=$((pass+1)); else fail=$((fail+1)); printf 'FAIL %s\n' "$_n"; fi
}
done_tests() { echo "makefs: $pass passed, $fail failed"; [ "$fail" = 0 ]; }
export LC_ALL=C

prog() {
    [ -x "$PROG_DIR/$1/$1" ] || (cd "$PROG_DIR/$1" && make > /dev/null 2>&1)
    echo "$PROG_DIR/$1/$1"
}
M=${M:-$PROG_DIR/makefs}
[ -x "$M" ] || (cd "$PROG_DIR" && make > /dev/null 2>&1)
F=$(cd "$PROG_DIR/../../sbin/fsck_ffs" 2>/dev/null && { [ -x fsck_ffs ] || make > /dev/null 2>&1; pwd; })/fsck_ffs
D=$(cd "$PROG_DIR/../../sbin/dumpfs" 2>/dev/null && { [ -x dumpfs ] || make > /dev/null 2>&1; pwd; })/dumpfs
for p in "$M" "$F" "$D"; do
    [ -x "$p" ] || { echo "makefs: cannot build $p"; exit 2; }
done
T=${TMPDIR:-/tmp}/test_makefs.$$
mkdir -m 700 "$T" || exit 2
trap 'rm -rf "$T"' EXIT

# fld IMG NAME: the value of a field of the file system, as dumpfs shows it
fld() {
    "$D" "$1" | awk -F'\t' -v n="$2" '{
        for (i = 1; i <= NF; i++) {
            if ($i == n) { v = $(i + 1); sub(/ .*/, "", v); print v; exit }
            if (index($i, n " ") == 1) { v = substr($i, length(n) + 2); print v; exit }
        }
    }'
}
# files IMG: the number of files fsck_ffs counts (and 0 when it finds anything wrong)
files() { "$F" -fn "$1" 2>&1 | sed -n 's/^\([0-9]*\) files, .*/\1/p'; }
clean() { "$F" -fn "$1" > /dev/null 2>&1; }
status() { "$@" > "$T/out" 2>&1; echo $?; }

# ---- usage and errors
ok "no arguments: usage, status 1" sh -c '[ "$("$1" 2>&1 >/dev/null | grep -c "^usage: makefs")" = 1 ]' sh "$M"
ok "an unknown option: status 1" [ "$(status "$M" -x "$T/i" "$T")" = 1 ]
ok "no directory: status 1" [ "$(status "$M" "$T/i")" = 1 ]
ok "a directory that is not there: status not 0" [ "$(status "$M" "$T/i" "$T/nonesuch")" != 0 ]
ok "-o nonesuch=1: status 1" [ "$(status "$M" -o nonesuch=1 "$T/i" "$T")" = 1 ]
ok "-o optimization=fast: status 1" [ "$(status "$M" -o optimization=fast "$T/i" "$T")" = 1 ]
ok "-T x: status not 0" [ "$(status "$M" -T x "$T/i" "$T")" != 0 ]
ok "-s 12q: status not 0" [ "$(status "$M" -s 12q "$T/i" "$T")" != 0 ]

# ---- a tree with everything in it
TREE=$T/tree
mkdir -p "$TREE/etc" "$TREE/bin" "$TREE/usr/lib/deep/er" "$TREE/empty" "$TREE/many"
echo hello > "$TREE/etc/motd"
: > "$TREE/etc/zero"
printf x > "$TREE/etc/one"
ln "$TREE/etc/motd" "$TREE/etc/hard"
ln -s /etc/motd "$TREE/etc/short"
LONG=/$(awk 'BEGIN { for (i = 0; i < 150; i++) printf "a" }')
ln -s "$LONG" "$TREE/etc/long"
dd if=/dev/zero bs=1000 count=20 2>/dev/null | tr '\0' 'y' > "$TREE/bin/file20k"
dd if=/dev/zero bs=1000 count=3000 2>/dev/null | tr '\0' 'z' > "$TREE/bin/file3m"
dd if=/dev/zero of="$TREE/bin/sparse" bs=1 count=1 seek=40000000 2>/dev/null
i=0; while [ $i -lt 300 ]; do echo $i > "$TREE/many/a_rather_long_file_name_to_fill_directory_chunks_$i"; i=$((i+1)); done
# the files fsck_ffs should count: the names found, the root of the tree among them, but motd and hard are one file
want=$(( $(find "$TREE" | wc -l) - 1 ))
mkfifo "$TREE/etc/fifo" 2>/dev/null && want=$((want + 1))

"$M" -U -s 128m "$T/a.img" "$TREE" > "$T/o" 2>&1; st=$?
ok "a tree with files, links, long names and a 40 MB sparse file: status 0" [ "$st" = 0 ]
ok "  fsck_ffs: clean" clean "$T/a.img"
ok "  fsck_ffs counts the files of the tree" [ "$(files "$T/a.img")" = "$want" ]
ok "  FFS2, the super block at 64 KiB" sh -c '[ "$("$1" "$2" | awk -F"\t" "NR == 1 { print \$2 }")" = "19540119 (FFS2)" ] && [ "$("$1" "$2" | awk -F"\t" "/superblock location/ { print \$2 }")" = 65536 ]' sh "$D" "$T/a.img"
ok "  block 16384, fragment 2048 (as newfs)" [ "$(fld "$T/a.img" bsize)/$(fld "$T/a.img" fsize)" = "16384/2048" ]
ok "  no cluster maps" [ "$(fld "$T/a.img" contigsumsize)" = 0 ]
ok "  the image is as big as -s says" [ "$(wc -c < "$T/a.img" | tr -d ' ')" = 134217728 ]

# the same tree, made in the other order, with the time fixed: the same image
"$M" -U -T 1700000000 -s 128m "$T/b1.img" "$TREE" > /dev/null 2>&1
mkdir "$T/tree2"; cp -R "$TREE/." "$T/tree2/" 2>/dev/null
rm -f "$T/tree2/etc/hard"; ln "$T/tree2/etc/motd" "$T/tree2/etc/hard"   # (cp makes the link a file of its own)
"$M" -U -T 1700000000 -s 128m "$T/b2.img" "$T/tree2" > /dev/null 2>&1
ok "the same tree and time twice: the same image" cmp -s "$T/b1.img" "$T/b2.img"
ok "  and it is clean" clean "$T/b1.img"

# ---- sizes
"$M" -U "$T/c.img" "$TREE" > /dev/null 2>&1; st=$?
ok "no -s: status 0" [ "$st" = 0 ]
ok "  the size is a multiple of the block size" [ $(( $(wc -c < "$T/c.img" | tr -d ' ') % 16384 )) = 0 ]
ok "  and fsck_ffs counts the same files" [ "$(files "$T/c.img")" = "$want" ]
"$M" -U -b 10m "$T/c2.img" "$TREE" > /dev/null 2>&1
ok "-b 10m: 10 MB more room" [ $(( $(wc -c < "$T/c2.img" | tr -d ' ') - $(wc -c < "$T/c.img" | tr -d ' ') )) -ge 10485760 ]
"$M" -U -M 200m "$T/c3.img" "$TREE" > /dev/null 2>&1
ok "-M 200m: not smaller than that" [ "$(wc -c < "$T/c3.img" | tr -d ' ')" -ge 209715200 ]
ok "-m 1m: the tree does not fit: status not 0" [ "$(status "$M" -U -m 1m "$T/c4.img" "$TREE")" != 0 ]
ok "-s 2m: the tree does not fit: status not 0" [ "$(status "$M" -U -s 2m "$T/c5.img" "$TREE")" != 0 ]
"$M" -U -s 128m "$T/f0.img" "$TREE" > /dev/null 2>&1
"$M" -U -s 128m -f 20000 "$T/f1.img" "$TREE" > /dev/null 2>&1
ok "-f 20000: room for 20000 more files" [ $(( $(fld "$T/f1.img" nifree) - $(fld "$T/f0.img" nifree) )) -ge 15000 ]

# ---- options
for o in "bsize=8192,fsize=1024" "bsize=32768,fsize=4096" "bsize=4096,fsize=512" "minfree=10" "optimization=space" "density=4096" "maxbpg=500" "label=ROOT"; do
    "$M" -U -s 128m -o "$o" "$T/o.img" "$TREE" > /dev/null 2>&1; st=$?
    ok "-o $o: status 0, clean, the same files" sh -c '[ "$1" = 0 ] && "$2" -fn "$3" > /dev/null 2>&1' sh "$st" "$F" "$T/o.img"
    ok "  the files are all there [$o]" [ "$(files "$T/o.img")" = "$want" ]
done
"$M" -U -s 128m -o bsize=8192,fsize=1024 "$T/o.img" "$TREE" > /dev/null 2>&1
ok "bsize=8192,fsize=1024 are in the super block" [ "$(fld "$T/o.img" bsize)/$(fld "$T/o.img" fsize)" = "8192/1024" ]
"$M" -U -s 128m -o minfree=10 "$T/o.img" "$TREE" > /dev/null 2>&1
ok "minfree=10 is in it" [ "$(fld "$T/o.img" minfree)" = "10%" ]
"$M" -U -s 128m -o label=ROOT "$T/o.img" "$TREE" > /dev/null 2>&1
ok "label=ROOT is in it" sh -c '"$1" "$2" | grep -q "volname.ROOT"' sh "$D" "$T/o.img"

# ---- small trees
mkdir "$T/t1"
"$M" -U "$T/t1.img" "$T/t1" > /dev/null 2>&1; st=$?
ok "an empty tree: status 0, clean, only the root" sh -c '[ "$1" = 0 ] && "$2" -fn "$3" > /dev/null 2>&1' sh "$st" "$F" "$T/t1.img"
ok "  one file (the root)" [ "$(files "$T/t1.img")" = 1 ]
mkdir "$T/t2"; echo x > "$T/t2/f"
"$M" -U -s 1m "$T/t2.img" "$T/t2" > /dev/null 2>&1
ok "one file in 1 MB: clean" clean "$T/t2.img"
ok "  two files" [ "$(files "$T/t2.img")" = 2 ]
# a directory that needs several blocks, and a file name of 255 bytes
mkdir "$T/t3"
N255=$(awk 'BEGIN { for (i = 0; i < 255; i++) printf "n" }')
echo x > "$T/t3/$N255"
i=0; while [ $i -lt 2000 ]; do : > "$T/t3/f$i"; i=$((i+1)); done
"$M" -U "$T/t3.img" "$T/t3" > /dev/null 2>&1; st=$?
ok "a directory of 2001 names, one of 255 bytes: status 0" [ "$st" = 0 ]
ok "  clean, 2002 files" sh -c '"$1" -fn "$2" > /dev/null 2>&1 && [ "$3" = 2002 ]' sh "$F" "$T/t3.img" "$(files "$T/t3.img")"
done_tests
