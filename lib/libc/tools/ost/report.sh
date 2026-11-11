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

# report.sh - on the host, after "ost.sh run" in V86BSD (shut down): take
# the results out of V86BSD's disk image and make os-test's report of them.
#   sh report.sh IMAGE OSTEST [DIR]
# IMAGE   V86BSD's disk image
# OSTEST  an os-test directory on the host, where misc/html is built and
#         the results and the report go (root/root/os-test, the one the
#         image was made from, will do)
# DIR     the os-test directory inside V86BSD (default /root/os-test)
# The results go to OSTEST/out/v86bsd (replacing what was there); the
# report is OSTEST/html/index.html and OSTEST/os-test.json.  Other
# systems' results in OSTEST/out (out/linux from os-test's own "make
# test" on Linux, say) are put beside V86BSD's.  Needs e2fsprogs (e2fsck,
# debugfs) and a C compiler (CC, default cc) for misc/html.
img=$1 ost=$2 dir=${3:-/root/os-test}
if [ ! -f "$img" ] || [ ! -f "$ost/misc/html.c" ]; then
	echo "usage: sh report.sh IMAGE OSTEST [DIR]" >&2
	exit 2
fi

# an image with a partition table (boot.sh, mkdisk.sh since the disklabel
# came) holds the file system at 1 MiB; a bare ext2 image starts at 0
fs=$img
[ "$(dd if="$img" bs=1 skip=510 count=2 2>/dev/null | od -An -tx1 | tr -d ' \n')" = 55aa ] &&
	fs="$img?offset=1048576"

# the image must be clean before debugfs reads it (1: errors corrected)
e2fsck -p "$fs" > /dev/null || [ $? -eq 1 ] || {
	echo "report.sh: e2fsck failed on $img" >&2; exit 1; }

tmp=${TMPDIR:-/tmp}/ost-report.$$
mkdir "$tmp" || exit 1
debugfs -c -R "rdump $dir/out/v86bsd $tmp" "$fs" > /dev/null 2>&1
if [ ! -d "$tmp/v86bsd" ]; then
	echo "report.sh: no $dir/out/v86bsd in $img" >&2
	rm -rf "$tmp"
	exit 1
fi
mkdir -p "$ost/out" && rm -rf "$ost/out/v86bsd" && mv "$tmp/v86bsd" "$ost/out/v86bsd" || exit 1
rmdir "$tmp"

cd "$ost" || exit 1
if [ ! -x misc/html ] || [ misc/html.c -nt misc/html ]; then
	${CC:-cc} misc/html.c -o misc/html || exit 1
fi
oses=$(cd out && for d in *; do [ -d "$d" ] && printf '%s ' "$d"; done)
suites=$(cat misc/suites.list)
misc/html --suites-list="$suites" --os-directory=out --os-list="$oses" || exit 1
misc/html --suites-list="$suites" --os-directory=out --os-list="$oses" \
	--format=json --output=os-test.json || exit 1
echo "report: $ost/html/index.html (and $ost/os-test.json) for: $oses"
