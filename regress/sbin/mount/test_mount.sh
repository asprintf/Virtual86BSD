#!/bin/sh
# test_mount.sh - tests for mount (V86BSD).  Run: sh test_mount.sh
cd "$(dirname "$0")" || exit 2
PROG_DIR=${PROG_DIR:-$PWD/../../../sbin/mount}; export PROG_DIR
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
U=${U:-$PROG_DIR/mount}
UM=${UM:-$PROG_DIR/../umount/umount}
T=${TMPDIR:-/tmp}/test_mount.$$
if [ "$(uname -s)" != V86BSD ] || [ "$(id -u)" != 0 ]; then
    echo "mount: 0 passed, 0 failed (skipped: needs root on V86BSD)"
    exit 0
fi
[ -x "$UM" ] || { cd ../../../sbin/umount && make > /dev/null && UM=$PWD/umount; cd - > /dev/null; }
mkdir -m 700 "$T" || exit 2
mkdir "$T/a" "$T/b" "$T/c"
cp /etc/fstab "$T/fstab.save" 2>/dev/null
cleanup() { "$UM" "$T/a" "$T/b" "$T/c" 2>/dev/null; if [ -f "$T/fstab.save" ]; then cp "$T/fstab.save" /etc/fstab; else rm -f /etc/fstab; fi; rm -rf "$T"; }
trap cleanup EXIT
usage1="usage: mount [-aruvw] [-t type[,type...]] [-o options] [special | node]"

# the list (the type of / is whatever the system booted with: ffs, or ext2)
RT=$("$U" | sed -n 's/^.* on \/ type \([^ ]*\) .*/\1/p')
ok "the list has /" sh -c '"$1" | grep " on / type $2 (local" > /dev/null' sh "$U" "$RT"
ok "the list has /proc" sh -c '"$1" | grep " on /proc type proc (local" > /dev/null' sh "$U"
ok "-t keeps to a type" sh -c '"$1" -t proc | grep -c " type proc " > /dev/null && ! "$1" -t proc | grep " type $2 " > /dev/null' sh "$U" "$RT"
t "-t of a type nothing has" "" 0 "$U" -t nosuchtype

# a mount by hand
t "mount tmpfs" "tmpfs on $T/a type tmpfs (local)" 0 sh -c '"$1" -t tmpfs tmpfs "$2" && "$1" | grep " on $2 "' sh "$U" "$T/a"
t "it is a new file system" "ok" 0 sh -c 'echo x > "$1/f" && [ "$(cat "$1/f")" = x ] && echo ok' sh "$T/a"
t "default type of a non-device: tmpfs" "tmpfs on $T/b type tmpfs (local)" 0 sh -c '"$1" tmpfs "$2" && "$1" | grep " on $2 "' sh "$U" "$T/b"
t "mounting on a mount stacks: the new one shows" "2 empty" 0 sh -c '"$1" -t tmpfs tmpfs "$2" && echo "$("$1" | grep -c " on $2 ") $([ -z "$(ls "$2")" ] && echo empty)"' sh "$U" "$T/a"
t "unmounting the top shows the one below" "x" 0 sh -c '"$1" "$2" && cat "$2/f"' sh "$UM" "$T/a"
te "a type that is not there" "mount: $T/c: No such device" 1 "$U" -t nosuchfs x "$T/c"
te "a directory that is not there" "mount: $T/none: No such file or directory" 1 "$U" -t tmpfs tmpfs "$T/none"
te "a device that is not there" "mount: $T/c: No such file or directory" 1 "$U" /dev/nosuchdisk "$T/c"
te "an option the kernel does not have" "mount: option not supported by this kernel: noexec" 1 "$U" -o noexec -t tmpfs tmpfs "$T/c"
t "nothing was mounted by that" "0" 0 sh -c '"$1" | grep -c " on $2 " || true' sh "$U" "$T/c"
te "bad usage" "$usage1
       mount [-ruvw] [-t type] [-o options] special node" 1 "$U" a b c
te "-a with an operand" "$usage1
       mount [-ruvw] [-t type] [-o options] special node" 1 "$U" -a x

