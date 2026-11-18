#!/bin/sh
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

# mkimage-freebsd.sh [-o IMAGE] [-s MiB] [-i INIT] [-d ROOTDIR]: make a bootable
# GPT disk image of the FreeBSD kernel switch: an EFI system partition (FAT,
# loader.efi as EFI/BOOT/BOOTX64.EFI), a freebsd-boot partition (pmbr in the
# protective MBR, gptboot) and a UFS2 root with /boot/loader, /boot/kernel/kernel
# and /sbin/init.  Boots by BIOS (gptboot) and by UEFI.  The FreeBSD parts come
# from distrib/build-freebsd.sh (kernel, loader) and its host tools makefs and
# mkimg.  INIT is the program for /sbin/init (default: distrib/testinit.c built
# with the host clang, used unless ROOTDIR has sbin/init); ROOTDIR, if given, is
# copied over the default root files.
# The same inputs give the same image (SOURCE_DATE_EPOCH, default 0).
set -eu
here=$(cd "$(dirname "$0")" && pwd)
top=$(dirname "$here")
objdir=$top/obj/freebsd
out=$top/obj/v86bsd.img
size=128
init=
extra=
while getopts o:s:i:d:O: c; do
	case $c in
	o) out=$OPTARG ;;
	s) size=$OPTARG ;;
	i) init=$OPTARG ;;
	d) extra=$(cd "$OPTARG" && pwd) ;;
	O) objdir=$(cd "$OPTARG" && pwd) ;;
	*) exit 2 ;;
	esac
done

# make.py puts the output under MAKEOBJDIRPREFIX/<source root>/amd64.amd64
b=$objdir/obj$objdir/src/amd64.amd64
makefs=$b/tmp/legacy/bin/makefs
mkimg=$b/tmp/legacy/bin/mkimg
for f in $makefs $mkimg $b/sys/V86/kernel $b/stand/i386/pmbr/pmbr \
    $b/stand/i386/gptboot/gptboot $b/stand/i386/loader_lua/loader_lua \
    $b/stand/efi/loader_lua/loader_lua.efi; do
	[ -f "$f" ] || { echo "$f is missing: run distrib/build-freebsd.sh (kernel-toolchain with WITH_DISK_IMAGE_TOOLS_BOOTSTRAP=yes, buildkernel, loader)" >&2; exit 1; }
done

epoch=${SOURCE_DATE_EPOCH:-0}
work=$(mktemp -d "${TMPDIR:-/tmp}/mkimage.XXXXXX")
trap 'rm -rf "$work"' EXIT
root=$work/root

if [ -z "$init" ] && [ ! -f "${extra:-/nonexistent}/sbin/init" ]; then
	clang -static -nostdlib -ffreestanding -fno-stack-protector -fuse-ld=lld \
	    -O2 -Wall -Wl,--build-id=none -o "$work/init" "$here/testinit.c"
	init=$work/init
fi

mkdir -p "$root/boot/kernel" "$root/boot/defaults" "$root/boot/lua" "$root/boot/loader.conf.d" \
    "$root/sbin" "$root/etc" "$root/dev"
cp "$b/sys/V86/kernel" "$root/boot/kernel/kernel"
cp "$b/stand/i386/loader_lua/loader_lua" "$root/boot/loader"
cp "$top"/stand/lua/*.lua "$root/boot/lua/"
cp "$top/stand/defaults/loader.conf" "$root/boot/defaults/loader.conf"
cp "$top/sys/amd64/conf/GENERIC.hints" "$root/boot/device.hints"
[ -z "$init" ] || cp "$init" "$root/sbin/init"
printf '%s\n' '# Device	Mountpoint	FStype	Options	Dump	Pass#' \
    '/dev/gpt/rootfs	/	ufs	ro	1	1' > "$root/etc/fstab"
cat > "$root/boot/loader.conf" <<'EOF'
console="comconsole,vidconsole"
boot_serial="YES"
beastie_disable="YES"
autoboot_delay="0"
hostuuid_load="NO"
screen.textmode="0"
EOF

# the console font: XNU's iso_font (fonts/iso_font_8x16.h) in place of the loader's built-in 8x16
mkdir -p "$root/boot/fonts"
clang -O1 -o "$work/mkvtfont" "$here/mkvtfont.c"
"$work/mkvtfont" "$root/boot/fonts/xnu8x16.fnt"
echo "xnu8x16.fnt:en:XNU iso_font 8x16" > "$root/boot/fonts/INDEX.fonts"

[ -z "$extra" ] || cp -R "$extra"/. "$root"/

# ownership and modes by an mtree spec: the host user must not leak into the image
(cd "$root" && find . | LC_ALL=C sort) | while read -r p; do
	if [ -L "$root/$p" ]; then
		echo "$p type=link uid=0 gid=0 mode=0777"
	elif [ -d "$root/$p" ]; then
		echo "$p type=dir uid=0 gid=0 mode=0755"
	elif [ "$p" = ./etc/master.passwd ]; then
		echo "$p type=file uid=0 gid=0 mode=0600"
	elif [ -x "$root/$p" ]; then
		echo "$p type=file uid=0 gid=0 mode=0555"
	else
		echo "$p type=file uid=0 gid=0 mode=0444"
	fi
done > "$work/root.spec"

# the ESP: FAT with loader.efi as the removable-media boot file
mkdir -p "$work/esp/EFI/BOOT"
cp "$b/stand/efi/loader_lua/loader_lua.efi" "$work/esp/EFI/BOOT/BOOTX64.EFI"

esp=32
bootp=1
rootmib=$((size - esp - bootp - 2))
"$makefs" -T "$epoch" -t msdos -o fat_type=16 -o sectors_per_cluster=1 -o volume_label=EFISYS \
    -s ${esp}m "$work/esp.img" "$work/esp" >/dev/null
"$makefs" -T "$epoch" -t ffs -o version=2 -o softupdates=1 -o label=v86root \
    -F "$work/root.spec" -s ${rootmib}m "$work/root.ufs" "$root" >/dev/null

mkdir -p "$(dirname "$out")"
"$mkimg" -s gpt -b "$b/stand/i386/pmbr/pmbr" -t "$epoch" \
    -p efi:="$work/esp.img" \
    -p freebsd-boot:="$b/stand/i386/gptboot/gptboot" \
    -p freebsd-ufs/rootfs:="$work/root.ufs" \
    -o "$out"
ls -l "$out"
