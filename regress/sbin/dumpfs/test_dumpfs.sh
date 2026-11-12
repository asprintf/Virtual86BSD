#!/bin/sh
# test_dumpfs.sh - tests for dumpfs (V86BSD).  Run: sh test_dumpfs.sh
# The file systems are made by newfs in image files.  Needs no root.
cd "$(dirname "$0")" || exit 2
PROG_DIR=${PROG_DIR:-$PWD/../../../sbin/dumpfs}; export PROG_DIR
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
D=${D:-$PROG_DIR/dumpfs}
[ -x "$D" ] || (cd "$PROG_DIR" && make > /dev/null 2>&1)
N=$(prog newfs)
F=$(prog fsck_ffs)
for p in "$N" "$F" "$D"; do
    [ -x "$p" ] || { echo "dumpfs: cannot build $p"; exit 2; }
done
T=${TMPDIR:-/tmp}/test_dumpfs.$$
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

# norm: what dumpfs says without the times and the ids, and without the spaces at the ends of lines
norm() { sed 's/	time	[A-Z][a-z][a-z] .*/	time	TIME/; s/\[ [0-9a-f]* [0-9a-f]* \]/[ID]/; s/[ 	]*$//'; }
# nots: the output of a command without the spaces at the ends of its lines
nots() {
    "$@" > "$T/nots.out" 2>&1; _s=$?
    sed 's/ *$//' "$T/nots.out"
    return $_s
}
# same NAME EXPECTED-FILE: dumpfs's output (normalised) is the file's
same() {
    "$D" "$IMG" 2>&1 | norm > "$T/got"
    if cmp -s "$T/got" "$2"; then pass=$((pass+1))
    else fail=$((fail+1)); printf 'FAIL %s\n' "$1"; diff "$2" "$T/got" | sed 's/^/    /'; fi
}

# ---- usage and errors
U="usage: dumpfs [-m] filesys | device"
usage1() { "$@" 2>&1 | grep '^usage:' | head -n 1; }
mkimg "$IMG" 8
t "no arguments" "$U" 0 usage1 "$D"
t "an unknown option" "$U" 0 usage1 "$D" -x "$IMG"
for a in "" "-x $IMG"; do
    ok "usage error is status 1 ($a)" sh -c '"$1" $2 > /dev/null 2>&1; [ $? = 1 ]' sh "$D" "$a"
done
te "a file that is not there" "dumpfs: $T/nosuch: No such file or directory" 1 "$D" "$T/nosuch"
te "zeroes" "dumpfs: cannot find filesystem superblock" 1 "$D" "$IMG"
te "zeroes with -m" "dumpfs: cannot find filesystem superblock" 1 "$D" -m "$IMG"
: > "$T/empty"
te "an empty file" "dumpfs: cannot find filesystem superblock" 1 "$D" "$T/empty"
te "a directory" "dumpfs: cannot find filesystem superblock" 1 "$D" "$T"

# ---- the output for a default file system, FFS2 and FFS1
cat > "$T/golden2" <<'EOF'
magic	19540119 (FFS2)	time	TIME
superblock location	65536	id	[ID]
ncg	4	size	32768	blocks	31639
bsize	16384	shift	14	mask	0xffffc000
fsize	2048	shift	11	mask	0xfffff800
frag	8	shift	3	fsbtodb	2
minfree	5%	optim	time	symlinklen 120
maxbsize 0	maxbpg	2048	maxcontig 1	contigsumsize 0
nbfree	3953	ndir	1	nifree	8189	nffree	14
bpg	1024	fpg	8192	ipg	2048
nindir	2048	inopb	64	maxfilesize	140806241583103
sbsize	2048	cgsize	2048	csaddr	312	cssize	2048
sblkno	40	cblkno	48	iblkno	56	dblkno	312
cgrotor	0	fmod	0	ronly	0	clean	1
avgfpdir 64	avgfilesize 16384
flags	none
fsmnt
volname		swuid	0

cs[].cs_(nbfree,ndir,nifree,nffree):

	(983,1,2045,14)
	(990,0,2048,0)
	(990,0,2048,0)
	(990,0,2048,0)

