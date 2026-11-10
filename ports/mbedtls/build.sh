#!/bin/sh
# build.sh - build and install Mbed TLS 3.6 inside a V86BSD system (see
# HOWTO.md beside it).
#
# Usage: sh build.sh [-I] [-t] [-T] [dir]
#
#   dir   Mbed TLS's sources: the release tarball mbedtls-3.6.7.tar.bz2
#         (default /usr/src/mbedtls-3.6.7)
#   -t    run the tests afterwards: the self-tests of every algorithm
#         (programs/test/selftest) and TLS 1.2 and 1.3 handshakes between
#         ssl_server2 and ssl_client2 over the loopback
#   -T    also compile and run the unit test suites (140 suites, some 44 000
#         test cases), whose C sources gentests.sh beside this script
#         generates on the host (it needs Python); about 20 minutes
#   -I    build only, don't install
#
# Mbed TLS's makefiles are GNU make's (ifdef, wildcard, patsubst), so this
# script compiles it itself, with c17 and ar, as they would: the three
# libraries, their test helpers and some programs.  shall.diff
# (beside this script, applied once) turns off the configuration's inline
# assembly, which c17 doesn't have, and replaces gettimeofday().  Installs, under /usr/local: lib/libmbedcrypto.a,
# libmbedx509.a, libmbedtls.a; include/mbedtls/ and include/psa/;
# bin/mbedtls_selftest, mbedtls_ssl_client2, mbedtls_ssl_server2,
# mbedtls_gen_key, mbedtls_cert_app.

usage() { echo "usage: sh build.sh [-I] [-t] [-T] [dir]" >&2; exit 2; }
inst=yes test=no units=no
while getopts ItT opt; do
	case $opt in
	I) inst=no ;;
	t) test=yes ;;
	T) units=yes ;;
	*) usage ;;
	esac
