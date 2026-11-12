#!/bin/sh
# Copyright (c) 2026, Joost Orchidee Winter
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

# mkdisk.sh - make a bootable disk image of a V86BSD system made by
# bootstrap.sh: an FFS2 file system holding the root (or ext2, with -t), to
# boot with V86BSD's kernel (root/bsd) in QEMU (see boot.sh).
#
# Usage: sh mkdisk.sh [-s size] [-t ffs|ext2] [root [image]]
#
#   root     the system (default: root/ beside this script)
#   image    the disk image to write (default: v86bsd.img beside this script);
#            it is made afresh
#   -s size  its size in MiB (default 1024)
#   -t type  the file system: ffs (the default; makefs, made by the build in
#            obj/stage2/root/usr/bin, or MAKEFS=path) or ext2 (e2fsprogs'
#            mke2fs with -d, version 1.43 or later)
#
# No root privileges.  The root itself is not changed: it is copied, completed
# with what a booted system needs (/dev, /proc, /tmp; /etc/resolv.conf
# as a link to the kernel's /proc/net/pnp, the name server DHCP gave), and
# written into the image with every file owned by root.
#
# Inside the booted system the disk is disk0 and the file system its partition
# disk0s1; whatever is written there
# stays in the image for the next boot.  (Shut down with "exit" from the
# console shell, so that init writes everything back, rather than by
# killing QEMU.)

usage() { echo "usage: sh mkdisk.sh [-s size] [-t ffs|ext2] [root [image]]" >&2; exit 2; }

here=$(pwd)
cd "$(dirname "$0")/.." || exit 2
SRC=$(pwd)
size=1024
type=ffs
while getopts s:t: opt; do
	case $opt in
	s) size=$OPTARG ;;
	t) type=$OPTARG ;;
	*) usage ;;
	esac
done
shift $((OPTIND - 1))
[ $# -le 2 ] || usage
case $size in ''|*[!0-9]*) echo "mkdisk: -s: a size in MiB" >&2; exit 2 ;; esac
case $type in ffs|ext2) ;; *) echo "mkdisk: -t: ffs or ext2" >&2; exit 2 ;; esac
root=${1:-$SRC/root}
img=${2:-$SRC/v86bsd.img}
case $root in /*) ;; *) root=$here/$root ;; esac
case $img in /*) ;; *) img=$here/$img ;; esac

[ -x "$root/bin/sh" ] || { echo "mkdisk: $root: not a V86BSD root (run sh bootstrap.sh first)" >&2; exit 1; }
[ -f "$root/bsd" ] ||
	echo "mkdisk: warning: $root has no bsd (made by bootstrap.sh)" >&2
PATH=$PATH:/sbin:/usr/sbin
if [ "$type" = ext2 ]; then
	command -v mke2fs >/dev/null 2>&1 ||
		{ echo "mkdisk: no mke2fs: install e2fsprogs" >&2; exit 1; }
elif [ ! -x "${MAKEFS:-obj/stage2/root/usr/bin/makefs}" ]; then
	echo "mkdisk: no makefs (${MAKEFS:-obj/stage2/root/usr/bin/makefs}): run make buildworld, or give MAKEFS=path" >&2
	exit 1
fi

# distrib/mkdisk-$type.sh does the work; run here, where there are no
# kernel test programs or init of a kernel build to add
sh "$SRC/distrib/mkdisk-$type.sh" "$root" "$img" "$size" || { echo "mkdisk: failed" >&2; exit 1; }
echo "$img: $size MiB, $type, root of $root"
echo "Boot it:  sh $SRC/boot.sh"
