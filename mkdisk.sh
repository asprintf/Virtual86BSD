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
