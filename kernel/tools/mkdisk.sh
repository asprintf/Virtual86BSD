#!/bin/sh
# mkdisk.sh - make an ext2 disk image of a Shall root.
#
# Usage: sh tools/mkdisk.sh rootdir image [size-in-MiB]
#
# The root is copied to a staging directory, completed with what a booted
# system needs (/sbin, /dev, /proc, /tmp, /sys, /run; user/init as /sbin/init if
# it has been built: make init), and written with the host's mke2fs -d
# (e2fsprogs).  Files are owned by root.  Plain ext2: filetype,
# sparse_super, large_file.
set -e
root=$1
img=$2
size=${3:-512}
[ -d "$root" ] || { echo "mkdisk: $root: not a directory" >&2; exit 1; }
stage=${TMPDIR:-/tmp}/mkdisk.$$
rm -rf "$stage"
cp -pR "$root" "$stage"
for d in dev proc tmp sys root run usr/sbin; do
    [ -d "$stage/$d" ] || mkdir -p "$stage/$d"
done
chmod 1777 "$stage/tmp"
[ -e "$stage/sbin" ] || ln -s usr/sbin "$stage/sbin"
# the name server DHCP gave (the kernel's /proc/net/pnp, as on Linux), also
# where root has the host's copy (chroot.sh's): its server, such as
# systemd-resolved's 127.0.0.53, may not be reachable from the guest
rm -f "$stage/etc/resolv.conf"
ln -s /proc/net/pnp "$stage/etc/resolv.conf"
[ -f user/init ] && cp user/init "$stage/usr/sbin/init"
# the kernel's own test programs (make testprogs), in /root/ktest
if ls tests/k_* >/dev/null 2>&1; then
    mkdir -p "$stage/root/ktest"
    for f in tests/k_*; do
        case $f in *.c|*.s) ;; *) cp "$f" "$stage/root/ktest/" ;; esac
    done
fi
if [ -d tests/libc ]; then
    mkdir -p "$stage/root/ktest"
    cp -pR tests/libc "$stage/root/ktest/libc"
fi
rm -f "$img"
mke2fs -q -t ext2 -b 4096 -O ^resize_inode,^dir_index,^ext_attr -L shall \
    -E root_owner=0:0 -d "$stage" "$img" "${size}M"
rm -rf "$stage"
