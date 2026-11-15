#!/bin/sh
# Copyright (c) 2026, Joost Orchidee Winter
# Copyright (c) 2026, asprintf
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice, this
#    list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
#
# 3. Neither the name of the copyright holder nor the names of its
#    contributors may be used to endorse or promote products derived from
#    this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# mkdisk-ffs.sh - make a disk image of a V86BSD root: a partition table (an
# MBR, or with `gpt` a GPT) with one partition (MBR type 0xa6 or OpenBSD's
# GPT type for data, from sector 2048, 1 MiB in) holding an FFS2 file system
# -- disk0s1 in the system, which boots from it.  If the root has the boot
# blocks (usr/mdec/mbr and usr/mdec/boot, which the kernel's build makes),
# they go to the disk: the loader in the sectors before the partition, so
# the image boots by itself, with no -kernel.
#
# Usage: sh distrib/mkdisk-ffs.sh rootdir image [size-in-MiB [mbr|gpt]]
#
# The root is copied to a staging directory, completed with what a booted
# system needs (/dev, /proc, /tmp; /sbin/init is in the root), and written
# with makefs (usr.sbin/makefs, MAKEFS=path names another; by default the one
# the build made in obj/) into a file, which goes to the partition.  Files
# are owned by root, and the file system has an inode for each 8 KiB, as
# newfs makes it.  Run from the top of the tree (distrib/mkdisk.sh does):
# the kernel's test programs are taken from there.
set -e
root=$1
img=$2
size=${3:-512}
scheme=${4:-mbr}
case $scheme in mbr|gpt) ;; *) echo "mkdisk: the partition table is mbr or gpt" >&2; exit 1 ;; esac
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
[ "$size" -ge 4 ] || { echo "mkdisk: the image is at least 4 MiB" >&2; exit 1; }
# the file system fills the partition: the rest of the disk after 1 MiB (the
# GPT keeps a copy of its table at the end of the disk: 1 MiB left for it)
if [ "$scheme" = gpt ]; then fssize=$((size - 2)); else fssize=$((size - 1)); fi
fsfile=$img.fs
makefs=${MAKEFS:-obj/stage2/root/usr/bin/makefs}
[ -x "$makefs" ] || { echo "mkdisk: no makefs ($makefs): run make buildworld, or give MAKEFS" >&2; exit 1; }
# density=8192: one inode for each 8 KiB, as newfs makes them (makefs alone
# gives a tree only 25% more inodes than it has files, whatever the size, and
# a disk made so is full after a few thousand new files)
"$makefs" -U -s "${fssize}m" -o label=v86bsd,density=8192 "$fsfile" "$stage" > /dev/null
rm -rf "$stage"
# the partition table: one entry, the file system 1 MiB in (the usual
# alignment) and all of the partition
le32() {
    n=$1
    printf "\\$(printf %03o $((n & 255)))\\$(printf %03o $(((n >> 8) & 255)))"
    printf "\\$(printf %03o $(((n >> 16) & 255)))\\$(printf %03o $(((n >> 24) & 255)))"
}
nsect=$((fssize * 2048))
dd if=/dev/null of="$img" bs=1M seek="$size" 2>/dev/null
if [ "$scheme" = gpt ]; then
    # (mkgpt writes the protective MBR, the table and its copy at the end)
    mkgpt=${TMPDIR:-/tmp}/mkgpt.$$
    ${HOSTCC:-cc} -o "$mkgpt" distrib/mkgpt.c || { echo "mkdisk: cannot build mkgpt: needs a C compiler on the host (HOSTCC)" >&2; exit 1; }
    "$mkgpt" -p "2048:$nsect" "$img" || { rm -f "$mkgpt"; exit 1; }
    rm -f "$mkgpt"
else
    {
        dd if=/dev/zero bs=446 count=1 2>/dev/null
        printf '\000\376\377\377\246\376\377\377'  # boot flag, CHS (use LBA), type 0xa6, CHS
        le32 2048
        le32 "$nsect"
        dd if=/dev/zero bs=48 count=1 2>/dev/null       # the three other entries
        printf '\125\252'
    } > "$img.mbr"
    dd if="$img.mbr" of="$img" bs=512 count=1 conv=notrunc 2>/dev/null
    rm -f "$img.mbr"
fi
dd if="$fsfile" of="$img" bs=1M seek=1 conv=notrunc,sparse 2>/dev/null
rm -f "$fsfile"
# the boot blocks: stage 1 is the first 440 bytes of sector 0 (the table
# stays), stage 2 starts at sector 34 (free in both kinds of table)
mdec=$root/usr/mdec
if [ -f "$mdec/mbr" ] && [ -f "$mdec/boot" ]; then
    [ "$(wc -c < "$mdec/boot")" -le $((2014 * 512)) ] || { echo "mkdisk: the loader does not fit before the partition" >&2; exit 1; }
    dd if="$mdec/mbr" of="$img" bs=440 count=1 conv=notrunc 2>/dev/null
    dd if="$mdec/boot" of="$img" bs=512 seek=34 conv=notrunc 2>/dev/null
fi
