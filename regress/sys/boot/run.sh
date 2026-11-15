#!/bin/sh
# run.sh - tests of the boot loader (sys/arch/amd64/stand): boots disk images
# in QEMU the way a PC does, with no -kernel, and types at its boot> prompt:
# an image with an MBR and one with a GPT, the commands, the error messages,
# the boot of the default kernel after the timeout and `boot -s`.  Run from
# the top of the tree after `make release` (it uses obj/root, or ROOT=dir:
# a root with usr/mdec/mbr and boot); on the host, as a user.
#
#   sh regress/sys/boot/run.sh
#
# The result is "N passed, M failed" (the transcript of a failed run is shown).
set -u
cd "$(dirname "$0")/../../.." || exit 2
ROOT=${ROOT:-${OBJDIR:-obj}/root}
[ -f "$ROOT/usr/mdec/boot" ] && [ -f "$ROOT/usr/mdec/mbr" ] ||
	{ echo "boot: no $ROOT/usr/mdec/{mbr,boot}: make release first (it needs a host compiler for 32-bit code)" >&2; exit 2; }
command -v qemu-system-x86_64 >/dev/null 2>&1 || { echo "boot: no qemu-system-x86_64" >&2; exit 2; }
T=$(mktemp -d "${TMPDIR:-/tmp}/bootrun.XXXXXX") || exit 2
trap 'rm -rf "$T"' EXIT
cc -O1 -o "$T/ptydrive" regress/lib/testlib/ptydrive.c || exit 2
accel=tcg
[ -r /dev/kvm ] && [ -w /dev/kvm ] && accel=kvm
pass=0 fail=0

# t NAME IMAGE ops...: boot a copy of IMAGE, do the ops of ptydrive, expect them all
t() {
	name=$1 img=$2
	shift 2
	cp --sparse=always "$img" "$T/disk" 2>/dev/null || cp "$img" "$T/disk"
	if "$T/ptydrive" -T 120 "$@" -- qemu-system-x86_64 -accel $accel -m 512M -no-reboot -nographic \
	    -nic none -drive "file=$T/disk,format=raw,if=virtio" > "$T/out" 2>&1; then
		pass=$((pass + 1))
		echo "ok   $name"
	else
		fail=$((fail + 1))
		echo "FAIL $name"
		tail -n 30 "$T/out"
	fi
}

for table in mbr gpt; do
	sh distrib/mkdisk.sh -s 512 -p $table "$ROOT" "$T/$table.img" > "$T/mkdisk.log" 2>&1 ||
		{ cat "$T/mkdisk.log"; exit 2; }
	upper=$(echo $table | tr a-z A-Z)
	t "$table: the banner and the disk" "$T/$table.img" \
		'e:probing: pc0 com0 mem[' 'e:disk: disk0*' 'e:>> V86BSD/amd64 BOOT' 'e:boot> '
	t "$table: machine diskinfo" "$T/$table.img" \
		'e:boot> ' 's:machine diskinfo\n' "e:disk0: BIOS drive 0x80" "e:, $upper" 'e:disk0s1: start 2048' 'e:boot> '
	t "$table: machine memory" "$T/$table.img" \
		'e:boot> ' 's:machine memory\n' 'e:type 1 (usable)' 'e:boot> '
	t "$table: ls" "$T/$table.img" \
		'e:boot> ' 's:ls\n' 'e:bsd' 'e:etc/' 's:ls /etc\n' 'e:fstab' 's:ls disk0s1:/sbin\n' 'e:init' 'e:boot> '
	t "$table: errors" "$T/$table.img" \
		'e:boot> ' 's:boot disk0s1:/nonexistent\n' 'e:disk0s1:/nonexistent: no such file' \
		'e:boot> ' 's:boot disk0s9:/bsd\n' 'e:disk0s9: no FFS2 file system' \
		'e:boot> ' 's:boot disk0s1:/etc\n' 'e:not a regular file' \
		'e:boot> ' 's:boot disk0s1:/etc/motd\n' 'e:not a Multiboot kernel' \
		'e:boot> ' 's:frobnicate\n' 'e:frobnicate: unknown command' 'e:boot> '
	t "$table: boot, the kernel finds its root" "$T/$table.img" \
		'e:boot> ' 's:boot\n' 'e:booting disk0s1:/bsd: ' "e:disk0: $upper, 1 partition" \
		'e:root on disk0s1 (ffs, read-only)' 'e:login: '
	t "$table: the timeout boots the default" "$T/$table.img" \
		'e:boot> ' 'e:booting disk0s1:/bsd: ' 'e:root on disk0s1 (ffs, read-only)' 'e:login: '
	t "$table: boot -s, flags and unknown words" "$T/$table.img" \
		'e:boot> ' 's:boot disk0s1:bsd -s -x root=/dev/foo\n' 'e:booting disk0s1:/bsd: ' \
		'e:boot flag -x is not supported' 'e:boot argument root=/dev/foo is not supported' \
		'e:bootargs: -s -x root=/dev/foo' 'e:v86bsd# ' 's:echo single-$((6*7))\n' 'e:single-42'
done
echo "$pass passed, $fail failed"
[ $fail = 0 ]
