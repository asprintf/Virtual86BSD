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

# vmrun.sh - on the host: run os-test in V86BSD, with a progress bar, and
# make the report.  A wrapper round ost.sh and report.sh.
#
#   sh vmrun.sh [-m memory] [-s size] [-t seconds] OBJDIR OSTEST WORKDIR
#
# OBJDIR   what "make buildworld buildkernel OBJDIR=..." built
# OSTEST   an os-test checkout on the host (it is copied, not changed)
# WORKDIR  an empty or new directory: root/ (the system, with os-test in
#          /root/os-test), v86bsd.img, vm.log, and in the end
#          os-test/ (the checkout with out/v86bsd, html/index.html and
#          os-test.json)
#   -m       the VM's memory (default 2G)
#   -s       the image's size in MiB (default 3072)
#   -t       seconds one test may take (default 120)
#
# V86BSD builds the tests itself (c17, its own C library), then runs them,
# on KVM where there is some.  This script shows build, then run, as
# "[#####.....] 41%  1234/5816  basic/stdlib/abs  12m30s", from the
# "ost: ..." lines ost.sh prints on the console.  A run that was cut short
# (the VM died, ^C) is started again with the same arguments and WORKDIR:
# the image is kept, and what is done is not done again.  Everything the
# VM printed is in WORKDIR/vm.log.

usage() { echo "usage: sh vmrun.sh [-m memory] [-s size] [-t seconds] OBJDIR OSTEST WORKDIR" >&2; exit 2; }

mem=2G size=3072 tmo=120
while getopts m:s:t: opt; do
	case $opt in
	m) mem=$OPTARG ;; s) size=$OPTARG ;; t) tmo=$OPTARG ;; *) usage ;;
	esac
done
shift $((OPTIND - 1))
[ $# -eq 3 ] || usage
here=$(pwd)
abs() { case $1 in /*) echo "$1" ;; *) echo "$here/$1" ;; esac; }
obj=$(abs "$1") ost=$(abs "$2") work=$(abs "$3")
top=$(cd "$(dirname "$0")/../../../.." && pwd)
[ -f "$obj/sys/arch/amd64/compile/GENERIC/bsd" ] || [ -f "$obj/bsd" ] || {
	echo "vmrun: no kernel built in $obj" >&2; exit 1; }
[ -f "$ost/misc/html.c" ] || { echo "vmrun: $ost is not os-test" >&2; exit 1; }

unset MAKEFLAGS
mkdir -p "$work" || exit 1
img=$work/v86bsd.img log=$work/vm.log
if [ ! -f "$img" ]; then
	echo "vmrun: making the system and the image in $work"
	{
		rm -rf "$work/root" "$work/os-test"
		cd "$top" &&
		make newroot DESTDIR="$work/root" OBJDIR="$obj" &&
		make installworld distribution installkernel DESTDIR="$work/root" OBJDIR="$obj" &&
		mkdir -p "$work/os-test" &&
		(cd "$ost" && tar cf - --exclude=.git .) | (cd "$work/os-test" && tar xf -) &&
		mkdir -p "$work/root/root" && cp -R "$work/os-test" "$work/root/root/os-test" &&
		cat > "$work/root/etc/rc" <<RC &&
. /etc/profile
mount -uw /
cd /root/os-test
TIMEOUT=$tmo
export TIMEOUT
sh /usr/src/lib/libc/tools/ost/ost.sh build
sh /usr/src/lib/libc/tools/ost/ost.sh run
(cd /root/os-test/out && pax -w -x ustar -f /dev/rdisk1 v86bsd)
echo OST DONE
RC
		sh mkdisk.sh -s "$size" "$work/root" "$img"
	} > "$work/prep.log" 2>&1 || {
		echo "vmrun: preparing failed:" >&2; tail -n 20 "$work/prep.log" >&2; exit 1; }
fi

echo "vmrun: booting ($mem), log $log"
cd "$top" || exit 1
# the results go to a second disk, a tar archive (the root is FFS, which the
# host cannot read)
res=$work/results.img
rm -f "$res" && : > "$res" && dd if=/dev/null of="$res" bs=1M seek=512 2>/dev/null
sh boot.sh -k "$work/root/bsd" -m "$mem" "$img" -- \
	-drive "file=$res,format=raw,if=virtio" < /dev/null > "$log" 2>&1 &
vm=$!
trap 'kill $vm 2>/dev/null; echo; echo "vmrun: stopped; run again to continue"; exit 130' 2 15

start=$(date +%s)
tty=no; [ -t 1 ] && tty=yes
last=
while kill -0 $vm 2>/dev/null; do
	line=$(tr -d '\r' < "$log" | grep '^ost: ' | tail -n 1)
	if [ -n "$line" ] && [ "$line" != "$last" ]; then
		last=$line
		set -- $line	# ost: PHASE DONE/TOTAL SUITE/TEST
		case $3 in
		done,) msg="$2 done" pct=100 d=- t=- cur= ;;
		*) d=${3%/*} t=${3#*/}
			pct=$((d * 100 / t)); cur=$4 ;;
		esac
		el=$(( $(date +%s) - start ))
		fill=$((pct / 5))
		bar=$(printf '%*s' "$fill" '' | tr ' ' '#')$(printf '%*s' $((20 - fill)) '' | tr ' ' '.')
		out=$(printf '%-5s [%s] %3d%%  %s/%s  %s  %dm%02ds' "$2" "$bar" "$pct" "$d" "$t" "$cur" $((el / 60)) $((el % 60)))
		if [ $tty = yes ]; then printf '\r\033[K%s' "$out"; else echo "$out"; fi
	fi
	# the run is over: the VM shuts down by itself, if not, stop it
	if tr -d '\r' < "$log" | grep -q '^OST DONE'; then
		sleep 15
		kill $vm 2>/dev/null
		break
	fi
	sleep 3
done
wait $vm 2>/dev/null
[ $tty = yes ] && echo

if ! tr -d '\r' < "$log" | grep -q '^OST DONE'; then
	echo "vmrun: the VM stopped before the run was done (see $log);" >&2
	echo "       run again with the same arguments to continue" >&2
	exit 1
fi
echo "vmrun: done after $(( ($(date +%s) - start) / 60 )) minutes; the report:"
sh "$top/lib/libc/tools/ost/report.sh" "$res" "$work/os-test"