# read-only and back: -u
t "-ur: remount read-only" "mount: ok" 0 sh -c '"$1" -ur "$2" && "$1" | grep " on $2 .*read-only" > /dev/null && echo mount: ok' sh "$U" "$T/a"
t "a write now fails" "1" 0 sh -c 'echo y > "$1/g" 2>/dev/null; [ -f "$1/g" ]; echo $?' sh "$T/a"
t "the file is still read" "x" 0 cat "$T/a/f"
t "-uw: read-write again" "0" 0 sh -c '"$1" -uw "$2" && echo y > "$2/g"; echo $?' sh "$U" "$T/a"
t "-u keeps a mount as it is" "no" 0 sh -c '"$1" -ur "$2"; "$1" -u "$2"; "$1" | grep " on $2 .*read-only" > /dev/null && echo no' sh "$U" "$T/a"
t "-o rw -u" "0" 0 sh -c '"$1" -u -o rw "$2" && echo y > "$2/h"; echo $?' sh "$U" "$T/a"
t "-r mounts read-only" "read-only" 0 sh -c '"$1" -r tmpfs "$2" && "$1" | grep " on $2 " | sed "s/.*, //; s/)//"' sh "$U" "$T/c"
"$UM" "$T/c"
te "-u: not mounted" "mount: $T/c: not mounted" 1 "$U" -u "$T/c"
te "-v shows what was done" "tmpfs on $T/c type tmpfs (rw)" 0 "$U" -v -t tmpfs tmpfs "$T/c"
"$UM" "$T/c"

# the fstab
"$UM" "$T/a" "$T/b"
cat > /etc/fstab <<FSTAB
# special  dir  type  options  freq passno
tmpfs $T/a tmpfs rw 0 0
tmpfs $T/b tmpfs rw,noauto 0 0
/dev/disk0s2 none swap sw 0 0
tmpfs $T/c tmpfs ro 0 0
FSTAB
te "-a mounts the entries that are auto" "" 0 "$U" -a
t "a is there, c read-only, b (noauto) not" "a c(ro) -" 0 sh -c 'x=$("$1" | grep -c " on $2/a "); y=$("$1" | grep " on $2/c " | grep -c read-only); z=$("$1" | grep -c " on $2/b "); [ $x = 1 ] && [ $y = 1 ] && [ $z = 0 ] && echo "a c(ro) -"' sh "$U" "$T"
te "-a again: what is mounted is left alone" "" 0 "$U" -a
t "no second mount" "1" 0 sh -c '"$1" | grep -c " on $2/a "' sh "$U" "$T"
t "-a -t: only that type" "0" 0 sh -c '"$1" -a -t $3; "$1" | grep -c " on $2/b "; true' sh "$U" "$T" "$RT"
t "a noauto entry by its directory" "1" 0 sh -c '"$1" "$2/b" && "$1" | grep -c " on $2/b "' sh "$U" "$T"
t "-u -a: updates what is mounted (c: ro stays, a: rw)" "ok" 0 sh -c '"$1" -ua && "$1" | grep " on $2/c " | grep read-only > /dev/null && echo ok' sh "$U" "$T"
"$UM" "$T/a" "$T/b" "$T/c"
t "an entry by its special file: a name in the fstab" "mounted" 0 sh -c 'printf "tmpfs $2/a tmpfs rw 0 0\n" > /etc/fstab; "$1" tmpfs && "$1" | grep " on $2/a " > /dev/null && echo mounted' sh "$U" "$T"
"$UM" "$T/a"
te "a name that is not in the fstab" "mount: /nowhere: not in /etc/fstab" 1 "$U" /nowhere
te "an fstab with a bad option" "mount: option not supported by this kernel: nosuid" 1 sh -c 'printf "tmpfs $1/a tmpfs nosuid 0 0\n" > /etc/fstab; "$2" -a' sh "$T" "$U"
rm -f /etc/fstab
te "no fstab" "mount: /etc/fstab: No such file or directory" 1 "$U" -a
done_tests mount
