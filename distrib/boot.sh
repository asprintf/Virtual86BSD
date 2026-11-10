#!/bin/sh
# boot.sh - boot a V86BSD system in QEMU: V86BSD's kernel with the disk image
# mkdisk.sh made.
#
# Usage: sh boot.sh [-egi] [-m memory] [-n] [-k kernel] [-p hostport:port]...
#                   [image [-- qemu-argument...]]
#
#   image     the disk image (default: v86bsd.img beside this script)
#   -k kernel the kernel (default: root/bsd beside this script)
#   -m memory QEMU's memory size (default 1G; for example 512M, 2G)
#   -g        a window with the VGA screen and the keyboard, instead of the
#             console on this terminal (the serial port)
#   -n        no network card
#   -e        QEMU's emulation of the processor, also where KVM is available
#   -i        the disk as a PC's IDE disk (/dev/sda: the ATA driver, much
#             slower) instead of a virtio disk (/dev/vda)
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
# Before booting, the disk image is checked with e2fsck -p (e2fsprogs),
# which repairs it after a system that did not shut down cleanly.
#
# What is needed: QEMU (qemu-system-x86_64; on Ubuntu the package
# qemu-system-x86) and e2fsprogs.  KVM is used when /dev/kvm can be opened, which makes
# the system much faster (V86BSD has been tested under QEMU's emulation;
# -e uses that even where KVM is available).

usage() {
	echo "usage: sh boot.sh [-egi] [-m memory] [-n] [-k kernel] [-p hostport:port]... [image [-- qemu-argument...]]" >&2
	exit 2
}

here=$(pwd)
cd "$(dirname "$0")/.." || exit 2
SRC=$(pwd)
kernel=$SRC/root/bsd mem=1G graphic=false net=true fwd= kvm=true ide=false
while getopts egik:m:np: opt; do
	case $opt in
	e) kvm=false ;;
	i) ide=true ;;
	g) graphic=true ;;
	k) kernel=$OPTARG ;;
	m) mem=$OPTARG ;;
	n) net=false ;;
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

# check the file system first, as a boot of Linux would: after a system
# that did not shut down (QEMU stopped by Ctrl-A x, or killed) the disk
# is marked "not clean", and e2fsck -p repairs what it safely can
PATH=$PATH:/sbin:/usr/sbin
if command -v e2fsck >/dev/null 2>&1; then
	e2fsck -p "$img"
	st=$?
	if [ $st -ge 4 ]; then
		echo "boot: $img has errors e2fsck -p cannot repair: run e2fsck -f $img" >&2
		exit 1
	fi
else
	echo "boot: no e2fsck (e2fsprogs): the disk is not checked" >&2
fi

# the disk: virtio (vda), or with -i the IDE disk of a PC (sda, the slower
# ATA driver)
if $ide; then dev=sda if=ide; else dev=vda if=virtio; fi
set -- -m "$mem" -no-reboot -kernel "$kernel" -append "root=/dev/$dev CONSOLE=/dev/ttyS0" \
	-drive "file=$img,format=raw,if=$if" "$@"
if $net; then
	set -- "$@" -nic "user,model=e1000$fwd"
else
	set -- "$@" -nic none
fi
# without -g the serial port is this terminal; with it, the console shows
# on the VGA screen too and takes the keyboard's input (it is the same
# terminal, /dev/ttyS0)
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