done
shift $((OPTIND - 1))
[ $# -le 1 ] || usage
here=$(cd "$(dirname "$0")" && pwd)
dir=${1:-/usr/src/mbedtls-3.6.7}
CC=${CC:-c17}
CFLAGS=${CFLAGS:--O 1}
fail() { echo "build.sh: $*" >&2; exit 1; }
say() { echo "== $*"; }

[ -f "$dir/include/mbedtls/build_info.h" ] && [ -f "$dir/library/Makefile" ] ||
	fail "$dir: no Mbed TLS sources there (sh ports/get.sh mbedtls, on the host)"
[ -f "$dir/framework/exported.make" ] ||
	fail "$dir: framework/ is empty: take the release tarball (a git clone needs 'git submodule update --init')"
cd "$dir" || exit 1
top=$(pwd)
log=$top/build.log
: > "$log"
run() {   # run what... : append the output to the log, stop on failure
	echo "$*" >> "$log"
	"$@" >> "$log" 2>&1 || fail "failed: $* (see $log)"
}

if grep 'Shall: c17 has no inline assembly' include/mbedtls/mbedtls_config.h >/dev/null; then
	say "shall.diff: already applied"
else
	say "applying shall.diff"
	run patch -p1 -i "$here/shall.diff"
fi

TP="-I $top/3rdparty/everest/include -I $top/3rdparty/everest/include/everest
    -I $top/3rdparty/everest/include/everest/kremlib
    -I $top/3rdparty/p256-m/p256-m/include -I $top/3rdparty/p256-m/p256-m/include/p256-m
    -I $top/3rdparty/p256-m/p256-m_driver_interface"
INC="-I $top/include -I $top/library $TP"
# objs NAME: the objects of OBJS_NAME in library/Makefile
objs() {
	awk -v v="OBJS_$1=" '$1 == v || index($0, v) == 1 { f = 1; next }
		f { for (i = 1; i <= NF; i++) if ($i ~ /\.o$/) print $i
		    if ($0 !~ /\\$/) f = 0 }' library/Makefile
}

say "the libraries (output in $log)"
rm -f library/*.o library/*.a 3rdparty/everest/library/*.o \
	3rdparty/p256-m/*.o 3rdparty/p256-m/p256-m/*.o
x509=$(objs X509) tls=$(objs TLS)
[ -n "$x509" ] && [ -n "$tls" ] || fail "can't read the object lists in library/Makefile"
crypto=
for f in library/*.c; do
	o=$(basename "$f" .c).o
	case " $(echo $x509 $tls) " in *" $o "*) ;; *) crypto="$crypto $o" ;; esac
	# shellcheck disable=SC2086
	run $CC $CFLAGS -D _FILE_OFFSET_BITS=64 $INC -c -o "library/$o" "$f"
done
third=
for f in 3rdparty/everest/library/everest.c 3rdparty/everest/library/x25519.c \
	 3rdparty/everest/library/Hacl_Curve25519_joined.c \
	 3rdparty/p256-m/p256-m_driver_entrypoints.c 3rdparty/p256-m/p256-m/p256-m.c; do
	# shellcheck disable=SC2086
	run $CC $CFLAGS $INC -c -o "${f%.c}.o" "$f"
	third="$third ${f%.c}.o"
done
(
	cd library || exit 1
	# shellcheck disable=SC2086
	ar -rc libmbedcrypto.a $crypto $(for o in $third; do echo "../$o"; done) &&
	ar -rc libmbedx509.a $x509 && ar -rc libmbedtls.a $tls
) >> "$log" 2>&1 || fail "ar failed (see $log)"

say "the test helpers and programs"
TINC="-I $top/tests/include -I $top/framework/tests/include -I $top/include $TP
      -I $top/framework/tests/programs -I $top/programs/ssl -I $top/library"
testobjs=
for f in framework/tests/src/*.c framework/tests/src/drivers/*.c \
	 tests/src/*.c tests/src/test_helpers/*.c; do
	[ -f "$f" ] || continue
	# shellcheck disable=SC2086
	run $CC $CFLAGS -D _FILE_OFFSET_BITS=64 $TINC -c -o "${f%.c}.o" "$f"
	testobjs="$testobjs ${f%.c}.o"
done
LIBS="library/libmbedtls.a library/libmbedx509.a library/libmbedcrypto.a"
prog() {   # prog name source... : a program in programs/
	p=$1; shift
	# shellcheck disable=SC2086
	run $CC $CFLAGS $TINC -o "programs/$p" "$@" $testobjs $LIBS
}
prog test/selftest programs/test/selftest.c
prog pkey/gen_key programs/pkey/gen_key.c
prog x509/cert_app programs/x509/cert_app.c
prog ssl/ssl_client2 programs/ssl/ssl_client2.c programs/ssl/ssl_test_lib.c programs/test/query_config.c
prog ssl/ssl_server2 programs/ssl/ssl_server2.c programs/ssl/ssl_test_lib.c programs/test/query_config.c

if [ $test = yes ]; then
	say "test: selftest (output in selftest.log)"
	programs/test/selftest > selftest.log 2>&1 || fail "selftest failed (see $top/selftest.log)"
	grep 'All tests PASS\|FAIL' selftest.log | sed 's/^ */  /'
	# TLS between ssl_server2 and ssl_client2, with the test certificates
	# built into them; each pair on its own port
	port=14433
	for v in tls12 tls13; do
		say "test: a $v handshake and an HTTP exchange, ssl_server2 <-> ssl_client2"
		programs/ssl/ssl_server2 server_port=$port force_version=$v \
			> server-$v.log 2>&1 &
		spid=$!
		i=0
		while [ $i -lt 50 ] && ! grep 'Waiting for a remote connection' server-$v.log >/dev/null; do
			sleep 1; i=$((i + 1))
		done
		programs/ssl/ssl_client2 server_port=$port force_version=$v \
			server_name=localhost > client-$v.log 2>&1
		st=$?
		kill $spid 2>/dev/null; wait $spid 2>/dev/null
		[ $st = 0 ] && grep 'Protocol is TLSv1.[23]' client-$v.log >/dev/null ||
			fail "$v failed (see $top/client-$v.log, server-$v.log)"
		grep 'Protocol is\|Ciphersuite is' client-$v.log | sed 's/^ *\[ */  /; s/ *\]$//'
		port=$((port + 1))
	done
fi

if [ $units = yes ]; then
	ls tests/test_suite_*.c >/dev/null 2>&1 ||
		fail "no tests/test_suite_*.c: run gentests.sh beside this script on the host first"
	say "the unit test suites (one log each: tests/test_suite_NAME.log)"
	pass=0 bad=0 cases=0 skipped=0
	for f in tests/test_suite_*.c; do
		n=$(basename "$f" .c)
		case $n in
		test_suite_platform_unix)
			# it tests gettimeofday() itself, which POSIX.1-2024 removed
			echo "  $n: not built (it tests gettimeofday)"; continue ;;
		esac
		# shellcheck disable=SC2086
		if ! $CC $CFLAGS -D _FILE_OFFSET_BITS=64 $TINC -o "tests/$n" "$f" $testobjs $LIBS \
			> "tests/$n.log" 2>&1; then
			echo "  $n: COMPILE FAILED (tests/$n.log)"; bad=$((bad + 1)); continue
		fi
		(cd tests && "./$n") > "tests/$n.log" 2>&1
		st=$?
		r=$(tail -n 1 "tests/$n.log")
		case $st:$r in
		0:PASSED*)
			pass=$((pass + 1))
			set -- $(echo "$r" | sed 's/[^0-9][^0-9]*/ /g')
			cases=$((cases + $1)) skipped=$((skipped + $3)) ;;
		*)	echo "  $n: FAILED, status $st: $r"; bad=$((bad + 1)) ;;
		esac
	done
	echo "  $pass suites passed, $bad failed: $cases test cases, $((cases - skipped)) run, $skipped skipped (not in this configuration)"
	[ $bad = 0 ] || fail "unit tests failed"
fi

[ $inst = yes ] || { say "done (not installed)"; exit 0; }
say "install"
p=/usr/local
mkdir -p $p/lib $p/include $p/bin || exit 1
cp library/libmbedcrypto.a library/libmbedx509.a library/libmbedtls.a $p/lib/ &&
rm -rf $p/include/mbedtls $p/include/psa &&
cp -R include/mbedtls include/psa $p/include/ || fail "installing"
for f in test/selftest pkey/gen_key x509/cert_app ssl/ssl_client2 ssl/ssl_server2; do
	cp "programs/$f" "$p/bin/mbedtls_$(basename $f)" || fail "installing $f"
done
say "done: libmbedtls, libmbedx509, libmbedcrypto in $p/lib"
