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

# boot.sh - boot a V86BSD system in QEMU: V86BSD's kernel with the disk image
# mkdisk.sh made.
#
# Usage: sh boot.sh [-egiv] [-m memory] [-n] [-k kernel] [-p hostport:port]...
#                   [image [-- qemu-argument...]]
#
#   image     the disk image (default: v86bsd.img beside this script)
#   -k kernel the kernel (default: root/bsd beside this script)
#   -m memory QEMU's memory size (default 1G; for example 512M, 2G)
#   -g        a window with the VGA screen and the keyboard, instead of the
#             console on this terminal (the serial port)
#   -n        no network card
#   -v        the network card is a virtio one (vio0), not the Intel e1000 (em0)
#   -e        QEMU's emulation of the processor, also where KVM is available
#   -i        the disk as a PC's IDE disk (disk0 at pciide0: the ATA driver,
#             much slower) instead of a virtio disk (disk0 at vioblk0)
#   -p H:G    forward TCP port H of this host (127.0.0.1) to port G of
#             the system (repeatable), for a server in it
#   -- ...    more arguments for qemu-system-x86_64
#
# Without -g the system's console is this terminal: the shell comes up
# here.  Ctrl-A x leaves QEMU at once (Ctrl-A h: more of QEMU's keys; the
# disk is then checked at the next boot);
# "exit" from the console shell shuts the system down cleanly (init writes
# everything back to the disk and powers off).
#
# The network is QEMU's user networking: the system gets 10.0.2.15 by DHCP,
# the host is 10.0.2.2, the name server 10.0.2.3, and connections out go
# through the host (ping reaches only 10.0.2.2).
#
# Before booting, an ext2 disk image is checked with e2fsck -p (e2fsprogs),
# which repairs it after a system that did not shut down cleanly.
#
# What is needed: QEMU (qemu-system-x86_64; on Ubuntu the package
# qemu-system-x86) and e2fsprogs.  KVM is used when /dev/kvm can be opened, which makes
# the system much faster (V86BSD has been tested under QEMU's emulation;
# -e uses that even where KVM is available).

usage() {
	echo "usage: sh boot.sh [-egiv] [-m memory] [-n] [-k kernel] [-p hostport:port]... [image [-- qemu-argument...]]" >&2
	exit 2
}

here=$(pwd)
cd "$(dirname "$0")/.." || exit 2
SRC=$(pwd)
kernel=$SRC/root/bsd mem=1G graphic=false net=true fwd= kvm=true ide=false nic=e1000
while getopts egik:m:nvp: opt; do
	case $opt in
	e) kvm=false ;;
	i) ide=true ;;
	g) graphic=true ;;
	k) kernel=$OPTARG ;;
	m) mem=$OPTARG ;;
	n) net=false ;;
	v) nic=virtio-net-pci ;;
	p)
		h=${OPTARG%%:*} g=${OPTARG#*:}
		case $h:$g in *[!0-9:]*|:*|*:|*:*:*) usage ;; esac
		[ "$h" != "$OPTARG" ] || usage
		fwd="$fwd,hostfwd=tcp:127.0.0.1:${OPTARG%%:*}-:${OPTARG#*:}" ;;
	*) usage ;;
	esac
done
shift $((OPTIND - 1))
img=$SRC/v86bsd.img
if [ $# -gt 0 ] && [ "$1" != -- ]; then img=$1; shift; fi
if [ $# -gt 0 ]; then [ "$1" = -- ] || usage; shift; fi
case $img in /*) ;; *) img=$here/$img ;; esac
case $kernel in /*) ;; *) kernel=$here/$kernel ;; esac

command -v qemu-system-x86_64 >/dev/null 2>&1 ||
	{ echo "boot: no qemu-system-x86_64: install QEMU (Ubuntu: qemu-system-x86)" >&2; exit 1; }
[ -f "$kernel" ] || { echo "boot: no kernel $kernel (run sh bootstrap.sh)" >&2; exit 1; }
[ -f "$img" ] || { echo "boot: no disk image $img (run sh mkdisk.sh)" >&2; exit 1; }

# An ext2 disk is checked first, as a boot of Linux would: after a system
# that did not shut down (QEMU stopped by Ctrl-A x, or killed) the disk is
# marked "not clean", and e2fsck -p repairs what it safely can.  An FFS disk
# is checked by the system itself: the kernel mounts a root that is not clean
# read-only, and /etc/rc runs fsck -p and reboots if it changed the root.
PATH=$PATH:/sbin:/usr/sbin
ptype=$(od -A n -t x1 -j 450 -N 1 "$img" 2>/dev/null | tr -d ' ')
if [ "$ptype" = 83 ]; then
	if command -v e2fsck >/dev/null 2>&1; then
		# (the file system is the partition: 1 MiB into the disk)
		e2fsck -p "$img?offset=1048576"
		st=$?
		if [ $st -ge 4 ]; then
			echo "boot: $img has errors e2fsck -p cannot repair: run e2fsck -f '$img?offset=1048576'" >&2
			exit 1
		fi
	else
		echo "boot: no e2fsck (e2fsprogs): the disk is not checked" >&2
	fi
fi

# the disk: virtio, or with -i the IDE disk of a PC (the slower ATA driver).
# Either way it is disk0 and the root is disk0s1 (the kernel's `root on')
if $ide; then if=ide; else if=virtio; fi
set -- -m "$mem" -no-reboot -kernel "$kernel" -append "CONSOLE=/dev/tty00" \
	-drive "file=$img,format=raw,if=$if" "$@"
if $net; then
	set -- "$@" -nic "user,model=$nic$fwd"
else
	set -- "$@" -nic none
fi
# without -g the serial port is this terminal; with it, the console shows
# on the VGA screen too and takes the keyboard's input (it is the same
# terminal, /dev/tty00)
$graphic || set -- "$@" -nographic
# KVM where this user may use it, else QEMU's emulation
if $kvm && [ -r /dev/kvm ] && [ -w /dev/kvm ]; then
	set -- -accel kvm "$@"
	echo "boot: with KVM" >&2
elif $kvm && [ -e /dev/kvm ]; then
	echo "boot: /dev/kvm is there but not usable by you (add yourself to its group," >&2
	echo "      e.g. sudo usermod -aG kvm \$USER, and log in again): QEMU's emulation, slow" >&2
else
	echo "boot: QEMU's emulation (no KVM): slow" >&2
fi
exec qemu-system-x86_64 "$@"
