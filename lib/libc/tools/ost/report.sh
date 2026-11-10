#!/bin/sh
# report.sh - on the host, after "ost.sh run" in Shall (shut down): take
# the results out of Shall's disk image and make os-test's report of them.
#   sh report.sh IMAGE OSTEST [DIR]
# IMAGE   Shall's disk image
# OSTEST  an os-test directory on the host, where misc/html is built and
#         the results and the report go (root/root/os-test, the one the
#         image was made from, will do)
# DIR     the os-test directory inside Shall (default /root/os-test)
# The results go to OSTEST/out/shall (replacing what was there); the
# report is OSTEST/html/index.html and OSTEST/os-test.json.  Other
# systems' results in OSTEST/out (out/linux from os-test's own "make
# test" on Linux, say) are put beside Shall's.  Needs e2fsprogs (e2fsck,
# debugfs) and a C compiler (CC, default cc) for misc/html.
img=$1 ost=$2 dir=${3:-/root/os-test}
if [ ! -f "$img" ] || [ ! -f "$ost/misc/html.c" ]; then
	echo "usage: sh report.sh IMAGE OSTEST [DIR]" >&2
	exit 2
fi

# the image must be clean before debugfs reads it (1: errors corrected)
e2fsck -p "$img" > /dev/null || [ $? -eq 1 ] || {
	echo "report.sh: e2fsck failed on $img" >&2; exit 1; }

tmp=${TMPDIR:-/tmp}/ost-report.$$
mkdir "$tmp" || exit 1
debugfs -c -R "rdump $dir/out/shall $tmp" "$img" > /dev/null 2>&1
if [ ! -d "$tmp/shall" ]; then
	echo "report.sh: no $dir/out/shall in $img" >&2
	rm -rf "$tmp"
	exit 1
fi
mkdir -p "$ost/out" && rm -rf "$ost/out/shall" && mv "$tmp/shall" "$ost/out/shall" || exit 1
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