cg 0:
magic	90255	tell	18000	time	TIME
cgx	0	ndblk	8192	niblk	2048	initiblk 128
nbfree	983	ndir	1	nifree	2045	nffree	14
rotor	0	irotor	0	frotor	0
frsum	0	0	0	0	0	0	2
sum of frsum: 14
inodes used:	0-2
blks free:	313-319, 321-8191

cg 1:
magic	90255	tell	1018000	time	TIME
cgx	1	ndblk	8192	niblk	2048	initiblk 128
nbfree	990	ndir	0	nifree	2048	nffree	0
rotor	0	irotor	0	frotor	0
frsum	0	0	0	0	0	0	0
sum of frsum: 0
inodes used:
blks free:	0-39, 312-8191

cg 2:
magic	90255	tell	2018000	time	TIME
cgx	2	ndblk	8192	niblk	2048	initiblk 128
nbfree	990	ndir	0	nifree	2048	nffree	0
rotor	0	irotor	0	frotor	0
frsum	0	0	0	0	0	0	0
sum of frsum: 0
inodes used:
blks free:	0-39, 312-8191

cg 3:
magic	90255	tell	3018000	time	TIME
cgx	3	ndblk	8192	niblk	2048	initiblk 128
nbfree	990	ndir	0	nifree	2048	nffree	0
rotor	0	irotor	0	frotor	0
frsum	0	0	0	0	0	0	0
sum of frsum: 0
inodes used:
blks free:	0-39, 312-8191
EOF
cat > "$T/golden1" <<'EOF'
magic	11954 (FFS1)	time	TIME
id	[ID]
cylgrp	dynamic	inodes	4.4BSD	fslevel 3
ncg	4	ncyl	4	size	32768	blocks	32183
bsize	16384	shift	14	mask	0xffffc000
fsize	2048	shift	11	mask	0xfffff800
frag	8	shift	3	fsbtodb	2
minfree	5%	optim	time	symlinklen 60
maxbpg	4096	maxcontig 1	contigsumsize 0
nbfree	4021	ndir	1	nifree	8189	nffree	14
cpg	1	bpg	1024	fpg	8192	ipg	2048
nindir	4096	inopb	128	nspf	4	maxfilesize	1126174852055039
sbsize	2048	cgsize	2048	cgoffset 0	cgmask	0xffffffff
csaddr	152	cssize	2048
rotdelay 0ms	rps	60	interleave 1
nsect	32768	npsect	32768	spc	32768
sblkno	8	cblkno	16	iblkno	24	dblkno	152
cgrotor	0	fmod	0	ronly	0	clean	1
avgfpdir 64	avgfilesize 16384
flags	none
fsmnt
volname		swuid	0

cs[].cs_(nbfree,ndir,nifree,nffree):

	(1003,1,2045,14)
	(1006,0,2048,0)
	(1006,0,2048,0)
	(1006,0,2048,0)

cg 0:
magic	90255	tell	8000	time	TIME
cgx	0	ncyl	1	niblk	2048	ndblk	8192
nbfree	1003	ndir	1	nifree	2045	nffree	14
rotor	0	irotor	0	frotor	0
frsum	0	0	0	0	0	0	2
sum of frsum: 14
inodes used:	0-2
blks free:	153-159, 161-8191

cg 1:
magic	90255	tell	1008000	time	TIME
cgx	1	ncyl	1	niblk	2048	ndblk	8192
nbfree	1006	ndir	0	nifree	2048	nffree	0
rotor	0	irotor	0	frotor	0
frsum	0	0	0	0	0	0	0
sum of frsum: 0
inodes used:
blks free:	0-7, 152-8191

cg 2:
magic	90255	tell	2008000	time	TIME
cgx	2	ncyl	1	niblk	2048	ndblk	8192
nbfree	1006	ndir	0	nifree	2048	nffree	0
rotor	0	irotor	0	frotor	0
frsum	0	0	0	0	0	0	0
sum of frsum: 0
inodes used:
blks free:	0-7, 152-8191

cg 3:
magic	90255	tell	3008000	time	TIME
cgx	3	ncyl	0	niblk	2048	ndblk	8192
nbfree	1006	ndir	0	nifree	2048	nffree	0
rotor	0	irotor	0	frotor	0
frsum	0	0	0	0	0	0	0
sum of frsum: 0
inodes used:
blks free:	0-7, 152-8191
EOF
mkimg "$IMG" 64; "$N" -q "$IMG" > /dev/null 2>&1
same "FFS2: everything dumpfs shows" "$T/golden2"
mkimg "$IMG" 64; "$N" -q -O 1 "$IMG" > /dev/null 2>&1
same "FFS1: everything dumpfs shows" "$T/golden1"

