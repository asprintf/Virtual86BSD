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

# ptsrun.sh - on the host: run the Open POSIX Test Suite in V86BSD, with a
# progress bar, and sum up the results.  A wrapper round pts.sh.
#
#   sh ptsrun.sh [-m memory] [-s size] [-t seconds] OBJDIR PTS WORKDIR
#
# OBJDIR   what "make buildworld buildkernel OBJDIR=..." built
# PTS      LTP's testcases/open_posix_testsuite on the host (it is copied)
# WORKDIR  an empty or new directory: root/, v86bsd.img, vm.log and, in the
#          end, results/ (pts.sh's OUT: "results", every test's log) and
#          summary.txt
#   -m       the VM's memory (default 2G)
#   -s       the image's size in MiB (default 2048)
#   -t       seconds one test may take (default 300, as LTP)
#
# V86BSD compiles and runs the tests itself (c17, its own C library, its
# own kernel; KVM where there is some).  The bar is made of the
# "pts: N of TOTAL" lines pts.sh prints.  A run cut short is continued by
# starting it again with the same arguments and WORKDIR (the image is
# kept; tests with a result are not run again).

usage() { echo "usage: sh ptsrun.sh [-m memory] [-s size] [-t seconds] OBJDIR PTS WORKDIR" >&2; exit 2; }

mem=2G size=2048 tmo=300
while getopts m:s:t: opt; do
	case $opt in
	m) mem=$OPTARG ;; s) size=$OPTARG ;; t) tmo=$OPTARG ;; *) usage ;;
	esac
done
shift $((OPTIND - 1))
[ $# -eq 3 ] || usage
here=$(pwd)
abs() { case $1 in /*) echo "$1" ;; *) echo "$here/$1" ;; esac; }
obj=$(abs "$1") pts=$(abs "$2") work=$(abs "$3")
top=$(cd "$(dirname "$0")/../../.." && pwd)
[ -f "$obj/sys/arch/amd64/compile/GENERIC/bsd" ] || {
	echo "ptsrun: no kernel built in $obj" >&2; exit 1; }
[ -f "$pts/lib/common.c" ] || { echo "ptsrun: $pts is not open_posix_testsuite" >&2; exit 1; }

unset MAKEFLAGS
mkdir -p "$work" || exit 1
img=$work/v86bsd.img log=$work/vm.log
if [ ! -f "$img" ]; then
	echo "ptsrun: making the system and the image in $work"
	{
		rm -rf "$work/root"
		cd "$top" &&
		make newroot DESTDIR="$work/root" OBJDIR="$obj" &&
		make installworld distribution installkernel DESTDIR="$work/root" OBJDIR="$obj" &&
		mkdir -p "$work/root/root/pts" &&
		(cd "$pts" && tar cf - .) | (cd "$work/root/root/pts" && tar xf -) &&
		cat > "$work/root/etc/rc" <<RC &&
. /etc/profile
cd /root
sh /usr/src/lib/libc/tools/pts.sh /root/pts /root/pts-out c17 $tmo
echo PTS DONE
RC
		sh mkdisk.sh -s "$size" "$work/root" "$img"
	} > "$work/prep.log" 2>&1 || {
		echo "ptsrun: preparing failed:" >&2; tail -n 20 "$work/prep.log" >&2; exit 1; }
fi

echo "ptsrun: booting ($mem), log $log"
cd "$top" || exit 1
sh boot.sh -k "$work/root/bsd" -m "$mem" "$img" < /dev/null > "$log" 2>&1 &
vm=$!
trap 'kill $vm 2>/dev/null; echo; echo "ptsrun: stopped; run again to continue"; exit 130' 2 15

start=$(date +%s)
tty=no; [ -t 1 ] && tty=yes
last=
while kill -0 $vm 2>/dev/null; do
	line=$(tr -d '\r' < "$log" | grep '^pts: [0-9]* of ' | tail -n 1)
	if [ -n "$line" ] && [ "$line" != "$last" ]; then
		last=$line
		set -- $line	# pts: N of TOTAL (HH:MM:SS)
		pct=$(($2 * 100 / $4))
		fill=$((pct / 5))
		bar=$(printf '%*s' "$fill" '' | tr ' ' '#')$(printf '%*s' $((20 - fill)) '' | tr ' ' '.')
		el=$(( $(date +%s) - start ))
		out=$(printf '[%s] %3d%%  %s/%s  %dm%02ds' "$bar" "$pct" "$2" "$4" $((el / 60)) $((el % 60)))
		if [ $tty = yes ]; then printf '\r\033[K%s' "$out"; else echo "$out"; fi
	fi
	if tr -d '\r' < "$log" | grep -q '^PTS DONE'; then
		sleep 15
		kill $vm 2>/dev/null
		break
	fi
	sleep 3
done
wait $vm 2>/dev/null
[ $tty = yes ] && echo

if ! tr -d '\r' < "$log" | grep -q '^PTS DONE'; then
	echo "ptsrun: the VM stopped before the run was done (see $log);" >&2
	echo "        run again with the same arguments to continue" >&2
	exit 1
fi
echo "ptsrun: done after $(( ($(date +%s) - start) / 60 )) minutes"

# the results out of the image (with its partition table the file system is
# at 1 MiB)
fs=$img
[ "$(dd if="$img" bs=1 skip=510 count=2 2>/dev/null | od -An -tx1 | tr -d ' \n')" = 55aa ] &&
	fs="$img?offset=1048576"
e2fsck -p "$fs" > /dev/null || [ $? -eq 1 ] || { echo "ptsrun: e2fsck failed" >&2; exit 1; }
rm -rf "$work/results" "$work/tmp.x"; mkdir "$work/tmp.x" || exit 1
debugfs -c -R "rdump /root/pts-out $work/tmp.x" "$fs" > /dev/null 2>&1
[ -f "$work/tmp.x/pts-out/results" ] || { echo "ptsrun: no results in the image" >&2; exit 1; }
mv "$work/tmp.x/pts-out" "$work/results" && rmdir "$work/tmp.x"
awk '{ c[$2]++ } END { for (s in c) printf "%-12s %5d\n", s, c[s] }' "$work/results/results" | sort | tee "$work/summary.txt"
echo "ptsrun: $work/results/results (NAME STATUS), summary in $work/summary.txt"
