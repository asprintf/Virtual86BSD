#!/bin/sh
# mkdisk.sh - make a bootable disk image of a Shall system made by
# bootstrap.sh: an ext2 file system holding the root, to boot with Shall's
# kernel (root/boot/shall-kernel) in QEMU (see boot.sh).
#
# Usage: sh mkdisk.sh [-s size] [root [image]]
#
#   root     the system (default: root/ beside this script)
#   image    the disk image to write (default: shall.img beside this script);
#            it is made afresh
#   -s size  its size in MiB (default 1024)
#
# What is needed: e2fsprogs (mke2fs with -d, version 1.43 or later); no
# root privileges.  The root itself is not changed: it is copied, completed
# with what a booted system needs (/dev, /proc, /sys, /tmp, /run; /etc/resolv.conf
# as a link to the kernel's /proc/net/pnp, the name server DHCP gave), and
# written into the image with every file owned by root.
#
# Inside the booted system the disk is /dev/sda; whatever is written there
# stays in the image for the next boot.  (Shut down with "exit" from the
# console shell, so that init writes everything back, rather than by
# killing QEMU.)

usage() { echo "usage: sh mkdisk.sh [-s size] [root [image]]" >&2; exit 2; }

here=$(pwd)
cd "$(dirname "$0")" || exit 2
SRC=$(pwd)
size=1024
while getopts s: opt; do
	case $opt in
	s) size=$OPTARG ;;
	*) usage ;;
	esac
done
shift $((OPTIND - 1))
[ $# -le 2 ] || usage
case $size in ''|*[!0-9]*) echo "mkdisk: -s: a size in MiB" >&2; exit 2 ;; esac
root=${1:-$SRC/root}
img=${2:-$SRC/shall.img}
case $root in /*) ;; *) root=$here/$root ;; esac
case $img in /*) ;; *) img=$here/$img ;; esac

[ -d "$root/usr/bin" ] || { echo "mkdisk: $root: not a Shall root (run sh bootstrap.sh first)" >&2; exit 1; }
[ -f "$root/boot/shall-kernel" ] ||
	echo "mkdisk: warning: $root has no boot/shall-kernel (made by bootstrap.sh)" >&2
command -v mke2fs >/dev/null 2>&1 || [ -x /sbin/mke2fs ] || [ -x /usr/sbin/mke2fs ] ||
	{ echo "mkdisk: no mke2fs: install e2fsprogs" >&2; exit 1; }
PATH=$PATH:/sbin:/usr/sbin

# kernel/tools/mkdisk.sh does the work; run here, where there are no
# kernel test programs or init of a kernel build to add
sh "$SRC/kernel/tools/mkdisk.sh" "$root" "$img" "$size" || { echo "mkdisk: failed" >&2; exit 1; }
echo "$img: $size MiB, ext2, root of $root"
echo "Boot it:  sh $SRC/boot.sh"