# ---- dumpfs only reads
mkimg "$IMG" 32; "$N" -q "$IMG" > /dev/null 2>&1
cp "$IMG" "$T/before"
"$D" "$IMG" > /dev/null 2>&1; "$D" -m "$IMG" > /dev/null 2>&1
ok "the image is as it was" cmp -s "$IMG" "$T/before"
chmod 444 "$IMG"
ok "an image that can only be read" sh -c '"$1" "$2" > /dev/null 2>&1' sh "$D" "$IMG"
chmod 644 "$IMG"
t "the time is a date" "1" 0 sh -c '"$1" "$2" | sed -n "1s/^magic.*time	//p" | grep -c "^[A-Z][a-z][a-z] [A-Z][a-z][a-z] "' sh "$D" "$IMG"
t "the magic line, FFS2" "magic	19540119 (FFS2)" 0 sh -c '"$1" "$2" | sed -n 1p | cut -f 1,2' sh "$D" "$IMG"
t "the id is two words in brackets" "1" 0 sh -c '"$1" "$2" | grep -c "id	\[ [0-9a-f]* [0-9a-f]* \]"' sh "$D" "$IMG"
mkimg "$IMG" 32; "$N" -q -O 1 "$IMG" > /dev/null 2>&1
t "the magic line, FFS1" "magic	11954 (FFS1)" 0 sh -c '"$1" "$2" | sed -n 1p | cut -f 1,2' sh "$D" "$IMG"
t "FFS1 has no maxbsize" "0" 1 sh -c '"$1" "$2" | grep -c "^maxbsize"' sh "$D" "$IMG"

# ---- two file systems
mkimg "$IMG" 16; mkimg "$T/img2" 16
"$N" -q "$IMG" > /dev/null 2>&1; "$N" -q -b 4096 -f 512 "$T/img2" > /dev/null 2>&1
"$D" "$IMG" "$T/img2" > "$T/both" 2>&1; ok "two file systems: status" [ $? = 0 ]
t "two file systems: both are dumped" "2" 0 sh -c 'grep -c "^magic	19540119" "$1"' sh "$T/both"
t "  with their own block sizes" "bsize	16384	shift	14	mask	0xffffc000 bsize	4096	shift	12	mask	0xfffff000" 0 sh -c 'grep "^bsize" "$1" | tr "\n" " " | sed "s/ \$//"' sh "$T/both"

# ---- the free space and the inodes in use, after the file system has changed
le() {
    _n=$1 _w=$2 _s=
    while [ "$_w" -gt 0 ]; do _s="$_s\\$(printf '%03o' $((_n & 255)))"; _n=$((_n >> 8)); _w=$((_w - 1)); done
    printf '%s' "$_s"
}
poke() { printf "$2" | dd of="$IMG" bs=1 seek="$1" conv=notrunc 2>/dev/null; }
inodes_used() { "$D" "$IMG" | sed -n '/^cg 0:/,/^cg 1:/s/^inodes used:	//p'; }
mkimg "$IMG" 32; "$N" -q "$IMG" > /dev/null 2>&1
t "the root is the only inode in use" "0-2" 0 inodes_used
fsz=$(fld fsize) iblkno=$(fld iblkno) dblkno=$(fld dblkno) frag=$(fld frag)
i3=$((iblkno * fsz + 3 * 256))
poke "$i3" "$(le 33188 2)$(le 1 2)"; poke $((i3 + 16)) "$(le 100 8)"; poke $((i3 + 112)) "$(le $((dblkno + 3 * frag)) 8)"
"$F" -fy "$IMG" > /dev/null 2>&1
t "fsck_ffs reconnected inode 3 and made lost+found: inodes 3 and 4 are used" "0-4" 0 inodes_used
t "  two directories" "2" 0 fld ndir
t "  the free inodes are those of the groups less the 5 in use" "$(($(fld ncg) * $(fld ipg) - 5))" 0 fld nifree

