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
# test" on Linux, say) are put beside V86BSD's.  Needs tar and a C compiler
# (CC, default cc) for misc/html.  IMAGE is the results disk vmrun.sh gave
# the VM: a tar archive of out/v86bsd, written by the VM's rc.
img=$1 ost=$2
if [ ! -f "$img" ] || [ ! -f "$ost/misc/html.c" ]; then
	echo "usage: sh report.sh RESULTS-DISK OSTEST" >&2
	exit 2
fi

tmp=${TMPDIR:-/tmp}/ost-report.$$
mkdir "$tmp" || exit 1
tar xf "$img" -C "$tmp" 2> /dev/null
if [ ! -d "$tmp/v86bsd" ]; then
	echo "report.sh: no v86bsd directory in the archive $img" >&2
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
