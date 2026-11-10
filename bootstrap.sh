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

# bootstrap.sh - build a Virtual/86 BSD system from scratch into a directory
# that can be entered with chroot (chroot.sh) or booted (mkdisk.sh, boot.sh).
# A wrapper: it runs the targets of the Makefile beside it (newroot buildworld
# buildkernel installworld distribution installkernel; distrib/build.sh says
# what each does).
#
# Usage: sh bootstrap.sh [-k] [-n] [-w workdir] [root]
#
#   root        the system to make (default: root/ beside this script); it
#               is made afresh
#   -w workdir  where it is built (default: obj/ beside this script)
#   -k          keep workdir afterwards (default: what was built there is
#               removed when all went well)
#   -n          all programs statically linked (default: those in /bin and
#               /sbin are, the rest are linked dynamically)
#
# What is needed: a POSIX system with a C compiler (CC; without it c17, c99
# or cc, the first there is), make and ar, on x86-64 Linux.  On Ubuntu:
# build-essential.  Nothing is installed outside workdir and root; root needs
# no privileges to build.  BUILDER=user@host in the environment names the
# builder in the kernel's banner.
#
# Then:   sh chroot.sh root   (with or without root privileges), or on
#         its own kernel in QEMU: sh mkdisk.sh root; sh boot.sh -k root/bsd
#
# Programs in the system are linked dynamically (those in /bin and /sbin,
# statically), and so are by default the programs c17 makes there
# ("c17 -B static" links V86BSD's libc.a); "c17 -G" makes shared libraries.

usage() { echo "usage: sh bootstrap.sh [-k] [-n] [-w workdir] [root]" >&2; exit 2; }

here=$(pwd)
cd "$(dirname "$0")" || exit 2
SRC=$(pwd)
keep=false work=$SRC/obj flags=
while getopts knw: opt; do
	case $opt in
	k) keep=true ;;
	n) flags=-n ;;
	w) work=$OPTARG ;;
	*) usage ;;
	esac
done
shift $((OPTIND - 1))
[ $# -le 1 ] || usage
root=${1:-$SRC/root}
case $root in /*) ;; *) root=$here/$root ;; esac
case $work in /*) ;; *) work=$here/$work ;; esac
[ "$root" != / ] || { echo "bootstrap: root must not be /" >&2; exit 2; }

make=${MAKE:-make}
set -- "$make" OBJDIR="$work" DESTDIR="$root" BUILDFLAGS="$flags" HOSTCC="$CC"
"$@" newroot buildworld buildkernel installworld distribution installkernel || exit 1

$keep || { rm -rf "$work/stage0" "$work/stage1" "$work/stage2" "$work/stage3" "$work/world" \
	"$work/sys" "$work/files" "$work/log" "$work"/stage*.profile; rmdir "$work" 2>/dev/null; }
echo "Enter it:  sh $SRC/chroot.sh $root   (as root: chroot; otherwise a user namespace)"
echo "Or boot it on its own kernel in QEMU:  sh $SRC/mkdisk.sh $root; sh $SRC/boot.sh -k $root/bsd"