# ---- -m: the newfs command that makes the same file system
mkimg "$IMG" 64; "$N" -q "$IMG" > /dev/null 2>&1
te "-m for a default file system" "# newfs command for $IMG
newfs -O 2 -b 16384 -e 2048 -f 2048 -g 16384 -h 64 -m 5 -o time -s 131072 $IMG" 0 nots "$D" -m "$IMG"
mkimg "$IMG" 64; "$N" -q -O 1 "$IMG" > /dev/null 2>&1
te "-m for FFS1" "# newfs command for $IMG
newfs -O 1 -b 16384 -e 4096 -f 2048 -g 16384 -h 64 -m 5 -o time -s 131072 $IMG" 0 nots "$D" -m "$IMG"
# (dumpfs -m does not give -i, the bytes per inode, so none is used here)
roundtrip() {   # roundtrip MIB OPTION...: newfs with what dumpfs -m says makes the same file system
    _m=$1; shift
    mkimg "$IMG" "$_m"; "$N" -q "$@" "$IMG" > /dev/null 2>&1 || return 1
    "$D" "$IMG" | norm > "$T/a"
    "$D" -m "$IMG" | sed -n 2p | sed 's/^newfs //; s/ [^ ]* *$//' > "$T/opts"
    mkimg "$IMG" "$_m"
    # shellcheck disable=SC2046
    "$N" -q $(cat "$T/opts") "$IMG" > /dev/null 2>&1 || return 1
    "$D" "$IMG" | norm > "$T/b"
    cmp -s "$T/a" "$T/b"
}
for g in "64|" "32|-b 4096 -f 512" "32|-b 8192 -f 1024" "32|-b 65536 -f 8192" "32|-b 4096 -f 4096" \
    "32|-m 10 -o space" "32|-O 1" "32|-O 1 -b 8192 -f 1024 -m 2" "32|-g 65536 -h 16" \
    "32|-b 8192 -f 2048" "32|-e 100" "32|-s 20000" "32|-b 8192 -f 2048 -m 3 -o space -g 8192 -h 32 -e 1000"; do
    mib=${g%%|*} o=${g#*|}
    ok "dumpfs -m reproduces newfs $o ($mib MiB)" roundtrip "$mib" $o
done

# ---- every field, for several shapes
chk() {   # chk MIB "OPTIONS" FIELD VALUE...
    _m=$1 _o=$2; shift 2
    mkimg "$IMG" "$_m"; "$N" -q $_o "$IMG" > /dev/null 2>&1
    while [ $# -ge 2 ]; do
        _g=$(fld "$1")
        if [ "$_g" = "$2" ]; then pass=$((pass+1))
        else fail=$((fail+1)); printf 'FAIL %s with [%s]\n  got:  [%s]\n  want: [%s]\n' "$1" "$_o" "$_g" "$2"; fi
        shift 2
    done
}
chk 32 "-b 4096 -f 512" bsize 4096 fsize 512 frag 8 fsbtodb 0 shift 12 inopb 16 nindir 512
chk 32 "-b 8192 -f 1024" bsize 8192 fsize 1024 frag 8 fsbtodb 1 shift 13 inopb 32 nindir 1024
chk 32 "" bsize 16384 fsize 2048 frag 8 fsbtodb 2 shift 14 inopb 64 nindir 2048 sbsize 2048
chk 64 "-b 65536 -f 8192" bsize 65536 fsize 8192 frag 8 fsbtodb 4 shift 16 inopb 256 nindir 8192
chk 32 "-b 4096 -f 4096" bsize 4096 fsize 4096 frag 1 fsbtodb 3
chk 32 "-O 1" magic 11954 inopb 128 nindir 4096 symlinklen 60 maxfilesize 1126174852055039
chk 32 "" magic 19540119 symlinklen 120 maxfilesize 140806241583103
chk 64 "" sblkno 40 cblkno 48 iblkno 56 dblkno 312 csaddr 312 cssize 2048
chk 32 "-m 7 -o space" minfree 7% optim space
chk 32 "-s 20000" size 5000
chk 32 "-s 65536" size 16384
chk 32 "-g 4096 -h 128" avgfilesize 4096 avgfpdir 128
chk 32 "-e 300" maxbpg 300
done_tests dumpfs
