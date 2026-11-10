#!/bin/sh
# mkdisk-ext2.sh - make an ext2 disk image of a V86BSD root.
#
# Usage: sh distrib/mkdisk-ext2.sh rootdir image [size-in-MiB]
#
# The root is copied to a staging directory, completed with what a booted
# system needs (/dev, /proc, /tmp; /sbin/init is in the root), and written
# with the host's mke2fs -d (e2fsprogs).  Files are owned by root.  Plain
# ext2: filetype, sparse_super, large_file.  Run from the top of the tree
# (distrib/mkdisk.sh does): the kernel's test programs are taken from there.
set -e
root=$1
img=$2
size=${3:-512}
[ -d "$root" ] || { echo "mkdisk: $root: not a directory" >&2; exit 1; }
stage=${TMPDIR:-/tmp}/mkdisk.$$
rm -rf "$stage"
cp -pR "$root" "$stage"
for d in dev proc tmp root; do
    [ -d "$stage/$d" ] || mkdir -p "$stage/$d"
done
chmod 1777 "$stage/tmp"
# the name server DHCP gave (the kernel's /proc/net/pnp, as on Linux), also
# where root has the host's copy (chroot.sh's): its server, such as
# systemd-resolved's 127.0.0.53, may not be reachable from the guest
rm -f "$stage/etc/resolv.conf"
ln -s /proc/net/pnp "$stage/etc/resolv.conf"
# the kernel's own test programs (make -C regress/sys/kern testprogs), in /root/ktest:
# only what has been built (the sources are in the tree too)
ktest=regress/sys/kern
built=false
for f in $ktest/k_*; do
    [ -f "$f" ] && [ -x "$f" ] && built=true
done
if $built; then
    mkdir -p "$stage/root/ktest"
    for f in $ktest/k_*; do
        case $f in *.c|*.s) ;; *) cp "$f" "$stage/root/ktest/" ;; esac
    done
fi
if [ -n "$(ls $ktest/libc/s_* 2>/dev/null)" ]; then
    mkdir -p "$stage/root/ktest"
    cp -pR regress/sys/kern/libc "$stage/root/ktest/libc"
fi
rm -f "$img"
mke2fs -q -t ext2 -b 4096 -O ^resize_inode,^dir_index,^ext_attr -L v86bsd \
    -E root_owner=0:0 -d "$stage" "$img" "${size}M"
rm -rf "$stage"
