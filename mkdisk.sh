#!/bin/sh
# mkdisk.sh - make a bootable disk image of a V86BSD system made by
# bootstrap.sh: an ext2 file system holding the root, to boot with V86BSD's
# kernel (root/bsd) in QEMU (see boot.sh).  A wrapper: it runs "make image"
# (distrib/mkdisk.sh does the work).
#
# Usage: sh mkdisk.sh [-s size] [root [image]]
#
#   root     the system (default: root/ beside this script)
#   image    the disk image to write (default: v86bsd.img beside this script);
#            it is made afresh
#   -s size  its size in MiB (default 1024)

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
root=${1:-$SRC/root}
img=${2:-$SRC/v86bsd.img}
case $root in /*) ;; *) root=$here/$root ;; esac
case $img in /*) ;; *) img=$here/$img ;; esac
exec "${MAKE:-make}" image IMGROOT="$root" IMAGE="$img" IMGSIZE="$size"
