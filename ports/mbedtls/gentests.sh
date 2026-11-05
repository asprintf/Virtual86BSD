#!/bin/sh
# gentests.sh - on the HOST: generate the C sources of Mbed TLS's unit test
# suites in a Shall system's copy of its sources, so that build.sh -T can
# compile and run them inside.  Mbed TLS writes its test suites as
# .function and .data files that a Python script turns into C; Shall has
# no Python (it isn't POSIX), the host usually has.
#
# Usage: sh gentests.sh [-r root] [dir]
#
#   -r root  the system (default: root/ beside this package's bootstrap.sh)
#   dir      Mbed TLS's sources (default root/usr/src/mbedtls-3.6.7)
#
# Needs python3 (only its standard library).  Writes tests/test_suite_*.c
# and tests/test_suite_*.datax: 140 suites.

usage() { echo "usage: sh gentests.sh [-r root] [dir]" >&2; exit 2; }
here=$(cd "$(dirname "$0")" && pwd)
root=$here/../../root
while getopts r: opt; do
	case $opt in
	r) root=$OPTARG ;;
	*) usage ;;
	esac
done
shift $((OPTIND - 1))
[ $# -le 1 ] || usage
dir=${1:-$root/usr/src/mbedtls-3.6.7}
fail() { echo "gentests.sh: $*" >&2; exit 1; }
command -v python3 >/dev/null 2>&1 || fail "no python3 on this host"
[ -f "$dir/framework/scripts/generate_test_code.py" ] ||
	fail "$dir: no Mbed TLS sources there (sh ports/get.sh mbedtls)"
cd "$dir/tests" || exit 1
n=0
for d in suites/*.data; do
	b=$(basename "$d" .data)
	f=${b%%.*}           # test_suite_x.y.data uses test_suite_x.function
	python3 ../framework/scripts/generate_test_code.py -f "suites/$f.function" -d "$d" \
		-t suites/main_test.function -p suites/host_test.function -s suites \
		--helpers-file suites/helpers.function -o . || fail "generating $b"
	n=$((n + 1))
done
echo "gentests.sh: $n suites in $dir/tests; inside the system: sh /usr/src/shall/ports/mbedtls/build.sh -T"
