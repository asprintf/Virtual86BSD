#!/bin/sh
# qemu-freebsd.sh [-g] [-e] [-i] [-n] [-m MiB] [-s CPUS] [-p HOSTPORT:PORT] [IMAGE] [-- QEMU-ARGS...]:
# boot a V86BSD disk image (default obj/boot/v86bsd.img, made by boot-freebsd.sh)
# on the FreeBSD kernel in QEMU.  The console is this terminal (serial); -g adds a
# window with the VGA screen and the keyboard.  -e: no KVM, -i: the disk is an AHCI
# disk (ada0) instead of virtio (vtbd0), -n: no network, -m: memory (default 1024),
# -s: processors (default 2), -p: forward a port of the host (127.0.0.1) to the system.
top=$(cd "$(dirname "$0")/.." && pwd)
cpu=host accel=kvm bus=virtio net=1 mem=1024 smp=2 gfx=-nographic fwd=
while getopts geinm:s:p: opt; do
	case $opt in
	g) gfx="-display gtk -serial mon:stdio" ;;
	e) accel=tcg cpu=max ;;
	i) bus=ahci ;;
	n) net=0 ;;
	m) mem=$OPTARG ;;
	s) smp=$OPTARG ;;
	p) fwd=,hostfwd=tcp:127.0.0.1:${OPTARG%%:*}-:${OPTARG#*:} ;;
	*) exit 2 ;;
	esac
done
shift $((OPTIND - 1))
img=$top/obj/boot/v86bsd.img
if [ $# -gt 0 ] && [ "$1" != -- ]; then img=$1; shift; fi
[ "${1:-}" = -- ] && shift
[ -f "$img" ] || { echo "qemu-freebsd: no $img: run sh distrib/boot-freebsd.sh -n (make image)" >&2; exit 1; }
if [ $bus = virtio ]; then drive="-drive file=$img,format=raw,if=virtio"; else drive="-drive file=$img,format=raw,if=none,id=d0 -device ich9-ahci,id=ahci -device ide-hd,drive=d0,bus=ahci.0"; fi
if [ $net = 1 ]; then nic="-nic user,model=virtio-net-pci$fwd"; else nic="-nic none"; fi
# shellcheck disable=SC2086
exec qemu-system-x86_64 -machine q35 -accel $accel -cpu $cpu \
	-m "$mem" -smp "$smp" $gfx $drive $nic "$@"
